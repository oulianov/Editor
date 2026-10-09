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
#include "common/editor_settings.h"
#include "common/image_loader.h"
#include "stringizer.h"
#include "model/move_route_codec.h"
#include "ui/common/rpg_model.h"
#include "ui/common/theme_icon.h"
#include "ui/event/event_commands_widget.h"
#include "ui/event/event_dialog.h"

#include <QApplication>
#include <QCompleter>
#include <QHeaderView>
#include <QListView>
#include <QRegularExpression>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QPixmapCache>
#include <QTabWidget>
#include <QTimer>
#include <algorithm>
#include <tuple>

namespace {

// The "map" of the results in the battle events of monster groups (0: common events)
constexpr int TroopResults = -1;

enum SuggestionRole { SuggestionID = Qt::UserRole, SuggestionType, SuggestionName };

class SearchSuggestions final : public QSortFilterProxyModel {
public:
	explicit SearchSuggestions(QObject* parent) : QSortFilterProxyModel(parent) {}

	void setQuery(const QString& text) {
		QString query = text.trimmed();
		static const QRegularExpression numbered(QStringLiteral("^(\\d+)(?::.*)?$"));
		const auto number = numbered.match(query);
		bool valid = false;
		int id = number.hasMatch() ? number.captured(1).toInt(&valid) : -1;
		if (valid) query = QString::number(id);
		else id = -1;
		// Highlighting a completion inserts its label; keep the popup selection intact.
		if (m_query == query && m_id == id) return;
		m_query = query;
		m_id = id;
		invalidate();
		sort(0);
	}

protected:
	bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
		return rank(sourceModel()->index(row, 0, parent)) < 6;
	}

	bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
		return std::tuple{rank(left), left.data(SuggestionID).toInt(), left.data(SuggestionType).toInt()} <
			std::tuple{rank(right), right.data(SuggestionID).toInt(), right.data(SuggestionType).toInt()};
	}

private:
	int rank(const QModelIndex& index) const {
		if (m_query.isEmpty()) return 6;
		const int id = index.data(SuggestionID).toInt();
		const QString name = index.data(SuggestionName).toString();
		const QString digits = QString::number(id);
		if (m_id >= 0 && id == m_id) return 0;
		if (name.compare(m_query, Qt::CaseInsensitive) == 0) return 1;
		if (digits.startsWith(m_query)) return 2;
		if (name.startsWith(m_query, Qt::CaseInsensitive)) return 3;
		if (digits.contains(m_query)) return 4;
		if (name.contains(m_query, Qt::CaseInsensitive)) return 5;
		return 6;
	}

	QString m_query;
	int m_id = -1;
};

}

SearchDialog::SearchDialog(ProjectData& project, QWidget *parent) :
	QDialog(parent), ui(new Ui::SearchDialog), m_project(project)
{
	ui->setupUi(this);
	connect(&EditorSettings::instance(), &EditorSettings::localStateReset, this, [this] {
		ui->search_text->clear();
		ui->filter_scope->setCurrentIndex(0);
		ui->filter_type->setCurrentIndex(0);
		ui->list_result->setRowCount(0);
		map_cache.clear();
	});
	ui->list_result->setColumnCount(5);
	ui->list_result->setHorizontalHeaderLabels({tr("Map"), tr("Event"), tr("Page"), tr("Type"), tr("Match")});
	ui->list_result->setIconSize(QSize(24, 32));
	ui->list_result->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->list_result->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->list_result->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->list_result->setWordWrap(false);
	auto* header = ui->list_result->horizontalHeader();
	header->setSectionResizeMode(QHeaderView::Interactive);
	header->setStretchLastSection(true);
	ui->list_result->setColumnWidth(0, 220);
	ui->list_result->setColumnWidth(1, 180);
	ui->list_result->setColumnWidth(2, 55);
	ui->list_result->setColumnWidth(3, 125);
	ui->list_result->verticalHeader()->hide();
	ui->list_result->verticalHeader()->setDefaultSectionSize(36);
	ui->search_text->setClearButtonEnabled(true);
	auto* suggestions = new SearchSuggestions(this);
	suggestions->setSourceModel(new QStandardItemModel(suggestions));
	auto* completer = new QCompleter(suggestions, this);
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
	ui->search_text->setCompleter(completer);
	connect(completer, qOverload<const QString&>(&QCompleter::activated), this, [this, suggestions](const QString& query) {
		// Inserting completion text changes the proxy; its old index is no longer reliable.
		const auto items = static_cast<QStandardItemModel*>(suggestions->sourceModel())->findItems(query);
		if (items.isEmpty()) return;
		const int type = items.first()->data(SuggestionType).toInt();
		// Let the completer finish inserting the query before rebuilding its model.
		QTimer::singleShot(0, this, [this, type, query] {
			if (ui->search_text->text() != query) return;
			ui->filter_type->setCurrentIndex(type);
			if (type == 4) ui->filter_scope->setCurrentIndex(2);
		});
	});
	connect(ui->filter_type, &QComboBox::currentIndexChanged, this, &SearchDialog::updateUI);
	connect(ui->search_text, &QLineEdit::returnPressed, this, &SearchDialog::on_button_search_clicked);
	connect(ui->search_text, &QLineEdit::textChanged, this, [this](const QString& text) {
		ui->button_search->setEnabled(!text.trimmed().isEmpty());
	});
	connect(ui->search_text, &QLineEdit::textEdited, suggestions, &SearchSuggestions::setQuery);
	connect(ui->list_result, &QTableWidget::clicked, this, &SearchDialog::openResult);
	connect(ui->list_result, &QTableWidget::activated, this, &SearchDialog::openResult);
	enableCache(false);
	updateUI();
}

