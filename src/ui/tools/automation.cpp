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

#include "automation.h"
#include "screenshot.h"
#include "core.h"
#include "common/dbstring.h"
#include "common/logging.h"
#include "controller/action_catalog.h"
#include "controller/edit_locks.h"
#include "controller/project_controller.h"
#include "model/dungeon_generator.h"
#include "model/game_runner.h"
#include "model/map_tools.h"
#include "model/project.h"
#include "qmlbinding/json_t.h"
#include "qmlbinding/project_data_gadget.h"
#include "qmlbinding/qml_dialog_service.h"
#include "qmlbinding/qml_host.h"
#include "qmlbinding/resource_windows.h"
#include "ui/event/command_editor_registry.h"
#include "ui/map/map_canvas_host.h"
#include "ui/picker/pickers.h"
#include "ui/shell/qml_engine.h"
#include "ui/shell/qml_shell.h"
#include <lcf/ldb/reader.h>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMetaEnum>
#include <QPalette>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTimer>
#include <array>
#include <memory>

namespace {

// Time for layouts and paints to settle between two commands
constexpr int step_delay = 100;

bool toInt(const QString& text, int& value) {
	bool ok = false;
	value = text.toInt(&ok);
	return ok;
}

// The words of a script line; "a b" is one word and "" an empty one
// (to clear a field), which QProcess::splitCommand would drop
QStringList splitLine(const QString& line) {
	QStringList words;
	QString word;
	bool quoted = false;
	bool started = false;
	for (const QChar c : line) {
		if (c == '"') {
			quoted = !quoted;
			started = true;
		} else if (c.isSpace() && !quoted) {
			if (started) {
				words << word;
			}
			word.clear();
			started = false;
		} else {
			word += c;
			started = true;
		}
	}
	if (started) {
		words << word;
	}
	return words;
}

// The map of the last generate-dungeon, for the expect-dungeon-* commands
struct GeneratedDungeon {
	lcf::rpg::Map map;
	std::vector<DungeonGenerator::Point> keep;
	uint32_t seed = 0;
};
GeneratedDungeon& lastDungeon() {
	static GeneratedDungeon dungeon;
	return dungeon;
}

// A map with the generator settings of RPG Maker's new maps and the tiles
// of the RTP dungeon chipset (as in TestGame-2003's dungeon maps)
lcf::rpg::Map dungeonSettings(int mode, int width, int height) {
	lcf::rpg::Map map = MapTools::newMap();
	map.width = width;
	map.height = height;
	map.generator_flag = true;
	map.generator_mode = mode;
	map.generator_tile_ids = {4499, 5052, 5046, 4149, 5034, 4199,
		10016, 10017, 10022, 10023, 10054, 10055, 10060, 10061, 10012, 10013, 10018, 10019};
	return map;
}

// The colors of a dark mode (Fusion's), for the platforms that have none to
// give (offscreen): the windows are checked in dark as the users see them
QPalette darkPalette() {
	const QColor window(50, 50, 50);
	const QColor base = window.darker(140);
	const QColor text(Qt::white);
	const QColor disabled_text(130, 130, 130);
	QPalette palette(text, window, window.lighter(150), window.darker(150), window.darker(130), text, text, base, window);
	palette.setColor(QPalette::AlternateBase, window.darker(110));
	palette.setColor(QPalette::Highlight, QColor(48, 140, 198));
	palette.setColor(QPalette::HighlightedText, text);
	palette.setColor(QPalette::Accent, QColor(48, 140, 198));
	palette.setColor(QPalette::Link, QColor(80, 170, 230));
	palette.setColor(QPalette::LinkVisited, QColor(170, 130, 220));
	palette.setColor(QPalette::ToolTipBase, base);
	palette.setColor(QPalette::ToolTipText, text);
	QColor placeholder = text;
	placeholder.setAlpha(128);
	palette.setColor(QPalette::PlaceholderText, placeholder);
	for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText}) {
		palette.setColor(QPalette::Disabled, role, disabled_text);
	}
	palette.setColor(QPalette::Disabled, QPalette::Base, window);
	palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(145, 145, 145));
	return palette;
}
}

Automation::Automation(QmlShell& window, const QString& script_file, QObject* parent) :
	QObject(parent), m_window(window), m_quick([this](const QString& message) { fail(message); }), m_file(script_file)
{
	QFile file(script_file);
	if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		m_lines = QString::fromUtf8(file.readAll()).split('\n');
		m_valid = true;
	}
}

bool Automation::isValid() const {
	return m_valid;
}

void Automation::start() {
	QTimer::singleShot(step_delay, this, &Automation::step);
}

void Automation::fail(const QString& message) {
	m_failures++;
	qCWarning(lcAutomation).noquote() << QString("%1:%2: %3").arg(m_file).arg(m_line).arg(message);
}

