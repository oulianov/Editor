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

#include "editor_bridge.h"
#include "common/editor_settings.h"
#include "model/game_runner.h"
#include "qml_host.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QScreen>
#include <QStyleHints>
#include <QTextBlock>
#include <QTextDocument>
#include <qpa/qplatformtheme.h>
#include <QtGui/private/qguiapplication_p.h>

EditorBridge::EditorBridge(QObject* parent) : QObject(parent) {
	connect(&EditorSettings::instance(), &EditorSettings::changed, this, &EditorBridge::settingsChanged);
	connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &EditorBridge::darkChanged);
}

QString EditorBridge::language() const { return EditorSettings::instance().language(); }
void EditorBridge::setLanguage(const QString& code) { EditorSettings::instance().setLanguage(code); }
QString EditorBridge::rtpPath() const { return EditorSettings::instance().rtpPath(); }
void EditorBridge::setRtpPath(const QString& path) { EditorSettings::instance().setRtpPath(path); }
QString EditorBridge::playerPath() const { return EditorSettings::instance().playerPath(); }
void EditorBridge::setPlayerPath(const QString& path) { EditorSettings::instance().setPlayerPath(path); }
QString EditorBridge::scriptEditor() const { return EditorSettings::instance().scriptEditor(); }
void EditorBridge::setScriptEditor(const QString& editor) { EditorSettings::instance().setScriptEditor(editor); }
bool EditorBridge::reloadExternalChanges() const { return EditorSettings::instance().reloadExternalChanges(); }
void EditorBridge::setReloadExternalChanges(bool on) { EditorSettings::instance().setReloadExternalChanges(on); }
bool EditorBridge::searchCache() const { return EditorSettings::instance().searchCache(); }
void EditorBridge::setSearchCache(bool on) { EditorSettings::instance().setSearchCache(on); }
bool EditorBridge::animateTiles() const { return EditorSettings::instance().animateTiles(); }
void EditorBridge::setAnimateTiles(bool on) { EditorSettings::instance().setAnimateTiles(on); }
bool EditorBridge::commandColors() const { return EditorSettings::instance().commandColors(); }
void EditorBridge::setCommandColors(bool on) { EditorSettings::instance().setCommandColors(on); }
bool EditorBridge::commandStripes() const { return EditorSettings::instance().commandStripes(); }
void EditorBridge::setCommandStripes(bool on) { EditorSettings::instance().setCommandStripes(on); }
QString EditorBridge::defaultScriptEditor() const { return EditorSettings::defaultScriptEditor(); }

QVariantList EditorBridge::languages() const {
	return {
		QVariantMap{{QStringLiteral("code"), QStringLiteral("en")}, {QStringLiteral("name"), QStringLiteral("English")}},
		QVariantMap{{QStringLiteral("code"), QStringLiteral("fr")}, {QStringLiteral("name"), QStringLiteral("Français (Rabbi-Bodom)")}},
		QVariantMap{{QStringLiteral("code"), QStringLiteral("de")}, {QStringLiteral("name"), QStringLiteral("Deutsch")}},
	};
}

QString EditorBridge::version() const {
	return QCoreApplication::applicationVersion();
}

bool EditorBridge::dark() const {
	return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

QFont EditorBridge::font() const {
	return formFont();
}

QFont EditorBridge::formFont() {
	QFont font = QGuiApplication::font();
#if defined(Q_OS_MACOS)
	// the small font of macOS's controls, dense like RPG Maker's windows
	if (const QPlatformTheme* theme = QGuiApplicationPrivate::platformTheme()) {
		if (const QFont* small = theme->font(QPlatformTheme::SmallFont)) {
			font = *small;
		}
	}
#endif
	return font;
}

QFont EditorBridge::monospaceFont() const {
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	if (!QFontInfo(font).fixedPitch()) {
		// no monospaced font of the system (some Linux desktops)
		font.setFamilies({QStringLiteral("Menlo"), QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans Mono"),
			QStringLiteral("Liberation Mono"), QStringLiteral("Courier New"), QStringLiteral("monospace")});
		font.setStyleHint(QFont::Monospace);
	}
	font.setPointSizeF(this->font().pointSizeF());
	return font;
}

QString EditorBridge::helpText() const {
	QFile file(QStringLiteral(":/help/en/index.html"));
	return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

int EditorBridge::anchorPosition(QQuickTextDocument* document, const QString& anchor) const {
	const QTextDocument* text = document ? document->textDocument() : nullptr;
	if (!text || anchor.isEmpty()) {
		return -1;
	}
	for (QTextBlock block = text->begin(); block.isValid(); block = block.next()) {
		// an empty <a name> at the start of a heading is the block's (as
		// QTextBrowser finds them)
		if (block.charFormat().anchorNames().contains(anchor)) {
			return block.position();
		}
		for (auto it = block.begin(); !it.atEnd(); ++it) {
			const QTextFragment fragment = it.fragment();
			if (fragment.isValid() && fragment.charFormat().anchorNames().contains(anchor)) {
				return fragment.position();
			}
		}
	}
	return -1;
}

bool EditorBridge::hasIcon(const QString& name) const {
	return !name.isEmpty() && QIcon::hasThemeIcon(name);
}

QRectF EditorBridge::screenArea(QQuickItem* item) const {
	QScreen* screen = nullptr;
	if (item) {
		screen = QGuiApplication::screenAt(item->mapToGlobal(QPointF(item->width() / 2, item->height() / 2)).toPoint());
		if (!screen && item->window()) {
			screen = item->window()->screen();
		}
	}
	screen = screen ? screen : QGuiApplication::primaryScreen();
	return screen ? QRectF(screen->availableGeometry()) : QRectF();
}

void EditorBridge::resetLocalState() {
	EditorSettings::instance().resetLocalState();
}

void EditorBridge::help(const QString& topic, QQuickItem* from) {
	QmlHost::help(from ? from->window() : nullptr, topic);
}

int EditorBridge::exec(const QString& type, const QVariantMap& properties, QQuickItem* from) {
	return QmlHost::exec(type, from ? from->window() : nullptr, properties);
}

QUrl EditorBridge::playerFolder() const {
	const QString player = GameRunner::findPlayer();
	return player.isEmpty() ? QUrl() : QUrl::fromLocalFile(QFileInfo(player).absolutePath());
}

QString EditorBridge::executableOf(const QUrl& file) const {
	return GameRunner::executableOf(file.toLocalFile());
}

QString EditorBridge::localPath(const QUrl& file) const {
	return file.isLocalFile() ? file.toLocalFile() : file.toString();
}

QUrl EditorBridge::url(const QString& path) const {
	return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(QDir::cleanPath(path));
}
