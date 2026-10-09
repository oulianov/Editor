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

#include "search_dialog.h"
#include "ui_search_dialog.h"
#include "ui/main_window.h"
#include "common/dbstring.h"
#include "stringizer.h"

#include <QCompleter>
#include <QHeaderView>
#include <QRegularExpression>
#include <QSet>
#include <QStringListModel>
#include <algorithm>
#include <tuple>

SearchDialog::SearchDialog(ProjectData& project, QWidget *parent) :
	QDialog(parent), ui(new Ui::SearchDialog), m_project(project)
{
	ui->setupUi(this);
	ui->list_result->setColumnCount(5);
	ui->list_result->setHorizontalHeaderLabels({tr("Map"), tr("Event"), tr("Event Page"), tr("Source Line"), tr("Match")});
	ui->list_result->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->list_result->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->list_result->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->list_result->horizontalHeader()->setStretchLastSection(true);
	ui->list_result->verticalHeader()->hide();
	ui->search_text->setClearButtonEnabled(true);
	auto* completer = new QCompleter(new QStringListModel(this), this);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setFilterMode(Qt::MatchContains);
	completer->setCompletionMode(QCompleter::PopupCompletion);
	ui->search_text->setCompleter(completer);
	connect(ui->filter_type, &QComboBox::currentIndexChanged, this, &SearchDialog::updateUI);
	connect(ui->search_text, &QLineEdit::returnPressed, this, &SearchDialog::on_button_search_clicked);
	connect(ui->search_text, &QLineEdit::textChanged, this, [this](const QString& text) {
		ui->button_search->setEnabled(!text.trimmed().isEmpty());
	});
	enableCache(false);
	updateUI();
}

SearchDialog::~SearchDialog()
{
	delete ui;
}

void SearchDialog::updateUI()
{
	QStringList suggestions;
	const int type = ui->filter_type->currentIndex();
	auto append = [&](const auto& records) {
		for (const auto& record : records) {
			suggestions.append(QStringLiteral("%1: %2").arg(record.ID).arg(ToQString(record.name)));
		}
	};
	const auto& database = m_project.database();
	if (type == 0 || type == 1) append(database.variables);
	if (type == 0 || type == 2) append(database.switches);
	if (type == 0 || type == 3) append(database.items);
	if (type == 0 || type == 4) append(database.commonevents);
	suggestions.removeDuplicates();
	static_cast<QStringListModel*>(ui->search_text->completer()->model())->setStringList(suggestions);
	map_cache.clear();
}

void SearchDialog::enableCache(bool enable)
{
	useCache = enable;
	map_cache.clear();
}