void Automation::step() {
	while (m_line < m_lines.size()) {
		QString line = m_lines[m_line++].trimmed();
		if (line.isEmpty() || line.startsWith('#')) {
			continue;
		}
		QStringList args = splitLine(line);
		// a folder of the script's own, empty when it starts (main.cpp)
		const QString script_dir = qEnvironmentVariable("EASYRPG_EDITOR_SCRIPT_DIR");
		for (QString& arg : args) {
			if (!script_dir.isEmpty()) {
				arg.replace(QStringLiteral("$SCRIPT_DIR"), script_dir);
			}
		}
		qCInfo(lcAutomation).noquote() << QString("%1:%2: %3").arg(m_file).arg(m_line).arg(line);

		if (args[0] == "quit") {
			break;
		}

		int delay = step_delay;
		if (args[0] == "wait" && (args.size() != 2 || !toInt(args[1], delay))) {
			fail("usage: wait MILLISECONDS");
			delay = step_delay;
		}
		// Schedule the next command first: commands opening a dialog only
		// return when it is closed, the script goes on inside the dialog
		QTimer::singleShot(delay, this, &Automation::step);
		const int failures = m_failures;
		if (args[0] != "wait" && !run(args)) {
			fail("failed: " + line);
			// one failure per line, even when the command explained it first
			m_failures = std::max(m_failures - 1, failures + 1);
		}
		return;
	}

	qCInfo(lcAutomation).noquote() << QString("%1: done, %2 failure(s)").arg(m_file).arg(m_failures);
	QCoreApplication::exit(std::min(m_failures, 125));
}

