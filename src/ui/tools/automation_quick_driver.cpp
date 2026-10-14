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

#include "automation_quick_driver.h"
#include "automation_quick_map.h"
#include "common/logging.h"
#include "qmlbinding/qml_host.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLineF>
#include <QMetaMethod>
#include <QPainter>
#include <QQuickItem>
#include <QPainter>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QtMath>
#include <QAbstractItemModel>
#include <algorithm>
#include <optional>
#include <qpa/qwindowsysteminterface.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qhighdpiscaling_p.h>
#include <QtGui/private/qshortcutmap_p.h>
#include <QtQuick/private/qquickwindow_p.h>

namespace {

bool toInt(const QString& text, int& value) {
	bool ok = false;
	value = text.toInt(&ok);
	return ok;
}

/** Every item of a tree, the hidden ones too */
void collect(QQuickItem* item, QList<QQuickItem*>& items) {
	if (!item) {
		return;
	}
	items.append(item);
	for (QQuickItem* child : item->childItems()) {
		collect(child, items);
	}
}

/** The pop-ups shown in windows of their own (Popup.Window: menus, the codes of a message...) over a window, the last opened first */
QList<QQuickWindow*> popupWindows(const QQuickWindow* window) {
	QList<QQuickWindow*> popups;
	for (QWindow* top : QGuiApplication::topLevelWindows()) {
		auto* quick = qobject_cast<QQuickWindow*>(top);
		if (!quick || quick == window || !quick->isVisible() || quick->type() != Qt::Popup) {
			continue;
		}
		// over this window (a submenu: over its menu)
		const QWindow* parent = quick->transientParent();
		while (parent && parent != window && parent->type() == Qt::Popup) {
			parent = parent->transientParent();
		}
		if (!parent || parent == window) {
			popups.prepend(quick);
		}
	}
	return popups;
}

/** The items of a window and of the pop-up windows shown over it */
QList<QQuickItem*> allItems(QQuickWindow* window) {
	QList<QQuickItem*> items;
	collect(window->contentItem(), items);
	for (QQuickWindow* popup : popupWindows(window)) {
		collect(popup->contentItem(), items);
	}
	return items;
}

/** The items of a window and of its pop-up windows shown, those of the pop-up windows first */
QList<QQuickItem*> itemsWithPopups(QQuickWindow* window) {
	QList<QQuickItem*> items;
	for (QQuickWindow* popup : popupWindows(window)) {
		collect(popup->contentItem(), items);
	}
	collect(window->contentItem(), items);
	return items;
}

/** What a window shows, with its pop-up windows over it (past its edges: the picture is larger) */
QImage grabWithPopups(QQuickWindow* window) {
	QImage image = window->grabWindow();
	const QList<QQuickWindow*> popups = popupWindows(window);
	if (popups.isEmpty() || window->width() <= 0) {
		return image;
	}
	// in pixels of the picture, from the top left corner of the window
	const qreal ratio = qreal(image.width()) / window->width();
	const QPoint origin = window->mapToGlobal(QPoint(0, 0));
	QRect bounds(QPoint(0, 0), image.size());
	QList<std::pair<QImage, QPoint>> grabs;
	// the first opened under the others
	for (auto it = popups.crbegin(); it != popups.crend(); ++it) {
		QQuickWindow* popup = *it;
		const QPoint at = (popup->mapToGlobal(QPoint(0, 0)) - origin) * ratio;
		const QImage grab = popup->grabWindow();
		grabs.append({grab, at});
		bounds = bounds.united(QRect(at, grab.size()));
	}
	QImage all(bounds.size(), QImage::Format_ARGB32_Premultiplied);
	all.fill(Qt::transparent);
	QPainter painter(&all);
	painter.drawImage(-bounds.topLeft(), image);
	for (const auto& [grab, at] : grabs) {
		painter.drawImage(at - bounds.topLeft(), grab);
	}
	painter.end();
	all.setDevicePixelRatio(image.devicePixelRatio());
	return all;
}

/** The items of a window, those of the pop-ups shown (a window of their own: a menu...; a dialog drawn in the window) first */
QList<QQuickItem*> searchOrder(QQuickWindow* window) {
	QList<QQuickItem*> popups;
	for (QQuickWindow* popup : popupWindows(window)) {
		popups += allItems(popup);
	}
	QList<QQuickItem*> others;
	for (QQuickItem* item : allItems(window)) {
		bool in_popup = item->window() != window;
		for (const QQuickItem* parent = item; parent && !in_popup; parent = parent->parentItem()) {
			in_popup = parent->inherits("QQuickOverlay");
		}
		(in_popup ? popups : others).append(item);
	}
	return popups + others;
}

/** A view of rows (ListView, GridView...) */
bool isItemView(const QQuickItem* item) {
	return item->inherits("QQuickItemView");
}

/** The row of a model showing exactly text, else the first one containing it (-1: none) */
int findRow(const QAbstractItemModel* model, const QString& text) {
	for (const auto flags : {Qt::MatchExactly, Qt::MatchContains}) {
		const QModelIndexList found = model->match(model->index(0, 0), Qt::DisplayRole, text, 1, flags);
		if (!found.isEmpty()) {
			return found.first().row();
		}
	}
	return -1;
}

bool isButton(const QQuickItem* item) {
	return item->inherits("QQuickAbstractButton");
}

bool isOption(const QQuickItem* item) {
	return item->inherits("QQuickCheckBox") || item->inherits("QQuickRadioButton") || item->inherits("QQuickSwitch");
}

/** A control of Qt Quick Controls, not one of their inner parts the style makes */
bool isControl(const QQuickItem* item) {
	// the text fields are text items, not QQuickControls; those of a combo
	// box or a spin box are their parts
	if (item->inherits("QQuickTextField") || item->inherits("QQuickTextArea")) {
		const QQuickItem* parent = item->parentItem();
		return !parent || (!parent->inherits("QQuickComboBox") && !parent->inherits("QQuickSpinBox"));
	}
	return item->inherits("QQuickControl") && !item->inherits("QQuickScrollBar")
		&& !item->inherits("QQuickScrollIndicator") && !item->inherits("QQuickToolTip");
}

/** The text of a button as written: "&OK" is "OK" */
QString buttonText(const QQuickItem* item) {
	return item->property("text").toString().remove(u'&');
}

/** A name for messages: its name, else the closest named item around it */
QString describe(const QQuickItem* item) {
	QString name = item->objectName();
	for (const QQuickItem* parent = item->parentItem(); name.isEmpty() && parent; parent = parent->parentItem()) {
		if (!parent->objectName().isEmpty()) {
			name = parent->objectName() + QStringLiteral("/…");
		}
	}
	if (name.isEmpty() && isButton(item)) {
		name = QStringLiteral("\"%1\"").arg(buttonText(item));
	}
	QString type = QString::fromLatin1(item->metaObject()->className());
	// "CheckBox_QMLTYPE_12": the type of the file
	type = type.section(QStringLiteral("_QML"), 0, 0);
	return QStringLiteral("%1 (%2)").arg(name, type);
}

QRectF sceneRect(const QQuickItem* item) {
	return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
}

/** In something that scrolls: what it shows is not bounded by the window */
bool scrolls(const QQuickItem* item) {
	for (const QQuickItem* parent = item->parentItem(); parent; parent = parent->parentItem()) {
		if (parent->inherits("QQuickFlickable")) {
			return true;
		}
	}
	return false;
}

/** Where a Text item draws its text, in the scene */
QRectF textRect(const QQuickItem* text) {
	const qreal width = text->property("contentWidth").toReal();
	const qreal height = text->property("contentHeight").toReal();
	const int horizontal = text->property("effectiveHorizontalAlignment").toInt();
	const int vertical = text->property("verticalAlignment").toInt();
	qreal x = 0;
	if (horizontal & Qt::AlignRight) {
		x = text->width() - width;
	} else if (horizontal & Qt::AlignHCenter) {
		x = (text->width() - width) / 2;
	}
	qreal y = 0;
	if (vertical & Qt::AlignBottom) {
		y = text->height() - height;
	} else if (vertical & Qt::AlignVCenter) {
		y = (text->height() - height) / 2;
	}
	return text->mapRectToScene(QRectF(x, y, width, height));
}

/** The first Text item in an item (itself included) */
QQuickItem* findText(QQuickItem* item) {
	if (!item) {
		return nullptr;
	}
	if (item->inherits("QQuickText")) {
		return item;
	}
	for (QQuickItem* child : item->childItems()) {
		if (QQuickItem* text = findText(child)) {
			return text;
		}
	}
	return nullptr;
}

/** What an option draws: its box and its text, without its padding */
QRectF optionRect(QQuickItem* option) {
	QRectF rect;
	if (auto* indicator = option->property("indicator").value<QQuickItem*>(); indicator && indicator->isVisible()) {
		rect = sceneRect(indicator);
	}
	auto* content = option->property("contentItem").value<QQuickItem*>();
	if (QQuickItem* text = findText(content); text && !option->property("text").toString().isEmpty()) {
		rect = rect.united(textRect(text));
	} else if (content) {
		rect = rect.united(sceneRect(content));
	}
	return rect.isNull() ? sceneRect(option) : rect;
}

QString rectText(const QRectF& rect) {
	return QStringLiteral("%1,%2 %3x%4").arg(qRound(rect.x())).arg(qRound(rect.y())).arg(qRound(rect.width())).arg(qRound(rect.height()));
}

/** The options of a window with what they draw, in the scene */
QList<std::pair<QQuickItem*, QRectF>> optionBounds(QQuickWindow* window) {
	QList<std::pair<QQuickItem*, QRectF>> options;
	for (QQuickItem* item : allItems(window)) {
		if (isOption(item) && item->isVisible()) {
			options.append({item, optionRect(item)});
		}
	}
	return options;
}

/** The space between two options of one column (negative: they overlap); none when side by side */
std::optional<qreal> optionClearance(const std::pair<QQuickItem*, QRectF>& first, const std::pair<QQuickItem*, QRectF>& second) {
	const auto& [a, bounds_a] = first;
	const auto& [b, bounds_b] = second;
	if (a->parentItem() != b->parentItem() || bounds_a.left() >= bounds_b.right() || bounds_b.left() >= bounds_a.right()) {
		return std::nullopt;
	}
	return std::max(bounds_a.top(), bounds_b.top()) - std::min(bounds_a.bottom(), bounds_b.bottom());
}

/** The lines of a list model: the cells of a line separated by " | ", the current one marked with ">" */
QString modelText(const QAbstractItemModel* model, int current, const QString& role) {
	int text_role = Qt::DisplayRole;
	if (!role.isEmpty()) {
		text_role = model->roleNames().key(role.toUtf8(), Qt::DisplayRole);
	}
	QStringList lines;
	for (int row = 0; row < model->rowCount(); ++row) {
		QStringList cells;
		for (int column = 0; column < model->columnCount(); ++column) {
			cells << model->index(row, column).data(text_role).toString();
		}
		lines << (row == current ? QStringLiteral("> ") : QStringLiteral("  ")) + cells.join(QStringLiteral(" | "));
	}
	return lines.join(u'\n');
}

bool invoke(QObject* object, const char* method) {
	return QMetaObject::invokeMethod(object, method);
}

/** The middle of a cell of a map or tile view (MapCellView.cellPoint), in the scene, scrolled into view */
std::optional<QPointF> cellPoint(QQuickItem* view, int x, int y) {
	QVariant point;
	if (x < 0 || y < 0 || x >= view->property("columns").toInt() || y >= view->property("rows").toInt()
			|| !QMetaObject::invokeMethod(view, "cellPoint", Q_RETURN_ARG(QVariant, point), Q_ARG(QVariant, x), Q_ARG(QVariant, y))) {
		return std::nullopt;
	}
	return view->mapToScene(point.toPointF());
}

} // namespace

