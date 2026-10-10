/*
 * This file is part of EasyRPG Editor.
 *
 * EasyRPG Editor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Editor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Editor. If not, see <http://www.gnu.org/licenses/>.
 */

#include "event_dialog.h"
#include "ui_event_dialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QCryptographicHash>
#include "event_page_widget.h"
#include "event_commands_widget.h"
#include "common/script_link.h"
#include <QShortcut>
#include <QTimer>
#include "core.h"
#include "common/editor_settings.h"
#include "ui/common/help.h"
#include "ui/common/view_state.h"
#include <QPushButton>
#include "common/dbstring.h"


static const char GEOMETRY_KEY[] = "event_dialog_geometry";
// RPG Maker's limit of pages in an event
static constexpr int MAX_PAGES = 100;

std::optional<lcf::rpg::EventPage> EventDialog::s_pageClipboard;

EventDialog::EventDialog(ProjectData& project, QWidget *parent, int mapID) :
	QDialog(parent),
	ui(new Ui::EventDialog),
	m_project(project), m_mapID(mapID)
{
	ui->setupUi(this);
	setProperty("view_state_contextual", true); // Pages belong to the selected event.
	connect(&EditorSettings::instance(), &EditorSettings::localStateReset, this, [this] {
		m_activePage = -1;
		m_pageViewStates.assign(m_pageViewStates.size(), QVariantMap{});
		ui->tabEventPages->setCurrentIndex(0);
		m_activePage = 0;
		ViewState::reset(this);
	});
	lst_result = QDialogButtonBox::Cancel;

	// The shared dialog state restores the saved size after the first show.
	resize(980, 640);
	setProperty("dialog_geometry_key", QString::fromLatin1(GEOMETRY_KEY));
	if (const QScreen* screen = parent ? parent->screen() : this->screen()) {
		const QSize available = screen->availableGeometry().size() * 0.95;
		resize(size().boundedTo(available));
	}
	connect(this, &QDialog::finished, this, [this] {
		QSettings().setValue(GEOMETRY_KEY, saveGeometry());
		if (!m_viewStateKey.isEmpty()) {
			if (m_activePage >= 0 && m_activePage < static_cast<int>(m_pageViewStates.size()))
				m_pageViewStates[m_activePage] = ViewState::capture(ui->tabEventPages->currentWidget());
			QVariantList pages;
			for (const auto& state : m_pageViewStates) pages.append(state);
			QSettings().setValue(m_viewStateKey, QVariantMap{{QStringLiteral("page"), m_activePage}, {QStringLiteral("views"), pages}});
		}
	});
	connect(ui->buttonBox->button(QDialogButtonBox::Ok),
			SIGNAL(clicked()),
			this,
			SLOT(ok()));
	connect(ui->buttonBox->button(QDialogButtonBox::Apply),
			SIGNAL(clicked()),
			this,
			SLOT(apply()));
	// the script of the page, for games edited with rpgsync
	if (ScriptLink::available() && m_mapID > 0) {
		auto* script = ui->buttonBox->addButton(tr("Open Script"), QDialogButtonBox::ActionRole);
		script->setObjectName(QStringLiteral("openScript"));
		script->setToolTip(tr("Open the script of this page at the selected command (Ctrl+Shift+O in the list)"));
		connect(script, &QPushButton::clicked, this, [this] {
			QWidget* page = ui->tabEventPages->currentWidget();
			if (auto* commands = page ? page->findChild<EventCommandsWidget*>(QStringLiteral("commands")) : nullptr) {
				commands->openScript();
			}
		});
	}
	connect(ui->buttonBox->button(QDialogButtonBox::Help), &QPushButton::clicked, this, [this] {
		Help::open(this, QStringLiteral("event-editor"));
	});

	// PgUp / PgDn switch pages like in RPG Maker, Ctrl+Tab as in other tabs
	connect(new QShortcut(QKeySequence(Qt::Key_PageUp), this), &QShortcut::activated, this, [this] { switchPage(-1); });
	connect(new QShortcut(QKeySequence::PreviousChild, this), &QShortcut::activated, this, [this] { switchPage(-1); });
	connect(new QShortcut(QKeySequence(Qt::Key_PageDown), this), &QShortcut::activated, this, [this] { switchPage(1); });
	connect(new QShortcut(QKeySequence::NextChild, this), &QShortcut::activated, this, [this] { switchPage(1); });
	updatePageButtons();
}