bool Automation::run(const QStringList& args) {
	const QString& cmd = args[0];
	int a = 0, b = 0;

	if (cmd == "log") {
		qCInfo(lcAutomation).noquote() << args.mid(1).join(' ');
		return true;
	}
	if (cmd == "expect-locks" && args.size() >= 2) {
		// what the editors open hold (EditLocks), in the order taken
		QStringList held;
		for (const EditLock& lock : EditLocks::instance().locks()) {
			switch (lock.kind) {
			case EditLock::Event:
				held << QStringLiteral("event:%1").arg(lock.map_id);
				break;
			case EditLock::Database:
				held << QStringLiteral("database");
				break;
			case EditLock::Dialog:
				held << QStringLiteral("dialog");
				break;
			}
		}
		const QString shown = held.isEmpty() ? QStringLiteral("none") : held.join(u' ');
		const QString expected = args.mid(1).join(u' ');
		if (shown != expected) {
			fail(QStringLiteral("the locks held are \"%1\", not \"%2\"").arg(shown, expected));
			return false;
		}
		return true;
	}
	// a window of the QML interface in front: its own commands
	if (QQuickWindow* quick = AutomationQuickDriver::frontWindow()) {
		bool handled = false;
		const bool ok = m_quick.run(quick, args, &handled);
		if (handled) {
			return ok;
		}
	}
	if (cmd == "qml-open" && args.size() >= 2) {
		// a window of the QML module, over the main window (tests of its
		// controls), with its properties: list=switches
		QVariantMap properties;
		for (const QString& arg : args.mid(2)) {
			const qsizetype equals = arg.indexOf(u'=');
			if (equals <= 0) {
				fail("usage: qml-open TYPE [PROPERTY=VALUE]...");
				return false;
			}
			properties.insert(arg.left(equals), arg.mid(equals + 1));
		}
		return QmlHost::open(args[1], m_window.handle(), properties) != nullptr;
	}
	if (cmd == "menu-role" && args.size() == 3) {
		// macOS's application menu, which no picture shows: the item of an
		// action with its role in the native menu bar (quit, about,
		// preferences, application: a copy there), the action in a menu of
		// the window's menu bar too, then chosen from the native one
		QmlShell* shell = &m_window;
		QQuickWindow* window = shell->window();
		if (!window) {
			fail("menu-role checks the QML main window");
			return false;
		}
		const QString& name = args[1];
		const QString& role = args[2];
		using Role = ActionCatalog::MenuRole;
		const Role expected = role == "quit" ? Role::Quit : role == "about" ? Role::About
			: role == "preferences" ? Role::Preferences : Role::None;
		if (!shell->actions()->contains(name) || shell->actions()->menuRole(name) != expected) {
			fail(QString("the catalog gives %1 no role %2").arg(name, role));
			return false;
		}
		QVariant places;
		QMetaObject::invokeMethod(window, "menuPlaces", Q_RETURN_ARG(QVariant, places), Q_ARG(QVariant, name), Q_ARG(QVariant, role));
		const QVariantMap found = places.toMap();
		if (!found.value("native").toBool()) {
			fail(QString("no item of %1 with the role %2 in the native menu bar").arg(name, role));
			return false;
		}
		if (found.value("menu").toString().isEmpty()) {
			fail(QString("%1 is in no menu of the menu bar").arg(name));
			return false;
		}
		qCInfo(lcAutomation).noquote() << QString("  %1 | %2 | %3%4").arg(name, role, found.value("menu").toString(),
			found.value("shown").toBool() ? QStringLiteral(" | shown by the platform") : QString());
		// queued: the window it opens must not block the script
		QMetaObject::invokeMethod(window, [window, name, role] {
			QMetaObject::invokeMethod(window, "triggerNativeItem", Q_ARG(QVariant, name), Q_ARG(QVariant, role));
		}, Qt::QueuedConnection);
		return true;
	}
	if ((cmd == "float-dock" && args.size() == 3 && (args[2] == "on" || args[2] == "off"))
			|| (cmd == "expect-dock" && args.size() == 3)) {
		// a dock of the main window in a window of its own (or back), its state:
		// docked, floating, closed
		QString state;
		if (QQuickWindow* window = m_window.window()) {
			QVariant result;
			if (cmd == "float-dock")
				QMetaObject::invokeMethod(window, "setDockFloating", Q_RETURN_ARG(QVariant, result),
					Q_ARG(QVariant, args[1]), Q_ARG(QVariant, args[2] == "on"));
			else
				QMetaObject::invokeMethod(window, "dockState", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, args[1]));
			state = result.typeId() == QMetaType::Bool ? (result.toBool() ? "ok" : "") : result.toString();
		}
		if (state.isEmpty()) {
			fail("no dock " + args[1]);
			return false;
		}
		if (cmd == "expect-dock" && state != args[2]) {
			fail(QString("the dock %1 is %2, not %3").arg(args[1], state, args[2]));
			return false;
		}
		return true;
	}

	if (cmd == "open-project-copy" && args.size() == 1) {
		// the project's files (not its pictures and sounds) in a folder of
		// their own: what is saved goes there
		if (!core().project()) {
			fail("no project");
			return false;
		}
		const QDir source = core().project()->projectDir();
		// a copy of a copy: the first one goes once the project left it
		auto copy = std::make_unique<QTemporaryDir>();
		if (!copy->isValid()) {
			fail("cannot make a temporary folder");
			return false;
		}
		for (const QFileInfo& file : source.entryInfoList(QDir::Files | QDir::Hidden)) {
			if (!QFile::copy(file.absoluteFilePath(), copy->filePath(file.fileName()))) {
				fail("cannot copy " + file.fileName());
				return false;
			}
		}
		m_window.LoadProject(copy->path());
		const bool opened = core().project() && core().project()->projectDir() == QDir(copy->path());
		m_projectCopy = std::move(copy);
		return opened;
	}
	if ((cmd == "edit-database-file" || cmd == "expect-database-file" || cmd == "expect-database") && args.size() >= 3) {
		return databaseFile(args);
	}
	if (cmd == "qml-map" && args.size() == 2 && toInt(args[1], a)) {
		// the QML map canvas and its palette on a map, in a window over the
		// main one: the controller's document, its tab's when it is open
		auto* controller = m_window.projectController();
		if (!controller || !m_window.hasMap(a)) {
			fail(QString("no map %1").arg(a));
			return false;
		}
		return MapCanvasHost::open(*controller, a, m_window.handle()) != nullptr;
	}
	if (cmd == "dialog-service" && args.size() >= 3) {
		// a question of the QML DialogService, as a controller asks it
		const QString& kind = args[1];
		const bool stale = kind == "stale" || kind == "stale-no-reload";
		const bool question = kind == "question" || kind == "question-cancel";
		const bool message = kind == "information" || kind == "warning" || kind == "error";
		if (!stale && ((!question && !message) || args.size() != 4)) {
			fail("usage: dialog-service stale|stale-no-reload WHAT [CONFLICT]..., "
				"dialog-service question|question-cancel|information|warning|error TITLE TEXT");
			return false;
		}
		QPointer<QWindow> over = AutomationQuickDriver::frontWindow();
		if (!over) {
			over = m_window.handle();
		}
		m_answer = QStringLiteral("(waiting)");
		// queued: the window waits for its answer, the script goes on in it
		QMetaObject::invokeMethod(this, [this, args, over] {
			QmlDialogService dialogs(over);
			const QString& kind = args[1];
			if (kind.startsWith(QLatin1String("stale"))) {
				const StaleChoice choice = dialogs.askStale(args[2], kind == "stale", args.mid(3));
				m_answer = choice == StaleChoice::Reload ? QStringLiteral("reload")
					: choice == StaleChoice::Overwrite ? QStringLiteral("overwrite") : QStringLiteral("cancel");
			} else if (kind.startsWith(QLatin1String("question"))) {
				const auto answer = dialogs.question(args[2], args[3], kind == "question-cancel"
					? DialogService::Buttons::YesNoCancel : DialogService::Buttons::YesNo);
				m_answer = answer == DialogService::Answer::Yes ? QStringLiteral("yes")
					: answer == DialogService::Answer::No ? QStringLiteral("no") : QStringLiteral("cancel");
			} else {
				if (kind == "information") {
					dialogs.information(args[2], args[3]);
				} else if (kind == "warning") {
					dialogs.warning(args[2], args[3]);
				} else {
					dialogs.error(args[2], args[3]);
				}
				m_answer = QStringLiteral("ok");
			}
			qCInfo(lcAutomation).noquote() << "answer:" << m_answer;
		}, Qt::QueuedConnection);
		return true;
	}
	if (cmd == "expect-player" && args.size() == 2) {
		// the Player test play runs: none, or one whose path has the text
		const QString player = GameRunner::findPlayer();
		const bool none = args[1] == QLatin1String("none");
		if (none ? !player.isEmpty() : !player.contains(args[1])) {
			fail(QStringLiteral("the Player found is \"%1\"").arg(player));
			return false;
		}
		return true;
	}
	if ((cmd == "pick-file" && args.size() >= 3) || (cmd == "pick-entry" && args.size() >= 3 && args.size() <= 4)) {
		// a picker as the windows open it (Pickers), over the window in front;
		// its answer for expect-answer: the choice, or cancel
		QPointer<QObject> over = AutomationQuickDriver::frontWindow();
		if (!over) {
			over = m_window.handle();
		}
		m_answer = QStringLiteral("(waiting)");
		QMetaObject::invokeMethod(this, [this, args, over] {
			if (args[0] == "pick-file") {
				Pickers::FileRequest request;
				request.folder = args[1];
				bool ok = false;
				const int preview = QMetaEnum::fromType<Pickers::Preview>().keyToValue(args[2].toLatin1(), &ok);
				request.preview = ok ? static_cast<Pickers::Preview>(preview) : Pickers::Preview::Image;
				// NAME and INDEX, then the settings of a sound as in the data
				// (volume=81...)
				QStringList positional;
				for (const QString& arg : args.mid(3)) {
					const QString key = arg.section(u'=', 0, 0);
					const int value = arg.section(u'=', 1).toInt();
					if (!arg.contains(u'=')) {
						positional << arg;
					} else if (key == QLatin1String("fadein")) {
						request.current.fadeIn = value;
					} else if (key == QLatin1String("volume")) {
						request.current.volume = value;
					} else if (key == QLatin1String("tempo")) {
						request.current.tempo = value;
					} else if (key == QLatin1String("balance")) {
						request.current.balance = value;
					}
				}
				request.current.name = positional.value(0);
				request.current.index = positional.value(1).toInt();
				auto text = [](const Pickers::FileChoice& choice) {
					return QStringList{choice.name, QString::number(choice.index), QString::number(choice.direction),
						QString::number(choice.pattern), QString::number(choice.hue), QString::number(choice.fadeIn),
						QString::number(choice.volume), QString::number(choice.tempo), QString::number(choice.balance)}.join(u':');
				};
				bool applied = false;
				// Apply: the choice given while the picker stays
				const auto choice = Pickers::pickFile(over, request, [&](const Pickers::FileChoice& given) {
					applied = true;
					m_answer = QStringLiteral("applied:") + text(given);
				});
				if (choice) {
					m_answer = text(*choice);
				} else if (!applied) {
					m_answer = QStringLiteral("cancel");
				}
			} else if (core().project()) {
				Pickers::EntryRequest request;
				request.list = args[1];
				request.id = args[2].toInt();
				request.none = args.value(3) == QLatin1String("none");
				const auto id = Pickers::pickEntry(over, core().project()->projectData(), request);
				m_answer = id ? QString::number(*id) : QStringLiteral("cancel");
			}
			qCInfo(lcAutomation).noquote() << "answer:" << m_answer;
		}, Qt::QueuedConnection);
		return true;
	}
	if (cmd == "pick-location" && args.size() >= 4 && args.size() <= 6) {
		// the map location picker as the windows open it, over the window in
		// front: MAP X Y [DIRECTION] [fixed]; its answer map:x:y:direction
		QPointer<QObject> over = AutomationQuickDriver::frontWindow();
		if (!over) {
			over = m_window.handle();
		}
		Pickers::LocationRequest request;
		request.current = {args[1].toInt(), args[2].toInt(), args[3].toInt(), 0};
		for (const QString& arg : args.mid(4)) {
			if (arg == QLatin1String("fixed")) {
				request.fixedMap = true;
			} else {
				request.facing = true;
				request.current.direction = arg.toInt();
			}
		}
		m_answer = QStringLiteral("(waiting)");
		QMetaObject::invokeMethod(this, [this, request, over] {
			const auto chosen = Pickers::pickLocation(over, request);
			m_answer = chosen ? QStringLiteral("%1:%2:%3:%4").arg(chosen->map).arg(chosen->x).arg(chosen->y).arg(chosen->direction)
				: QStringLiteral("cancel");
			qCInfo(lcAutomation).noquote() << "answer:" << m_answer;
		}, Qt::QueuedConnection);
		return true;
	}
	if (cmd == "expect-answer" && args.size() == 2) {
		if (m_answer != args[1]) {
			fail(QStringLiteral("the answer is \"%1\", not \"%2\"").arg(m_answer, args[1]));
			return false;
		}
		return true;
	}
	if (cmd == "color-scheme" && args.size() == 2 && (args[1] == "dark" || args[1] == "light")) {
		// the windows in dark (or light) where the platform cannot be asked
		// (offscreen): its colors and the icons drawn for them
		static const QPalette system_palette = QGuiApplication::palette();
		const bool dark = args[1] == "dark";
		// on a display: the windows of the platform too (its scheme asked)
		QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
		QGuiApplication::setPalette(dark ? darkPalette() : system_palette);
		QIcon::setThemeName(dark ? QStringLiteral("ezdark") : QStringLiteral("ezbright"));
		return true;
	}
	if (cmd == "open-project" && args.size() <= 2) {
		// without a folder: the project open, opened again (none open: the
		// copy the script made)
		const QString folder = args.size() == 2 ? args[1]
			: core().project() ? core().project()->projectDir().absolutePath()
			: m_projectCopy ? m_projectCopy->path() : QString();
		if (folder.isEmpty()) {
			fail("no project open");
			return false;
		}
		m_window.LoadProject(folder);
		return core().project() != nullptr;
	}
	if (cmd == "reload-tree" && args.size() == 1) {
		// as when another program changed the map tree
		return m_window.projectController()->reloadTreeMap();
	}
	if (cmd == "open-copy" && args.size() == 1) {
		// the project copied in the script's folder: what the script writes
		// in it (settings of the export...) stays there
		const QString script_dir = qEnvironmentVariable("EASYRPG_EDITOR_SCRIPT_DIR");
		if (!core().project() || script_dir.isEmpty()) {
			fail("no project, or no folder of the script");
			return false;
		}
		const QDir from = core().project()->projectDir();
		const QString to = QDir(script_dir).filePath(from.dirName());
		QDirIterator it(from.absolutePath(), QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const QString file = it.next();
			const QString target = QDir(to).filePath(from.relativeFilePath(file));
			if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(file, target)) {
				fail("cannot copy " + file);
				return false;
			}
		}
		m_window.LoadProject(to);
		return core().project() && core().project()->projectDir().absolutePath() == QDir(to).absolutePath();
	}
	if (cmd == "open-map" && args.size() == 2 && toInt(args[1], a)) {
		if (!m_window.hasMap(a)) {
			fail(QString("no map %1").arg(a));
			return false;
		}
		m_window.openScene(a);
		return true;
	}
	if (cmd == "open-event" && args.size() == 3 && toInt(args[1], a) && toInt(args[2], b)) {
		if (!m_window.hasMap(a)) {
			fail(QString("no map %1").arg(a));
			return false;
		}
		// its map shown, then the event (modal)
		if (!m_window.editMapEvent(a, b)) {
			fail(QString("no event %1 on map %2").arg(b).arg(a));
			return false;
		}
		return true;
	}
	if (cmd == "action" && args.size() == 3 && m_window.actions()->contains(args[1])) {
		// a setting of the main window: checked or unchecked, whatever its state
		QmlShell& shell = m_window;
		ActionCatalog* actions = shell.actions();
		if (!actions->isEnabled(args[1])) {
			fail("the action " + args[1] + " is disabled");
			return false;
		}
		if (!actions->isCheckable(args[1]) || (args[2] != "on" && args[2] != "off")) {
			return false;
		}
		if (actions->isChecked(args[1]) != (args[2] == "on")) {
			shell.trigger(args[1], args[2] == "on");
		}
		return true;
	}
	if (cmd == "action" && args.size() == 2) {
		// an action of the main window, from a window over it too
		ActionCatalog* actions = m_window.actions();
		if (!actions->contains(args[1])) {
			fail("no action " + args[1]);
			return false;
		}
		if (!actions->isEnabled(args[1])) {
			fail("the action " + args[1] + " is disabled");
			return false;
		}
		// queued: an action opening a dialog must not block the script
		QMetaObject::invokeMethod(&m_window, [shell = &m_window, name = args[1]] { shell->actions()->trigger(name); }, Qt::QueuedConnection);
		return true;
	}

	if (cmd == "screenshot" && args.size() <= 2) {
		return !Screenshot::save(m_window.window(), args.value(1)).isEmpty();
	}
	if (cmd == "check-command-editors" && args.size() == 1) {
		// the editor of every command, on a new one, loads without a warning
		if (!core().project()) {
			fail("no project");
			return false;
		}
		const QStringList problems = CommandEditorRegistry::checkEditors(core().project()->projectData());
		for (const QString& problem : problems) {
			fail(problem);
		}
		return problems.isEmpty();
	}
	if (cmd == "qml-smoke" && args.size() == 2) {
		return qmlSmoke(args[1]);
	}
	if (cmd == "select-map" && args.size() == 2 && toInt(args[1], a)) {
		// by id: map names can repeat; 0 is the project
		if (!m_window.selectNode(a, true)) {
			fail(QString("no map %1 in the map tree").arg(a));
			return false;
		}
		return true;
	}
	if (cmd == "drop-map" && args.size() == 4 && toInt(args[1], a) && toInt(args[2], b)
			&& (args[3] == "above" || args[3] == "below" || args[3] == "on")) {
		// A drag and drop in the map tree: the map pressed, then dropped as
		// its drag drops it
		const int position = args[3] == "above" ? 0 : args[3] == "below" ? 1 : 2;
		if (!m_window.canDrop(a, b, position)) {
			fail(QString("the tree refuses map %1 %2 %3").arg(args[1], args[3], args[2]));
			return false;
		}
		m_window.selectNode(a, true);
		return m_window.dropMap(a, b, position);
	}
	if (cmd == "import-image-dialog" && args.size() == 2) {
		// the window of an image import (transparent colour, zoom), without
		// the file chooser before it
		return ResourceWindows::openImportImage(m_window.handle(), args[1]) != nullptr;
	}
	if (cmd == "expect-project" && args.size() == 2) {
		// the project open: none, or one whose folder has the text
		const QString folder = core().project() ? core().project()->projectDir().absolutePath() : QString();
		const bool none = args[1] == QLatin1String("none");
		if (none ? !folder.isEmpty() : !folder.contains(args[1])) {
			fail(QStringLiteral("the project open is \"%1\"").arg(folder));
			return false;
		}
		return true;
	}
	if (cmd == "palette" && (args.size() == 2 || args.size() == 4) && toInt(args[1], a)) {
		// tiles of the palette of the current layer (0: the first after the eraser row), W x H
		int w = 1, h = 1;
		if (args.size() == 4 && (!toInt(args[2], w) || !toInt(args[3], h))) {
			return false;
		}
		const bool upper = core().layer() == Core::UPPER;
		std::vector<short> selection;
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				const int index = a + x + y * 6;
				selection.push_back(core().translate(upper ? index + 162 : index, SAMPLE));
			}
		}
		core().setSelection(selection, w, h);
		return true;
	}
	if (cmd == "expect-start" && args.size() == 5) {
		int map = 0, x = 0, y = 0;
		if (!core().project() || !toInt(args[2], map) || !toInt(args[3], x) || !toInt(args[4], y)) {
			return false;
		}
		const auto& start = core().project()->treeMap().start;
		const QMap<QString, std::array<int, 3>> positions = {
			{"party", {start.party_map_id, start.party_x, start.party_y}},
			{"boat", {start.boat_map_id, start.boat_x, start.boat_y}},
			{"ship", {start.ship_map_id, start.ship_x, start.ship_y}},
			{"airship", {start.airship_map_id, start.airship_x, start.airship_y}},
		};
		if (!positions.contains(args[1])) {
			return false;
		}
		const auto& pos = positions[args[1]];
		if (pos[0] != map || pos[1] != x || pos[2] != y) {
			fail(QString("%1 starts at map %2 (%3,%4)").arg(args[1]).arg(pos[0]).arg(pos[1]).arg(pos[2]));
			return false;
		}
		return true;
	}
	if (cmd == "generate-dungeon" && args.size() >= 5 && toInt(args[1], a)) {
		// generate-dungeon MODE W H SEED [granularity=1|2] [upper-wall=0|1] [surround=0|1]
		//   [room=WxH] [floor-b=0|1] [floor-c=0|1] [extra-b=0|1] [extra-c=0|1] [keep=X,Y]...
		// MODE: 0 winding passage, 1 rooms, 2 maze, 3 open room. Logs the dungeon as text.
		int w = 0, h = 0, seed = 0;
		if (a < 0 || a > 3 || !toInt(args[2], w) || !toInt(args[3], h) || !toInt(args[4], seed)) {
			fail("usage: generate-dungeon MODE W H SEED [OPTION=VALUE]...");
			return false;
		}
		GeneratedDungeon& dungeon = lastDungeon();
		dungeon.map = dungeonSettings(a, w, h);
		dungeon.keep.clear();
		dungeon.seed = static_cast<uint32_t>(seed);
		lcf::rpg::Map& map = dungeon.map;
		for (const QString& option : args.mid(5)) {
			const QString key = option.section('=', 0, 0);
			const QString value = option.section('=', 1);
			const bool on = value != "0";
			if (key == "granularity")
				map.generator_tiles = value == "2" ? lcf::rpg::Map::GeneratorTiles_two : lcf::rpg::Map::GeneratorTiles_one;
			else if (key == "upper-wall")
				map.generator_upper_wall = on;
			else if (key == "surround")
				map.generator_surround = on;
			else if (key == "floor-b")
				map.generator_floor_b = on;
			else if (key == "floor-c")
				map.generator_floor_c = on;
			else if (key == "extra-b")
				map.generator_extra_b = on;
			else if (key == "extra-c")
				map.generator_extra_c = on;
			else if (key == "room") {
				map.generator_width = value.section('x', 0, 0).toInt();
				map.generator_height = value.section('x', 1).toInt();
			} else if (key == "keep")
				dungeon.keep.push_back({value.section(',', 0, 0).toInt(), value.section(',', 1).toInt()});
			else {
				fail("unknown option " + option);
				return false;
			}
		}
		DungeonGenerator::generate(map, dungeon.seed, dungeon.keep);
		qCInfo(lcAutomation).noquote() << "dungeon:\n" + QString::fromStdString(DungeonGenerator::toText(map));
		return true;
	}
	if (cmd == "expect-dungeon-connected" && args.size() == 1) {
		// every floor cell reachable from the others, the keep cells too, the border closed
		const std::string problems = DungeonGenerator::check(lastDungeon().map, lastDungeon().keep);
		if (!problems.empty()) {
			fail(QString::fromStdString(problems));
			return false;
		}
		return true;
	}
	if (cmd == "expect-dungeons-valid" && args.size() == 2 && toInt(args[1], a)) {
		// every mode and option, 3 sizes, seeds 0..N-1: generate twice (same result), check
		int runs = 0, failures = 0;
		const int sizes[3][2] = {{20, 15}, {57, 43}, {120, 90}};
		for (int mode = 0; mode < 4; mode++)
		for (int options = 0; options < 8; options++)
		for (const auto& size : sizes)
		for (int seed = 0; seed < a; seed++) {
			lcf::rpg::Map map = dungeonSettings(mode, size[0], size[1]);
			map.generator_tiles = options & 1 ? lcf::rpg::Map::GeneratorTiles_two : lcf::rpg::Map::GeneratorTiles_one;
			map.generator_upper_wall = options & 2;
			map.generator_surround = options & 4;
			map.generator_width = 2 + seed % 7;
			map.generator_height = 2 + seed % 5;
			const std::vector<DungeonGenerator::Point> keep = {{5, 5}, {size[0] - 1, size[1] - 1}};
			lcf::rpg::Map again = map;
			DungeonGenerator::generate(map, static_cast<uint32_t>(seed), keep);
			DungeonGenerator::generate(again, static_cast<uint32_t>(seed), keep);
			std::string problems = DungeonGenerator::check(map, keep);
			if (again.lower_layer != map.lower_layer || again.upper_layer != map.upper_layer)
				problems += " not deterministic";
			runs++;
			if (!problems.empty() && failures++ < 5)
				qCWarning(lcAutomation).noquote() << QString("dungeon mode %1 options %2 size %3x%4 seed %5: %6\n%7")
					.arg(mode).arg(options).arg(size[0]).arg(size[1]).arg(seed)
					.arg(QString::fromStdString(problems), QString::fromStdString(DungeonGenerator::toText(map)));
		}
		qCInfo(lcAutomation).noquote() << QString("dungeons: %1 generated, %2 invalid").arg(runs).arg(failures);
		if (failures > 0) {
			fail(QString("%1 invalid dungeons").arg(failures));
			return false;
		}
		return true;
	}

	fail("unknown command or wrong arguments");
	return false;
}

