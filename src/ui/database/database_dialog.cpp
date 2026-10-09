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

#include "core.h"
#include "database_list_data.h"
#include "ui/common/page_scroll_area.h"
#include "ui/common/stale_file.h"
#include "model/data_merge.h"
#include "common/logging.h"
#include "common/editor_settings.h"
#include "ui/common/help.h"
#include "ui/common/view_state.h"
#include "ui/event/event_commands_widget.h"
#include "database_dialog.h"
#include "ui_database_dialog.h"
#include "ui/common/theme_icon.h"
#include <QPushButton>
#include <QInputDialog>
#include <QSettings>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QTimer>
#include <QTreeWidgetItemIterator>
#include <lcf/ldb/reader.h>
#include "database_split_widget.h"

DatabaseDialog::DatabaseDialog(ProjectData& project, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::DatabaseDialog),
	m_project(project)
{
	ui->setupUi(this);
	setProperty("view_state_contextual", true); // Pages and entries belong to this project.
	on_currentActorChanged(nullptr);

	m_projectDataCopy = project;
	m_base = project.database();

	pageActors = new DatabaseSplitWidget<lcf::rpg::Actor>(m_projectDataCopy, this);
	pageClasses = new DatabaseSplitWidget<lcf::rpg::Class>(m_projectDataCopy, this);
	pageSkills = new DatabaseSplitWidget<lcf::rpg::Skill>(m_projectDataCopy, this);
	pageItems = new DatabaseSplitWidget<lcf::rpg::Item>(m_projectDataCopy, this);
	pageEnemies = new DatabaseSplitWidget<lcf::rpg::Enemy>(m_projectDataCopy, this);
	pageEnemyGroups = new DatabaseSplitWidget<lcf::rpg::Troop>(m_projectDataCopy, this);
	pageAttributes = new DatabaseSplitWidget<lcf::rpg::Attribute>(m_projectDataCopy, this);
	pageStates = new DatabaseSplitWidget<lcf::rpg::State>(m_projectDataCopy, this);
	pageBattleAnimations = new DatabaseSplitWidget<lcf::rpg::Animation>(m_projectDataCopy, this);
	pageBattleAnimations2 = new DatabaseSplitWidget<lcf::rpg::BattlerAnimation>(m_projectDataCopy, this);
	pageBattleScreen = new BattleScreenWidget(m_projectDataCopy, this);
	pageTerrain = new DatabaseSplitWidget<lcf::rpg::Terrain>(m_projectDataCopy, this);
	pageChipset = new DatabaseSplitWidget<lcf::rpg::Chipset>(m_projectDataCopy, this);
	pageCommonevents = new DatabaseSplitWidget<lcf::rpg::CommonEvent>(m_projectDataCopy, this);

	pageVocabulary = new VocabularyWidget(m_projectDataCopy, this);
	pageSystem = new SystemWidget(m_projectDataCopy, this);
	pageSystem2  = new System2Widget(m_projectDataCopy, this);
	ui->tabOld_Pages->insertTab(0, pageActors, tr("Characters"));
	ui->tabOld_Pages->insertTab(1, pageClasses, tr("Professions"));
	ui->tabOld_Pages->insertTab(2, pageSkills, tr("Skills"));
	ui->tabOld_Pages->insertTab(3, pageItems, tr("Items"));
	ui->tabOld_Pages->insertTab(4, pageEnemies, tr("Monsters"));
	ui->tabOld_Pages->insertTab(5, pageEnemyGroups, tr("Monster Groups"));
	ui->tabOld_Pages->insertTab(6, pageAttributes, tr("Attributes"));
	ui->tabOld_Pages->insertTab(7, pageStates, tr("Hero Status"));
	ui->tabOld_Pages->insertTab(8, pageBattleAnimations, tr("Battle Animation"));
	ui->tabOld_Pages->insertTab(9, pageBattleAnimations2, tr("Battle Animation 2"));
	ui->tabOld_Pages->insertTab(10, new PageScrollArea(pageBattleScreen), tr("Battle screen"));
	ui->tabOld_Pages->insertTab(11, pageTerrain, tr("Terrain"));
	ui->tabOld_Pages->insertTab(12, pageChipset, tr("Chipset"));
	// these scroll their own contents
	ui->tabOld_Pages->insertTab(13, pageVocabulary, tr("Vocabulary"));
	ui->tabOld_Pages->insertTab(14, pageSystem, tr("System"));
	ui->tabOld_Pages->insertTab(15, pageSystem2, tr("System 2"));
	ui->tabOld_Pages->insertTab(16, pageCommonevents, tr("Common events"));
	// the tabs at the edge are cut: their full name on hover
	for (int i = 0; i < ui->tabOld_Pages->count(); ++i) {
		ui->tabOld_Pages->setTabToolTip(i, ui->tabOld_Pages->tabText(i));
	}

	// the "new style" page is an unfinished experiment: one database window
	ui->toolSwitchStyle->hide();
	ui->toolNew_CharacterAddProperty->setIcon(themeIcon(this, QStringLiteral("new")));
	ui->toolNew_CharacterDeleteProperty->setIcon(themeIcon(this, QStringLiteral("edit-delete")));
	ui->toolNew_CharacterEditProperty->setIcon(themeIcon(this, QStringLiteral("properties")));

	m_viewStateKey = QStringLiteral("database_views/%1").arg(QString::fromLatin1(QCryptographicHash::hash(
		project.project().projectDir().absolutePath().toUtf8(), QCryptographicHash::Sha256).toHex()));
	m_viewStates = QSettings().value(m_viewStateKey).toMap();
	int lastPage = m_viewStates.value(QStringLiteral("active_page"),
		QSettings().value(QStringLiteral("database_page"), 0)).toInt();
	if (lastPage < 0 || lastPage >= ui->tabOld_Pages->count()) lastPage = 0;
	if (ui->tabOld_Pages->currentIndex() == lastPage) on_tabOld_Pages_currentChanged(lastPage);
	else ui->tabOld_Pages->setCurrentIndex(lastPage);
	connect(this, &QDialog::finished, this, [this] {
		m_viewStates.insert(QString::number(m_activePage), ViewState::capture(ui->tabOld_Pages->currentWidget()));
		m_viewStates.insert(QStringLiteral("active_page"), ui->tabOld_Pages->currentIndex());
		QSettings().setValue(m_viewStateKey, m_viewStates);
		QSettings().setValue(QStringLiteral("database_page"), ui->tabOld_Pages->currentIndex());
	});
	ui->stackedStyle->setCurrentWidget(ui->pageOld);
	connect(&EditorSettings::instance(), &EditorSettings::localStateReset, this, [this] {
		m_viewStates.clear();
		m_activePage = -1;
		ui->tabOld_Pages->setCurrentIndex(0);
		m_activePage = 0;
		ViewState::reset(this);
	});
}

