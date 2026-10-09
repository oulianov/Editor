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
#include "event_page_widget.h"
#include "core.h"
#include "common/dbstring.h"


static const char GEOMETRY_KEY[] = "event_dialog_geometry";

EventDialog::EventDialog(ProjectData& project, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::EventDialog),
	m_project(project)
{
	ui->setupUi(this);
	lst_result = QDialogButtonBox::Cancel;

	// Open with the size of last time, within the screen
	QSettings settings;
	if (!restoreGeometry(settings.value(GEOMETRY_KEY).toByteArray())) {
		resize(980, 640);
	}
	if (const QScreen* screen = parent ? parent->screen() : this->screen()) {
		const QSize available = screen->availableGeometry().size() * 0.95;
		resize(size().boundedTo(available));
	}
	connect(this, &QDialog::finished, this, [this] {
		QSettings().setValue(GEOMETRY_KEY, saveGeometry());
	});
	connect(ui->buttonBox->button(QDialogButtonBox::Ok),
			SIGNAL(clicked()),
			this,
			SLOT(ok()));
	connect(ui->buttonBox->button(QDialogButtonBox::Apply),
			SIGNAL(clicked()),
			this,
			SLOT(apply()));
}

EventDialog::~EventDialog()
{
	delete ui;
}

int EventDialog::edit(QWidget *parent, lcf::rpg::Event& event, ProjectData& project)
{
	// FIXME: Use WidgetAsDialogWrapper instead of this function
	EventDialog dlg(project, parent);
	dlg.setEvent(event);
	dlg.exec();
	if (dlg.lst_result != QDialogButtonBox::Cancel)
		event = dlg.getEvent();
	return dlg.lst_result;
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
	m_event = event;
	ui->lineName->setText(ToQString(m_event.name));
	this->setWindowTitle(QString("EV: %1").arg(m_event.ID));
	refreshEventPageTabs();
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
	return lst_result != QDialogButtonBox::Cancel || !equalEvents(current, r_event);
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
	if (equalEvents(a_event, r_event))
		lst_result = QDialogButtonBox::Cancel;
	else
		lst_result = QDialogButtonBox::Apply;
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
	int cur_index = ui->tabEventPages->currentIndex();
	m_event.pages.insert(m_event.pages.begin() + cur_index + 1, lcf::rpg::EventPage());
	refreshEventPageTabs();
	ui->tabEventPages->setCurrentIndex(cur_index + 1);
}

void EventDialog::on_pushCopyPage_clicked() {
	event_page_clipboard = m_event.pages[ui->tabEventPages->currentIndex()];
	ui->pushPastePage->setEnabled(true);
}

void EventDialog::on_pushPastePage_clicked() {
	int cur_index = ui->tabEventPages->currentIndex();
	m_event.pages.insert(m_event.pages.begin() + cur_index + 1, event_page_clipboard);
	refreshEventPageTabs();
	ui->tabEventPages->setCurrentIndex(cur_index + 1);
}

void EventDialog::on_pushDeletePage_clicked() {
	int cur_index = ui->tabEventPages->currentIndex();
	m_event.pages.erase(m_event.pages.begin() + cur_index);
	refreshEventPageTabs();
	ui->tabEventPages->setCurrentIndex(std::max(0, cur_index - 1));
}

void EventDialog::on_pushClearPage_clicked() {
	int cur_index = ui->tabEventPages->currentIndex();
	lcf::rpg::EventPage empty;
	empty.ID = m_event.pages[cur_index].ID;
	m_event.pages[cur_index] = empty;
	refreshEventPageTabs();
	ui->tabEventPages->setCurrentIndex(cur_index);
}

void EventDialog::refreshEventPageTabs() {
	while (ui->tabEventPages->count() > 0) {
		QWidget* page = ui->tabEventPages->widget(0);
		ui->tabEventPages->removeTab(0);
		page->deleteLater();
	}
	for (unsigned int i = 0; i < m_event.pages.size(); i++)
	{
		EventPageWidget *tab = new EventPageWidget(m_project, this);
		tab->setEventPage(&(m_event.pages[i]));
		ui->tabEventPages->addTab(tab, QString::number(i+1));
	}
	ui->pushDeletePage->setEnabled(m_event.pages.size() >= 2);
}