void SearchDialog::on_button_search_clicked()
{
	const QString text = ui->search_text->text().trimmed();
	if (text.isEmpty()) return;
	objectData.clear();
	ui->list_result->setRowCount(0);
	const int type = ui->filter_type->currentIndex();
	const int scope = ui->filter_scope->currentIndex();
	// IDs match exactly, including a completed "ID: Name" suggestion. Names match partially.
	static const QRegularExpression numbered(QStringLiteral("^(\\d+)(?::\\s.*)?$"));
	const auto number = numbered.match(text);
	const int id = number.hasMatch() ? number.captured(1).toInt() : -1;
	auto nameMatches = [&](int recordID, const auto& name) {
		return number.hasMatch() ? recordID == id : ToQString(name).contains(text, Qt::CaseInsensitive);
	};
	auto matchingIDs = [&](const auto& records, int category) {
		QSet<int> ids;
		if (type == 0 || type == category) {
			for (const auto& record : records) {
				if (nameMatches(record.ID, record.name)) ids.insert(record.ID);
			}
			// References to an unnamed or deleted database entry still have a meaningful ID.
			if (id > 0) ids.insert(id);
		}
		return ids;
	};
	const auto& database = m_project.database();
	const auto variables = matchingIDs(database.variables, 1);
	const auto switches = matchingIDs(database.switches, 2);
	const auto items = matchingIDs(database.items, 3);
	static const QRegularExpression messageVariable(QStringLiteral("\\\\[vV]\\[\\s*(\\d+)\\s*\\]"));
	using Cmd = lcf::rpg::EventCommand::Code;
	auto matchesCommand = [&](const lcf::rpg::EventCommand& command) {
		auto p = [&](size_t i) { return i < command.parameters.size() ? command.parameters[i] : -1; };
		auto variable = [&](size_t i) { return variables.contains(p(i)); };
		auto range = [&](const QSet<int>& ids) {
			return std::any_of(ids.begin(), ids.end(), [&](int candidate) {
				return p(0) == 0 ? candidate == p(1) : p(0) == 1 && candidate >= p(1) && candidate <= p(2);
			});
		};
		bool match = false;
		switch (static_cast<Cmd>(command.code)) {
		case Cmd::ShowMessage:
		case Cmd::ShowMessage_2:
		case Cmd::ShowChoiceOption: {
			auto references = messageVariable.globalMatch(ToQString(command.string));
			while (references.hasNext()) {
				if (variables.contains(references.next().captured(1).toInt())) { match = true; break; }
			}
			break;
		}
		case Cmd::InputNumber:
			match = variable(1); break;
		case Cmd::ControlSwitches:
			match = range(switches) || (p(0) == 2 && variable(1)); break;
		case Cmd::ControlVars:
			match = range(variables) || (p(0) == 2 && variable(1)) ||
				((p(4) == 1 || p(4) == 2) && variable(5)) || (p(4) == 4 && items.contains(p(5))); break;
		case Cmd::TimerOperation:
			match = p(0) == 0 && p(1) == 1 && variable(2); break;
		case Cmd::ChangeGold:
		case Cmd::ChangePartyMembers:
			match = p(1) == 1 && variable(2); break;
		case Cmd::ChangeItems:
			match = (p(1) == 1 && variable(2)) || (p(3) == 1 && variable(4)) ||
				(p(1) == 0 && items.contains(p(2))); break;
		case Cmd::EnemyEncounter:
			match = p(0) == 1 && variable(1); break;
		case Cmd::ChangeExp:
		case Cmd::ChangeLevel:
		case Cmd::ChangeSkills:
		case Cmd::ChangeEquipment:
		case Cmd::ChangeHP:
		case Cmd::ChangeSP:
			match = (p(0) == 2 && variable(1)) || (p(3) == 1 && variable(4)) ||
				(command.code == static_cast<int>(Cmd::ChangeEquipment) && p(2) == 0 && p(3) == 0 && items.contains(p(4))); break;
		case Cmd::ChangeParameters:
			match = (p(0) == 2 && variable(1)) || (p(4) == 1 && variable(5)); break;
		case Cmd::ChangeCondition:
		case Cmd::FullHeal:
			match = p(0) == 2 && variable(1); break;
		case Cmd::SimulatedAttack:
			match = (p(0) == 2 && variable(1)) || (p(6) == 1 && variable(7)); break;
		case Cmd::MemorizeLocation:
		case Cmd::RecallToLocation:
			match = variable(0) || variable(1) || variable(2); break;
		case Cmd::SetVehicleLocation:
			match = p(1) == 1 && (variable(2) || variable(3) || variable(4)); break;
		case Cmd::ChangeEventLocation:
		case Cmd::ShowPicture:
		case Cmd::MovePicture:
			match = (p(1) == 1 && (variable(2) || variable(3))); break;
		case Cmd::StoreTerrainID:
		case Cmd::StoreEventID:
			match = variable(3) || (p(0) == 1 && (variable(1) || variable(2))); break;
		case Cmd::KeyInputProc:
			match = variable(0) || (p(8) == 1 && variable(7)); break;
		case Cmd::ConditionalBranch:
			match = (p(0) == 0 && switches.contains(p(1))) ||
				(p(0) == 1 && (variable(1) || (p(2) == 1 && variable(3)))) ||
				(p(0) == 4 && items.contains(p(1))); break;
		case Cmd::CallEvent:
			match = p(0) == 2 && (variable(1) || variable(2)); break;
		case Cmd::PlayMovie:
			match = p(0) == 1 && (variable(1) || variable(2)); break;
		default: break;
		}
		return match || ((type == 0 || type == 5) && !command.string.empty() &&
			ToQString(command.string).contains(text, Qt::CaseInsensitive));
	};
	auto searchCommands = [&](int mapID, int eventID, int pageID, const auto& commands) {
		for (size_t line = 0; line < commands.size(); ++line) {
			if (matchesCommand(commands[line])) {
				addResult(mapID, eventID, pageID, static_cast<int>(line + 1), Stringizer::stringize(commands[line]));
			}
		}
	};
	auto searchMap = [&](int mapID) {
		const auto map = loadMap(mapID);
		if (!map) return;
		for (const auto& event : map->events) {
			if ((type == 0 || type == 4) && nameMatches(event.ID, event.name)) {
				addResult(mapID, event.ID, 0, 0, ToQString(event.name));
			}
			for (const auto& page : event.pages) {
				const auto& condition = page.condition;
				if ((condition.flags.variable && variables.contains(condition.variable_id)) ||
					(condition.flags.switch_a && switches.contains(condition.switch_a_id)) ||
					(condition.flags.switch_b && switches.contains(condition.switch_b_id)) ||
					(condition.flags.item && items.contains(condition.item_id))) {
					addResult(mapID, event.ID, page.ID, 0, tr("Event page condition"));
				}
				searchCommands(mapID, event.ID, page.ID, page.event_commands);
			}
		}
	};
	ui->button_search->setEnabled(false);
	setCursor(Qt::WaitCursor);
	auto* window = qobject_cast<MainWindow*>(parent());
	if (scope == 1) {
		if (auto* scene = window ? window->currentScene() : nullptr) searchMap(scene->id());
	} else if (scope == 0) {
		for (const auto& map : m_project.treeMap().maps) {
			if (map.ID > 0 && map.type == lcf::rpg::TreeMap::MapType_map) searchMap(map.ID);
		}
	}
	if (scope != 1) {
		for (const auto& event : database.commonevents) {
			if ((type == 0 || type == 4) && nameMatches(event.ID, event.name)) {
				addResult(0, event.ID, 0, 0, ToQString(event.name));
			}
			if (event.switch_flag && switches.contains(event.switch_id)) {
				addResult(0, event.ID, 0, 0, tr("Common event condition"));
			}
			searchCommands(0, event.ID, 0, event.event_commands);
		}
	}
	unsetCursor();
	ui->button_search->setEnabled(true);
	ui->label_status->setText(tr("%n result(s)", nullptr, ui->list_result->rowCount()));
	for (int column = 0; column < 4; ++column) ui->list_result->resizeColumnToContents(column);
}

