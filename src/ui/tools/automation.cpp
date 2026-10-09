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
#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QLineEdit>
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
		if (args[0] != "wait" && !run(args)) {
			fail("failed: " + line);
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
		if (!scene || !scene->findEvent(b)) {
			fail(QString("no event %1 on map %2").arg(b).arg(a));
			return false;
		}
		scene->editEvent(b);
		return true;
	}
	if (cmd == "action" && (args.size() == 2 || args.size() == 3)) {
		auto* action = m_window.findChild<QAction*>(args[1]);
		if (!action) {
			fail("no action " + args[1]);
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
		m_failures += problems;
		qCInfo(lcAutomation).noquote() << QString("layout of %1: %2 problem(s)").arg(describe(window)).arg(problems);
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
	if (cmd == "click" && args.size() == 2) {
		QWidget* window = Screenshot::activeWindow();
		QAbstractButton* found = nullptr;
		for (auto* button : window ? window->findChildren<QAbstractButton*>() : QList<QAbstractButton*>()) {
			QString text = button->text().remove('&');
			if (button->isVisibleTo(window) && (button->objectName() == args[1] || text == args[1] || text == args[1] + "...")) {
				found = button;
				break;
			}
		}
		if (!found) {
			fail("no button " + args[1]);
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
				|| qobject_cast<QHeaderView*>(widget) || qobject_cast<QTabBar*>(widget)) {
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