AutomationQuickDriver::AutomationQuickDriver(Fail fail) : m_fail(std::move(fail)) {}

QQuickWindow* AutomationQuickDriver::frontWindow() {
	const auto windows = QmlHost::windows();
	QWindow* modal = QGuiApplication::modalWindow();
	// a QML window over the modal one (its guide) is usable while it is
	// modal: the user works in it
	for (auto it = windows.crbegin(); modal && it != windows.crend(); ++it) {
		for (QWindow* parent = *it; parent; parent = parent->transientParent()) {
			if (parent == modal) {
				return *it;
			}
		}
	}
	// a modal window
	if (modal) {
		return qobject_cast<QQuickWindow*>(modal);
	}
	return windows.isEmpty() ? nullptr : windows.last();
}

QQuickItem* AutomationQuickDriver::findItem(QQuickWindow* window, const QString& name) const {
	const auto items = searchOrder(window);
	// the one shown, when several have the name
	QQuickItem* hidden = nullptr;
	for (QQuickItem* item : items) {
		if (item->objectName() == name) {
			if (item->isVisible()) {
				return item;
			}
			hidden = hidden ? hidden : item;
		}
	}
	if (hidden) {
		return hidden;
	}
	// a button by its text
	for (QQuickItem* item : items) {
		if (isButton(item)) {
			const QString text = buttonText(item);
			if (text == name || text == name + QStringLiteral("...")) {
				if (item->isVisible()) {
					return item;
				}
				hidden = hidden ? hidden : item;
			}
		}
	}
	return hidden;
}

QObject* AutomationQuickDriver::findObject(QQuickWindow* window, const QString& name) const {
	if (QObject* object = window->findChild<QObject*>(name)) {
		return object;
	}
	return window->contentItem()->findChild<QObject*>(name);
}

void AutomationQuickDriver::scrollTo(QQuickItem* item, const QRectF& rect) {
	for (QQuickItem* parent = item->parentItem(); parent; parent = parent->parentItem()) {
		if (parent->inherits("QQuickFlickable")) {
			auto* content = parent->property("contentItem").value<QQuickItem*>();
			if (content) {
				const QRectF shown = item->mapRectToItem(content, rect);
				const qreal y = parent->property("contentY").toReal();
				if (shown.top() < y || shown.bottom() > y + parent->height()) {
					parent->setProperty("contentY", std::max(0.0, shown.center().y() - parent->height() / 2));
				}
			}
			break;
		}
	}
}