void EventDialog::switchPage(int step)
{
	const int count = ui->tabEventPages->count();
	if (count > 0) {
		ui->tabEventPages->setCurrentIndex(std::clamp(ui->tabEventPages->currentIndex() + step, 0, count - 1));
	}
}

void EventDialog::updatePageButtons()
{
	const bool room = m_event.pages.size() < static_cast<size_t>(MAX_PAGES);
	ui->pushNewPage->setEnabled(room);
	ui->pushPastePage->setEnabled(room && s_pageClipboard.has_value());
	ui->pushDeletePage->setEnabled(m_event.pages.size() >= 2);
}

void EventDialog::showCommand(int pageID, int line)
{
	// the pages are numbered from 1 in order (refreshEventPageTabs)
	const int index = pageID - 1;
	if (index < 0 || index >= ui->tabEventPages->count()) {
		return;
	}
	ui->tabEventPages->setCurrentIndex(index);
	if (line <= 0) {
		return;
	}
	// after the saved view of the page, restored on the next event-loop turn
	QTimer::singleShot(0, this, [this, index, line] {
		auto* page = ui->tabEventPages->widget(index);
		if (auto* commands = page ? page->findChild<EventCommandsWidget*>(QStringLiteral("commands")) : nullptr) {
			commands->selectIndex(line - 1);
			commands->setFocus();
		}
	});
}

EventDialog::~EventDialog()
{
	delete ui;
}

bool EventDialog::equalEvents(const lcf::rpg::Event &e1, const lcf::rpg::Event &e2)
{
#define chk(property) if (e1.property != e2.property) return false
	if (e1.ID != e2.ID) return false; //Prevents warning
	chk(name);
	chk(x);
	chk(y);
	chk(pages.size());
	for (unsigned int i = 0; i < e1.pages.size(); i++)
	{
		chk(pages[i].condition.flags.switch_a);
		chk(pages[i].condition.flags.switch_b);
		chk(pages[i].condition.flags.variable);
		chk(pages[i].condition.flags.item);
		chk(pages[i].condition.flags.actor);
		chk(pages[i].condition.flags.timer);
		chk(pages[i].condition.flags.timer2);
		chk(pages[i].condition.switch_a_id);
		chk(pages[i].condition.switch_b_id);
		chk(pages[i].condition.variable_id);
		chk(pages[i].condition.variable_value);
		chk(pages[i].condition.item_id);
		chk(pages[i].condition.actor_id);
		chk(pages[i].condition.timer_sec);
		chk(pages[i].condition.timer2_sec);
		chk(pages[i].condition.compare_operator);
		chk(pages[i].character_name);
		chk(pages[i].character_index);
		chk(pages[i].character_direction);
		chk(pages[i].character_pattern);
		chk(pages[i].translucent);
		chk(pages[i].move_type);
		chk(pages[i].move_frequency);
		chk(pages[i].trigger);
		chk(pages[i].layer);
		chk(pages[i].overlap_forbidden);
		chk(pages[i].animation_type);
		chk(pages[i].move_speed);
		chk(pages[i].move_route.repeat);
		chk(pages[i].move_route.skippable);
		chk(pages[i].move_route.move_commands.size());
		for (unsigned int j = 0; j < e1.pages[i].move_route.move_commands.size(); j++)
		{
			chk(pages[i].move_route.move_commands[j].command_id);
			chk(pages[i].move_route.move_commands[j].parameter_string);
			chk(pages[i].move_route.move_commands[j].parameter_a);
			chk(pages[i].move_route.move_commands[j].parameter_b);
			chk(pages[i].move_route.move_commands[j].parameter_c);
		}
		chk(pages[i].event_commands.size());
		for (unsigned int j = 0; j < e1.pages[i].event_commands.size(); j++)
		{
			chk(pages[i].event_commands[j].code);
			chk(pages[i].event_commands[j].indent);
			chk(pages[i].event_commands[j].string);
			chk(pages[i].event_commands[j].parameters.size());
			for (unsigned int k = 0; k < e1.pages[i].event_commands[j].parameters.size(); k++)
				chk(pages[i].event_commands[j].parameters[k]);
		}
	}
	return true;
#undef chk
}

