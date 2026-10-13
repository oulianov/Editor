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
#include <functional>
#include "screenshot.h"
#include "core.h"
#include "common/logging.h"
#include "ui/main_window.h"
#include "ui/map/map_scene.h"
#include "ui/common/map_preview.h"
#include "defines.h"
#include "model/dungeon_generator.h"
#include "model/map_tools.h"
#include <QAbstractButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QAction>
#include <QComboBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QStyleOptionButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QHeaderView>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QAbstractSlider>
#include <qpa/qwindowsysteminterface.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qshortcutmap_p.h>
#include <QWindow>
#include <QPaintEvent>
#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QGraphicsView>
#include <QMenu>
#include "ui/common/import_image_dialog.h"
#include <QNativeGestureEvent>
#include <QPointingDevice>
#include <cmath>
#include <QContextMenuEvent>
#include <QAbstractItemView>
#include <array>
#include <optional>

namespace {
// Time for layouts and paints to settle between two commands
constexpr int step_delay = 100;

QString describe(const QWidget* widget) {
	QString name = widget->objectName();
	// unnamed widgets and Qt's inner ones (qt_...): name the closest parent
	// named in the Editor, which tells where it is
	const QWidget* parent = widget->parentWidget();
	while ((name.isEmpty() || name.startsWith(QLatin1String("qt_"))) && parent) {
		if (!parent->objectName().isEmpty() && !parent->objectName().startsWith(QLatin1String("qt_"))) {
			name = parent->objectName() + "/" + (name.isEmpty() ? QStringLiteral("…") : name);
			break;
		}
		parent = parent->parentWidget();
	}
	return QString("%1 (%2)").arg(name, widget->metaObject()->className());
}

bool toInt(const QString& text, int& value) {
	bool ok = false;
	value = text.toInt(&ok);
	return ok;
}

// Counts the paint events of all the widgets (what a scroll repaints)
class PaintCounter : public QObject {
public:
	int count = 0;
	qint64 pixels = 0;
	QMap<QString, int> widgets;

