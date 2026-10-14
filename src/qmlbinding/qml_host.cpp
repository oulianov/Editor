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

#include "qml_host.h"
#include "common/editor_settings.h"
#include "common/logging.h"
#include "controller/edit_locks.h"
#include "controller/workspace_store.h"
#include "ui/shell/qml_engine.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QGuiApplication>
#include <QHash>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QScreen>

namespace {

QList<QPointer<QQuickWindow>>& opened() {
	static QList<QPointer<QQuickWindow>> windows;
	return windows;
}

/** Centered on the window it belongs to, on its screen */
void center(QQuickWindow* window) {
	QWindow* over = window->transientParent();
	if (!over) {
		return;
	}
	const QRect frame = over->frameGeometry();
	QRect geometry(QPoint(), window->size());
	geometry.moveCenter(frame.center());
	if (const QScreen* screen = over->screen()) {
		const QRect available = screen->availableGeometry();
		geometry.moveLeft(std::clamp(geometry.left(), available.left(), std::max(available.left(), available.right() - geometry.width())));
		geometry.moveTop(std::clamp(geometry.top(), available.top(), std::max(available.top(), available.bottom() - geometry.height())));
	}
	window->setPosition(geometry.topLeft());
}

QQuickWindow* create(const QString& type, QWindow* over, const QVariantMap& properties) {
	// a modal window holds an edit lock while it is shown
	ModalWindowLocks::install();
	QQmlEngine* engine = QmlEngine::instance();
	QQmlComponent component(engine, QStringLiteral("org.easyrpg.editor"), type);
	QObject* object = component.isReady() ? component.createWithInitialProperties(properties) : nullptr;
	auto* window = qobject_cast<QQuickWindow*>(object);
	if (!window) {
		qCWarning(lcQml).noquote() << QStringLiteral("The QML window %1 could not be made: %2").arg(type, component.errorString());
		delete object;
		return nullptr;
	}
	QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
	if (over) {
		window->setTransientParent(over);
	}
	// a window with a stateKey opens where and as large as it was last time
	const QString key = window->property("stateKey").toString();
	const QRect saved = key.isEmpty() ? QRect() : WorkspaceStore().dialogGeometry(key);
	if (saved.isValid()) {
		const QScreen* screen = over ? over->screen() : window->screen();
		QRect geometry = saved;
		if (screen) {
			const QRect available = screen->availableGeometry();
			// within the screen, but never smaller than at its first opening
			// (a screen smaller than that: as the first opening)
			const QSize largest = available.size().expandedTo(window->size());
			geometry.setSize(geometry.size().boundedTo(largest).expandedTo(window->minimumSize()));
			geometry.moveLeft(std::clamp(geometry.left(), available.left(), std::max(available.left(), available.right() - geometry.width())));
			geometry.moveTop(std::clamp(geometry.top(), available.top(), std::max(available.top(), available.bottom() - geometry.height())));
		}
		window->setProperty("defaultSize", window->size());
		window->setGeometry(geometry);
	} else {
		window->setProperty("defaultSize", window->size());
		center(window);
	}
	if (!key.isEmpty()) {
		QObject::connect(window, &QWindow::visibleChanged, window, [window, key](bool visible) {
			if (!visible && !QCoreApplication::instance()->property("local_state_resetting").toBool()) {
				WorkspaceStore().setDialogGeometry(key, window->geometry());
			}
		});
		// Settings > Reset local state: its first size, centered again
		QObject::connect(&EditorSettings::instance(), &EditorSettings::localStateReset, window, [window] {
			window->resize(window->property("defaultSize").toSize().expandedTo(window->minimumSize()));
			center(window);
		});
	}
	opened().removeAll(nullptr);
	opened().append(window);
	return window;
}

} // namespace

QQuickWindow* QmlHost::open(const QString& type, QWindow* over, const QVariantMap& properties) {
	QQuickWindow* window = create(type, over, properties);
	if (!window) {
		return nullptr;
	}
	QObject::connect(window, &QWindow::visibleChanged, window, [window](bool visible) {
		if (!visible) {
			window->deleteLater();
		}
	});
	window->show();
	window->requestActivate();
	return window;
}

QQuickWindow* QmlHost::make(const QString& type, const QVariantMap& properties) {
	return create(type, nullptr, properties);
}

int QmlHost::exec(const QString& type, QWindow* over, const QVariantMap& properties,
		const std::function<void(QQuickWindow*)>& closed) {
	QPointer<QQuickWindow> window = create(type, over, properties);
	if (!window) {
		return 0;
	}
	window->setModality(Qt::ApplicationModal);
	// What another program changes waits until the caller has read the
	// answer and the window is gone, not only while it is shown: the timer
	// of ProjectWatcher can fire in the loop after it hid
	EditLocks::Lock lock;
	if (!window->property(ModalWindowLocks::OwnLockProperty).toBool()) {
		lock = EditLocks::Lock({EditLock::Dialog});
		window->setProperty(ModalWindowLocks::OwnLockProperty, true);
	}
	QEventLoop loop;
	QObject::connect(window, &QWindow::visibleChanged, &loop, [&loop](bool visible) {
		if (!visible) {
			loop.quit();
		}
	});
	QObject::connect(window, &QObject::destroyed, &loop, &QEventLoop::quit);
	window->show();
	window->requestActivate();
	loop.exec(QEventLoop::DialogExec);
	if (!window) {
		return 0;
	}
	const int result = window->property("result").toInt();
	if (closed) {
		closed(window);
	}
	// now: what it was given (often on the caller's stack) outlives it
	delete window.data();
	return result;
}

QQuickWindow* QmlHost::help(QWindow* over, const QString& topic) {
	static QHash<QWindow*, QPointer<QQuickWindow>> guides;
	QPointer<QQuickWindow>& guide = guides[over];
	if (guide && guide->isVisible()) {
		guide->setProperty("topic", topic);
		guide->raise();
		guide->requestActivate();
		return guide;
	}
	guide = open(QStringLiteral("HelpWindow"), over, {{QStringLiteral("topic"), topic}});
	return guide;
}

QList<QQuickWindow*> QmlHost::windows() {
	QList<QQuickWindow*> shown;
	for (const auto& window : opened()) {
		if (window && window->isVisible()) {
			shown.append(window);
		}
	}
	return shown;
}

QQuickWindow* QmlHost::frontWindow() {
	const QList<QQuickWindow*> shown = windows();
	// a modal window is in front of the others
	if (QWindow* modal = QGuiApplication::modalWindow()) {
		auto* quick = qobject_cast<QQuickWindow*>(modal);
		return shown.contains(quick) ? quick : nullptr;
	}
	auto* active = qobject_cast<QQuickWindow*>(QGuiApplication::focusWindow());
	return shown.contains(active) ? active : nullptr;
}