void AutomationQuickDriver::click(QQuickWindow* window, QQuickItem* item, bool double_click) {
	// scrolled into view first, as a user would
	scrollTo(item, QRectF(0, 0, item->width(), item->height()));
	// in the window of the item: a pop-up or a menu may have one of its own
	if (item->window()) {
		window = item->window();
	}
	// in the pixels of the window system (QT_SCALE_FACTOR scales them)
	const QPointF scene = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
	const QPointF local = QHighDpi::toNativeLocalPosition(scene, window);
	const QPointF global = QHighDpi::toNativeGlobalPosition(window->mapToGlobal(scene), window);
	// as the window system sends it: queued, a button opening a modal
	// window does not block the script
	const int presses = double_click ? 2 : 1;
	for (int i = 0; i < presses; ++i) {
		QWindowSystemInterface::handleMouseEvent(window, m_timestamp += 10, local, global,
			Qt::LeftButton, Qt::LeftButton, QEvent::MouseButtonPress);
		QWindowSystemInterface::handleMouseEvent(window, m_timestamp += 10, local, global,
			Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease);
	}
}

void AutomationQuickDriver::sendKey(QQuickWindow* window, int key, Qt::KeyboardModifiers modifiers, const QString& text) {
	QWindowSystemInterface::handleKeyEvent(window, QEvent::KeyPress, key, modifiers, text);
	QWindowSystemInterface::handleKeyEvent(window, QEvent::KeyRelease, key, modifiers, text);
}

QString AutomationQuickDriver::itemText(QQuickItem* item) {
	// an item drawing what it shows itself (Ez.ArrayTable) says it
	if (item->metaObject()->indexOfMethod("automationText()") >= 0) {
		QVariant text;
		QMetaObject::invokeMethod(item, "automationText", Q_RETURN_ARG(QVariant, text));
		return text.toString();
	}
	if (item->inherits("QQuickComboBox")) {
		return item->property("displayText").toString();
	}
	if (item->inherits("QQuickSpinBox")) {
		const QVariant shown = item->property("displayText");
		return shown.isValid() ? shown.toString() : item->property("value").toString();
	}
	if (item->inherits("QQuickProgressBar")) {
		// "3/10", "busy" while the end is not known
		if (item->property("indeterminate").toBool()) {
			return QStringLiteral("busy");
		}
		return QStringLiteral("%1/%2").arg(item->property("value").toDouble()).arg(item->property("to").toDouble());
	}
	if (item->inherits("QQuickTabBar")) {
		QStringList names;
		const int count = item->property("count").toInt();
		for (int i = 0; i < count; ++i) {
			QQuickItem* tab = nullptr;
			QMetaObject::invokeMethod(item, "itemAt", Q_RETURN_ARG(QQuickItem*, tab), Q_ARG(int, i));
			names << (tab ? buttonText(tab) : QString());
		}
		return names.join(QStringLiteral(" | "));
	}
	if (item->inherits("QQuickDialogButtonBox")) {
		// its buttons from left to right, as the platform orders them
		QList<QQuickItem*> buttons;
		const int count = item->property("count").toInt();
		for (int i = 0; i < count; ++i) {
			QQuickItem* button = nullptr;
			QMetaObject::invokeMethod(item, "itemAt", Q_RETURN_ARG(QQuickItem*, button), Q_ARG(int, i));
			if (button && button->isVisible()) {
				buttons.append(button);
			}
		}
		std::ranges::sort(buttons, {}, [](const QQuickItem* button) { return button->x(); });
		QStringList names;
		for (const QQuickItem* button : buttons) {
			names << buttonText(button);
		}
		return names.join(QStringLiteral(" | "));
	}
	if (auto* model = item->property("model").value<QAbstractItemModel*>()) {
		return modelText(model, item->property("currentIndex").isValid() ? item->property("currentIndex").toInt() : -1,
			item->property("textRole").toString());
	}
	const QVariant text = item->property("text");
	if (text.isValid()) {
		return text.toString();
	}
	return item->property("title").toString();
}

bool AutomationQuickDriver::setControl(QQuickItem* item, const QString& value) {
	int number = 0;
	if (item->inherits("QQuickComboBox") && toInt(value, number)) {
		item->setProperty("currentIndex", number);
		// as chosen by the user
		QMetaObject::invokeMethod(item, "activated", Q_ARG(int, number));
		return true;
	}
	if (item->inherits("QQuickSpinBox") && toInt(value, number)) {
		item->setProperty("value", number);
		invoke(item, "valueModified");
		return true;
	}
	if (item->inherits("QQuickSlider")) {
		item->setProperty("value", value.toDouble());
		invoke(item, "moved");
		return true;
	}
	if (isButton(item) && item->property("checkable").toBool()) {
		const bool on = value == QLatin1String("on");
		if (item->property("checked").toBool() != on) {
			// as clicked: toggled, then clicked
			invoke(item, "toggle");
			invoke(item, "toggled");
			invoke(item, "clicked");
		}
		return true;
	}
	if (item->inherits("QQuickTextField")) {
		// as typed: fields that save on textEdited see it
		item->forceActiveFocus(Qt::OtherFocusReason);
		item->setProperty("text", value);
		invoke(item, "textEdited");
		invoke(item, "editingFinished");
		return true;
	}
	if (item->inherits("QQuickTextArea") || item->inherits("QQuickTextEdit")) {
		item->forceActiveFocus(Qt::OtherFocusReason);
		item->setProperty("text", value);
		return true;
	}
	if (item->inherits("QQuickTabBar") && toInt(value, number)) {
		item->setProperty("currentIndex", number);
		return true;
	}
	return false;
}