SearchDialog::~SearchDialog()
{
	delete ui;
}

void SearchDialog::updateUI()
{
	auto* suggestions = static_cast<SearchSuggestions*>(ui->search_text->completer()->model());
	auto* model = static_cast<QStandardItemModel*>(suggestions->sourceModel());
	model->clear();
	const int type = ui->filter_type->currentIndex();
	auto append = [&](const auto& records, int category, const QString& label, const QString& icon) {
		for (const auto& record : records) {
			const QString name = ToQString(record.name);
			auto* item = new QStandardItem(themeIcon(this, icon), QStringLiteral("%1: %2 — %3").arg(record.ID).arg(label, name));
			item->setData(record.ID, SuggestionID);
			item->setData(category, SuggestionType);
			item->setData(name, SuggestionName);
			model->appendRow(item);
		}
	};
	const auto& database = m_project.database();
	if (type == 0 || type == 1) append(database.variables, 1, tr("Variable"), QStringLiteral("variable"));
	if (type == 0 || type == 2) append(database.switches, 2, tr("Switch"), QStringLiteral("switch"));
	if (type == 0 || type == 3) append(database.items, 3, tr("Item"), QStringLiteral("resources"));
	if (type == 0 || type == 4) append(database.commonevents, 4, tr("Common event"), QStringLiteral("code"));
	suggestions->setQuery(ui->search_text->text());
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
	const std::string encoding = m_project.project().encoding().toStdString();
	// The switches turned on or off by the moves of a route
	auto routeUsesSwitch = [&](const lcf::rpg::MoveRoute& route) {
		using Move = lcf::rpg::MoveCommand::Code;
		return std::any_of(route.move_commands.begin(), route.move_commands.end(), [&](const auto& move) {
			const auto code = static_cast<Move>(move.command_id);
			return (code == Move::switch_on || code == Move::switch_off) && switches.contains(move.parameter_a);
		});
	};
	QStringList matchedTypes;
	auto matchesCommand = [&](const lcf::rpg::EventCommand& command) {
		matchedTypes.clear();
		auto p = [&](size_t i) { return i < command.parameters.size() ? command.parameters[i] : -1; };
		auto reference = [&](const QSet<int>& ids, int candidate, const QString& label) {
			if (!ids.contains(candidate)) return false;
			if (!matchedTypes.contains(label)) matchedTypes.append(label);
			return true;
		};
		auto variable = [&](size_t i) { return reference(variables, p(i), tr("Variable")); };
		auto range = [&](const QSet<int>& ids, const QString& label) {
			const bool found = std::any_of(ids.begin(), ids.end(), [&](int candidate) {
				return p(0) == 0 ? candidate == p(1) : p(0) == 1 && candidate >= p(1) && candidate <= p(2);
			});
			if (found && !matchedTypes.contains(label)) matchedTypes.append(label);
			return found;
		};
		bool match = false;
		switch (static_cast<Cmd>(command.code)) {
		case Cmd::ShowMessage:
		case Cmd::ShowMessage_2:
		case Cmd::ShowChoiceOption: {
			auto references = messageVariable.globalMatch(ToQString(command.string));
			while (references.hasNext()) {
				if (reference(variables, references.next().captured(1).toInt(), tr("Variable"))) { match = true; break; }
			}
			break;
		}
		case Cmd::InputNumber:
			match = variable(1); break;
		case Cmd::ControlSwitches:
			match = range(switches, tr("Switch")) || (p(0) == 2 && variable(1)); break;
		case Cmd::ControlVars:
			match = range(variables, tr("Variable")) || (p(0) == 2 && variable(1)) ||
				((p(4) == 1 || p(4) == 2) && variable(5)) || (p(4) == 4 && reference(items, p(5), tr("Item"))); break;
		case Cmd::TimerOperation:
			match = p(0) == 0 && p(1) == 1 && variable(2); break;
		case Cmd::ChangeGold:
		case Cmd::ChangePartyMembers:
			match = p(1) == 1 && variable(2); break;
		case Cmd::ChangeItems:
			match = (p(1) == 1 && variable(2)) || (p(3) == 1 && variable(4)) ||
				(p(1) == 0 && reference(items, p(2), tr("Item"))); break;
		case Cmd::EnemyEncounter:
			match = p(0) == 1 && variable(1); break;
		case Cmd::ChangeExp:
		case Cmd::ChangeLevel:
		case Cmd::ChangeSkills:
		case Cmd::ChangeEquipment:
		case Cmd::ChangeHP:
		case Cmd::ChangeSP:
			match = (p(0) == 2 && variable(1)) || (p(3) == 1 && variable(4)) ||
				(command.code == static_cast<int>(Cmd::ChangeEquipment) && p(2) == 0 && p(3) == 0 && reference(items, p(4), tr("Item"))); break;
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
			match = (p(0) == 0 && reference(switches, p(1), tr("Switch"))) ||
				(p(0) == 1 && (variable(1) || (p(2) == 1 && variable(3)))) ||
				(p(0) == 4 && reference(items, p(1), tr("Item"))); break;
		case Cmd::ConditionalBranch_B:
			match = (p(0) == 0 && reference(switches, p(1), tr("Switch"))) ||
				(p(0) == 1 && (variable(1) || (p(2) == 1 && variable(3)))); break;
		case Cmd::TeleportTargets:
			// a switch only when a target is added
			match = p(0) == 0 && p(4) != 0 && reference(switches, p(5), tr("Switch")); break;
		case Cmd::EscapeTarget:
			match = p(3) != 0 && reference(switches, p(4), tr("Switch")); break;
		case Cmd::MoveEvent:
			if (!switches.isEmpty() && routeUsesSwitch(MoveRouteCodec::decode(command, encoding).route)) {
				matchedTypes.append(tr("Switch"));
				match = true;
			}
			break;
		case Cmd::CallEvent:
			match = p(0) == 2 && (variable(1) || variable(2)); break;
		case Cmd::PlayMovie:
			match = p(0) == 1 && (variable(1) || variable(2)); break;
		default: break;
		}
		if ((type == 0 || type == 5) && !command.string.empty() && ToQString(command.string).contains(text, Qt::CaseInsensitive)) {
			matchedTypes.append(tr("Event text"));
			match = true;
		}
		return match;
	};
	auto searchCommands = [&](int mapID, int eventID, int pageID, const auto& commands, const lcf::rpg::Event* event = nullptr) {
		for (size_t line = 0; line < commands.size(); ++line) {
			if (matchesCommand(commands[line])) {
				addResult(mapID, eventID, pageID, static_cast<int>(line + 1), Stringizer::stringize(commands[line]), matchedTypes.join(", "), event);
			}
		}
	};
	auto searchMap = [&](int mapID) {
		const auto map = loadMap(mapID);
		if (!map) return;
		for (const auto& event : map->events) {
			if ((type == 0 || type == 4) && nameMatches(event.ID, event.name)) {
				addResult(mapID, event.ID, 0, 0, ToQString(event.name), tr("Event"), &event);
			}
			for (const auto& page : event.pages) {
				const auto& condition = page.condition;
				QStringList conditions;
				if (condition.flags.variable && variables.contains(condition.variable_id)) conditions.append(tr("Variable"));
				if ((condition.flags.switch_a && switches.contains(condition.switch_a_id)) ||
					(condition.flags.switch_b && switches.contains(condition.switch_b_id))) conditions.append(tr("Switch"));
				if (condition.flags.item && items.contains(condition.item_id)) conditions.append(tr("Item"));
				if (!conditions.isEmpty()) addResult(mapID, event.ID, page.ID, 0, tr("Event page condition"), conditions.join(", "), &event);
				if (routeUsesSwitch(page.move_route)) addResult(mapID, event.ID, page.ID, 0, tr("Custom movement pattern"), tr("Switch"), &event);
				searchCommands(mapID, event.ID, page.ID, page.event_commands, &event);
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
				addResult(0, event.ID, 0, 0, ToQString(event.name), tr("Common event"));
			}
			if (event.switch_flag && switches.contains(event.switch_id)) {
				addResult(0, event.ID, 0, 0, tr("Common event condition"), tr("Switch"));
			}
			searchCommands(0, event.ID, 0, event.event_commands);
		}
	}
	if (scope == 0) {
		// the battle events of the monster groups
		for (const auto& troop : database.troops) {
			for (const auto& page : troop.pages) {
				const auto& condition = page.condition;
				QStringList conditions;
				if (condition.flags.variable && variables.contains(condition.variable_id)) conditions.append(tr("Variable"));
				if ((condition.flags.switch_a && switches.contains(condition.switch_a_id)) ||
					(condition.flags.switch_b && switches.contains(condition.switch_b_id))) conditions.append(tr("Switch"));
				if (!conditions.isEmpty()) addResult(TroopResults, troop.ID, page.ID, 0, tr("Battle event condition"), conditions.join(", "));
				searchCommands(TroopResults, troop.ID, page.ID, page.event_commands);
			}
		}
	}
	unsetCursor();
	ui->button_search->setEnabled(true);
	ui->label_status->setText(tr("%n result(s)", nullptr, ui->list_result->rowCount()));
}