lcf::rpg::Event EventDialog::getEvent() const
{
	switch(lst_result)
	{
	case (QDialogButtonBox::Apply):
		return a_event;
	case (QDialogButtonBox::Ok):
		return m_event;
	}
	return r_event;
}

void EventDialog::setEvent(const lcf::rpg::Event& event)
{
	int lastPage = ui->tabEventPages->currentIndex();
	if (m_viewStateKey.isEmpty() || m_event.ID != event.ID) {
		m_viewStateKey = QStringLiteral("event_views/%1/%2/%3").arg(QString::fromLatin1(QCryptographicHash::hash(
			m_project.project().projectDir().absolutePath().toUtf8(), QCryptographicHash::Sha256).toHex())).arg(m_mapID).arg(event.ID);
		const auto state = QSettings().value(m_viewStateKey).toMap();
		lastPage = state.value(QStringLiteral("page"), 0).toInt();
		m_pageViewStates.clear();
		for (const auto& page : state.value(QStringLiteral("views")).toList()) m_pageViewStates.push_back(page.toMap());
	} else if (m_activePage >= 0 && m_activePage < static_cast<int>(m_pageViewStates.size())) {
		m_pageViewStates[m_activePage] = ViewState::capture(ui->tabEventPages->currentWidget());
	}
	m_pageViewStates.resize(event.pages.size());
	m_event = event;
	ui->lineName->setText(ToQString(m_event.name));
	this->setWindowTitle(QString("EV: %1").arg(m_event.ID));
	refreshEventPageTabs();
	ui->tabEventPages->setCurrentIndex(std::clamp(lastPage, 0, std::max(0, ui->tabEventPages->count() - 1)));
	// after the pages, which normalize the command indentation
	r_event = m_event;
}

int EventDialog::editResult() const
{
	return lst_result;
}

bool EventDialog::isModified() const
{
	lcf::rpg::Event current = m_event;
	current.name = ToDBString(ui->lineName->text());
	return !equalEvents(current, r_event);
}

void EventDialog::reloadEvent(const lcf::rpg::Event* event)
{
	if (!event) {
		QMessageBox::information(this,
			tr("Event removed"),
			tr("This event was removed from the map by another program."));
		lst_result = QDialogButtonBox::Cancel;
		reject();
		return;
	}

	if (isModified()) {
		auto answer = QMessageBox::question(this,
			tr("Event changed on disk"),
			tr("This event was changed by another program.\n"
			   "Show the new version and discard your changes?"),
			QMessageBox::Yes | QMessageBox::No);
		if (answer != QMessageBox::Yes) {
			return;
		}
	}

	int page = ui->tabEventPages->currentIndex();
	lst_result = QDialogButtonBox::Cancel;
	setEvent(*event);
	ui->tabEventPages->setCurrentIndex(std::min(page, ui->tabEventPages->count() - 1));
}

void EventDialog::apply()
{
	m_event.name = ToDBString(ui->lineName->text());
	a_event = m_event;
	if (equalEvents(a_event, r_event)) {
		return;
	}
	// Put it on the map now, like RPG Maker; later changes compare to it
	lst_result = QDialogButtonBox::Apply;
	r_event = a_event;
	emit applied(a_event);
}

void EventDialog::ok()
{
	m_event.name = ToDBString(ui->lineName->text());
	if (equalEvents(m_event, r_event))
		lst_result = QDialogButtonBox::Cancel;
	else
		lst_result = QDialogButtonBox::Ok;
}

