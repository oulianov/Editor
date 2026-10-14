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
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QStandardPaths>

namespace {
/** The file of a screenshot: the path given (its folder made), else in
 *  Pictures/EasyRPG Editor */
QString screenshotFile(const QString& path) {
	if (!path.isEmpty()) {
		QDir().mkpath(QFileInfo(path).absolutePath());
		return path;
	}
	// a script's in its own folder (main.cpp), never in the user's pictures
	const QString script_dir = qEnvironmentVariable("EASYRPG_EDITOR_SCRIPT_DIR");
	QDir dir(script_dir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) : script_dir);
	dir.mkpath(QStringLiteral("EasyRPG Editor"));
	dir.cd(QStringLiteral("EasyRPG Editor"));
	return dir.filePath(QStringLiteral("editor-%1.png")
		.arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))));
}

class ShortcutFilter : public QObject {
public:
	ShortcutFilter(std::function<void(const QString&)> saved, QObject* parent) :
		QObject(parent), m_saved(std::move(saved)) {}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override {
		if (event->type() == QEvent::KeyPress) {
			auto* key = static_cast<QKeyEvent*>(event);
			if (key->key() == Qt::Key_S && !key->isAutoRepeat()
					&& key->modifiers() == (Qt::ControlModifier | Qt::AltModifier)) {
				auto* window = qobject_cast<QQuickWindow*>(QGuiApplication::focusWindow());
				const QString file = window ? Screenshot::save(window) : QString();
				if (!file.isEmpty() && m_saved) {
					m_saved(QObject::tr("Screenshot saved to %1").arg(file));
				}
				return true;
			}
		}
		return QObject::eventFilter(watched, event);
	}

private:
	std::function<void(const QString&)> m_saved;
};
}

QString Screenshot::save(QQuickWindow* window, const QString& path) {
	if (!window) {
		qCWarning(lcAutomation) << "screenshot: no window to capture";
		return QString();
	}
	const QString file = screenshotFile(path);
	if (!window->grabWindow().save(file)) {
		qCWarning(lcAutomation) << "screenshot: cannot write" << file;
		return QString();
	}
	qCInfo(lcAutomation) << "screenshot saved:" << file;
	return file;
}

void Screenshot::installShortcut(std::function<void(const QString& message)> saved) {
	qApp->installEventFilter(new ShortcutFilter(std::move(saved), qApp));
}
