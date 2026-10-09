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
#include "common/logging.h"
#include "ui/main_window.h"
#include "ui/map/map_scene.h"
#include "defines.h"
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
#include <QGroupBox>
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
#include <QProcess>
#include <QTabWidget>
#include <QTimer>
#include <qpa/qwindowsysteminterface.h>
#include <QWindow>
#include <QPaintEvent>
#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QGraphicsView>
#include <array>

namespace {
// Time for layouts and paints to settle between two commands
constexpr int step_delay = 100;

QString describe(const QWidget* widget) {
	QString name = widget->objectName();
	// unnamed widgets (inside custom widgets): name the closest named parent
	const QWidget* parent = widget->parentWidget();
	while (name.isEmpty() && parent) {
		if (!parent->objectName().isEmpty()) {
			name = parent->objectName() + "/…";
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
		QStringList args = QProcess::splitCommand(line);
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
		// the map shown first: each open map has its own menu actions
		MapScene* scene = m_window.currentScene();
		auto* action = scene ? scene->findChild<QAction*>(args[1]) : nullptr;
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
		for (QTreeWidgetItemIterator it(tree); tree && *it; ++it) {
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
		// posted: a key opening a dialog must not block the script
		QApplication::postEvent(target, new QKeyEvent(QEvent::KeyPress, combination.key(), combination.keyboardModifiers()));
		QApplication::postEvent(target, new QKeyEvent(QEvent::KeyRelease, combination.key(), combination.keyboardModifiers()));
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
		QWidget* window = Screenshot::activeWindow();
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
	if (cmd == "click-item" && args.size() >= 3) {
		// a mouse click on the first line of a list showing the text
		auto* tree = qobject_cast<QTreeWidget*>(findWidget(args[1]));
		const QString text = args.mid(2).join(' ');
		QTreeWidgetItem* found = nullptr;
		for (QTreeWidgetItemIterator it(tree); tree && *it && !found; ++it) {
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
		// or a menu action, by object name or text
		QAction* action = nullptr;
		for (auto* candidate : widget ? QList<QAction*>() : m_window.findChildren<QAction*>()) {
			if (candidate->objectName() == name || candidate->text().remove('&') == name) {
				action = candidate;
				break;
			}
		}
		const bool enabled = widget ? widget->isEnabled() : action && action->isEnabled();
		if ((!widget && !action) || enabled != on) {
			fail(QString("%1 is not %2").arg(name, on ? "enabled" : "disabled"));
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
	if (cmd == "expect-not" && args.size() == 3) {
		QWidget* widget = findWidget(args[1]);
		if (!widget) {
			fail("no widget " + args[1]);
			return false;
		}
		if (widgetText(widget).contains(args[2])) {
			fail(QString("%1 shows \"%2\"").arg(args[1], args[2]));
			return false;
		}
		return true;
	}
	if (cmd == "expect" && args.size() == 3) {
		QWidget* widget = findWidget(args[1]);
		if (!widget) {
			fail("no widget " + args[1]);
			return false;
		}
		QString text = widgetText(widget);
		if (!text.contains(args[2])) {
			fail(QString("%1 does not show \"%2\", it shows:\n%3").arg(args[1], args[2], text));
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
			control.insert("width", widget->width());
			control.insert("minimumWidth", widget->minimumSizeHint().width());
			controls.append(control);
		}
		QFile file(args[1]);
		return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(controls).toJson()) >= 0;
	}
	if (cmd == "select-map" && args.size() == 2 && toInt(args[1], a)) {
		// by id: map names can repeat; 0 is the project
		auto* tree = m_window.findChild<QTreeWidget*>(QStringLiteral("treeMap"));
		for (QTreeWidgetItemIterator it(tree); tree && *it; ++it) {
			if ((*it)->data(1, Qt::DisplayRole).toInt() == a) {
				tree->setCurrentItem(*it);
				tree->scrollToItem(*it);
				tree->setFocus(Qt::OtherFocusReason);
				return true;
			}
		}
		fail(QString("no map %1 in the map tree").arg(a));
		return false;
	}
	if ((cmd == "map-click" || cmd == "map-double-click") && (args.size() == 3 || args.size() == 4)
			&& toInt(args[1], a) && toInt(args[2], b)) {
		MapScene* scene = m_window.currentScene();
		if (!scene || (args.size() == 4 && args[3] != "left" && args[3] != "right")) {
			fail("no map shown");
			return false;
		}
		const Qt::MouseButton button = args.value(3) == "right" ? Qt::RightButton : Qt::LeftButton;
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
		auto* area = qobject_cast<QAbstractScrollArea*>(findWidget(args[1]));
		QWidget* viewport = area ? area->viewport() : findWidget(args[1]);
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
			QApplication::processEvents();
			slowest = std::max(slowest, step_timer.elapsed());
			events++;
		};
		const int direction = a > 0 ? -1 : 1;
		send(0, Qt::ScrollBegin, Qt::MouseEventNotSynthesized);
		for (int i = 0; i < std::abs(a); i++) {
			send(direction * 8, Qt::ScrollUpdate, Qt::MouseEventNotSynthesized);
		}
		send(0, Qt::ScrollEnd, Qt::MouseEventNotSynthesized);
		for (int speed = 12; speed > 0; speed--) {
			send(direction * speed, Qt::ScrollMomentum, Qt::MouseEventSynthesizedBySystem);
		}
		qApp->removeEventFilter(&paints);
		const int after = area ? area->verticalScrollBar()->value() : 0;
		qCInfo(lcAutomation).noquote() << QString("wheel %1: %2 events in %3 ms, slowest %4 ms, scrolled %5 px")
			.arg(args[1]).arg(events).arg(timer.elapsed()).arg(slowest).arg(after - before);
		qCInfo(lcAutomation).noquote() << QString("wheel %1: painted %2 widgets per event (%3 pixels; the scrolled area has %4): %5")
			.arg(args[1]).arg(paints.count / std::max(events, 1)).arg(paints.pixels / std::max(events, 1))
			.arg(viewport->width() * viewport->height()).arg(paints.summary());
		return true;
	}
	if (cmd == "close" && args.size() == 1) {
		auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
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
		// a button by its text
		for (auto* button : window->findChildren<QAbstractButton*>()) {
			const QString text = button->text().remove('&');
			if (button->isVisibleTo(window) && (text == name || text == name + "...")) {
				return button;
			}
		}
	}
	return hidden;
}

QString Automation::widgetText(QWidget* widget) {
	if (auto* tree = qobject_cast<QTreeWidget*>(widget)) {
		QStringList lines;
		// the current line is marked with ">"
		for (QTreeWidgetItemIterator it(tree); *it; ++it) {
			lines << ((*it) == tree->currentItem() ? "> " : "  ") + (*it)->text(0);
		}
		return lines.join('\n');
	}
	if (auto* edit = qobject_cast<QLineEdit*>(widget)) {
		return edit->text();
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
				|| qobject_cast<QTabBar*>(widget->parentWidget())) {
			continue;
		}
		QSize minimum = widget->minimumSizeHint().expandedTo(widget->minimumSize());
		if (widget->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored) {
			minimum.setWidth(0);
		}
		if (widget->sizePolicy().verticalPolicy() == QSizePolicy::Ignored) {
			minimum.setHeight(0);
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
	return problems;
}