DatabaseDialog::~DatabaseDialog()
{
	delete ui;
}

void DatabaseDialog::selectCommonEvent(int eventID, int line)
{
	ui->tabOld_Pages->setCurrentWidget(pageCommonevents);
	// The saved view restores on the next event-loop turn; explicit navigation follows it.
	QTimer::singleShot(0, this, [this, eventID, line] {
		auto* list = pageCommonevents->listWidget();
		for (int row = 0; row < list->model()->rowCount(); ++row) {
			const QModelIndex index = list->model()->index(row, 0);
			if (index.data(ModelDataId).toInt() != eventID) continue;
			list->setCurrentIndex(index);
			list->scrollTo(index);
			if (line > 0) {
				auto* commands = pageCommonevents->contentWidget()->findChild<EventCommandsWidget*>(QStringLiteral("commands"));
				for (QTreeWidgetItemIterator item(commands); *item; ++item) {
					if ((*item)->text(1).toInt() == line - 1) {
						commands->setCurrentItem(*item);
						commands->scrollToItem(*item);
						commands->setFocus();
						break;
					}
				}
			}
			break;
		}
	});
}

void DatabaseDialog::on_currentActorChanged(lcf::rpg::Actor */* actor */)
{
#if 0
	m_currentActor = actor;
	if (actor == nullptr){
		/* Clear Table */
		for (int i = 0; i < ui->tableNew_CharacterProperties->rowCount(); i++)
			ui->tableNew_CharacterProperties->item(i,1)->setText("");
		/* Disable Table */
		ui->tableNew_CharacterProperties->setEnabled(false);
		return;
	}

	/* Fill Table */
	QString yes = tr("YES");
	QString no = tr("NO");
	ui->tableNew_CharacterProperties->item(0,1)->setText(actor->name.c_str());
	ui->tableNew_CharacterProperties->item(1,1)->setText(actor->title.c_str());
	ui->tableNew_CharacterProperties->item(2,1)->setText(QString::number(actor->initial_level));
	ui->tableNew_CharacterProperties->item(3,1)->setText(QString::number(actor->final_level));
	ui->tableNew_CharacterProperties->item(4,1)->setText(actor->critical_hit ? yes : no);
	ui->tableNew_CharacterProperties->item(5,1)->setText(tr("1 of %1 turns").arg(actor->critical_hit_chance));
	ui->tableNew_CharacterProperties->item(6,1)->setText(actor->two_weapon ? yes :no);
	ui->tableNew_CharacterProperties->item(7,1)->setText(actor->lock_equipment ? yes :no);
	ui->tableNew_CharacterProperties->item(8,1)->setText(actor->auto_battle ? yes :no);
	ui->tableNew_CharacterProperties->item(9,1)->setText(actor->super_guard ? yes :no);
	ui->tableNew_CharacterProperties->item(10,1)->setText(actor->class_id < 1 ? tr("<none>") :
														  actor->class_id >= static_cast<int>(m_dataCopy.classes.size()) ?
														  tr("<%1?>").arg(actor->class_id) :
														  m_dataCopy.classes[static_cast<size_t>(actor->class_id)-1].name.c_str());
	ui->tableNew_CharacterProperties->item(11,1)->setText(QString("%1[%2]")
														  .arg(actor->face_name.c_str())
														  .arg(actor->face_index));
	ui->tableNew_CharacterProperties->item(12,1)->setText(QString("%1[%2]%3")
														  .arg(actor->character_name.c_str())
														  .arg(actor->character_index)
														  .arg(actor->transparent ? " [Transparent]" : ""));
	ui->tableNew_CharacterProperties->item(13,1)->setText(actor->battler_animation < 1 ? tr("<none>") :
														  actor->battler_animation >= static_cast<int>(m_dataCopy.battleranimations.size()) ?
														  tr("<%1?>").arg(actor->battler_animation) :
														  m_dataCopy.battleranimations[static_cast<size_t>(actor->battler_animation) - 1].name.c_str());

	/* TODO: fill the following information */
	ui->tableNew_CharacterProperties->item(14,1)->setText("Click to Edit");
	ui->tableNew_CharacterProperties->item(15,1)->setText("Click to Edit");
	ui->tableNew_CharacterProperties->item(16,1)->setText("Click to Edit");
	ui->tableNew_CharacterProperties->item(17,1)->setText("Click to Edit");
	ui->tableNew_CharacterProperties->item(18,1)->setText("Click to Edit");
	ui->tableNew_CharacterProperties->item(19,1)->setText("Click to Edit");
	ui->tableNew_CharacterProperties->item(20,1)->setText("Click to Edit");
	ui->tableNew_CharacterProperties->item(21,1)->setText("ItemName");
	ui->tableNew_CharacterProperties->item(22,1)->setText("ItemName");
	ui->tableNew_CharacterProperties->item(23,1)->setText("ItemName");
	ui->tableNew_CharacterProperties->item(24,1)->setText("ItemName");
	ui->tableNew_CharacterProperties->item(25,1)->setText("ItemName");
	ui->tableNew_CharacterProperties->item(26,1)->setText("AnimName");
	ui->tableNew_CharacterProperties->item(27,1)->setText("Click to Edit");
	ui->tableNew_CharacterProperties->item(28,1)->setText("Click to Edit");
	ui->tableNew_CharacterProperties->item(29,1)->setText("Click to Edit");

	/* Enable Table */
	ui->tableNew_CharacterProperties->setEnabled(true);
#endif
}