bool Automation::qmlSmoke(const QString& file) {
	// The QML stack of the interface in one window (Kirigami, Breeze, a dock
	// of KDDockWidgets, the project's data): it must load, show the first
	// hero of the project and draw something
	QQmlApplicationEngine* engine = QmlEngine::instance();
	auto* project_data = engine->singletonInstance<ProjectDataGadget*>(
		QStringLiteral("org.easyrpg.editor"), QStringLiteral("ProjectData"));
	if (project_data && core().project()) {
		project_data->setProjectData(&core().project()->projectData());
	}
	QQmlComponent component(engine, QStringLiteral("org.easyrpg.editor"), QStringLiteral("SmokeWindow"));
	std::unique_ptr<QObject> object(component.create());
	auto* window = qobject_cast<QQuickWindow*>(object.get());
	if (!window) {
		fail("the QML window could not be made: " + component.errorString());
		return false;
	}
	window->show();
	// the first frame, drawn by the render loop
	QEventLoop loop;
	QObject::connect(window, &QQuickWindow::frameSwapped, &loop, &QEventLoop::quit);
	QTimer::singleShot(5000, &loop, &QEventLoop::quit);
	loop.exec();
	const QImage image = window->grabWindow();
	window->close();

	QSet<QRgb> colors;
	for (int y = 0; y < image.height() && colors.size() < 3; y += 7) {
		for (int x = 0; x < image.width() && colors.size() < 3; x += 7) {
			colors.insert(image.pixel(x, y));
		}
	}
	if (image.isNull() || colors.size() < 3) {
		fail("the QML window drew nothing");
		return false;
	}
	if (!image.save(file)) {
		fail("cannot save " + file);
		return false;
	}
	auto* hero = window->findChild<QObject*>(QStringLiteral("smokeHero"));
	const QString name = hero ? hero->property("text").toString() : QString();
	const QString expected = core().project() && !core().project()->database().actors.empty()
		? ToQString(core().project()->database().actors.front().name) : QString();
	if (name != expected) {
		fail(QString("the QML window shows the hero \"%1\", not \"%2\"").arg(name, expected));
		return false;
	}
	// the other way: a name given to the field is in the data and shown
	// (in memory only; put back)
	auto* field = window->findChild<QObject*>(QStringLiteral("smokeHeroField"));
	if (!field) {
		fail("the QML window has no field of the hero's name");
		return false;
	}
	if (!expected.isEmpty()) {
		const QString renamed = expected + QStringLiteral(" (QML)");
		field->setProperty("value", renamed);
		const QString in_data = ToQString(core().project()->database().actors.front().name);
		const QString shown = hero->property("text").toString();
		field->setProperty("value", expected);
		if (in_data != renamed || shown != renamed) {
			fail(QString("the name given to the QML field is \"%1\" in the data, \"%2\" in the window").arg(in_data, shown));
			return false;
		}
	}
	qCInfo(lcAutomation).noquote() << QString("QML window: style %1, %2x%3, hero \"%4\" (read and written)")
		.arg(QQuickStyle::name()).arg(image.width()).arg(image.height()).arg(name);
	return true;
}


