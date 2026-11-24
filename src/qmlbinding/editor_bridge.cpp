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
#include <vector>
#include <cmath>
#include <QPixmap>
#include <QImage>
#include <QTextImageFormat>
#include <QTextCursor>
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
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QScreen>
#include <QTextBlock>
#include <QTextDocument>
#include <qpa/qplatformtheme.h>
#include <QtGui/private/qguiapplication_p.h>

EditorBridge::EditorBridge(QObject* parent) : QObject(parent) {
	connect(&EditorSettings::instance(), &EditorSettings::changed, this, &EditorBridge::settingsChanged);
	connect(&EditorSettings::instance(), &EditorSettings::darkChanged, this, &EditorBridge::darkChanged);
}

QString EditorBridge::language() const { return EditorSettings::instance().language(); }
void EditorBridge::setLanguage(const QString& code) { EditorSettings::instance().setLanguage(code); }
QStringList EditorBridge::rtpFolders() const { return EditorSettings::instance().rtpFolders(); }
void EditorBridge::setRtpFolders(const QStringList& folders) { EditorSettings::instance().setRtpFolders(folders); }
QString EditorBridge::rtpDefault(int engine) const { return EditorSettings::instance().rtpDefault(engine); }
void EditorBridge::setRtpDefault(int engine, const QString& folder) { EditorSettings::instance().setRtpDefault(engine, folder); }

QVariantMap EditorBridge::rtpFolderInfo(const QString& folder) const {
	const int engine = EditorSettings::rtpEngine(folder);
	return {
		{QStringLiteral("exists"), !folder.isEmpty() && QFileInfo(folder).isDir()},
		{QStringLiteral("engine"), engine},
		{QStringLiteral("translation"), engine ? EditorSettings::rtpTranslation(folder, engine) : QString()},
		{QStringLiteral("easyrpg"), engine != 0 && EditorSettings::isEasyRpgRtp(folder)},
	};
}

QString EditorBridge::playerPath() const { return EditorSettings::instance().playerPath(); }
void EditorBridge::setPlayerPath(const QString& path) { EditorSettings::instance().setPlayerPath(path); }
QString EditorBridge::scriptEditor() const { return EditorSettings::instance().scriptEditor(); }
void EditorBridge::setScriptEditor(const QString& editor) { EditorSettings::instance().setScriptEditor(editor); }
bool EditorBridge::reloadExternalChanges() const { return EditorSettings::instance().reloadExternalChanges(); }
void EditorBridge::setReloadExternalChanges(bool on) { EditorSettings::instance().setReloadExternalChanges(on); }
bool EditorBridge::searchCache() const { return EditorSettings::instance().searchCache(); }
void EditorBridge::setSearchCache(bool on) { EditorSettings::instance().setSearchCache(on); }
bool EditorBridge::checkForUpdates() const { return EditorSettings::instance().checkForUpdates(); }
void EditorBridge::setCheckForUpdates(bool on) { EditorSettings::instance().setCheckForUpdates(on); }
bool EditorBridge::animateTiles() const { return EditorSettings::instance().animateTiles(); }
void EditorBridge::setAnimateTiles(bool on) { EditorSettings::instance().setAnimateTiles(on); }
bool EditorBridge::commandColors() const { return EditorSettings::instance().commandColors(); }
void EditorBridge::setCommandColors(bool on) { EditorSettings::instance().setCommandColors(on); }
bool EditorBridge::commandsByTopic() const { return EditorSettings::instance().commandsByTopic(); }
void EditorBridge::setCommandsByTopic(bool on) { EditorSettings::instance().setCommandsByTopic(on); }
bool EditorBridge::commandStripes() const { return EditorSettings::instance().commandStripes(); }
void EditorBridge::setCommandStripes(bool on) { EditorSettings::instance().setCommandStripes(on); }
bool EditorBridge::originBadges() const { return EditorSettings::instance().originBadges(); }
void EditorBridge::setOriginBadges(bool on) { EditorSettings::instance().setOriginBadges(on); }
bool EditorBridge::unavailableShown() const { return EditorSettings::instance().unavailableShown(); }
void EditorBridge::setUnavailableShown(bool on) { EditorSettings::instance().setUnavailableShown(on); }
QString EditorBridge::colorScheme() const { return EditorSettings::instance().colorScheme(); }
void EditorBridge::setColorScheme(const QString& scheme) { EditorSettings::instance().setColorScheme(scheme); }
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
	return EditorSettings::instance().isDark();
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

bool EditorBridge::fitImages(QQuickTextDocument* document, qreal width) const {
	QTextDocument* text = document ? document->textDocument() : nullptr;
	if (!text || width <= 0) {
		return false;
	}
	// the width the text gave the picture (<img width>), kept apart: the
	// one set here changes with the width of the view
	constexpr int given_width = QTextFormat::UserProperty + 1;
	struct Fit {
		int position;
		int length;
		QTextImageFormat format;
	};
	std::vector<Fit> fits;
	for (QTextBlock block = text->begin(); block.isValid(); block = block.next()) {
		for (auto it = block.begin(); !it.atEnd(); ++it) {
			const QTextFragment fragment = it.fragment();
			if (!fragment.isValid() || !fragment.charFormat().isImageFormat()) {
				continue;
			}
			QTextImageFormat format = fragment.charFormat().toImageFormat();
			const QVariant resource = text->resource(QTextDocument::ImageResource, QUrl(format.name()));
			QSize natural;
			if (resource.canConvert<QImage>()) {
				natural = resource.value<QImage>().size();
			}
			if (natural.isEmpty() && resource.canConvert<QPixmap>()) {
				natural = resource.value<QPixmap>().size();
			}
			if (natural.isEmpty()) {
				continue; // not loaded yet
			}
			if (!format.hasProperty(given_width)) {
				format.setProperty(given_width, format.width());
			}
			const qreal given = format.property(given_width).toReal();
			const qreal fitted = std::min(given > 0 ? given : natural.width(), std::floor(width));
			const qreal height = fitted * natural.height() / natural.width();
			if (qAbs(format.width() - fitted) < 0.5 && qAbs(format.height() - height) < 0.5) {
				continue;
			}
			format.setWidth(fitted);
			format.setHeight(height);
			fits.push_back({fragment.position(), fragment.length(), format});
		}
	}
	for (const Fit& fit : fits) {
		QTextCursor cursor(text);
		cursor.setPosition(fit.position);
		cursor.setPosition(fit.position + fit.length, QTextCursor::KeepAnchor);
		cursor.setCharFormat(fit.format);
	}
	// the view draws the whole text again: else what follows a picture
	// resized stays drawn where it was, or not at all
	if (!fits.empty()) {
		text->markContentsDirty(0, text->characterCount());
	}
	return !fits.empty();
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

bool EditorBridge::passKey(QQuickItem* item, QObject* event) const {
	if (!item || !event) {
		return false;
	}
	QKeyEvent press(QEvent::KeyPress, event->property("key").toInt(),
		Qt::KeyboardModifiers(event->property("modifiers").toInt()), event->property("text").toString(),
		event->property("isAutoRepeat").toBool(), quint16(event->property("count").toInt()));
	// as the window delivers a key the item in focus leaves
	for (QQuickItem* parent = item->parentItem(); parent; parent = parent->parentItem()) {
		press.accept();
		QCoreApplication::sendEvent(parent, &press);
		if (press.isAccepted()) {
			return true;
		}
	}
	return false;
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