void DatabaseDialog::on_tabOld_Pages_currentChanged(int index)
{
	if (!m_viewStateKey.isEmpty()) {
		if (m_activePage >= 0) m_viewStates.insert(QString::number(m_activePage), ViewState::capture(ui->tabOld_Pages->widget(m_activePage)));
		m_activePage = index;
		ViewState::restore(ui->tabOld_Pages->widget(index), m_viewStates.value(QString::number(index)).toMap());
	}
	ui->listNew_Pages->setCurrentRow(index);
	emit ui->listNew_Pages->currentRowChanged(index);
}

void DatabaseDialog::on_toolSwitchStyle_clicked(bool checked)
{
	ui->stackedStyle->setCurrentIndex(static_cast<int>(checked));
}

bool DatabaseDialog::mergeFromDisk(QStringList* conflicts)
{
	auto theirs = core().project()->readDatabase();
	if (!theirs) {
		return false;
	}
	lcf::rpg::Database merged;
	const auto result = DataMerge::database(m_base, m_projectDataCopy.database(), *theirs, merged);
	if (!result.ok()) {
		*conflicts = result.conflicts;
		return false;
	}
	qCInfo(lcReload) << "database merged with the changes on disk";
	// the pages show the merge: their lists are told what changed
	ProjectData copy = m_projectDataCopy;
	copy.database() = std::move(merged);
	DatabaseListNotifier::assign(m_projectDataCopy, copy, this);
	m_base = std::move(*theirs);
	return true;
}