void SearchDialog::on_list_result_doubleClicked(const QModelIndex &index)
{
	if (index.row() < 0 || static_cast<size_t>(index.row()) >= objectData.size()) return;
	const auto [mapID, eventID, pageID, line] = objectData[static_cast<size_t>(index.row())];
	if (mapID == 0) return;
	auto* window = qobject_cast<MainWindow*>(parent());
	if (!window) return;
	window->openScene(mapID);
	if (auto* scene = window->currentScene()) {
		if (auto* event = (*scene->mapEvents())[eventID]) {
			window->selectTile(event->x, event->y);
			scene->centerOnTile(event->x, event->y);
		}
	}
}

std::shared_ptr<lcf::rpg::Map> SearchDialog::loadMap(int mapID)
{
	// Searches include the current contents of open tabs, without saving them.
	if (auto* window = qobject_cast<MainWindow*>(parent())) {
		if (auto* scene = window->getScene(mapID)) return std::make_shared<lcf::rpg::Map>(scene->mapData());
	}
	if (useCache && map_cache.contains(mapID)) return map_cache.value(mapID);
	std::shared_ptr<lcf::rpg::Map> map{m_project.project().loadMap(mapID)};
	if (useCache && map) map_cache.insert(mapID, map);
	return map;
}

void SearchDialog::addResult(int mapID, int eventID, int pageID, int line, const QString& description)
{
	QStringList path;
	if (mapID == 0) path.append(tr("Common Events"));
	else {
		QSet<int> visited;
		int parentID = mapID;
		while (parentID > 0 && !visited.contains(parentID)) {
			visited.insert(parentID);
			const auto& maps = m_project.treeMap().maps;
			const auto info = std::find_if(maps.begin(), maps.end(), [&](const auto& map) { return map.ID == parentID; });
			if (info == maps.end()) break;
			path.prepend(ToQString(info->name));
			parentID = info->parent_map;
		}
	}
	const int row = ui->list_result->rowCount();
	ui->list_result->insertRow(row);
	ui->list_result->setItem(row, 0, new QTableWidgetItem(path.join(QStringLiteral(" > "))));
	ui->list_result->setItem(row, 1, new QTableWidgetItem(QString::number(eventID)));
	ui->list_result->setItem(row, 2, new QTableWidgetItem(pageID > 0 ? QString::number(pageID) : QString()));
	ui->list_result->setItem(row, 3, new QTableWidgetItem(line > 0 ? QString::number(line) : QString()));
	ui->list_result->setItem(row, 4, new QTableWidgetItem(description));
	objectData.emplace_back(mapID, eventID, pageID, line);
}