bool AutomationQuickDriver::run(QQuickWindow* window, const QStringList& args, bool* handled) {
	const QString& cmd = args[0];
	int a = 0, b = 0;
	*handled = true;

	// laid out as the next frame will be: a window just shown has not
	// placed its items yet, a click would miss them
	QQuickWindowPrivate::get(window)->polishItems();

	// the map canvas and its palette, when the window has them
	if (const std::optional<bool> done = AutomationQuickMap::run(window, args, m_fail, m_timestamp)) {
		return *done;
	}
	if (cmd == "screenshot" && args.size() <= 2) {
		QString file = args.value(1);
		if (file.isEmpty()) {
			QDir dir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
			dir.mkpath(QStringLiteral("EasyRPG Editor"));
			dir.cd(QStringLiteral("EasyRPG Editor"));
			file = dir.filePath(QStringLiteral("editor-%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))));
		} else {
			QDir().mkpath(QFileInfo(file).absolutePath());
		}
		// at the size it has (its resize may still be queued)
		QWindowSystemInterface::flushWindowSystemEvents();
		if (!grabWithPopups(window).save(file)) {
			m_fail("cannot write " + file);
			return false;
		}
		qCInfo(lcAutomation) << "screenshot saved:" << file;
		return true;
	}
	if (cmd == "screenshot-menu" && args.size() == 2) {
		// a menu or pop-up shown: drawn in the window, or a window of its own
		QDir().mkpath(QFileInfo(args[1]).absolutePath());
		for (QQuickItem* item : allItems(window)) {
			if (item->inherits("QQuickPopupItem") && item->isVisible() && item->width() > 0) {
				// its window: this one, or a window of its own
				QQuickWindow* in = item->window();
				const QImage image = in->grabWindow();
				// the pixels of the picture for a point of the window
				const qreal ratio = in->width() > 0 ? qreal(image.width()) / in->width() : 1;
				const QRectF rect = sceneRect(item);
				const QRect pixels(qFloor(rect.x() * ratio), qFloor(rect.y() * ratio), qCeil(rect.width() * ratio), qCeil(rect.height() * ratio));
				if (!image.copy(pixels).save(args[1])) {
					m_fail("cannot write " + args[1]);
					return false;
				}
				return true;
			}
		}
		for (QWindow* popup : QGuiApplication::topLevelWindows()) {
			auto* quick = qobject_cast<QQuickWindow*>(popup);
			if (quick && quick != window && quick->isVisible() && quick->type() == Qt::Popup) {
				if (!quick->grabWindow().save(args[1])) {
					m_fail("cannot write " + args[1]);
					return false;
				}
				return true;
			}
		}
		m_fail("no menu shown");
		return false;
	}
	if (cmd == "expect-menu-at" && args.size() == 4 && toInt(args[2], a) && toInt(args[3], b)) {
		// the menu shown opens at the point of the item (where it was
		// clicked), its top-left corner there; flipped over it where the
		// usable screen ends (the Dock, the menu bar), as the menus of the system
		QQuickItem* item = findItem(window, args[1]);
		if (!item) {
			m_fail("no item " + args[1]);
			return false;
		}
		const QPointF point = window->mapToGlobal(item->mapToScene(QPointF(a, b)));
		QQuickItem* menu = nullptr;
		for (QQuickItem* popup : allItems(window)) {
			if (!menu && popup->inherits("QQuickPopupItem") && popup->isVisible() && popup->width() > 0) {
				menu = popup;
			}
		}
		if (!menu) {
			m_fail("no menu shown");
			return false;
		}
		const QRectF shown(menu->window()->mapToGlobal(menu->mapToScene(QPointF(0, 0))), menu->size());
		const QRect screen = window->screen() ? window->screen()->availableGeometry() : QRect();
		QPointF expected = point;
		if (!screen.isEmpty() && point.x() + shown.width() > screen.right() + 1) {
			expected.rx() -= shown.width();
		}
		if (!screen.isEmpty() && point.y() + shown.height() > screen.bottom() + 1) {
			expected.ry() -= shown.height();
		}
		const QPointF offset = shown.topLeft() - expected;
		qCInfo(lcAutomation).noquote() << QStringLiteral("menu %1x%2 at %3,%4 of %5%6 (offset %7,%8)")
			.arg(shown.width()).arg(shown.height())
			.arg(shown.x() - point.x() + a).arg(shown.y() - point.y() + b).arg(args[1])
			.arg(expected != point ? QStringLiteral(", flipped at the edge of the screen (%1x%2)").arg(screen.width()).arg(screen.height()) : QString())
			.arg(offset.x()).arg(offset.y());
		if (std::abs(offset.x()) > 1 || std::abs(offset.y()) > 1) {
			m_fail(QStringLiteral("the menu is not at %1,%2 of %3: offset %4,%5").arg(a).arg(b).arg(args[1]).arg(offset.x()).arg(offset.y()));
			return false;
		}
		return true;
	}
	if (cmd == "resize" && args.size() == 3 && toInt(args[1], a) && toInt(args[2], b)) {
		// no smaller than the window allows, as the window systems do
		window->resize(QSize(a, b).expandedTo(window->minimumSize()));
		return true;
	}
	if (cmd == "close" && args.size() == 1) {
		// a dialog window says no (DialogWindow.reject()), others just close
		if (window->metaObject()->indexOfMethod("reject()") >= 0) {
			return invoke(window, "reject");
		}
		window->close();
		return true;
	}
	if (cmd == "check-layout" && args.size() == 1) {
		// the window has its size (its resize may still be queued), the layouts follow when it is drawn
		QWindowSystemInterface::flushWindowSystemEvents();
		window->grabWindow();
		const int problems = checkLayout(window);
		qCInfo(lcAutomation).noquote() << QStringLiteral("layout of %1 (%2x%3, minimum %4x%5): %6 problem(s)")
			.arg(window->objectName()).arg(window->width()).arg(window->height())
			.arg(window->minimumWidth()).arg(window->minimumHeight()).arg(problems);
		return true;
	}
	if (cmd == "check-disabled" && args.size() == 1) {
		const int problems = checkDisabled(window);
		qCInfo(lcAutomation).noquote() << QStringLiteral("disabled controls of %1: %2 problem(s)")
			.arg(window->objectName()).arg(problems);
		return true;
	}
	if (cmd == "check-option-spacing" && args.size() == 2 && toInt(args[1], a) && a >= 0) {
		const auto options = optionBounds(window);
		for (qsizetype i = 0; i < options.size(); ++i) {
			for (qsizetype j = i + 1; j < options.size(); ++j) {
				const auto gap = optionClearance(options[i], options[j]);
				if (gap && *gap < a) {
					m_fail(QStringLiteral("options %1 and %2 have %3px clearance, require %4px (%5 and %6)")
						.arg(describe(options[i].first), describe(options[j].first)).arg(*gap).arg(a)
						.arg(rectText(options[i].second), rectText(options[j].second)));
				}
			}
		}
		return true;
	}
	if (cmd == "action" && args.size() == 2) {
		// an action of the window (Controls.Action), else one of the main window's
		// one of an item shown (the list of the page shown), else any
		QObject* action = nullptr;
		QObject* hidden = nullptr;
		for (QQuickItem* item : searchOrder(window)) {
			for (QObject* child : item->findChildren<QObject*>(args[1], Qt::FindDirectChildrenOnly)) {
				if (child->inherits("QQuickAction")) {
					(item->isVisible() ? action : hidden) = child;
				}
			}
			if (action) {
				break;
			}
		}
		action = action ? action : hidden ? hidden : findObject(window, args[1]);
		if (!action || !action->inherits("QQuickAction")) {
			*handled = false;
			return false;
		}
		if (!action->property("enabled").toBool()) {
			m_fail("the action " + args[1] + " is disabled");
			return false;
		}
		return QMetaObject::invokeMethod(action, "trigger", Qt::QueuedConnection);
	}
	if (cmd == "inspect-controls" && args.size() == 2) {
		QJsonArray controls;
		for (QQuickItem* item : allItems(window)) {
			const bool section = item->property("body").value<QQuickItem*>() && item->property("title").isValid();
			if (!isControl(item) && !section) {
				continue;
			}
			QJsonObject control{{"name", item->objectName()}, {"enabled", item->isEnabled()}, {"visible", item->isVisible()}};
			if (item->inherits("QQuickSpinBox")) {
				control.insert("type", "spin");
				control.insert("value", item->property("value").toInt());
				control.insert("minimum", item->property("from").toInt());
				control.insert("maximum", item->property("to").toInt());
			} else if (item->inherits("QQuickComboBox")) {
				control.insert("type", "combo");
				control.insert("value", item->property("currentIndex").toInt());
				QJsonArray texts;
				const int count = item->property("count").toInt();
				for (int i = 0; i < count; ++i) {
					QString text;
					QMetaObject::invokeMethod(item, "textAt", Q_RETURN_ARG(QString, text), Q_ARG(int, i));
					texts.append(text);
				}
				control.insert("items", texts);
			} else if (isButton(item) && item->property("checkable").toBool()) {
				control.insert("type", "check");
				control.insert("value", item->property("checked").toBool());
				control.insert("text", buttonText(item));
			} else if (item->inherits("QQuickTextField")) {
				control.insert("type", "text");
				control.insert("value", item->property("text").toString());
			} else if (item->inherits("QQuickTextArea")) {
				control.insert("type", "plain-text");
				control.insert("value", item->property("text").toString());
			} else if (section) {
				control.insert("type", "group");
				control.insert("text", item->property("title").toString());
			} else {
				continue;
			}
			const QRectF rect = sceneRect(item);
			control.insert("x", qRound(rect.x()));
			control.insert("y", qRound(rect.y()));
			control.insert("width", qRound(rect.width()));
			control.insert("height", qRound(rect.height()));
			control.insert("minimumWidth", qRound(item->implicitWidth()));
			controls.append(control);
		}
		QFile file(args[1]);
		return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(controls).toJson()) >= 0;
	}
	if (cmd == "call" && args.size() >= 3) {
		// a function of the window or of an item (by objectName): what a file
		// window gives it, what cannot be clicked (a pixel of an image)
		QObject* target = args[1] == window->objectName() ? static_cast<QObject*>(window) : findItem(window, args[1]);
		if (!target) {
			m_fail("no item " + args[1]);
			return false;
		}
		QMetaMethod method;
		const QByteArray name = args[2].toUtf8();
		for (int i = target->metaObject()->methodCount() - 1; i >= 0 && !method.isValid(); --i) {
			if (target->metaObject()->method(i).name() == name) {
				method = target->metaObject()->method(i);
			}
		}
		if (!method.isValid() || method.parameterCount() > 4) {
			m_fail(QStringLiteral("%1 has no function %2").arg(args[1], args[2]));
			return false;
		}
		// numbers as numbers; more arguments than it takes: the last ones as a list
		QVariantList values;
		for (const QString& arg : args.mid(3)) {
			bool number = false;
			const double value = arg.toDouble(&number);
			values << (number ? QVariant(value) : QVariant(arg));
		}
		const int count = method.parameterCount();
		if (count > 0 && values.size() > count) {
			const QVariantList rest = values.mid(count - 1);
			values = values.mid(0, count - 1);
			values << QVariant(rest);
		}
		while (values.size() < count) {
			values << QVariant();
		}
		// queued: a function that opens a window (a question) does not block the script
		QMetaObject::invokeMethod(target, [target, method, values, call = args.mid(1, 2).join(u'.')] {
			QVariant result;
			const auto arg = [&values](int i) { return Q_ARG(QVariant, values.value(i)); };
			bool done = false;
			switch (values.size()) {
			case 0: done = method.invoke(target, Qt::DirectConnection, Q_RETURN_ARG(QVariant, result)); break;
			case 1: done = method.invoke(target, Qt::DirectConnection, Q_RETURN_ARG(QVariant, result), arg(0)); break;
			case 2: done = method.invoke(target, Qt::DirectConnection, Q_RETURN_ARG(QVariant, result), arg(0), arg(1)); break;
			case 3: done = method.invoke(target, Qt::DirectConnection, Q_RETURN_ARG(QVariant, result), arg(0), arg(1), arg(2)); break;
			default: done = method.invoke(target, Qt::DirectConnection, Q_RETURN_ARG(QVariant, result), arg(0), arg(1), arg(2), arg(3)); break;
			}
			qCInfo(lcAutomation).noquote() << QStringLiteral("%1: %2").arg(call, done ? result.toString() : QStringLiteral("not called"));
		}, Qt::QueuedConnection);
		return true;
	}
	if (cmd == "focus" && args.size() == 2) {
		QQuickItem* item = findItem(window, args[1]);
		if (!item) {
			m_fail("no control " + args[1]);
			return false;
		}
		if (QGuiApplication::focusWindow() != window) {
			QWindowSystemInterface::handleFocusWindowChanged<QWindowSystemInterface::SynchronousDelivery>(window);
		}
		item->forceActiveFocus(Qt::OtherFocusReason);
		return true;
	}
	if (cmd == "key" && args.size() == 2) {
		const QKeySequence sequence = QKeySequence::fromString(args[1]);
		if (sequence.isEmpty()) {
			return false;
		}
		const QKeyCombination combination = sequence[0];
		// a pop-up or menu in a window of its own takes the keys while shown
		if (const QList<QQuickWindow*> popups = popupWindows(window); !popups.isEmpty()) {
			window = popups.first();
		}
		// the window takes the keys: its shortcuts first, as when typed
		if (QGuiApplication::focusWindow() != window) {
			QWindowSystemInterface::handleFocusWindowChanged<QWindowSystemInterface::SynchronousDelivery>(window);
		}
		// unless the focused control takes the key first (a field being typed
		// in takes Escape and Return), as the window system asks it
		QKeyEvent override(QEvent::ShortcutOverride, combination.key(), combination.keyboardModifiers());
		override.ignore();
		QCoreApplication::sendEvent(window, &override);
		QKeyEvent press(QEvent::KeyPress, combination.key(), combination.keyboardModifiers());
		if (!override.isAccepted() && QGuiApplicationPrivate::instance()->shortcutMap.tryShortcut(&press)) {
			return true;
		}
		sendKey(window, combination.key(), combination.keyboardModifiers(), QString());
		return true;
	}
	if (cmd == "type" && args.size() >= 2) {
		// typed in the window in front: active now (a window just shown
		// may not be yet, its focused item has no active focus then)
		if (QGuiApplication::focusWindow() != window) {
			QWindowSystemInterface::handleFocusWindowChanged<QWindowSystemInterface::SynchronousDelivery>(window);
		}
		if (!window->activeFocusItem()) {
			m_fail("nothing has the focus");
			return false;
		}
		for (const QChar c : args.mid(1).join(u' ')) {
			sendKey(window, 0, Qt::NoModifier, QString(c));
		}
		return true;
	}
	if ((cmd == "click" || cmd == "double-click") && args.size() >= 2) {
		const QString name = args.mid(1).join(u' ');
		QQuickItem* item = findItem(window, name);
		if (!item || !item->isVisible()) {
			m_fail("no button " + name);
			return false;
		}
		click(window, item, cmd == "double-click");
		return true;
	}
	if ((cmd == "press" || cmd == "release" || cmd == "hover") && args.size() >= 2) {
		// half a click: a wait between the two, as a person clicks; or the
		// mouse over the control, no button pressed
		const QString name = args.mid(1).join(u' ');
		QQuickItem* item = findItem(window, name);
		if (!item || !item->isVisible()) {
			m_fail("no button " + name);
			return false;
		}
		QQuickWindow* target = item->window() ? item->window() : window;
		const QPointF scene = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
		const QPointF local = QHighDpi::toNativeLocalPosition(scene, target);
		const QPointF global = QHighDpi::toNativeGlobalPosition(target->mapToGlobal(scene), target);
		if (cmd == "hover") {
			QWindowSystemInterface::handleMouseEvent(target, m_timestamp += 10, local, global,
				Qt::NoButton, Qt::NoButton, QEvent::MouseMove);
			return true;
		}
		const bool press = cmd == "press";
		QWindowSystemInterface::handleMouseEvent(target, m_timestamp += 10, local, global,
			press ? Qt::LeftButton : Qt::NoButton, Qt::LeftButton, press ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease);
		return true;
	}
	if ((cmd == "click-item" || cmd == "double-click-item") && args.size() >= 3) {
		// a line of a list (by object name), by its text
		QQuickItem* view = findItem(window, args[1]);
		// a view that finds its lines itself (the map tree)
		if (view && view->metaObject()->indexOfMethod("automationItem(QVariant)") >= 0) {
			QVariant found;
			QMetaObject::invokeMethod(view, "automationItem", Q_RETURN_ARG(QVariant, found), Q_ARG(QVariant, args.mid(2).join(u' ')));
			auto* line = found.value<QQuickItem*>();
			if (!line) {
				m_fail(QStringLiteral("no line \"%1\" in %2").arg(args.mid(2).join(u' '), args[1]));
				return false;
			}
			click(window, line, cmd == "double-click-item");
			return true;
		}
		auto* model = view ? view->property("model").value<QAbstractItemModel*>() : nullptr;
		if (!view || !isItemView(view) || !model) {
			*handled = view != nullptr;
			if (view) {
				m_fail("no list " + args[1]);
			}
			return false;
		}
		const QString text = args.mid(2).join(u' ');
		const int row = findRow(model, text);
		if (row < 0) {
			m_fail(QStringLiteral("no line \"%1\" in %2").arg(text, args[1]));
			return false;
		}
		// scrolled to it first, as a user would
		QMetaObject::invokeMethod(view, "positionViewAtIndex", Q_ARG(int, row), Q_ARG(int, 2 /* ListView.Contain */));
		QQuickItem* line = nullptr;
		QMetaObject::invokeMethod(view, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, line), Q_ARG(int, row));
		if (!line) {
			m_fail(QStringLiteral("the line \"%1\" of %2 is not shown").arg(text, args[1]));
			return false;
		}
		click(window, line, cmd == "double-click-item");
		return true;
	}
	if (((cmd == "preview-click" || cmd == "preview-double-click") && args.size() == 4 && toInt(args[2], a) && toInt(args[3], b))
			|| (cmd == "preview-drag" && args.size() == 6 && toInt(args[2], a) && toInt(args[3], b))) {
		// a cell of a map or tile view (MapCellView: mapPreview, chipSelector)
		QQuickItem* view = findItem(window, args[1]);
		const auto first = view ? cellPoint(view, a, b) : std::nullopt;
		if (!first) {
			m_fail(QStringLiteral("no cell %1 %2 in %3").arg(a).arg(b).arg(args[1]));
			return false;
		}
		// at once: the point is right until the view scrolls for the next cell
		auto send = [&](const QPointF& scene, Qt::MouseButtons buttons, Qt::MouseButton button, QEvent::Type type) {
			const QPointF local = QHighDpi::toNativeLocalPosition(scene, window);
			const QPointF global = QHighDpi::toNativeGlobalPosition(window->mapToGlobal(scene), window);
			QWindowSystemInterface::handleMouseEvent<QWindowSystemInterface::SynchronousDelivery>(window,
				m_timestamp += 10, local, global, buttons, button, type);
		};
		send(*first, Qt::LeftButton, Qt::LeftButton, QEvent::MouseButtonPress);
		QPointF last = *first;
		if (cmd == "preview-drag") {
			int x2 = 0;
			int y2 = 0;
			const auto second = toInt(args[4], x2) && toInt(args[5], y2) ? cellPoint(view, x2, y2) : std::nullopt;
			if (!second) {
				send(last, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease);
				m_fail(QStringLiteral("no cell %1 %2 in %3").arg(args[4], args[5], args[1]));
				return false;
			}
			last = *second;
			send(last, Qt::LeftButton, Qt::NoButton, QEvent::MouseMove);
		}
		send(last, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease);
		if (cmd == "preview-double-click") {
			send(last, Qt::LeftButton, Qt::LeftButton, QEvent::MouseButtonPress);
			send(last, Qt::NoButton, Qt::LeftButton, QEvent::MouseButtonRelease);
		}
		return true;
	}
	if (cmd == "select-command" && args.size() == 2 && toInt(args[1], a)) {
		// the command list shown (CommandList.qml): a line by its index
		QQuickItem* list = findItem(window, QStringLiteral("commands"));
		QQuickItem* owner = list;
		while (owner && owner->metaObject()->indexOfMethod("selectLine(QVariant)") < 0) {
			owner = owner->parentItem();
		}
		if (!owner) {
			*handled = list != nullptr;
			if (list) {
				m_fail("no event command list");
			}
			return false;
		}
		if (QGuiApplication::focusWindow() != window) {
			QWindowSystemInterface::handleFocusWindowChanged<QWindowSystemInterface::SynchronousDelivery>(window);
		}
		QVariant selected;
		QMetaObject::invokeMethod(owner, "selectLine", Q_RETURN_ARG(QVariant, selected), Q_ARG(QVariant, a));
		if (!selected.toBool()) {
			m_fail(QStringLiteral("no event command at index %1").arg(a));
			return false;
		}
		return true;
	}
	if (cmd == "view-click" || cmd == "view-drag") {
		// a point of an item (a tile view, a graphic) in its own coordinates
		const bool drag = cmd == "view-drag";
		const int count = drag ? 4 : 2;
		QQuickItem* item = findItem(window, args.value(1));
		if (!item) {
			m_fail("no item " + args.value(1));
			return false;
		}
		// after the points: the right button, a double-click, the keys held
		// during a drag (shift, ctrl), Escape before the button is released
		bool ok = args.size() >= count + 2;
		bool right = false;
		bool doubleClick = false;
		bool escape = false;
		Qt::KeyboardModifiers modifiers = Qt::NoModifier;
		for (const QString& option : args.mid(count + 2)) {
			if (option == QLatin1String("right")) {
				right = true;
			} else if (!drag && option == QLatin1String("double")) {
				doubleClick = true;
			} else if (drag && option == QLatin1String("shift")) {
				modifiers |= Qt::ShiftModifier;
			} else if (drag && option == QLatin1String("ctrl")) {
				modifiers |= Qt::ControlModifier;
			} else if (drag && option == QLatin1String("escape")) {
				escape = true;
			} else {
				ok = false;
			}
		}
		std::vector<int> v(static_cast<size_t>(count));
		for (int i = 0; ok && i < count; ++i) {
			ok = toInt(args.value(i + 2), v[static_cast<size_t>(i)]);
		}
		if (!ok) {
			m_fail("usage: " + cmd + " NAME X Y" + (drag ? " X2 Y2 [right] [shift] [ctrl] [escape]" : " [right|double]"));
			return false;
		}
		const QPointF from(v[0], v[1]);
		const QPointF to = drag ? QPointF(v[2], v[3]) : from;
		scrollTo(item, QRectF(from, to).normalized().adjusted(-1, -1, 1, 1));
		const Qt::MouseButton button = right ? Qt::RightButton : Qt::LeftButton;
		auto send = [&](const QPointF& point, Qt::MouseButtons buttons, Qt::MouseButton changed, QEvent::Type type) {
			const QPointF scene = item->mapToScene(point);
			const QPointF local = QHighDpi::toNativeLocalPosition(scene, window);
			const QPointF global = QHighDpi::toNativeGlobalPosition(window->mapToGlobal(scene), window);
			// queued: a click opening a modal window does not block the script
			QWindowSystemInterface::handleMouseEvent(window, m_timestamp += 10, local, global, buttons, changed, type, modifiers);
		};
		send(from, Qt::NoButton, Qt::NoButton, QEvent::MouseMove);
		send(from, button, button, QEvent::MouseButtonPress);
		// small steps, as a hand would
		const int steps = drag ? std::max(1, qRound(QLineF(from, to).length() / 4)) : 0;
		for (int i = 1; i <= steps; ++i) {
			send(from + (to - from) * i / steps, button, Qt::NoButton, QEvent::MouseMove);
		}
		if (escape) {
			QWindowSystemInterface::handleKeyEvent(window, QEvent::KeyPress, Qt::Key_Escape, modifiers);
			QWindowSystemInterface::handleKeyEvent(window, QEvent::KeyRelease, Qt::Key_Escape, modifiers);
		}
		send(to, Qt::NoButton, button, QEvent::MouseButtonRelease);
		if (doubleClick) {
			send(to, button, button, QEvent::MouseButtonPress);
			send(to, Qt::NoButton, button, QEvent::MouseButtonRelease);
		}
		return true;
	}
	if (cmd == "view-wheel" && args.size() == 5 && toInt(args[2], a) && toInt(args[3], b)) {
		// the wheel turned over a point of an item (a notch is 120)
		QQuickItem* item = findItem(window, args[1]);
		int delta = 0;
		if (!item || !toInt(args[4], delta)) {
			m_fail("usage: view-wheel NAME X Y DELTA");
			return false;
		}
		const QPointF scene = item->mapToScene(QPointF(a, b));
		const QPointF local = QHighDpi::toNativeLocalPosition(scene, window);
		const QPointF global = QHighDpi::toNativeGlobalPosition(window->mapToGlobal(scene), window);
		QWindowSystemInterface::handleMouseEvent(window, m_timestamp += 10, local, global, Qt::NoButton, Qt::NoButton,
			QEvent::MouseMove, Qt::NoModifier);
		QWindowSystemInterface::handleWheelEvent(window, m_timestamp += 10, local, global, QPoint(), QPoint(0, delta),
			Qt::NoModifier);
		return true;
	}
	if (cmd == "context-menu" && args.size() == 2) {
		// a right click on the item, on its current line for a list
		QQuickItem* item = findItem(window, args[1]);
		if (!item) {
			m_fail("no item " + args[1]);
			return false;
		}
		QQuickItem* target = item;
		if (auto* current = item->property("currentItem").value<QQuickItem*>(); current && isItemView(item)) {
			target = current;
		}
		const QPointF local = target->mapToScene(QPointF(target->width() / 2, target->height() / 2));
		const QPointF global = window->mapToGlobal(local);
		QWindowSystemInterface::handleMouseEvent(window, m_timestamp += 10, local, global,
			Qt::RightButton, Qt::RightButton, QEvent::MouseButtonPress);
		QWindowSystemInterface::handleMouseEvent(window, m_timestamp += 10, local, global,
			Qt::NoButton, Qt::RightButton, QEvent::MouseButtonRelease);
		return true;
	}
	if (cmd == "menu" && args.size() >= 2) {
		// an item of the menu shown, by its text
		const QString text = args.mid(1).join(u' ');
		for (QQuickItem* item : itemsWithPopups(window)) {
			if (item->inherits("QQuickMenuItem") && item->isVisible() && item->isEnabled()
					&& (buttonText(item) == text || buttonText(item) == text + QStringLiteral("...") || item->objectName() == text)) {
				click(window, item);
				return true;
			}
		}
		m_fail("no menu item " + text);
		return false;
	}
	if (cmd == "expect-menu" && args.size() == 2 && (args[1] == QLatin1String("shown") || args[1] == QLatin1String("none"))) {
		// a pop-up menu (one of its items is shown), or a pop-up in a window of its own
		const auto items = itemsWithPopups(window);
		const bool shown = !popupWindows(window).isEmpty() || std::ranges::any_of(items, [](const QQuickItem* item) {
			return item->inherits("QQuickMenuItem") && item->isVisible();
		});
		if (shown != (args[1] == QLatin1String("shown"))) {
			m_fail(shown ? QStringLiteral("a menu is shown") : QStringLiteral("no menu shown"));
			return false;
		}
		return true;
	}
	if (cmd == "set-control" && args.size() >= 3) {
		QQuickItem* item = findItem(window, args[1]);
		if (!item || !setControl(item, args.mid(2).join(u' '))) {
			m_fail("no control " + args[1]);
			return false;
		}
		return true;
	}
	if (cmd == "set-enabled" && args.size() == 3 && (args[2] == QLatin1String("on") || args[2] == QLatin1String("off"))) {
		// a control (or a section, with its controls) greyed, to see how the
		// windows draw what cannot be changed
		QQuickItem* item = findItem(window, args[1]);
		if (!item) {
			m_fail("no control " + args[1]);
			return false;
		}
		item->setEnabled(args[2] == QLatin1String("on"));
		return true;
	}
	if (cmd == "tab" && args.size() == 3 && toInt(args[2], a)) {
		QQuickItem* item = findItem(window, args[1]);
		if (!item || a < 0 || a >= item->property("count").toInt()) {
			m_fail(QStringLiteral("no tab %1 in %2").arg(a).arg(args[1]));
			return false;
		}
		item->setProperty("currentIndex", a);
		return true;
	}
	if (((cmd == "close-tab" || cmd == "click-tab") && args.size() == 3 && toInt(args[2], a))
			|| (cmd == "move-tab" && args.size() == 4 && toInt(args[2], a) && toInt(args[3], b))) {
		// a tab bar that closes and moves its tabs (the map tabs)
		QQuickItem* bar = findItem(window, args[1]);
		if (!bar || !bar->inherits("QQuickTabBar")) {
			*handled = bar != nullptr;
			if (bar) {
				m_fail("no tab bar " + args[1]);
			}
			return false;
		}
		const int count = bar->property("count").toInt();
		if (a < 0 || a >= count || (cmd == "move-tab" && (b < 0 || b >= count))) {
			m_fail("no tab " + args[2] + " in " + args[1]);
			return false;
		}
		if (cmd == "click-tab") {
			QQuickItem* tab = nullptr;
			QMetaObject::invokeMethod(bar, "itemAt", Q_RETURN_ARG(QQuickItem*, tab), Q_ARG(int, a));
			if (!tab) {
				return false;
			}
			click(window, tab);
			return true;
		}
		// queued: a question about its changes must not block the script
		if (cmd == "close-tab") {
			return QMetaObject::invokeMethod(bar, "closeTab", Qt::QueuedConnection, Q_ARG(QVariant, a));
		}
		return QMetaObject::invokeMethod(bar, "moveTab", Q_ARG(QVariant, a), Q_ARG(QVariant, b));
	}
	if (cmd == "expect-tab" && args.size() >= 3) {
		// the tab shown of a tab bar
		QQuickItem* bar = findItem(window, args[1]);
		if (!bar || !bar->inherits("QQuickTabBar")) {
			*handled = bar != nullptr;
			if (bar) {
				m_fail("no tab bar " + args[1]);
			}
			return false;
		}
		const QString expected = args.mid(2).join(u' ');
		QQuickItem* tab = bar->property("currentItem").value<QQuickItem*>();
		const QString shown = tab ? buttonText(tab) : QString();
		if (shown != expected) {
			m_fail(QStringLiteral("%1 shows the tab \"%2\", not \"%3\"").arg(args[1], shown, expected));
			return false;
		}
		return true;
	}
	if (cmd == "expect-option-enabled" && args.size() == 4 && toInt(args[2], a)) {
		// an entry of a combo box (Ez.ComboBox.optionEnabled)
		QQuickItem* item = findItem(window, args[1]);
		QVariant enabled;
		if (!item || !QMetaObject::invokeMethod(item, "optionEnabled", Q_RETURN_ARG(QVariant, enabled), Q_ARG(QVariant, a))) {
			m_fail("no combo box " + args[1]);
			return false;
		}
		if (enabled.toBool() != (args[3] == QLatin1String("on"))) {
			m_fail(QStringLiteral("the option %1 of %2 is not %3").arg(args[2], args[1], args[3] == QLatin1String("on") ? "enabled" : "disabled"));
			return false;
		}
		return true;
	}
	if ((cmd == "expect-checked" || cmd == "expect-enabled") && args.size() >= 3) {
		const QString name = args.mid(1, args.size() - 2).join(u' ');
		const bool on = args.last() == QLatin1String("on");
		QQuickItem* item = findItem(window, name);
		if (!item) {
			m_fail("no control " + name);
			return false;
		}
		const bool checked = cmd == "expect-checked";
		const bool state = checked ? item->property("checked").toBool() : item->isEnabled();
		if (state != on) {
			m_fail(QStringLiteral("%1 is not %2").arg(name, checked ? (on ? "checked" : "unchecked") : (on ? "enabled" : "disabled")));
			return false;
		}
		return true;
	}
	if ((cmd == "expect" || cmd == "expect-not") && args.size() >= 3) {
		// the window itself (its objectName): its title
		const bool title = !window->objectName().isEmpty() && args[1] == window->objectName();
		QQuickItem* item = title ? nullptr : findItem(window, args[1]);
		if (!item && !title) {
			m_fail("no control " + args[1]);
			return false;
		}
		const QString expected = args.mid(2).join(u' ');
		const QString text = item ? itemText(item) : window->title();
		if (cmd == "expect" && !text.contains(expected)) {
			m_fail(QStringLiteral("%1 does not show \"%2\", it shows:\n%3").arg(args[1], expected, text));
			return false;
		}
		if (cmd == "expect-not" && text.contains(expected)) {
			m_fail(QStringLiteral("%1 shows \"%2\"").arg(args[1], expected));
			return false;
		}
		return true;
	}

	// a command of Automation itself (the project, the main window...)
	*handled = false;
	return false;
}