void SearchDialog::openResult(const QModelIndex &index)
{
	if (!isVisible()) return;
	if (index.row() < 0 || static_cast<size_t>(index.row()) >= objectData.size()) return;
	const auto [mapID, eventID, pageID, line] = objectData[static_cast<size_t>(index.row())];
	auto* window = qobject_cast<MainWindow*>(parent());
	if (!window) return;
	if (mapID == 0) {
		const auto& events = m_project.database().commonevents;
		if (std::none_of(events.begin(), events.end(), [&](const auto& event) { return event.ID == eventID; })) return;
		hide();
		DatabaseDialog dialog(m_project, window);
		dialog.selectCommonEvent(eventID, line);
		dialog.exec();
		return;
	}
	if (mapID == TroopResults) {
		const auto& troops = m_project.database().troops;
		if (std::none_of(troops.begin(), troops.end(), [&](const auto& troop) { return troop.ID == eventID; })) return;
		hide();
		DatabaseDialog dialog(m_project, window);
		auto* tabs = dialog.findChild<QTabWidget*>(QStringLiteral("tabOld_Pages"));
		if (!tabs) return;
		tabs->setCurrentIndex(5);  // Monster Groups
		// The group, its page and the line, after the saved view of the tab
		// (restored on the next event-loop turn)
		QTimer::singleShot(0, &dialog, [tabs, troopID = eventID, pageID, line] {
			QWidget* groups = tabs->currentWidget();
			auto* list = groups ? groups->findChild<QListView*>(QStringLiteral("list")) : nullptr;
			if (!list) return;
			for (int row = 0; row < list->model()->rowCount(); ++row) {
				const QModelIndex index = list->model()->index(row, 0);
				if (index.data(ModelDataId).toInt() != troopID) continue;
				list->setCurrentIndex(index);
				list->scrollTo(index);
				break;
			}
			auto* pages = groups->findChild<QTabWidget*>(QStringLiteral("tabPages"));
			for (int i = 0; pages && i < pages->count(); ++i) {
				if (pages->tabText(i) != QString::number(pageID)) continue;
				pages->setCurrentIndex(i);
				auto* commands = pages->widget(i)->findChild<EventCommandsWidget*>(QStringLiteral("commands"));
				if (commands && line > 0) {
					commands->selectIndex(line - 1);
					commands->setFocus();
				}
				break;
			}
		});
		dialog.exec();
		return;
	}
	if (!window->hasMap(mapID)) return;
	window->openScene(mapID);
	if (auto* scene = window->currentScene(); scene && scene->id() == mapID) {
		if (auto* event = scene->findEvent(eventID)) {
			for (auto* view : scene->views()) view->setProperty("workspace_scroll", QVariant());
			window->selectTile(event->x, event->y);
			scene->centerOnTile(event->x, event->y);
			hide();
			window->raise();
			window->activateWindow();
			if (!scene->views().isEmpty()) scene->views().first()->setFocus();
			// Then the event, at the page and the line found, like RPG Maker
			QTimer::singleShot(0, window, [pageID, line] {
				if (auto* dialog = qobject_cast<EventDialog*>(QApplication::activeModalWidget())) {
					dialog->showCommand(pageID, line);
				}
			});
			scene->editEvent(*event);
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

void SearchDialog::addResult(int mapID, int eventID, int pageID, int line, const QString& description,
	const QString& type, const lcf::rpg::Event* event)
{
	QStringList path;
	if (mapID == 0) path.append(tr("Common Events"));
	else if (mapID == TroopResults) path.append(tr("Monster Groups"));
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
	ui->list_result->setItem(row, 0, new QTableWidgetItem(themeIcon(this, mapID <= 0 ? "database" : "tree"), path.join(QStringLiteral(" > "))));
	ui->list_result->item(row, 0)->setToolTip(path.join(QStringLiteral(" > ")));
	QIcon icon = themeIcon(this, mapID <= 0 ? "code" : "layer-event");
	QString name;
	if (event) {
		name = ToQString(event->name);
		const auto page = std::find_if(event->pages.begin(), event->pages.end(), [&](const auto& page) { return page.ID == pageID; });
		const auto* graphic = page != event->pages.end() ? &*page : event->pages.empty() ? nullptr : &event->pages.front();
		if (graphic && !graphic->character_name.empty()) {
			const QString charsetName = ToQString(graphic->character_name);
			const QString filename = m_project.project().findFile(CHARSET, charsetName, FileFinder::FileType::Image);
			if (!filename.isEmpty()) {
				QPixmap charset;
				const QString key = "search-charset:" + filename;
				if (!QPixmapCache::find(key, &charset)) {
					charset = ImageLoader::Load(filename);
					QPixmapCache::insert(key, charset);
				}
				const QRect frame((graphic->character_index % 4) * 72 + graphic->character_pattern * 24,
					(graphic->character_index / 4) * 128 + graphic->character_direction * 32, 24, 32);
				if (charset.rect().contains(frame)) icon = QIcon(charset.copy(frame));
			}
		}
	} else if (mapID == 0) {
		const auto& events = m_project.database().commonevents;
		const auto common = std::find_if(events.begin(), events.end(), [&](const auto& common) { return common.ID == eventID; });
		if (common != events.end()) name = ToQString(common->name);
	} else if (mapID == TroopResults) {
		const auto& troops = m_project.database().troops;
		const auto troop = std::find_if(troops.begin(), troops.end(), [&](const auto& troop) { return troop.ID == eventID; });
		if (troop != troops.end()) name = ToQString(troop->name);
	}
	ui->list_result->setItem(row, 1, new QTableWidgetItem(icon, QStringLiteral("%1: %2").arg(eventID).arg(name)));
	ui->list_result->setItem(row, 2, new QTableWidgetItem(pageID > 0 ? QString::number(pageID) : QString()));
	const QString kind = type.isEmpty() ? tr("Event") : type;
	const QString typeIcon = kind.startsWith(tr("Variable")) ? "variable" : kind.startsWith(tr("Switch")) ? "switch" :
		kind.startsWith(tr("Item")) ? "resources" : "code";
	ui->list_result->setItem(row, 3, new QTableWidgetItem(themeIcon(this, typeIcon), kind));
	ui->list_result->setItem(row, 4, new QTableWidgetItem(description));
	objectData.emplace_back(mapID, eventID, pageID, line);
}
