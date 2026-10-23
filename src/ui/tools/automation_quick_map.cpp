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

#include "automation_quick_map.h"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMetaEnum>
#include <QPointer>
#include <QPointingDevice>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRect>
#include <QSGRendererInterface>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <qpa/qwindowsysteminterface.h>
#include <QtGui/private/qhighdpiscaling_p.h>
#include "common/logging.h"
#include "model/autotile.h"
#include "model/map_document.h"
#include "ui/map/map_editor.h"

namespace {

using Sync = QWindowSystemInterface::SynchronousDelivery;

bool toInt(const QString& text, int& value)
{
	bool ok = false;
	value = text.toInt(&ok);
	return ok;
}

/** Integers from the arguments, from first on */
bool toInts(const QStringList& args, int first, std::initializer_list<int*> values)
{
	int i = first;
	for (int* value : values) {
		if (i >= args.size() || !toInt(args[i++], *value))
			return false;
	}
	return true;
}

/** An item of the window by name, the one shown first */
QQuickItem* findItem(QQuickItem* item, const QString& name, QQuickItem* hidden = nullptr)
{
	QList<QQuickItem*> items{item};
	while (!items.isEmpty()) {
		QQuickItem* next = items.takeFirst();
		if (next->objectName() == name) {
			if (next->isVisible())
				return next;
			hidden = hidden ? hidden : next;
		}
		items.append(next->childItems());
	}
	return hidden;
}

QString rectText(const QRect& rect)
{
	return rect.isEmpty() ? QStringLiteral("none")
		: QStringLiteral("%1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

/** "X Y W H" or "none" from the arguments */
std::optional<QRect> toRect(const QStringList& args, int first)
{
	if (args.size() == first + 1 && args[first] == QLatin1String("none"))
		return QRect();
	int x = 0, y = 0, w = 0, h = 0;
	if (args.size() != first + 4 || !toInts(args, first, {&x, &y, &w, &h}))
		return std::nullopt;
	return QRect(x, y, w, h);
}

// What the input commands keep between them: where the pointer is in the
// window, the buttons held, the view when they were pressed
struct Pointer
{
	QPointer<QQuickWindow> window;
	QPointF position;
	Qt::MouseButtons buttons;
	QPointF pressScroll;
};

Pointer& pointer()
{
	static Pointer state;
	return state;
}

/**
 * Times the frames of a window while its map view scrolls a step a frame
 * (back at the edges), and logs them once it has enough
 */
class FrameProbe : public QObject
{
public:
	FrameProbe(QQuickWindow* window, QQuickItem* view, int frames, const QPointF& step, const QString& name) :
		QObject(window), m_window(window), m_view(view), m_frames(frames), m_step(step), m_name(name)
	{
		// on the thread of the scene graph: what a frame costs it
		connect(window, &QQuickWindow::beforeSynchronizing, this, [this]() { m_render.start(); }, Qt::DirectConnection);
		connect(window, &QQuickWindow::frameSwapped, this, [this]() {
			if (m_render.isValid())
				m_renderTimes.append(m_render.nsecsElapsed() / 1e6);
		}, Qt::DirectConnection);
		connect(window, &QQuickWindow::frameSwapped, this, &FrameProbe::nextFrame, Qt::QueuedConnection);
		m_interval.start();
		m_window->update();
	}

private:
	void nextFrame()
	{
		if (!m_view || m_done)
			return;
		m_intervals.append(m_interval.nsecsElapsed() / 1e6);
		m_interval.restart();
		if (m_intervals.size() >= m_frames) {
			m_done = true;
			report();
			deleteLater();
			return;
		}
		// a step, back at the ends
		for (const auto& [axis, size, view_size, step] : {
				std::tuple{"contentX", "contentWidth", "width", &m_step.rx()},
				std::tuple{"contentY", "contentHeight", "height", &m_step.ry()}}) {
			if (*step == 0)
				continue;
			const qreal end = m_view->property(size).toReal() - m_view->property(view_size).toReal();
			qreal next = m_view->property(axis).toReal() + *step;
			if (next < 0 || next > end) {
				*step = -*step;
				next = std::clamp(next, 0.0, std::max(0.0, end));
			}
			m_view->setProperty(axis, next);
		}
		m_window->update();
	}

	void report() const
	{
		auto stats = [](QList<double> times) {
			if (times.isEmpty())
				return QStringLiteral("none");
			std::ranges::sort(times);
			const double mean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
			const double p95 = times[std::min<qsizetype>(times.size() - 1, times.size() * 95 / 100)];
			return QStringLiteral("mean %1 ms, median %2 ms, 95% %3 ms, max %4 ms")
				.arg(mean, 0, 'f', 2).arg(times[times.size() / 2], 0, 'f', 2).arg(p95, 0, 'f', 2).arg(times.last(), 0, 'f', 2);
		};
		// the first is the scene graph made for the view
		const QList<double> intervals = m_intervals.mid(1);
		const QString api = m_window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software
			? QStringLiteral("software") : QStringLiteral("rhi %1").arg(m_window->rendererInterface()->graphicsApi());
		qCInfo(lcAutomation).noquote() << QStringLiteral("map frames (%1, %2, pixel ratio %3): %4 frames, between them: %5")
			.arg(m_name, api).arg(m_window->effectiveDevicePixelRatio()).arg(intervals.size()).arg(stats(intervals));
		qCInfo(lcAutomation).noquote() << QStringLiteral("map frames (%1): synchronizing and rendering: %2").arg(m_name, stats(m_renderTimes.mid(1)));
	}

	QPointer<QQuickWindow> m_window;
	QPointer<QQuickItem> m_view;
	int m_frames;
	QPointF m_step;
	QString m_name;
	bool m_done = false;
	QElapsedTimer m_interval;
	QElapsedTimer m_render;
	QList<double> m_intervals;
	QList<double> m_renderTimes;
};

class MapDriver
{
public:
	MapDriver(QQuickWindow* window, const AutomationQuickMap::Fail& fail, ulong& timestamp) :
		m_window(window), m_fail(fail), m_timestamp(timestamp)
	{
		m_canvas = findItem(window->contentItem(), QStringLiteral("mapView"));
		m_palette = findItem(window->contentItem(), QStringLiteral("graphicsPalette"));
		if (m_canvas) {
			m_editor = qobject_cast<MapEditor*>(m_canvas->property("editor").value<QObject*>());
			m_doc = m_editor ? m_editor->document() : nullptr;
		}
	}

	bool hasMap() const
	{
		return m_canvas && m_editor && m_doc;
	}

	std::optional<bool> run(const QStringList& args);

private:
	/** The middle of a cell of the map in the window; reveal: scrolled to it first */
	QPointF cellPoint(int x, int y, bool reveal)
	{
		QVariant point;
		QMetaObject::invokeMethod(m_canvas, "cellPoint", Q_RETURN_ARG(QVariant, point), Q_ARG(QVariant, x),
			Q_ARG(QVariant, y), Q_ARG(QVariant, reveal));
		return m_canvas->mapToScene(point.toPointF());
	}
	QPointF paletteCellPoint(int x, int y)
	{
		QVariant point;
		QMetaObject::invokeMethod(m_palette, "cellPoint", Q_RETURN_ARG(QVariant, point), Q_ARG(QVariant, x), Q_ARG(QVariant, y));
		return m_palette->mapToScene(point.toPointF());
	}
	QPointF scroll() const
	{
		const auto* view = m_canvas->property("view").value<QQuickItem*>();
		return view ? QPointF(view->property("contentX").toReal(), view->property("contentY").toReal()) : QPointF();
	}

	/** A point of the window as the window system gives it (QT_SCALE_FACTOR...) */
	QPointF native(const QPointF& position) const
	{
		return QHighDpi::toNativeLocalPosition(position, m_window);
	}
	QPointF nativeGlobal(const QPointF& position) const
	{
		return QHighDpi::toNativeGlobalPosition(m_window->mapToGlobal(position), m_window);
	}

	/** A mouse event as the window system sends it, delivered at once */
	void mouse(QEvent::Type type, const QPointF& position, Qt::MouseButton button, Qt::MouseButtons buttons,
		Qt::KeyboardModifiers modifiers = Qt::NoModifier)
	{
		Pointer& state = pointer();
		state.window = m_window;
		state.position = position;
		state.buttons = buttons;
		QWindowSystemInterface::handleMouseEvent<Sync>(m_window, m_timestamp += 10, native(position), nativeGlobal(position),
			buttons, button, type, modifiers);
	}
	void click(const QPointF& position, Qt::MouseButton button, bool twice = false,
		Qt::KeyboardModifiers modifiers = Qt::NoModifier)
	{
		mouse(QEvent::MouseMove, position, Qt::NoButton, Qt::NoButton, modifiers);
		for (int i = 0; i < (twice ? 2 : 1); i++) {
			mouse(QEvent::MouseButtonPress, position, button, button, modifiers);
			mouse(QEvent::MouseButtonRelease, position, button, Qt::NoButton, modifiers);
		}
	}
	/** Through every cell between two, as a hand would */
	void drag(const QPoint& from, const QPoint& to, Qt::MouseButton button, bool palette = false,
		Qt::KeyboardModifiers modifiers = Qt::NoModifier)
	{
		auto at = [&](int x, int y) { return palette ? paletteCellPoint(x, y) : cellPoint(x, y, false); };
		const QPointF start = palette ? paletteCellPoint(from.x(), from.y()) : cellPoint(from.x(), from.y(), true);
		mouse(QEvent::MouseMove, start, Qt::NoButton, Qt::NoButton, modifiers);
		mouse(QEvent::MouseButtonPress, start, button, button, modifiers);
		const int steps = std::max(std::abs(to.x() - from.x()), std::abs(to.y() - from.y()));
		for (int i = 1; i <= steps; i++) {
			mouse(QEvent::MouseMove, at(from.x() + (to.x() - from.x()) * i / steps, from.y() + (to.y() - from.y()) * i / steps),
				Qt::NoButton, button, modifiers);
		}
		mouse(QEvent::MouseButtonRelease, at(to.x(), to.y()), button, Qt::NoButton, modifiers);
	}
	bool expectRect(const QString& what, const QRect& shown, const QStringList& args)
	{
		const std::optional<QRect> expected = toRect(args, 1);
		if (!expected) {
			m_fail(QStringLiteral("usage: %1 X Y W H|none").arg(args[0]));
			return false;
		}
		if (shown.isEmpty() != expected->isEmpty() || (!shown.isEmpty() && shown != *expected)) {
			m_fail(QStringLiteral("%1 is %2, not %3").arg(what, rectText(shown), rectText(*expected)));
			return false;
		}
		return true;
	}

	QQuickWindow* m_window;
	const AutomationQuickMap::Fail& m_fail;
	ulong& m_timestamp;
	QQuickItem* m_canvas = nullptr;
	QQuickItem* m_palette = nullptr;
	MapEditor* m_editor = nullptr;
	MapDocument* m_doc = nullptr;
};

std::optional<bool> MapDriver::run(const QStringList& args)
{
	const QString& cmd = args[0];
	int x = 0, y = 0, x2 = 0, y2 = 0, a = 0;
	// apart from the last command's: not a double click
	m_timestamp += 1000;

	// the actions of the menus of the map, which are not items
	auto enabled_check = [](const QString& command) { return command == QLatin1String("expect-enabled"); };
	if ((cmd == "expect-enabled" || cmd == "expect-shown" || cmd == "expect-checked") && args.size() == 3) {
		// the QML objects are children of the window, the items of their own
		QObject* action = m_window->findChild<QObject*>(args[1]);
		// a submenu too
		if (!action || (!action->inherits("QQuickAction") && !(enabled_check(cmd) && action->inherits("QQuickPopup"))))
			return std::nullopt;
		const bool on = args[2] == QLatin1String("on");
		const QString property = cmd == "expect-enabled" ? QStringLiteral("enabled")
			: cmd == "expect-checked" ? QStringLiteral("checked") : QStringLiteral("shown");
		const QVariant state = action->property(property.toLatin1());
		if (!state.isValid() || state.toBool() != on) {
			const QString shown = property == "enabled" ? (on ? "enabled" : "disabled")
				: property == "checked" ? (on ? "checked" : "unchecked") : (on ? "shown" : "hidden");
			m_fail(QStringLiteral("%1 is not %2").arg(args[1], shown));
			return false;
		}
		return true;
	}

	// the palette
	if (m_palette && (cmd == "palette-click" || cmd == "palette-drag")) {
		const bool dragged = cmd == "palette-drag";
		const QString option = args.size() == (dragged ? 6 : 4) ? args.last() : QString();
		if (!(dragged ? toInts(args, 1, {&x, &y, &x2, &y2}) : toInts(args, 1, {&x, &y}))
				|| (!option.isEmpty() && option != (dragged ? "cancel" : "right"))) {
			m_fail(dragged ? "usage: palette-drag X1 Y1 X2 Y2 [cancel]" : "usage: palette-click X Y [right]");
			return false;
		}
		if (!dragged) {
			click(paletteCellPoint(x, y), option == "right" ? Qt::RightButton : Qt::LeftButton);
			return true;
		}
		if (option != "cancel") {
			drag(QPoint(x, y), QPoint(x2, y2), Qt::LeftButton, true);
			return true;
		}
		// the right button before the left one is released
		const QPointF from = paletteCellPoint(x, y);
		const QPointF to = paletteCellPoint(x2, y2);
		mouse(QEvent::MouseMove, from, Qt::NoButton, Qt::NoButton);
		mouse(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
		mouse(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
		mouse(QEvent::MouseButtonPress, to, Qt::RightButton, Qt::LeftButton | Qt::RightButton);
		mouse(QEvent::MouseButtonRelease, to, Qt::RightButton, Qt::LeftButton);
		mouse(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
		return true;
	}
	if (m_palette && cmd == "expect-palette") {
		return expectRect(QStringLiteral("the palette's selection"), m_palette->property("selectionRect").toRect(), args);
	}
	if (m_palette && cmd == "palette-wheel" && args.size() == 5 && toInts(args, 1, {&x, &y, &a}) && args[4] == "ctrl") {
		// Ctrl + wheel over a cell of the palette: its zoom
		const QPointF at = paletteCellPoint(x, y);
		mouse(QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton, Qt::ControlModifier);
		QWindowSystemInterface::handleWheelEvent(m_window, m_timestamp += 10, native(at), nativeGlobal(at), QPoint(),
			QPoint(0, a), Qt::ControlModifier);
		QWindowSystemInterface::flushWindowSystemEvents();
		return true;
	}
	if (m_palette && cmd == "expect-palette-zoom" && args.size() == 2) {
		const double zoom = m_palette->property("zoom").toDouble();
		if (!qFuzzyCompare(zoom, args[1].toDouble())) {
			m_fail(QStringLiteral("the palette's zoom is %1, not %2").arg(zoom).arg(args[1]));
			return false;
		}
		return true;
	}

	if (!hasMap())
		return std::nullopt;

	if (cmd == "expect-picked" && args.size() == 4 && toInts(args, 1, {&a, &x, &y})) {
		// the tiles picked for the layer edited: the first one, the size
		const EditorState* state = m_editor->state();
		const int tile = state->selection(0, 0);
		if (tile != a || state->selWidth() != x || state->selHeight() != y) {
			m_fail(QStringLiteral("the tiles picked are %1 (%2 x %3), not %4 (%5 x %6)").arg(tile)
				.arg(state->selWidth()).arg(state->selHeight()).arg(a).arg(x).arg(y));
			return false;
		}
		return true;
	}
	// the words after the cells: the button, Shift held
	auto options = [&](qsizetype from, const QStringList& allowed) -> std::optional<QStringList> {
		const QStringList words = args.mid(from);
		for (const QString& word : words)
			if (!allowed.contains(word))
				return std::nullopt;
		return words;
	};
	if ((cmd == "map-click" || cmd == "map-double-click") && args.size() >= 3 && toInts(args, 1, {&x, &y})) {
		const auto words = options(3, {"left", "right", "shift"});
		if (!words)
			return false;
		click(cellPoint(x, y, true), words->contains("right") ? Qt::RightButton : Qt::LeftButton, cmd == "map-double-click",
			words->contains("shift") ? Qt::ShiftModifier : Qt::NoModifier);
		return true;
	}
	if (cmd == "map-drag" && args.size() >= 5 && toInts(args, 1, {&x, &y, &x2, &y2})) {
		const auto words = options(5, {"right", "shift"});
		if (!words)
			return false;
		drag(QPoint(x, y), QPoint(x2, y2), words->contains("right") ? Qt::RightButton : Qt::LeftButton, false,
			words->contains("shift") ? Qt::ShiftModifier : Qt::NoModifier);
		return true;
	}
	if (cmd == "map-press" && (args.size() == 3 || args.size() == 4) && toInts(args, 1, {&x, &y})) {
		const Qt::MouseButton button = args.value(3) == "right" ? Qt::RightButton : Qt::LeftButton;
		const QPointF at = cellPoint(x, y, true);
		pointer().pressScroll = scroll();
		mouse(QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton);
		mouse(QEvent::MouseButtonPress, at, button, pointer().buttons | button);
		return true;
	}
	if (cmd == "map-move" && args.size() == 3 && toInts(args, 1, {&x, &y})) {
		// the buttons held stay held
		mouse(QEvent::MouseMove, cellPoint(x, y, pointer().buttons == Qt::NoButton), Qt::NoButton, pointer().buttons);
		return true;
	}
	if (cmd == "map-move-out" && args.size() == 3 && toInt(args[2], a)) {
		// past an edge of the view, the other coordinate kept
		const auto* view = m_canvas->property("view").value<QQuickItem*>();
		const QRectF shown = view->mapRectToScene(QRectF(0, 0, view->width(), view->height()));
		QPointF at = pointer().window == m_window ? pointer().position : shown.center();
		if (args[1] == "left")
			at.setX(shown.left() - a);
		else if (args[1] == "right")
			at.setX(shown.right() + a);
		else if (args[1] == "up")
			at.setY(shown.top() - a);
		else if (args[1] == "down")
			at.setY(shown.bottom() + a);
		else
			return false;
		mouse(QEvent::MouseMove, at, Qt::NoButton, pointer().buttons);
		return true;
	}
	if (cmd == "map-release" && args.size() == 1) {
		const Qt::MouseButton button = pointer().buttons & Qt::LeftButton ? Qt::LeftButton : Qt::RightButton;
		mouse(QEvent::MouseButtonRelease, pointer().position, button, pointer().buttons & ~button);
		return true;
	}
	if (cmd == "map-key" && args.size() == 2) {
		// as typed on the map: the shortcuts of the window unless the map
		// takes the key first (ShortcutOverride), else the map
		const QKeySequence sequence = QKeySequence::fromString(args[1]);
		if (sequence.isEmpty())
			return false;
		const QKeyCombination key = sequence[0];
		if (QGuiApplication::focusWindow() != m_window)
			QWindowSystemInterface::handleFocusWindowChanged<Sync>(m_window);
		m_canvas->forceActiveFocus(Qt::OtherFocusReason);
		if (QWindowSystemInterface::handleShortcutEvent(m_window, m_timestamp += 10, key.key(), key.keyboardModifiers(), 0, 0, 0))
			return true;
		QWindowSystemInterface::handleKeyEvent<Sync>(m_window, m_timestamp += 10, QEvent::KeyPress, key.key(), key.keyboardModifiers());
		QWindowSystemInterface::handleKeyEvent<Sync>(m_window, m_timestamp += 10, QEvent::KeyRelease, key.key(), key.keyboardModifiers());
		return true;
	}
	if (cmd == "map-wheel" && (args.size() == 4 || args.size() == 5) && toInts(args, 1, {&x, &y, &a})) {
		// a wheel turned over a cell (a notch is 120), with Ctrl to zoom
		if (args.size() == 5 && args[4] != "ctrl")
			return false;
		const Qt::KeyboardModifiers modifiers = args.size() == 5 ? Qt::ControlModifier : Qt::NoModifier;
		const QPointF at = cellPoint(x, y, true);
		mouse(QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton, modifiers);
		QWindowSystemInterface::handleWheelEvent(m_window, m_timestamp += 10, native(at), nativeGlobal(at), QPoint(),
			QPoint(0, a), modifiers);
		QWindowSystemInterface::flushWindowSystemEvents();
		return true;
	}
	if (cmd == "pinch" && args.size() == 3 && args[1] == "map") {
		// a trackpad pinch over the middle of the view, the fingers
		// spreading (factor above 1) or closing, in ten events as macOS
		bool ok = false;
		const double factor = args[2].toDouble(&ok);
		if (!ok || factor <= 0)
			return false;
		const auto* view = m_canvas->property("view").value<QQuickItem*>();
		const QPointF at = view->mapToScene(QPointF(view->width() / 2, view->height() / 2));
		const QPointF local = native(at);
		const QPointF global = nativeGlobal(at);
		// a touchpad, as the platform has
		static const QPointingDevice* touchpad = [] {
			auto* device = new QPointingDevice(QStringLiteral("automation touchpad"), 1000, QInputDevice::DeviceType::TouchPad,
				QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 5, 0);
			QWindowSystemInterface::registerInputDevice(device);
			return device;
		}();
		mouse(QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton);
		const double step = std::pow(factor, 0.1) - 1.0;
		QWindowSystemInterface::handleGestureEvent(m_window, m_timestamp += 10, touchpad, Qt::BeginNativeGesture, local, global, 2);
		for (int i = 0; i < 10; ++i)
			QWindowSystemInterface::handleGestureEventWithRealValue(m_window, m_timestamp += 10, touchpad, Qt::ZoomNativeGesture, step, local, global, 2);
		QWindowSystemInterface::handleGestureEvent(m_window, m_timestamp += 10, touchpad, Qt::EndNativeGesture, local, global, 2);
		QWindowSystemInterface::flushWindowSystemEvents();
		qCInfo(lcAutomation).noquote() << QStringLiteral("map zoomed x%1").arg(m_editor->scale());
		return true;
	}
	if (cmd == "map-zoom" && args.size() == 2) {
		// as the zoom menu sets it, snapped to the screen
		bool ok = false;
		const double zoom = args[1].toDouble(&ok);
		if (!ok || zoom <= 0)
			return false;
		m_editor->setScale(zoom);
		return true;
	}
	if (cmd == "map-area" && (args.size() == 2 || args.size() == 5)) {
		// the dashed boundaries of an area, centered (MapCanvas.showArea)
		const std::optional<QRect> area = toRect(args, 1);
		if (!area)
			return false;
		return QMetaObject::invokeMethod(m_canvas, "showArea", Q_ARG(QVariant, QRectF(*area)));
	}
	if (cmd == "map-scroll" && args.size() == 3 && toInts(args, 1, {&x, &y})) {
		// the view at a point of the map, in its pixels (expect-scrolled from there)
		const bool done = QMetaObject::invokeMethod(m_canvas, "scrollTo", Q_ARG(QVariant, x), Q_ARG(QVariant, y), Q_ARG(QVariant, true));
		pointer().pressScroll = scroll();
		return done;
	}
	if (cmd == "map-frames" && args.size() >= 4 && toInts(args, 1, {&a, &x, &y}) && a > 1) {
		// times the next frames while the view scrolls (x, y pixels a frame);
		// logged once done, with the rest of the line: wait meanwhile
		auto* view = m_canvas->property("view").value<QQuickItem*>();
		new FrameProbe(m_window, view, a, QPointF(x, y), args.mid(4).join(u' '));
		return true;
	}
	if (cmd == "expect-tile" && (args.size() == 5 || (args.size() == 6 && args[5] == "exact"))
			&& (args[1] == "lower" || args[1] == "upper") && toInts(args, 2, {&x, &y, &a})) {
		const int tile = m_doc->tileAt(args[1] == "lower" ? EditorState::LOWER : EditorState::UPPER, x, y);
		// lower tiles: the terrain is compared (autotiles change with their
		// neighbours), unless exact: the borders too
		const bool same = args[1] == "lower" && args.size() == 5
			? Autotile::translate(static_cast<short>(tile)) == Autotile::translate(static_cast<short>(a))
			: tile == a;
		if (!same) {
			m_fail(QStringLiteral("%1 tile at %2,%3 is %4, not %5").arg(args[1]).arg(x).arg(y).arg(tile).arg(a));
			return false;
		}
		return true;
	}
	if (cmd == "expect-event" && args.size() == 4 && toInts(args, 1, {&x, &y, &a})) {
		if (m_doc->eventIdAt(x, y) != a) {
			m_fail(QStringLiteral("event at %1,%2 is %3, not %4").arg(x).arg(y).arg(m_doc->eventIdAt(x, y)).arg(a));
			return false;
		}
		return true;
	}
	if (cmd == "expect-cell" && args.size() == 3 && toInts(args, 1, {&x, &y})) {
		const QPoint cell = m_editor->selectedCell();
		if (cell != QPoint(x, y)) {
			m_fail(QStringLiteral("the cell selected is %1,%2, not %3,%4").arg(cell.x()).arg(cell.y()).arg(x).arg(y));
			return false;
		}
		return true;
	}
	if (cmd == "expect-cell-under" && args.size() == 3 && toInts(args, 1, {&x, &y})) {
		// the cell under the pointer now (after a zoom, a scroll)
		QVariant cell;
		const QPointF at = pointer().position;
		QMetaObject::invokeMethod(m_canvas, "cellAtScene", Q_RETURN_ARG(QVariant, cell), Q_ARG(QVariant, at.x()), Q_ARG(QVariant, at.y()));
		if (cell.toPointF().toPoint() != QPoint(x, y)) {
			m_fail(QStringLiteral("the cell under the pointer is %1,%2, not %3,%4").arg(cell.toPointF().x()).arg(cell.toPointF().y())
				.arg(x).arg(y));
			return false;
		}
		return true;
	}
	if (cmd == "expect-zoom" && args.size() == 2) {
		bool ok = false;
		const double zoom = args[1].toDouble(&ok);
		const double shown = m_canvas->property("zoom").toDouble();
		if (!ok || !qFuzzyCompare(zoom, shown) || !qFuzzyCompare(zoom, m_editor->scale())) {
			m_fail(QStringLiteral("the zoom is %1 (shown %2), not %3").arg(m_editor->scale()).arg(shown).arg(args[1]));
			return false;
		}
		return true;
	}
	if (cmd == "expect-selection")
		return expectRect(QStringLiteral("the selection"), m_editor->selectionRect(), args);
	if (cmd == "expect-brush")
		return expectRect(QStringLiteral("the brush"), m_editor->brushRect(), args);
	if (cmd == "expect-floating")
		return expectRect(QStringLiteral("the pasted tiles"), m_editor->floatingRect(), args);
	if (cmd == "expect-cursor" && args.size() == 2) {
		const char* shown = QMetaEnum::fromType<MapEditor::Cursor>().valueToKey(m_editor->cursor());
		if (args[1] != QLatin1String(shown)) {
			m_fail(QStringLiteral("the cursor is %1, not %2").arg(QLatin1String(shown), args[1]));
			return false;
		}
		// over the map, the overlay shows it
		const auto* overlay = m_canvas->property("overlay").value<QQuickItem*>();
		const bool has_cursor = overlay && overlay->cursor().shape() != Qt::ArrowCursor;
		if (overlay && has_cursor != (m_editor->cursor() != MapEditor::ArrowCursor)) {
			m_fail(QStringLiteral("the map does not show the cursor %1").arg(args[1]));
			return false;
		}
		return true;
	}
	if (cmd == "expect-dragging" && args.size() == 2) {
		if (m_editor->isDragging() != (args[1] == "on")) {
			m_fail(QStringLiteral("the map is %1 dragged on").arg(m_editor->isDragging() ? "" : "not"));
			return false;
		}
		return true;
	}
	if (cmd == "expect-scrolled" && args.size() == 2) {
		// since the last map-press or map-scroll
		const QPointF moved = scroll() - pointer().pressScroll;
		const QString direction = moved.x() < 0 ? QStringLiteral("left") : moved.x() > 0 ? QStringLiteral("right")
			: moved.y() < 0 ? QStringLiteral("up") : moved.y() > 0 ? QStringLiteral("down") : QStringLiteral("none");
		qCInfo(lcAutomation).noquote() << QStringLiteral("the map scrolled by %1, %2").arg(moved.x()).arg(moved.y());
		if (direction != args[1]) {
			m_fail(QStringLiteral("the map scrolled %1, not %2").arg(direction, args[1]));
			return false;
		}
		return true;
	}
	if (cmd == "log-map" && args.size() == 1) {
		const QRectF cells = m_editor->viewCells();
		qCInfo(lcAutomation).noquote() << QStringLiteral("map: zoom %1, scrolled to %2, %3, cells %4,%5 to %6,%7 shown, selection %8, brush %9")
			.arg(m_editor->scale()).arg(scroll().x()).arg(scroll().y()).arg(cells.left(), 0, 'f', 1).arg(cells.top(), 0, 'f', 1)
			.arg(cells.right(), 0, 'f', 1).arg(cells.bottom(), 0, 'f', 1).arg(rectText(m_editor->selectionRect()), rectText(m_editor->brushRect()));
		return true;
	}
	return std::nullopt;
}

}

std::optional<bool> AutomationQuickMap::run(QQuickWindow* window, const QStringList& args, const Fail& fail, ulong& timestamp)
{
	MapDriver driver(window, fail, timestamp);
	return driver.run(args);
}
