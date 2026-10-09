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

#include "screenshot.h"
#include "common/logging.h"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMainWindow>
#include <QStandardPaths>
#include <QStatusBar>
#include <QWidget>

namespace {
class ShortcutFilter : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject* watched, QEvent* event) override {
		if (event->type() == QEvent::KeyPress) {
			auto* key = static_cast<QKeyEvent*>(event);
			if (key->key() == Qt::Key_S && !key->isAutoRepeat()
					&& key->modifiers() == (Qt::ControlModifier | Qt::AltModifier)) {
				QString file = Screenshot::save();
				auto* main = qobject_cast<QMainWindow*>(qApp->activeWindow());
				if (main && !file.isEmpty()) {
					main->statusBar()->showMessage(QObject::tr("Screenshot saved to %1").arg(file), 5000);
				}
				return true;
			}
		}
		return QObject::eventFilter(watched, event);
	}
};
}

QWidget* Screenshot::activeWindow() {
	if (QWidget* modal = QApplication::activeModalWidget()) {
		return modal;
	}
	if (QWidget* active = QApplication::activeWindow()) {
		return active;
	}
	// Offscreen there is no active window: take the main window
	for (QWidget* top : QApplication::topLevelWidgets()) {
		if (top->isVisible() && qobject_cast<QMainWindow*>(top)) {
			return top;
		}
	}
	return nullptr;
}

QString Screenshot::save(QWidget* widget, const QString& path) {
	if (!widget) {
		widget = activeWindow();
	}
	if (!widget) {
		qCWarning(lcAutomation) << "screenshot: no window to capture";
		return QString();
	}

	QString file = path;
	if (file.isEmpty()) {
		QDir dir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
		dir.mkpath(QStringLiteral("EasyRPG Editor"));
		dir.cd(QStringLiteral("EasyRPG Editor"));
		file = dir.filePath(QStringLiteral("editor-%1.png")
			.arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))));
	} else {
		QDir().mkpath(QFileInfo(file).absolutePath());
	}

	if (!widget->grab().save(file)) {
		qCWarning(lcAutomation) << "screenshot: cannot write" << file;
		return QString();
	}
	qCInfo(lcAutomation) << "screenshot saved:" << file;
	return file;
}

void Screenshot::installShortcut() {
	qApp->installEventFilter(new ShortcutFilter(qApp));
}