bool Automation::databaseFile(const QStringList& args) {
	// as another program (rpgsync) reads and writes it, apart from the Editor;
	// expect-database: the one of the project, as read (again)
	auto project = core().project();
	const bool in_project = args[0] == "expect-database";
	std::unique_ptr<lcf::rpg::Database> database = project && !in_project ? project->readDatabase() : nullptr;
	if (!project || (!in_project && !database)) {
		fail("cannot read the database file");
		return false;
	}
	JsonT<lcf::rpg::Database> json;
	json.setData(in_project ? &project->database() : database.get());
	const QString& pointer = args[1];
	const QString value = args.mid(2).join(u' ');
	if (!json.exists(pointer)) {
		fail("no value " + pointer + " in the database");
		return false;
	}
	if (args[0] != "edit-database-file") {
		const QString shown = json.value(pointer).toString();
		if (shown != value) {
			fail(QStringLiteral("the database %1 has \"%2\" at %3, not \"%4\"")
				.arg(in_project ? QStringLiteral("of the project") : QStringLiteral("file"), shown, pointer, value));
			return false;
		}
		return true;
	}
	// a number for the numbers
	const QVariant current = json.value(pointer);
	bool number = false;
	const int as_number = value.toInt(&number);
	if (!json.set(pointer, current.typeId() == QMetaType::QString || !number ? QVariant(value) : QVariant(as_number))) {
		fail("cannot write " + value + " at " + pointer);
		return false;
	}
	const std::string path = project->databaseFile().toStdString();
	const bool saved = project->projectType() == FileFinder::ProjectType::Legacy
		? lcf::LDB_Reader::Save(path, *database, project->encoding().toStdString())
		: lcf::LDB_Reader::SaveXml(path, *database);
	if (!saved) {
		fail("cannot write the database file");
		return false;
	}
	qCInfo(lcAutomation).noquote() << "database file changed:" << pointer << "=" << value;
	return true;
}