	bool eventFilter(QObject* watched, QEvent* event) override {
		if (event->type() == QEvent::Paint && watched->isWidgetType()) {
			const QRect rect = static_cast<QPaintEvent*>(event)->rect();
			count++;
			pixels += qint64(rect.width()) * rect.height();
			widgets[describe(static_cast<QWidget*>(watched))]++;
		}
		return false;
	}
	QString summary() const {
		QList<std::pair<int, QString>> sorted;
		for (auto it = widgets.begin(); it != widgets.end(); ++it) {
			sorted.append({it.value(), it.key()});
		}
		std::sort(sorted.begin(), sorted.end(), std::greater<>());
		QStringList top;
		for (int i = 0; i < std::min<qsizetype>(8, sorted.size()); i++) {
			top << QString("%1 x%2").arg(sorted[i].second).arg(sorted[i].first);
		}
		return top.join(", ");
	}
};

// A mouse event on a cell of a map preview (Select Location, Define Area Boundaries, Select Tile)
void previewMouse(MapPreview* preview, QEvent::Type type, int x, int y, Qt::MouseButton button, Qt::MouseButtons buttons) {
	const QPoint pos = preview->cellPosition(x, y);
	QWidget* viewport = preview->viewport();
	// sent at once: the position is only right until the view scrolls for the next cell
	QMouseEvent event(type, pos, viewport->mapToGlobal(pos), button, buttons, Qt::NoModifier);
	QApplication::sendEvent(viewport, &event);
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

// The check boxes and radio buttons of window with what they draw: their box
// can lie outside the widget
QList<std::pair<QAbstractButton*, QRect>> optionBounds(QWidget* window) {
	QList<std::pair<QAbstractButton*, QRect>> options;
	for (auto* button : window->findChildren<QAbstractButton*>()) {
		const bool checkbox = qobject_cast<QCheckBox*>(button);
		if ((!checkbox && !qobject_cast<QRadioButton*>(button)) || !button->isVisibleTo(window)) continue;
		QStyleOptionButton option;
		option.initFrom(button);
		if (button->testAttribute(Qt::WA_MacSmallSize)) option.state |= QStyle::State_Small;
		if (button->testAttribute(Qt::WA_MacMiniSize)) option.state |= QStyle::State_Mini;
		option.text = button->text();
		const QRect indicator = button->style()->subElementRect(checkbox ? QStyle::SE_CheckBoxIndicator :
			QStyle::SE_RadioButtonIndicator, &option, button);
		options.append({button, button->rect().united(indicator).translated(button->pos())});
	}
	return options;
}

/** The space between two options of one column (negative: they overlap); none when side by side */
std::optional<int> optionClearance(const std::pair<QAbstractButton*, QRect>& first, const std::pair<QAbstractButton*, QRect>& second) {
	const auto& [a, bounds_a] = first;
	const auto& [b, bounds_b] = second;
	if (a->parentWidget() != b->parentWidget() || bounds_a.left() > bounds_b.right() || bounds_b.left() > bounds_a.right()) {
		return std::nullopt;
	}
	return std::max(bounds_a.top(), bounds_b.top()) - std::min(bounds_a.bottom(), bounds_b.bottom()) - 1;
}

QString rectText(const QRect& rect) {
	return QString("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

// A mouse event on a graphics view, at a point of its scene
void viewMouse(QGraphicsView* view, QEvent::Type type, QPointF scene_pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
	const QPoint pos = view->mapFromScene(scene_pos);
	QWidget* viewport = view->viewport();
	QMouseEvent event(type, pos, viewport->mapToGlobal(pos), button, buttons, Qt::NoModifier);
	QApplication::sendEvent(viewport, &event);
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

// A mouse event on a cell of the map (posted: a double click opens a dialog)
void mapMouse(MapScene* scene, QEvent::Type type, int x, int y, Qt::MouseButton button, Qt::MouseButtons buttons) {
	QGraphicsView* view = scene->views().value(0);
	if (!view) {
		return;
	}
	const double size = core().tileSize() * static_cast<double>(scene->scale());
	const QPointF scene_pos((x + 0.5) * size, (y + 0.5) * size);
	view->ensureVisible(QRectF(scene_pos, QSizeF(1, 1)), 0, 0);
	const QPoint pos = view->mapFromScene(scene_pos);
	QWidget* viewport = view->viewport();
	QApplication::postEvent(viewport, new QMouseEvent(type, pos, viewport->mapToGlobal(pos), button, buttons, Qt::NoModifier));
}
}

Automation::Automation(MainWindow& window, const QString& script_file, QObject* parent) :
	QObject(parent), m_window(window), m_file(script_file)
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
	if (cmd == "open-project" && args.size() == 2) {
		m_window.LoadProject(args[1]);
		return core().project() != nullptr;
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
		m_window.openScene(a);
		MapScene* scene = m_window.currentScene();
		const lcf::rpg::Event* event = scene ? scene->findEvent(b) : nullptr;
		if (!event) {
			fail(QString("no event %1 on map %2").arg(b).arg(a));
			return false;
		}
		scene->editEvent(*event);
		return true;
	}
	if (cmd == "action" && (args.size() == 2 || args.size() == 3)) {
		// a dialog's own action first (the one of the page shown), then the
		// map shown: each open map has its own menu actions
		MapScene* scene = m_window.currentScene();
		QAction* action = nullptr;
		if (QWidget* window = Screenshot::activeWindow(); window && window != &m_window) {
			for (auto* candidate : window->findChildren<QAction*>(args[1])) {
				auto* owner = qobject_cast<QWidget*>(candidate->parent());
				if (owner && owner->isVisibleTo(window)) {
					action = candidate;
					break;
				}
			}
		}
		action = action ? action : (scene ? scene->findChild<QAction*>(args[1]) : nullptr);
		action = action ? action : m_window.findChild<QAction*>(args[1]);
		if (!action) {
			fail("no action " + args[1]);
			return false;
		}
		if (!action->isEnabled()) {
			fail("the action " + args[1] + " is disabled");
			return false;
		}
		if (args.size() == 3) {
			// a setting: check or uncheck, whatever its current state
			if (!action->isCheckable() || (args[2] != "on" && args[2] != "off")) {
				return false;
			}
			action->setChecked(args[2] == "on");
			return true;
		}
		// queued: an action opening a dialog must not block the script
		QMetaObject::invokeMethod(action, &QAction::trigger, Qt::QueuedConnection);
		return true;
	}
	if (cmd == "tab" && args.size() == 3 && toInt(args[2], a)) {
		QWidget* window = Screenshot::activeWindow();
		auto* tabs = window ? window->findChild<QTabWidget*>(args[1]) : nullptr;
		if (!tabs || a < 0 || a >= tabs->count()) {
			fail(QString("no tab %1 in %2").arg(a).arg(args[1]));
			return false;
		}
		tabs->setCurrentIndex(a);
		return true;
	}
	if (cmd == "resize" && args.size() == 3 && toInt(args[1], a) && toInt(args[2], b)) {
		QWidget* window = Screenshot::activeWindow();
		if (!window) {
			return false;
		}
		window->resize(a, b);
		return true;
	}
	if (cmd == "screenshot" && args.size() <= 2) {
		return !Screenshot::save(nullptr, args.value(1)).isEmpty();
	}
	if (cmd == "screenshot-menu" && args.size() == 2) {
		// the menu (or the searchable list) shown: a window of its own,
		// outside the window's screenshot
		QWidget* menu = QApplication::activePopupWidget();
		if (!menu || !menu->grab().save(args[1])) {
			fail("no menu shown");
			return false;
		}
		return true;
	}
	if (cmd == "check-layout" && args.size() == 1) {
		QWidget* window = Screenshot::activeWindow();
		if (!window) {
			return false;
		}
		int problems = checkLayout(window);
		// the window itself: smaller than its layout squeezes everything
		const QSize minimum = window->layout() ? window->layout()->totalMinimumSize() : QSize();
		if (window->width() < minimum.width() || window->height() < minimum.height()) {
			qCWarning(lcAutomation).noquote() << QString("%1 is %2x%3, smaller than its layout (%4x%5)")
				.arg(describe(window)).arg(window->width()).arg(window->height()).arg(minimum.width()).arg(minimum.height());
			problems++;
		}
		m_failures += problems;
		qCInfo(lcAutomation).noquote() << QString("layout of %1 (%2x%3, minimum %4x%5): %6 problem(s)").arg(describe(window))
			.arg(window->width()).arg(window->height()).arg(window->minimumWidth()).arg(window->minimumHeight()).arg(problems);
		return true;
	}
	if (cmd == "check-option-spacing" && args.size() == 2 && toInt(args[1], a) && a >= 0) {
		QWidget* window = Screenshot::activeWindow();
		if (!window) return false;
		const auto options = optionBounds(window);
		for (qsizetype i = 0; i < options.size(); ++i) {
			for (qsizetype j = i + 1; j < options.size(); ++j) {
				const auto gap = optionClearance(options[i], options[j]);
				if (gap && *gap < a) fail(QString("options %1 and %2 have %3px clearance, require %4px (%5 and %6)")
					.arg(describe(options[i].first), describe(options[j].first)).arg(*gap).arg(a)
					.arg(rectText(options[i].second), rectText(options[j].second)));
			}
		}
		return true;
	}
	if (cmd == "focus" && args.size() == 2) {
		QWidget* widget = findWidget(args[1]);
		if (!widget) {
			fail("no widget " + args[1]);
			return false;
		}
		widget->activateWindow();
		widget->setFocus(Qt::OtherFocusReason);
		return true;
	}
	if (cmd == "select-command" && args.size() == 2 && toInt(args[1], a)) {
		auto* tree = qobject_cast<QTreeWidget*>(findWidget(QStringLiteral("commands")));
		if (!tree) {
			fail("no event command list");
			return false;
		}
		for (QTreeWidgetItemIterator it(tree); *it; ++it) {
			if ((*it)->text(1).toInt() == a) {
				tree->setCurrentItem(*it);
				tree->scrollToItem(*it);
				tree->setFocus(Qt::OtherFocusReason);
				return true;
			}
		}
		fail(QString("no event command at index %1").arg(a));
		return false;
	}
	if (cmd == "key" && args.size() == 2) {
		QKeySequence sequence = QKeySequence::fromString(args[1]);
		QWidget* target = keyTarget();
		if (sequence.isEmpty() || !target) {
			return false;
		}
		const QKeyCombination combination = sequence[0];
		qCDebug(lcAutomation) << "key" << sequence << "to" << target;
		// queued: a key opening a dialog must not block the script
		QMetaObject::invokeMethod(target, [target, combination] {
			// as when typed: the shortcuts first (QShortcut, menu actions),
			// unless the widget with the focus takes the key
			// a key goes to the active window: the one of the target
			if (QWindow* window = target->window()->windowHandle(); window && QGuiApplication::focusWindow() != window) {
				QWindowSystemInterface::handleFocusWindowChanged<QWindowSystemInterface::SynchronousDelivery>(window);
			}
			QKeyEvent press(QEvent::KeyPress, combination.key(), combination.keyboardModifiers());
			if (QGuiApplicationPrivate::instance()->shortcutMap.tryShortcut(&press)) {
				return;
			}
			QApplication::sendEvent(target, &press);
			QKeyEvent release(QEvent::KeyRelease, combination.key(), combination.keyboardModifiers());
			QApplication::sendEvent(target, &release);
		}, Qt::QueuedConnection);
		return true;
	}
	if (cmd == "type" && args.size() >= 2) {
		QWidget* target = keyTarget();
		if (!target) {
			fail("nothing has the focus");
			return false;
		}
		for (QChar c : args.mid(1).join(' ')) {
			QApplication::postEvent(target, new QKeyEvent(QEvent::KeyPress, 0, Qt::NoModifier, QString(c)));
			QApplication::postEvent(target, new QKeyEvent(QEvent::KeyRelease, 0, Qt::NoModifier, QString(c)));
		}
		return true;
	}
	if (cmd == "click" && args.size() >= 2) {
		// the buttons of a pop-up shown first (the codes of a message)
		QWidget* window = QApplication::activePopupWidget();
		if (!window) {
			window = Screenshot::activeWindow();
		}
		const QString name = args.mid(1).join(' ');
		QAbstractButton* found = nullptr;
		for (auto* button : window ? window->findChildren<QAbstractButton*>() : QList<QAbstractButton*>()) {
			QString text = button->text().remove('&');
			if (button->isVisibleTo(window) && (button->objectName() == name || text == name || text == name + "...")) {
				found = button;
				break;
			}
		}
		if (!found) {
			fail("no button " + name);
			return false;
		}
		// queued: a button opening a dialog must not block the script
		QMetaObject::invokeMethod(found, &QAbstractButton::click, Qt::QueuedConnection);
		return true;
	}
	if (cmd == "context-menu" && args.size() == 2) {
		// the menu of a right click on the widget (on its current line for a list)
		QWidget* widget = findWidget(args[1]);
		if (!widget) {
			fail("no widget " + args[1]);
			return false;
		}
		QWidget* target = widget;
		QPoint pos = widget->rect().center();
		if (auto* view = qobject_cast<QAbstractItemView*>(widget)) {
			target = view->viewport();
			pos = view->currentIndex().isValid() ? view->visualRect(view->currentIndex()).center() : target->rect().center();
		}
		// posted: the menu runs its own loop, the script goes on in it
		QApplication::postEvent(target, new QContextMenuEvent(QContextMenuEvent::Mouse, pos, target->mapToGlobal(pos)));
		return true;
	}
	if (cmd == "dump-menu" && args.size() <= 2) {
		// each item of the menu shown (or of a menu by object name, such as a
		// menu of the menu bar): its text, its shortcut, whether it has an
		// icon, whether it is greyed, its tooltip; the separators as "--"
		auto* menu = args.size() == 2 ? m_window.findChild<QMenu*>(args[1])
			: qobject_cast<QMenu*>(QApplication::activePopupWidget());
		if (!menu) {
			fail(args.size() == 2 ? "no menu " + args[1] : QString("no menu shown"));
			return false;
		}
		for (QAction* action : menu->actions()) {
			if (action->isSeparator()) {
				if (args.size() == 2)
					qCInfo(lcAutomation).noquote() << "  --";
				continue;
			}
			const QString tip = action->toolTip() != action->text().remove('&') ? " | " + action->toolTip() : QString();
			qCInfo(lcAutomation).noquote() << QString("  %1 | %2 | %3%4%5").arg(action->text().remove('&'),
				action->shortcut().toString(QKeySequence::PortableText), action->icon().isNull() ? "-" : "icon",
				action->isEnabled() ? "" : " | greyed", tip);
		}
		return true;
	}
	if (cmd == "close-menu" && args.size() == 1) {
		if (QWidget* popup = QApplication::activePopupWidget()) {
			popup->close();
		}
		return true;
	}
	if (cmd == "menu" && args.size() >= 2) {
		// an item of the pop-up menu shown (the menu of a button, a context menu), by its text
		auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
		const QString text = args.mid(1).join(' ');
		for (QAction* action : menu ? menu->actions() : QList<QAction*>()) {
			const QString label = action->text().remove('&');
			if (action->isEnabled() && (label == text || label == text + "...")) {
				// chosen like with the keyboard: the menu closes, then the item runs
				menu->setActiveAction(action);
				QApplication::postEvent(menu, new QKeyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier));
				return true;
			}
		}
		fail("no menu item " + text);
		return false;
	}
	if ((cmd == "click-item" || cmd == "double-click-item") && args.size() >= 3) {
		// a list view (database lists): the lines of its model
		if (auto* view = qobject_cast<QAbstractItemView*>(findWidget(args[1]));
				view && !qobject_cast<QTreeWidget*>(view) && view->model()) {
			const QString text = args.mid(2).join(' ');
			QAbstractItemModel* model = view->model();
			QModelIndexList found = model->match(model->index(0, 0), Qt::DisplayRole, text, 1, Qt::MatchExactly);
			if (found.isEmpty()) {
				found = model->match(model->index(0, 0), Qt::DisplayRole, text, 1, Qt::MatchContains);
			}
			if (found.isEmpty()) {
				fail(QString("no line \"%1\" in %2").arg(text, args[1]));
				return false;
			}
			view->scrollTo(found.first());
			const QPoint pos = view->visualRect(found.first()).center();
			const QPoint global = view->viewport()->mapToGlobal(pos);
			QApplication::postEvent(view->viewport(), new QMouseEvent(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
			QApplication::postEvent(view->viewport(), new QMouseEvent(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier));
			return true;
		}
		// a mouse click on the line showing exactly the text, else on the
		// first line containing it
		auto* tree = qobject_cast<QTreeWidget*>(findWidget(args[1]));
		if (!tree) {
			fail("no list " + args[1]);
			return false;
		}
		const QString text = args.mid(2).join(' ');
		QTreeWidgetItem* found = nullptr;
		for (QTreeWidgetItemIterator it(tree); *it && !found; ++it) {
			if ((*it)->text(0) == text) {
				found = *it;
			}
		}
		for (QTreeWidgetItemIterator it(tree); *it && !found; ++it) {
			if ((*it)->text(0).contains(text)) {
				found = *it;
			}
		}
		if (!found) {
			fail(QString("no line \"%1\" in %2").arg(text, args[1]));
			return false;
		}
		tree->scrollToItem(found);
		const QPoint pos = tree->visualItemRect(found).center();
		const QPoint global = tree->viewport()->mapToGlobal(pos);
		QApplication::postEvent(tree->viewport(), new QMouseEvent(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
		QApplication::postEvent(tree->viewport(), new QMouseEvent(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier));
		if (cmd == "double-click-item") {
			// the second press of a double-click comes as one
			QApplication::postEvent(tree->viewport(), new QMouseEvent(QEvent::MouseButtonDblClick, pos, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier));
			QApplication::postEvent(tree->viewport(), new QMouseEvent(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier));
		}
		return true;
	}
	if (cmd == "set-control" && args.size() >= 3) {
		// a value for a control: index of a combo box, number, on/off, text, button id of a group
		const QString value = args.mid(2).join(' ');
		QWidget* window = Screenshot::activeWindow();
		if (auto* group = window ? window->findChild<QButtonGroup*>(args[1]) : nullptr) {
			if (auto* button = group->button(value.toInt())) {
				button->click();
				return true;
			}
			return false;
		}
		QWidget* widget = findWidget(args[1]);
		if (auto* combo = qobject_cast<QComboBox*>(widget)) {
			combo->setCurrentIndex(value.toInt());
		} else if (auto* spin = qobject_cast<QSpinBox*>(widget)) {
			spin->setValue(value.toInt());
		} else if (auto* button = qobject_cast<QAbstractButton*>(widget); button && button->isCheckable()) {
			if (button->isChecked() != (value == "on")) {
				button->click();
			}
		} else if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
			// as typed: fields that save on textEdited see it
			edit->setText(value);
			emit edit->textEdited(value);
		} else if (auto* text = qobject_cast<QPlainTextEdit*>(widget)) {
			text->setPlainText(value);
		} else if (auto* slider = qobject_cast<QAbstractSlider*>(widget)) {
			slider->setValue(value.toInt());
		} else if (auto* group = qobject_cast<QGroupBox*>(widget); group && group->isCheckable()) {
			group->setChecked(value == "on");
		} else {
			fail("no control " + args[1]);
			return false;
		}
		return true;
	}
	if (cmd == "expect-enabled" && args.size() >= 3) {
		// a name, or the text of a button
		const QString name = args.mid(1, args.size() - 2).join(' ');
		const bool on = args.last() == "on";
		QWidget* widget = findWidget(name);
		// or a menu action, by object name or text: those of the menu shown
		// and its submenus first, as each map tab has actions of the same names
		QList<QAction*> candidates;
		if (!widget) {
			std::function<void(QMenu*)> collect = [&](QMenu* menu) {
				for (QAction* item : menu->actions()) {
					candidates.append(item);
					if (item->menu())
						collect(item->menu());
				}
			};
			if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget()))
				collect(menu);
			candidates += m_window.findChildren<QAction*>();
		}
		QAction* action = nullptr;
		for (auto* candidate : candidates) {
			if (candidate->objectName() == name || candidate->text().remove('&') == name) {
				action = candidate;
				break;
			}
		}
		const bool enabled = widget ? widget->isEnabled() : action && action->isEnabled();
		if (!widget && !action) {
			fail("no control " + name);
			return false;
		}
		if (enabled != on) {
			fail(QString("%1 is not %2").arg(name, on ? "enabled" : "disabled"));
			return false;
		}
		return true;
	}
	if (cmd == "expect-checked" && args.size() >= 3) {
		// a check box, radio button or menu option, by object name or text
		const QString name = args.mid(1, args.size() - 2).join(' ');
		const bool on = args.last() == "on";
		auto* button = qobject_cast<QAbstractButton*>(findWidget(name));
		QAction* action = button ? nullptr : m_window.findChild<QAction*>(name);
		if (!button && !action) {
			fail("no control " + name);
			return false;
		}
		if ((button ? button->isChecked() : action->isChecked()) != on) {
			fail(QString("%1 is not %2").arg(name, on ? "checked" : "unchecked"));
			return false;
		}
		return true;
	}
	if (cmd == "expect-option-enabled" && args.size() == 4) {
		auto* combo = qobject_cast<QComboBox*>(findWidget(args[1]));
		auto* model = combo ? qobject_cast<QStandardItemModel*>(combo->model()) : nullptr;
		const QStandardItem* item = model ? model->item(args[2].toInt()) : nullptr;
		if (!item || item->isEnabled() != (args[3] == "on")) {
			fail(QString("option %1 of %2 is not %3").arg(args[2], args[1], args[3] == "on" ? "enabled" : "disabled"));
			return false;
		}
		return true;
	}
	if (cmd == "expect-not" && args.size() >= 3) {
		QWidget* widget = findWidget(args[1]);
		if (!widget) {
			fail("no widget " + args[1]);
			return false;
		}
		// the text: the rest of the line, spaces included
		const QString expected = args.mid(2).join(' ');
		if (widgetText(widget).contains(expected)) {
			fail(QString("%1 shows \"%2\"").arg(args[1], expected));
			return false;
		}
		return true;
	}
	if (cmd == "expect" && args.size() >= 3) {
		QWidget* widget = findWidget(args[1]);
		if (!widget) {
			fail("no widget " + args[1]);
			return false;
		}
		const QString expected = args.mid(2).join(' ');
		QString text = widgetText(widget);
		if (!text.contains(expected)) {
			fail(QString("%1 does not show \"%2\", it shows:\n%3").arg(args[1], expected, text));
			return false;
		}
		return true;
	}
	if (cmd == "inspect-controls" && args.size() == 2) {
		QWidget* window = Screenshot::activeWindow();
		if (!window) {
			return false;
		}
		QJsonArray controls;
		for (auto* widget : window->findChildren<QWidget*>()) {
			QJsonObject control{{"name", widget->objectName()}, {"enabled", widget->isEnabled()},
				{"visible", widget->isVisibleTo(window)}};
			if (auto* spin = qobject_cast<QSpinBox*>(widget)) {
				control.insert("type", "spin");
				control.insert("value", spin->value());
				control.insert("minimum", spin->minimum());
				control.insert("maximum", spin->maximum());
			} else if (auto* combo = qobject_cast<QComboBox*>(widget)) {
				control.insert("type", "combo");
				control.insert("value", combo->currentIndex());
				QJsonArray items;
				for (int i = 0; i < combo->count(); ++i) {
					items.append(combo->itemText(i));
				}
				control.insert("items", items);
			} else if (auto* button = qobject_cast<QAbstractButton*>(widget); button && button->isCheckable()) {
				control.insert("type", "check");
				control.insert("value", button->isChecked());
				control.insert("text", button->text());
			} else if (auto* edit = qobject_cast<QLineEdit*>(widget); edit && !qobject_cast<QSpinBox*>(edit->parentWidget())) {
				control.insert("type", "text");
				control.insert("value", edit->text());
			} else if (auto* edit = qobject_cast<QPlainTextEdit*>(widget)) {
				control.insert("type", "plain-text");
				control.insert("value", edit->toPlainText());
			} else if (auto* group = qobject_cast<QGroupBox*>(widget)) {
				control.insert("type", "group");
				control.insert("text", group->title());
			} else {
				continue;
			}
			// what makes a window wider than wanted
			control.insert("x", widget->mapTo(window, QPoint()).x());
			control.insert("y", widget->mapTo(window, QPoint()).y());
			control.insert("width", widget->width());
			control.insert("height", widget->height());
			control.insert("minimumWidth", widget->minimumSizeHint().width());
			controls.append(control);
		}
		QFile file(args[1]);
		return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(controls).toJson()) >= 0;
	}
	if (cmd == "select-map" && args.size() == 2 && toInt(args[1], a)) {
		// by id: map names can repeat; 0 is the project
		auto* tree = m_window.findChild<QTreeWidget*>(QStringLiteral("treeMap"));
		if (!tree) {
			fail("no map tree");
			return false;
		}
		for (QTreeWidgetItemIterator it(tree); *it; ++it) {
			if ((*it)->data(1, Qt::DisplayRole).toInt() == a) {
				// as a click: the tree has the focus when the map is selected
				tree->setFocus(Qt::OtherFocusReason);
				tree->setCurrentItem(*it);
				tree->scrollToItem(*it);
				return true;
			}
		}
		fail(QString("no map %1 in the map tree").arg(a));
		return false;
	}
	if (cmd == "spin-click" && args.size() == 3 && (args[2] == "up" || args[2] == "down")) {
		// a mouse click in the middle of an arrow of a spin box, where the
		// style draws it
		auto* spin = qobject_cast<QAbstractSpinBox*>(findWidget(args[1]));
		if (!spin) {
			fail("no spin box " + args[1]);
			return false;
		}
		QStyleOptionSpinBox option;
		option.initFrom(spin);
		option.frame = spin->hasFrame();
		option.subControls = QStyle::SC_All;
		option.buttonSymbols = spin->buttonSymbols();
		const QPointF pos = spin->style()->subControlRect(QStyle::CC_SpinBox, &option,
			args[2] == "up" ? QStyle::SC_SpinBoxUp : QStyle::SC_SpinBoxDown, spin).center();
		for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
			QMouseEvent event(type, pos, spin->mapToGlobal(pos), Qt::LeftButton,
				type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
			QApplication::sendEvent(spin, &event);
		}
		return true;
	}
	if (cmd == "dump-geometry" && args.size() == 2) {
		// where a widget is in its window, what of it the layouts align (its
		// layout item) and, for a spin box, its field and its arrows
		QWidget* widget = findWidget(args[1]);
		if (!widget) {
			fail("no widget " + args[1]);
			return false;
		}
		auto text = [](const QRect& r) { return QString("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()); };
		const QRect in_window(widget->mapTo(widget->window(), QPoint()), widget->size());
		QWidgetItem item(widget);
		qCInfo(lcAutomation).noquote() << QString("%1: %2, size hint %3x%4, layout item %5x%6").arg(describe(widget), text(in_window))
			.arg(widget->sizeHint().width()).arg(widget->sizeHint().height()).arg(item.sizeHint().width()).arg(item.sizeHint().height());
		auto dumpCombo = [&text](QComboBox* combo) {
			QStyleOptionComboBox option;
			option.initFrom(combo);
			option.editable = combo->isEditable();
			option.frame = combo->hasFrame();
			option.subControls = QStyle::SC_All;
			for (auto [name, sub] : {std::pair{"frame", QStyle::SC_ComboBoxFrame}, {"field", QStyle::SC_ComboBoxEditField},
					{"arrow", QStyle::SC_ComboBoxArrow}}) {
				qCInfo(lcAutomation).noquote() << QString("    %1: %2").arg(name, text(combo->style()->subControlRect(QStyle::CC_ComboBox, &option, sub, combo)));
			}
			qCInfo(lcAutomation).noquote() << "    layout item: " + text(combo->style()->subElementRect(QStyle::SE_ComboBoxLayoutItem, &option, combo))
				+ (combo->isEditable() ? " (editable)" : "");
		};
		if (auto* combo = qobject_cast<QComboBox*>(widget)) {
			dumpCombo(combo);
		}
		// the widgets of its layout (a list and its "..." button...)
		for (int i = 0; widget->layout() && i < widget->layout()->count(); ++i) {
			if (QWidget* part = widget->layout()->itemAt(i)->widget()) {
				const QRect r = part->geometry();
				QWidgetItem part_item(part);
				qCInfo(lcAutomation).noquote() << QString("  part %1: %2, layout item %3x%4").arg(QString::fromLatin1(part->metaObject()->className()), text(r))
					.arg(part_item.sizeHint().width()).arg(part_item.sizeHint().height());
				if (auto* combo = qobject_cast<QComboBox*>(part)) {
					dumpCombo(combo);
				}
			}
		}
		if (auto* spin = qobject_cast<QAbstractSpinBox*>(widget)) {
			QStyleOptionSpinBox option;
			option.initFrom(spin);
			option.frame = spin->hasFrame();
			option.subControls = QStyle::SC_All;
			option.buttonSymbols = spin->buttonSymbols();
			option.stepEnabled = QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
			for (auto [name, sub] : {std::pair{"frame", QStyle::SC_SpinBoxFrame}, {"field", QStyle::SC_SpinBoxEditField},
					{"up", QStyle::SC_SpinBoxUp}, {"down", QStyle::SC_SpinBoxDown}}) {
				qCInfo(lcAutomation).noquote() << QString("  %1: %2").arg(name, text(spin->style()->subControlRect(QStyle::CC_SpinBox, &option, sub, spin)));
			}
		}
		return true;
	}
	if (cmd == "import-image-dialog" && args.size() == 2) {
		// the window of an image import (transparent colour, zoom), without
		// the file chooser before it
		auto* dialog = new ImportImageDialog(args[1], &m_window);
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->show();
		return true;
	}
	if (cmd == "pinch" && args.size() == 3) {
		// a trackpad pinch on the map ("map") or a view, the fingers spreading
		// (factor > 1) or closing, in ten events as macOS sends them
		bool ok = false;
		const double factor = args[2].toDouble(&ok);
		QGraphicsView* view = nullptr;
		MapScene* scene = args[1] == "map" ? m_window.currentScene() : nullptr;
		if (scene && !scene->views().isEmpty()) {
			view = scene->views().first();
		} else if (args[1] != "map") {
			view = qobject_cast<QGraphicsView*>(findWidget(args[1]));
		}
		if (!view || !ok || factor <= 0) {
			fail("no view " + args[1]);
			return false;
		}
		QWidget* target = view->viewport();
		const QPointF pos = target->rect().center();
		const QPointF global = target->mapToGlobal(pos);
		const double step = std::pow(factor, 0.1) - 1.0;
		auto send = [&](Qt::NativeGestureType type, double value) {
			QNativeGestureEvent gesture(type, QPointingDevice::primaryPointingDevice(), 2, pos, pos, global, value, QPointF());
			QApplication::sendEvent(target, &gesture);
		};
		send(Qt::BeginNativeGesture, 0);
		for (int i = 0; i < 10; ++i) {
			send(Qt::ZoomNativeGesture, step);
		}
		send(Qt::EndNativeGesture, 0);
		qCInfo(lcAutomation).noquote() << QString("%1 zoomed x%2").arg(args[1])
			.arg(scene ? static_cast<double>(scene->scale()) : view->transform().m11());
		return true;
	}
	if ((cmd == "map-click" || cmd == "map-double-click") && (args.size() == 3 || args.size() == 4)
			&& toInt(args[1], a) && toInt(args[2], b)) {
		MapScene* scene = m_window.currentScene();
		if (!scene || (args.size() == 4 && args[3] != "left" && args[3] != "right")) {
			fail("no map shown");
			return false;
		}
		const Qt::MouseButton button = args.value(3) == "right" ? Qt::RightButton : Qt::LeftButton;
		// as a click: the map takes the keys
		if (!scene->views().isEmpty()) {
			scene->views().first()->setFocus(Qt::MouseFocusReason);
		}
		mapMouse(scene, QEvent::MouseMove, a, b, Qt::NoButton, Qt::NoButton);
		mapMouse(scene, QEvent::MouseButtonPress, a, b, button, button);
		mapMouse(scene, QEvent::MouseButtonRelease, a, b, button, Qt::NoButton);
		if (cmd == "map-double-click") {
			mapMouse(scene, QEvent::MouseButtonDblClick, a, b, button, button);
			mapMouse(scene, QEvent::MouseButtonRelease, a, b, button, Qt::NoButton);
		}
		return true;
	}
	if (cmd == "map-drag" && args.size() == 5) {
		int x2 = 0, y2 = 0;
		MapScene* scene = m_window.currentScene();
		if (!scene || !toInt(args[1], a) || !toInt(args[2], b) || !toInt(args[3], x2) || !toInt(args[4], y2)) {
			fail("no map shown");
			return false;
		}
		mapMouse(scene, QEvent::MouseMove, a, b, Qt::NoButton, Qt::NoButton);
		mapMouse(scene, QEvent::MouseButtonPress, a, b, Qt::LeftButton, Qt::LeftButton);
		// through every cell, as a hand would
		const int steps = std::max(std::abs(x2 - a), std::abs(y2 - b));
		for (int i = 1; i <= steps; i++) {
			mapMouse(scene, QEvent::MouseMove, a + (x2 - a) * i / steps, b + (y2 - b) * i / steps,
				Qt::NoButton, Qt::LeftButton);
		}
		mapMouse(scene, QEvent::MouseButtonRelease, x2, y2, Qt::LeftButton, Qt::NoButton);
		return true;
	}
	if ((cmd == "preview-click" || cmd == "preview-double-click") && args.size() == 4
			&& toInt(args[2], a) && toInt(args[3], b)) {
		auto* preview = qobject_cast<MapPreview*>(findWidget(args[1]));
		if (!preview || a < 0 || b < 0 || a >= preview->columns() || b >= preview->rows()) {
			fail(QString("no cell %1 %2 in %3").arg(a).arg(b).arg(args[1]));
			return false;
		}
		previewMouse(preview, QEvent::MouseButtonPress, a, b, Qt::LeftButton, Qt::LeftButton);
		previewMouse(preview, QEvent::MouseButtonRelease, a, b, Qt::LeftButton, Qt::NoButton);
		if (cmd == "preview-double-click") {
			previewMouse(preview, QEvent::MouseButtonDblClick, a, b, Qt::LeftButton, Qt::LeftButton);
			previewMouse(preview, QEvent::MouseButtonRelease, a, b, Qt::LeftButton, Qt::NoButton);
		}
		return true;
	}
	if (cmd == "preview-drag" && args.size() == 6) {
		int x2 = 0, y2 = 0;
		auto* preview = qobject_cast<MapPreview*>(findWidget(args[1]));
		if (!preview || !toInt(args[2], a) || !toInt(args[3], b) || !toInt(args[4], x2) || !toInt(args[5], y2)) {
			fail("no map preview " + args[1]);
			return false;
		}
		previewMouse(preview, QEvent::MouseButtonPress, a, b, Qt::LeftButton, Qt::LeftButton);
		previewMouse(preview, QEvent::MouseMove, x2, y2, Qt::NoButton, Qt::LeftButton);
		previewMouse(preview, QEvent::MouseButtonRelease, x2, y2, Qt::LeftButton, Qt::NoButton);
		return true;
	}
	if (cmd == "expect-tab" && args.size() == 3) {
		// the tab shown in a tab widget, by its text
		auto* tabs = qobject_cast<QTabWidget*>(findWidget(args[1]));
		const QString shown = tabs ? tabs->tabText(tabs->currentIndex()).remove('&') : QString();
		if (shown != args[2]) {
			fail(QString("%1 shows the tab \"%2\", not \"%3\"").arg(args[1], shown, args[2]));
			return false;
		}
		return true;
	}
	if (cmd == "click-tab" && args.size() == 3 && toInt(args[2], a)) {
		// a click on a tab of a tab widget, as the mouse does it (the click
		// focuses the bar, then the arrows go to the next tabs)
		auto* tabs = qobject_cast<QTabWidget*>(findWidget(args[1]));
		QTabBar* bar = tabs ? tabs->tabBar() : nullptr;
		if (!bar || a < 0 || a >= bar->count()) {
			fail("no tab " + args[2] + " in " + args[1]);
			return false;
		}
		const QPointF pos = bar->tabRect(a).center();
		for (auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
			QMouseEvent event(type, pos, bar->mapToGlobal(pos), Qt::LeftButton,
				type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
			QApplication::sendEvent(bar, &event);
		}
		return true;
	}
	if (cmd == "view-click" || cmd == "view-drag") {
		// a QGraphicsView by object name, in the coordinates of its scene
		const bool drag = cmd == "view-drag";
		const int count = drag ? 4 : 2;
		std::vector<int> v(static_cast<size_t>(count));
		auto* view = qobject_cast<QGraphicsView*>(findWidget(args.value(1)));
		// a click can be a right click or (view-click only) a double-click
		const QString option = args.size() == count + 3 ? args.last() : QString();
		bool ok = view && (args.size() == count + 2 || option == "right" || (!drag && option == "double"));
		for (int i = 0; ok && i < count; i++) {
			ok = toInt(args[i + 2], v[static_cast<size_t>(i)]);
		}
		if (!ok) {
			fail("no graphics view " + args.value(1));
			return false;
		}
		const Qt::MouseButton button = option == "right" ? Qt::RightButton : Qt::LeftButton;
		const QPointF from(v[0], v[1]);
		const QPointF to = drag ? QPointF(v[2], v[3]) : from;
		viewMouse(view, QEvent::MouseMove, from, Qt::NoButton, Qt::NoButton);
		viewMouse(view, QEvent::MouseButtonPress, from, button, button);
		// small steps, as a hand would: a drag paints every tile on the way
		const int steps = drag ? std::max(1, qRound(QLineF(from, to).length() / 4)) : 0;
		for (int i = 1; i <= steps; i++) {
			viewMouse(view, QEvent::MouseMove, from + (to - from) * i / steps, Qt::NoButton, button);
		}
		viewMouse(view, QEvent::MouseButtonRelease, to, button, Qt::NoButton);
		if (option == "double") {
			viewMouse(view, QEvent::MouseButtonDblClick, to, button, button);
			viewMouse(view, QEvent::MouseButtonRelease, to, button, Qt::NoButton);
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
	if (cmd == "expect-tile" && args.size() == 5) {
		int x = 0, y = 0;
		MapScene* scene = m_window.currentScene();
		if (!scene || (args[1] != "lower" && args[1] != "upper")
				|| !toInt(args[2], x) || !toInt(args[3], y) || !toInt(args[4], a)) {
			fail("usage: expect-tile lower|upper X Y ID, with a map shown");
			return false;
		}
		const int tile = scene->tileAt(args[1] == "lower" ? Core::LOWER : Core::UPPER, x, y);
		// lower tiles: the terrain is compared (autotiles change with their neighbours)
		const bool same = args[1] == "lower"
			? core().translate(static_cast<short>(tile)) == core().translate(static_cast<short>(a))
			: tile == a;
		if (!same) {
			fail(QString("%1 tile at %2,%3 is %4, not %5").arg(args[1]).arg(x).arg(y).arg(tile).arg(a));
			return false;
		}
		return true;
	}
	if (cmd == "expect-event" && args.size() == 4 && toInt(args[1], a) && toInt(args[2], b)) {
		int id = 0;
		MapScene* scene = m_window.currentScene();
		if (!scene || !toInt(args[3], id)) {
			fail("no map shown");
			return false;
		}
		if (scene->eventIdAt(a, b) != id) {
			fail(QString("event at %1,%2 is %3, not %4").arg(a).arg(b).arg(scene->eventIdAt(a, b)).arg(id));
			return false;
		}
		return true;
	}
	if (cmd == "expect-cell" && args.size() == 3 && toInt(args[1], a) && toInt(args[2], b)) {
		MapScene* scene = m_window.currentScene();
		if (!scene) {
			fail("no map shown");
			return false;
		}
		const QPoint cell = scene->selectedCell();
		if (cell != QPoint(a, b)) {
			fail(QString("the cell selected is %1,%2, not %3,%4").arg(cell.x()).arg(cell.y()).arg(a).arg(b));
			return false;
		}
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
	if (cmd == "wheel" && args.size() == 3 && toInt(args[2], a)) {
		// A trackpad swipe over the middle of a widget, injected like the
		// platform's events: STEPS small pixel scrolls (negative: up) then
		// momentum. Logs the time, the slowest event, how far it scrolled and
		// what was painted for it.
		QWidget* target = findWidget(args[1]);
		QWidget* window = Screenshot::activeWindow();
		for (QWidget* widget : window && !target ? window->findChildren<QWidget*>() : QList<QWidget*>()) {
			// else the first one shown of a class (QScrollArea...)
			if (widget->inherits(args[1].toUtf8()) && widget->isVisible()) {
				target = widget;
				break;
			}
		}
		auto* area = qobject_cast<QAbstractScrollArea*>(target);
		QWidget* viewport = area ? area->viewport() : target;
		QWindow* handle = viewport ? viewport->window()->windowHandle() : nullptr;
		if (!handle) {
			fail("no widget " + args[1]);
			return false;
		}
		const QPoint pos = viewport->rect().center();
		const QPointF local = viewport->mapTo(viewport->window(), pos);
		const QPointF global = viewport->mapToGlobal(pos);
		const int before = area ? area->verticalScrollBar()->value() : 0;
		PaintCounter paints;
		qApp->installEventFilter(&paints);
		QElapsedTimer timer, step_timer;
		timer.start();
		qint64 slowest = 0;
		int events = 0;
		auto send = [&](int pixels, Qt::ScrollPhase phase, Qt::MouseEventSource source) {
			step_timer.start();
			QWindowSystemInterface::handleWheelEvent(handle, local, global, QPoint(0, pixels), QPoint(0, pixels * 2),
				Qt::NoModifier, phase, source);
			QWindowSystemInterface::flushWindowSystemEvents();
			// the repaint (UpdateRequest is posted); not processEvents(): its
			// timers would run the next commands inside this one
			QCoreApplication::sendPostedEvents();
			slowest = std::max(slowest, step_timer.elapsed());
			events++;
		};
		const int direction = a > 0 ? -1 : 1;
		send(0, Qt::ScrollBegin, Qt::MouseEventNotSynthesized);
		for (int i = 0; i < std::abs(a); i++) {
			send(direction * 8, Qt::ScrollUpdate, Qt::MouseEventNotSynthesized);
		}
		// Cocoa sends the end after the momentum: until then, the scrolled
		// widget keeps the wheel (QApplicationPrivate::wheel_widget)
		for (int speed = 12; speed > 0; speed--) {
			send(direction * speed, Qt::ScrollMomentum, Qt::MouseEventSynthesizedBySystem);
		}
		send(0, Qt::ScrollEnd, Qt::MouseEventSynthesizedBySystem);
		qApp->removeEventFilter(&paints);
		const int after = area ? area->verticalScrollBar()->value() : 0;
		qCInfo(lcAutomation).noquote() << QString("wheel %1: %2 events in %3 ms, slowest %4 ms, scrolled %5 px")
			.arg(describe(target)).arg(events).arg(timer.elapsed()).arg(slowest).arg(after - before);
		qCInfo(lcAutomation).noquote() << QString("wheel %1: painted %2 widgets per event (%3 pixels; the scrolled area has %4): %5")
			.arg(describe(target)).arg(paints.count / std::max(events, 1)).arg(paints.pixels / std::max(events, 1))
			.arg(viewport->width() * viewport->height()).arg(paints.summary());
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
	if (cmd == "close" && args.size() == 1) {
		// the modal dialog, else the window in front (the search window...)
		auto* dialog = qobject_cast<QDialog*>(Screenshot::activeWindow());
		if (!dialog) {
			fail("no dialog to close");
			return false;
		}
		dialog->reject();
		return true;
	}

	fail("unknown command or wrong arguments");
	return false;
}

QWidget* Automation::keyTarget() {
	// a pop-up shown takes the keys (the search field of a list), as typed
	if (QWidget* popup = QApplication::activePopupWidget()) {
		return popup->focusWidget() ? popup->focusWidget() : popup;
	}
	// Not QApplication::focusWidget(): it is null while another application
	// is active, keys go to the focused widget of the dialog or window
	QWidget* window = Screenshot::activeWindow();
	if (window && window->focusWidget()) {
		return window->focusWidget();
	}
	return QApplication::focusWidget() ? QApplication::focusWidget() : window;
}

QWidget* Automation::findWidget(const QString& name) {
	QWidget* window = Screenshot::activeWindow();
	if (!window) {
		return nullptr;
	}
	// the one shown, when several have the name (one per event page...)
	QWidget* hidden = nullptr;
	for (QWidget* widget : window->findChildren<QWidget*>(name)) {
		if (widget->isVisibleTo(window)) {
			return widget;
		}
		hidden = hidden ? hidden : widget;
	}
	if (!hidden) {
		// a button by its text, the one shown first (else on a page not shown)
		for (auto* button : window->findChildren<QAbstractButton*>()) {
			const QString text = button->text().remove('&');
			if (text == name || text == name + "...") {
				if (button->isVisibleTo(window)) {
					return button;
				}
				hidden = hidden ? hidden : button;
			}
		}
	}
	return hidden;
}

QString Automation::widgetText(QWidget* widget) {
	if (auto* tree = qobject_cast<QTreeWidget*>(widget)) {
		QStringList lines;
		// the current line is marked with ">"
		// the lines shown: those a filter hides do not count
		for (QTreeWidgetItemIterator it(tree, QTreeWidgetItemIterator::NotHidden); *it; ++it) {
			lines << ((*it) == tree->currentItem() ? "> " : "  ") + (*it)->text(0);
		}
		return lines.join('\n');
	}
	if (auto* view = qobject_cast<QAbstractItemView*>(widget); view && view->model()) {
		// a list or a table: its lines, the cells of a line separated by " | "
		// (those of the folder a picker shows: the root of the view)
		QStringList lines;
		const auto* model = view->model();
		const QModelIndex root = view->rootIndex();
		const QModelIndex current = view->currentIndex();
		for (int row = 0; row < model->rowCount(root); ++row) {
			QStringList cells;
			for (int column = 0; column < model->columnCount(root); ++column) {
				cells << model->index(row, column, root).data().toString();
			}
			const bool is_current = current.isValid() && current.row() == row && current.parent() == root;
			lines << (is_current ? "> " : "  ") + cells.join(" | ");
		}
		return lines.join('\n');
	}
	if (auto* tabs = qobject_cast<QTabWidget*>(widget)) {
		// its tabs, the temporary ones (a map only looked at) marked with "~"
		QStringList names;
		for (int i = 0; i < tabs->count(); ++i) {
			const bool temporary = tabs->tabBar()->tabData(i).toString() == QLatin1String("temporary");
			names << (temporary ? QStringLiteral("~") : QString()) + tabs->tabText(i);
		}
		return names.join(QStringLiteral(" | "));
	}
	if (auto* text = qobject_cast<QPlainTextEdit*>(widget)) {
		return text->toPlainText();
	}
	if (auto* text = qobject_cast<QTextEdit*>(widget)) {
		return text->toPlainText();
	}
	if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
		return edit->text();
	}
	if (auto* spin = qobject_cast<QAbstractSpinBox*>(widget)) {
		return spin->text();
	}
	if (auto* label = qobject_cast<QLabel*>(widget)) {
		return label->text();
	}
	if (auto* combo = qobject_cast<QComboBox*>(widget)) {
		return combo->currentText();
	}
	if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
		return button->text();
	}
	return widget->windowTitle();
}

int Automation::checkLayout(QWidget* window) {
	int problems = 0;
	const auto widgets = window->findChildren<QWidget*>();
	for (QWidget* widget : widgets) {
		// headers size their sections themselves, tab bars scroll
		if (widget->isWindow() || !widget->isVisibleTo(window)
				|| qobject_cast<QHeaderView*>(widget) || qobject_cast<QTabBar*>(widget)
				|| qobject_cast<QTabBar*>(widget->parentWidget())
				// the clear button Qt puts in a line edit sizes itself, and
				// the field of a spin box or an editable combo box fills
				// what its arrows leave
				|| qobject_cast<QLineEdit*>(widget->parentWidget())
				|| qobject_cast<QAbstractSpinBox*>(widget->parentWidget())
				|| qobject_cast<QComboBox*>(widget->parentWidget())) {
			continue;
		}
		QSize minimum = widget->minimumSizeHint().expandedTo(widget->minimumSize());
		if (widget->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored) {
			minimum.setWidth(0);
		}
		if (widget->sizePolicy().verticalPolicy() == QSizePolicy::Ignored) {
			minimum.setHeight(0);
		}
		// text that wraps (WrapCheckBox...): the height its lines take at
		// the width it has, not at its narrowest
		if (widget->hasHeightForWidth()) {
			minimum.setHeight(widget->heightForWidth(widget->width()));
		} else if (widget->layout() && widget->layout()->hasHeightForWidth()) {
			minimum.setHeight(widget->layout()->totalHeightForWidth(widget->width()));
		}
		// widgets made smaller on purpose (fixed sizes) are fine
		minimum = minimum.boundedTo(widget->maximumSize());
		if (!minimum.isValid()) {
			continue;
		}
		// one pixel of rounding is fine
		if (widget->width() + 1 < minimum.width() || widget->height() + 1 < minimum.height()) {
			problems++;
			qCWarning(lcAutomation).noquote() << QString("squeezed: %1 is %2x%3, needs %4x%5")
				.arg(describe(widget)).arg(widget->width()).arg(widget->height())
				.arg(minimum.width()).arg(minimum.height());
		}
	}
	// Check boxes and radio buttons, with the box they draw, never touch:
	// the box of the native style can lie outside the widget
	const auto options = optionBounds(window);
	for (qsizetype i = 0; i < options.size(); ++i) {
		for (qsizetype j = i + 1; j < options.size(); ++j) {
			const auto gap = optionClearance(options[i], options[j]);
			if (gap && *gap < 1) {
				problems++;
				qCWarning(lcAutomation).noquote() << QString("overlapping options: %1 and %2 (%3px apart)")
					.arg(describe(options[i].first), describe(options[j].first)).arg(*gap);
			}
		}
	}
	return problems;
}