int AutomationQuickDriver::checkDisabled(QQuickWindow* window) {
	// a disabled control is faded (Ez.Metrics.disabledOpacity): Breeze draws
	// it nearly as an enabled one with Kirigami's basic theme
	constexpr qreal faded = 0.7;
	int problems = 0;
	for (QQuickItem* item : allItems(window)) {
		if (!item->isVisible() || item->isEnabled() || item->width() <= 0 || item->height() <= 0) {
			continue;
		}
		QQuickItem* parent = item->parentItem();
		// the text inside a combo box or a spin box is the box's
		const bool inner_field = parent && (parent->inherits("QQuickComboBox") || parent->inherits("QQuickSpinBox"));
		const bool control = isButton(item) || item->inherits("QQuickComboBox") || item->inherits("QQuickSpinBox")
			|| item->inherits("QQuickSlider") || item->inherits("QQuickLabel")
			|| ((item->inherits("QQuickTextField") || item->inherits("QQuickTextArea")) && !inner_field);
		if (!control) {
			continue;
		}
		qreal opacity = 1;
		for (QQuickItem* up = item; up; up = up->parentItem()) {
			opacity *= up->opacity();
		}
		if (opacity > faded) {
			problems++;
			m_fail(QStringLiteral("drawn as enabled: %1 is disabled, at opacity %2").arg(describe(item)).arg(opacity));
		}
	}
	return problems;
}