void DatabaseDialog::on_buttonBox_clicked(QAbstractButton *button)
{
	switch(ui->buttonBox->standardButton(button))
	{
		// Standard buttons:
		case QDialogButtonBox::Apply:
		case QDialogButtonBox::Ok: {
			// Another program (rpgsync...) changed the database since it was
			// read: never replace its changes without asking
			// Another program (rpgsync...) changed the database since it was
			// read: what it changed is merged entry by entry; an entry both
			// sides changed is asked, never replaced silently
			auto& watcher = core().watcher();
			QStringList conflicts;
			if (watcher.isStale(core().project()->databaseFile()) && !mergeFromDisk(&conflicts)) {
				switch (askStale(this, tr("The database"), true, conflicts)) {
				case StaleChoice::Cancel:
					return;
				case StaleChoice::Reload:
					reject();
					watcher.requestReload(ProjectWatcher::Database);
					return;
				case StaleChoice::Overwrite:
					break;
				}
			}
			// the windows showing the project list what changed again
			DatabaseListNotifier::assign(m_project, m_projectDataCopy, this);
			lcf::LDB_Reader::PrepareSave(m_project.database());
			if (!core().project()->saveDatabase()) {
				QMessageBox::critical(this, tr("Database"), tr("The database could not be written."));
				return;
			}
			m_base = m_project.database();
			if (ui->buttonBox->standardButton(button) == QDialogButtonBox::Ok) {
				accept();
			}
		}
		break;
		case QDialogButtonBox::Help: {
			const QWidget* page = ui->tabOld_Pages->currentWidget();
			const std::vector<std::pair<const QWidget*, const char*>> topics = {
				{pageActors, "database-heroes"}, {pageClasses, "database-classes"}, {pageSkills, "database-skills"},
				{pageItems, "database-items"}, {pageEnemies, "database-monsters"}, {pageEnemyGroups, "database-monster-groups"},
				{pageAttributes, "database-attributes"}, {pageStates, "database-conditions"},
				{pageBattleAnimations, "database-battle-animations"}, {pageBattleAnimations2, "database-battler-animations"},
				{pageBattleScreen, "database-battle-screen"}, {pageTerrain, "database-terrain"}, {pageChipset, "database-chipset"},
				{pageVocabulary, "database-vocabulary"}, {pageSystem, "database-system"}, {pageSystem2, "database-system2"},
				{pageCommonevents, "database-common-events"},
			};
			QString topic = QStringLiteral("database");
			for (const auto& [widget, name] : topics) {
				// the page, or the scroll area showing it
				if (widget == page || page->isAncestorOf(widget)) {
					topic = QString::fromLatin1(name);
				}
			}
			Help::open(this, topic);
		}
		break;
		default:
			// shouldn't happen
			break;
	}
}
void DatabaseDialog::on_pushNew_CharacterMax_clicked()
{
	/* TODO: resize characters */
}

void DatabaseDialog::on_lineNew_CharacterFilter_textChanged(const QString &arg1)
{
	for (int i = 0; i < ui->listNew_Character->count(); i++)
		ui->listNew_Character->item(i)->setHidden(ui->listNew_Character->item(i)->text().contains(arg1,Qt::CaseInsensitive));
}

void DatabaseDialog::on_listNew_Character_currentRowChanged(int currentRow)
{
	auto& database = m_projectDataCopy.database();

	if (currentRow < 0 || currentRow >= static_cast<int>(database.actors.size()))
	{
		on_currentActorChanged(nullptr);
		emit currentActorChanged(nullptr);
		return; //invalid
	}

	on_currentActorChanged(&database.actors[static_cast<size_t>(currentRow)]);
	emit currentActorChanged(&database.actors[static_cast<size_t>(currentRow)]);
}