void EventDialog::on_pushNewPage_clicked() {
	if (m_event.pages.size() >= static_cast<size_t>(MAX_PAGES)) {
		return;
	}
	int cur_index = ui->tabEventPages->currentIndex();
	if (cur_index >= 0 && cur_index < static_cast<int>(m_pageViewStates.size()))
		m_pageViewStates[cur_index] = ViewState::capture(ui->tabEventPages->currentWidget());
	m_pageViewStates.insert(m_pageViewStates.begin() + cur_index + 1, QVariantMap{});
	m_event.pages.insert(m_event.pages.begin() + cur_index + 1, lcf::rpg::EventPage());
	refreshEventPageTabs();
	ui->tabEventPages->setCurrentIndex(cur_index + 1);
}

void EventDialog::on_pushCopyPage_clicked() {
	s_pageClipboard = m_event.pages[ui->tabEventPages->currentIndex()];
	updatePageButtons();
}

void EventDialog::on_pushPastePage_clicked() {
	if (!s_pageClipboard || m_event.pages.size() >= static_cast<size_t>(MAX_PAGES)) {
		return;
	}
	int cur_index = ui->tabEventPages->currentIndex();
	m_pageViewStates[cur_index] = ViewState::capture(ui->tabEventPages->currentWidget());
	m_pageViewStates.insert(m_pageViewStates.begin() + cur_index + 1, QVariantMap{});
	m_event.pages.insert(m_event.pages.begin() + cur_index + 1, *s_pageClipboard);
	refreshEventPageTabs();
	ui->tabEventPages->setCurrentIndex(cur_index + 1);
}

void EventDialog::on_pushDeletePage_clicked() {
	int cur_index = ui->tabEventPages->currentIndex();
	m_pageViewStates.erase(m_pageViewStates.begin() + cur_index);
	m_event.pages.erase(m_event.pages.begin() + cur_index);
	refreshEventPageTabs();
	ui->tabEventPages->setCurrentIndex(std::max(0, cur_index - 1));
}

void EventDialog::on_pushClearPage_clicked() {
	int cur_index = ui->tabEventPages->currentIndex();
	m_pageViewStates[cur_index].clear();
	lcf::rpg::EventPage empty;
	empty.ID = m_event.pages[cur_index].ID;
	m_event.pages[cur_index] = empty;
	refreshEventPageTabs();
	ui->tabEventPages->setCurrentIndex(cur_index);
}

void EventDialog::refreshEventPageTabs() {
	m_refreshingPages = true;
	while (ui->tabEventPages->count() > 0) {
		QWidget* page = ui->tabEventPages->widget(0);
		ui->tabEventPages->removeTab(0);
		page->deleteLater();
	}
	for (unsigned int i = 0; i < m_event.pages.size(); i++)
	{
		// numbered in order, as RPG Maker saves them (new and pasted pages too)
		m_event.pages[i].ID = static_cast<int>(i + 1);
		EventPageWidget *tab = new EventPageWidget(m_project, this);
		tab->setEventPage(&(m_event.pages[i]));
		if (auto* commands = tab->findChild<EventCommandsWidget*>(QStringLiteral("commands"))) {
			commands->setScriptLocation({QStringLiteral("--map"), QString::number(m_mapID), QStringLiteral("--event"),
				QString::number(m_event.ID), QStringLiteral("--page"), QString::number(i + 1)});
		}
		ui->tabEventPages->addTab(tab, QString::number(i+1));
	}
	updatePageButtons();
	m_refreshingPages = false;
	m_activePage = -1;
	on_tabEventPages_currentChanged(ui->tabEventPages->currentIndex());
}

void EventDialog::on_tabEventPages_currentChanged(int index) {
	if (m_refreshingPages) return;
	if (m_activePage >= 0 && m_activePage < static_cast<int>(m_pageViewStates.size()))
		m_pageViewStates[m_activePage] = ViewState::capture(ui->tabEventPages->widget(m_activePage));
	m_activePage = index;
	if (index >= 0 && index < static_cast<int>(m_pageViewStates.size()))
		ViewState::restore(ui->tabEventPages->widget(index), m_pageViewStates[index]);
}