int AutomationQuickDriver::checkLayout(QQuickWindow* window) {
	int problems = 0;
	const QRectF bounds(0, 0, window->width(), window->height());
	auto report = [&](const QString& message) {
		problems++;
		m_fail(message);
	};
	for (QQuickItem* item : allItems(window)) {
		if (!item->isVisible() || item->width() <= 0 || item->height() <= 0) {
			continue;
		}
		if (isControl(item)) {
			// smaller than it needs: its text or its box is cut (one pixel of
			// rounding is fine); fields scroll their text, they can be
			// narrower, and a scroll view is smaller than what it scrolls
			const bool needs_width = isButton(item) || item->inherits("QQuickComboBox") || item->inherits("QQuickSpinBox");
			const qreal width = needs_width ? item->implicitWidth() : 0;
			const bool scroll_view = item->inherits("QQuickScrollView");
			if (!scroll_view && (item->width() + 1 < width || item->height() + 1 < item->implicitHeight())) {
				report(QStringLiteral("squeezed: %1 is %2x%3, needs %4x%5").arg(describe(item))
					.arg(qRound(item->width())).arg(qRound(item->height()))
					.arg(qRound(width)).arg(qRound(item->implicitHeight())));
			}
			// past the edge of the window (what scrolls is not)
			const QRectF rect = sceneRect(item);
			if (!scrolls(item) && !bounds.adjusted(-1, -1, 1, 1).contains(rect)) {
				report(QStringLiteral("outside the window: %1 at %2, the window is %3x%4").arg(describe(item), rectText(rect))
					.arg(window->width()).arg(window->height()));
			}
		}
		// a text wider or taller than its item, unless elided on purpose
		if (item->inherits("QQuickText") && item->property("elide").toInt() == Qt::ElideNone
				&& !item->property("text").toString().isEmpty()) {
			const bool wraps = item->property("wrapMode").toInt() != 0;
			const qreal content_width = item->property("contentWidth").toReal();
			const qreal content_height = item->property("contentHeight").toReal();
			if ((!wraps && content_width > item->width() + 1) || content_height > item->height() + 1) {
				report(QStringLiteral("cut text: %1 \"%2\" is %3x%4, its text %5x%6").arg(describe(item),
					item->property("text").toString().left(40)).arg(qRound(item->width())).arg(qRound(item->height()))
					.arg(qRound(content_width)).arg(qRound(content_height)));
			}
		}
	}
	// check boxes and radio buttons never touch
	const auto options = optionBounds(window);
	for (qsizetype i = 0; i < options.size(); ++i) {
		for (qsizetype j = i + 1; j < options.size(); ++j) {
			const auto gap = optionClearance(options[i], options[j]);
			if (gap && *gap < 1) {
				report(QStringLiteral("overlapping options: %1 and %2 (%3px apart)")
					.arg(describe(options[i].first), describe(options[j].first)).arg(*gap));
			}
		}
	}
	return problems;
}
