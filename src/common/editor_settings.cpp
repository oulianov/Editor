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

#include "editor_settings.h"
#include "defines.h"
#include "model/game_runner.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QPalette>
#include <QSet>
#include <QStandardPaths>
#include <QStyleHints>
#include <QVariantMap>
#include <algorithm>

namespace {
	constexpr const char* ANIMATE_TILES_KEY = "animate_tiles";
	constexpr const char* COMMAND_COLORS_KEY = "event_command_colors";
	constexpr const char* COMMAND_STRIPES_KEY = "event_command_stripes";
	constexpr const char* SEARCH_CACHE_KEY = "search_cache";
	constexpr const char* CHECK_UPDATES_KEY = "check_for_updates";
	constexpr const char* MANIAC_MODE_KEY = "maniac_mode";
}

EditorSettings& EditorSettings::instance() {
	static EditorSettings settings;
	return settings;
}

QVariant EditorSettings::value(const char* key, const QVariant& fallback) const {
	return QSettings().value(QLatin1String(key), fallback);
}

void EditorSettings::setValue(const char* key, const QVariant& value) {
	QSettings settings;
	if (settings.value(QLatin1String(key)) == value) {
		return;
	}
	settings.setValue(QLatin1String(key), value);
	emit changed();
}

QString EditorSettings::language() const { return value(LANGUAGE_KEY, QString()).toString(); }
void EditorSettings::setLanguage(const QString& code) { setValue(LANGUAGE_KEY, code); }

namespace {

// What KColorScheme reads instead of the system's scheme, as
// KColorSchemeManager sets it: Breeze's Kirigami theme and KDE's platform
// theme take their colors from it, not from the palette of the application
constexpr const char* KDE_SCHEME_PATH = "KDE_COLOR_SCHEME_PATH";

bool darkColors(const QPalette& palette) {
	return palette.color(QPalette::Window).lightness() < 128;
}

// The colors of Qt's Fusion, for the platforms that cannot show the scheme
// chosen (offscreen)
QPalette fusionPalette(bool dark) {
	const QColor window = dark ? QColor(50, 50, 50) : QColor(239, 239, 239);
	const QColor base = dark ? window.darker(140) : QColor(Qt::white);
	const QColor text = dark ? QColor(Qt::white) : QColor(Qt::black);
	const QColor disabled_text = dark ? QColor(130, 130, 130) : QColor(190, 190, 190);
	QPalette palette(text, window, window.lighter(150), window.darker(150), window.darker(130), text,
		dark ? text : QColor(Qt::white), base, window);
	palette.setColor(QPalette::AlternateBase, window.darker(110));
	palette.setColor(QPalette::Highlight, QColor(48, 140, 198));
	palette.setColor(QPalette::HighlightedText, dark ? text : QColor(Qt::white));
	palette.setColor(QPalette::Accent, QColor(48, 140, 198));
	palette.setColor(QPalette::Link, dark ? QColor(80, 170, 230) : QColor(0, 0, 255));
	palette.setColor(QPalette::LinkVisited, dark ? QColor(170, 130, 220) : QColor(255, 0, 255));
	palette.setColor(QPalette::ToolTipBase, dark ? base : QColor(255, 255, 220));
	palette.setColor(QPalette::ToolTipText, QColor(dark ? Qt::white : Qt::black));
	QColor placeholder = text;
	placeholder.setAlpha(128);
	palette.setColor(QPalette::PlaceholderText, placeholder);
	for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText}) {
		palette.setColor(QPalette::Disabled, role, disabled_text);
	}
	palette.setColor(QPalette::Disabled, QPalette::Base, window);
	palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(145, 145, 145));
	return palette;
}

// The colors of a KDE color scheme (Breeze's .colors), as KColorScheme
// gives them to the applications; Fusion's for what the file lacks
QPalette kdePalette(const QString& path, bool dark) {
	const QSettings file(path, QSettings::IniFormat);
	QPalette palette = fusionPalette(dark);
	auto color = [&file](const char* set, const char* key) {
		const QStringList rgb = file.value(QStringLiteral("Colors:%1/%2").arg(QLatin1String(set), QLatin1String(key))).toStringList();
		return rgb.size() >= 3 ? QColor(rgb[0].toInt(), rgb[1].toInt(), rgb[2].toInt()) : QColor();
	};
	const struct { QPalette::ColorRole role; const char* set; const char* key; } roles[] = {
		{QPalette::Window, "Window", "BackgroundNormal"},
		{QPalette::WindowText, "Window", "ForegroundNormal"},
		{QPalette::Base, "View", "BackgroundNormal"},
		{QPalette::AlternateBase, "View", "BackgroundAlternate"},
		{QPalette::Text, "View", "ForegroundNormal"},
		{QPalette::PlaceholderText, "View", "ForegroundInactive"},
		{QPalette::Link, "View", "ForegroundLink"},
		{QPalette::LinkVisited, "View", "ForegroundVisited"},
		{QPalette::Button, "Button", "BackgroundNormal"},
		{QPalette::ButtonText, "Button", "ForegroundNormal"},
		{QPalette::Highlight, "Selection", "BackgroundNormal"},
		{QPalette::Accent, "Selection", "BackgroundNormal"},
		{QPalette::HighlightedText, "Selection", "ForegroundNormal"},
		{QPalette::ToolTipBase, "Tooltip", "BackgroundNormal"},
		{QPalette::ToolTipText, "Tooltip", "ForegroundNormal"},
	};
	for (const auto& role : roles) {
		if (const QColor c = color(role.set, role.key); c.isValid()) {
			palette.setColor(role.role, c);
		}
	}
	// the frames shaded from the face of the buttons, the disabled text in
	// the inactive one (KColorScheme computes both, from the file's effects)
	const QColor button = palette.color(QPalette::Button);
	palette.setColor(QPalette::Light, button.lighter(150));
	palette.setColor(QPalette::Midlight, button.lighter(120));
	palette.setColor(QPalette::Mid, button.darker(130));
	palette.setColor(QPalette::Dark, button.darker(150));
	if (const QColor inactive = color("Window", "ForegroundInactive"); inactive.isValid()) {
		for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
			palette.setColor(QPalette::Disabled, role, inactive);
		}
	}
	return palette;
}

}

QString EditorSettings::colorScheme() const { return value(COLOR_SCHEME_KEY, QStringLiteral("system")).toString(); }
void EditorSettings::setColorScheme(const QString& scheme) {
	setValue(COLOR_SCHEME_KEY, scheme);
	applyColorScheme();
}

bool EditorSettings::isDark() const { return m_dark; }

void EditorSettings::applyColorScheme() {
	auto hints = QGuiApplication::styleHints();
	if (!m_watching) {
		// the system's scheme followed, or the one chosen shown again over it
		m_watching = true;
		connect(hints, &QStyleHints::colorSchemeChanged, this, &EditorSettings::updateColors);
	}
	// macOS shows the windows in it, title bars and menus too; Windows with
	// its windows11 and fusion styles; some Linux themes. Unknown: the
	// system's again
	const QString scheme = colorScheme();
	hints->setColorScheme(scheme == QLatin1String("dark") ? Qt::ColorScheme::Dark
		: scheme == QLatin1String("light") ? Qt::ColorScheme::Light : Qt::ColorScheme::Unknown);
	updateColors();
}

void EditorSettings::updateColors() {
	// the colors of the platform now (in the scheme asked for when it could
	// show it): ours dropped, an empty palette is the platform's
	if (qApp->property(KDE_SCHEME_PATH).isValid()) {
		qApp->setProperty(KDE_SCHEME_PATH, QVariant());
	}
	QGuiApplication::setPalette(QPalette());
	const bool system_dark = darkColors(QGuiApplication::palette());
	const QString scheme = colorScheme();
	const bool dark = scheme == QLatin1String("dark") || (scheme != QLatin1String("light") && system_dark);

	// a scheme the platform did not show (offscreen, KDE's platform theme...):
	// the colors of KDE's Breeze in it where they are, else Fusion's
	if (dark != system_dark) {
		const QString kde_scheme = QStandardPaths::locate(QStandardPaths::GenericDataLocation, dark
			? QStringLiteral("color-schemes/BreezeDark.colors") : QStringLiteral("color-schemes/BreezeLight.colors"));
		// before the palette: KDE's themes read it again when the palette changes
		if (!kde_scheme.isEmpty()) {
			qApp->setProperty(KDE_SCHEME_PATH, kde_scheme);
		}
		QGuiApplication::setPalette(kde_scheme.isEmpty() ? fusionPalette(dark) : kdePalette(kde_scheme, dark));
	}

	QIcon::setThemeName(dark ? QStringLiteral("ezdark") : QStringLiteral("ezbright"));
	if (dark != m_dark) {
		m_dark = dark;
		emit darkChanged();
	}
}

namespace {

constexpr const char* RTP_FOLDERS_KEY = "rtp_folders";
constexpr const char* RTP_DEFAULT_2000_KEY = "rtp_default_2000";
constexpr const char* RTP_DEFAULT_2003_KEY = "rtp_default_2003";
constexpr const char* RTP_PROJECTS_KEY = "rtp_projects";

// The folders of an RTP (EasyRPG Player's RTP tables): those of both
// engines, and those that only RPG Maker 2003's has
constexpr const char* rtp_folders[] = { "backdrop", "battle", "charset", "chipset", "faceset", "gameover",
	"monster", "music", "panorama", "sound", "system", "title" };
constexpr const char* rtp_2003_folders[] = { "battlecharset", "battleweapon", "system2" };

// The charset of the first hero, a name of its own in each translation
struct RtpHero {
	int engine;
	const char* translation;
	const char* name;
};
constexpr RtpHero rtp_heroes[] = {
	{ 2000, "ja", "主人公1" }, { 2000, "en", "actor1" }, { 2000, "dm", "chara1" },
	{ 2003, "ja", "主人公1" }, { 2003, "en", "actor1" }, { 2003, "rpga", "hero1" }, { 2003, "ru", "chara1" },
	{ 2003, "rpgu", "protagonist1" }, { 2003, "ko", "주인공1" }, { 2003, "zh", "主角1" },
};

const char* rtpDefaultKey(int engine) {
	return engine == 2003 ? RTP_DEFAULT_2003_KEY : RTP_DEFAULT_2000_KEY;
}

// The single folder of the Editors before the list, once: its first line,
// the default of the engine it looks like (of both when it looks like
// neither). rtp_path stays, for these Editors.
void moveRtpPath(QSettings& settings) {
	const QString old = settings.value(QLatin1String(RTP_KEY)).toString().trimmed();
	if (old.isEmpty() || settings.contains(QLatin1String(RTP_FOLDERS_KEY))) {
		return;
	}
	const QString folder = QDir::cleanPath(old);
	// the EasyRPG RTP: for both
	const int engine = EditorSettings::isEasyRpgRtp(folder) ? 0 : EditorSettings::rtpEngine(folder);
	settings.setValue(QLatin1String(RTP_FOLDERS_KEY), QStringList{ folder });
	if (engine != 2003) {
		settings.setValue(QLatin1String(RTP_DEFAULT_2000_KEY), folder);
	}
	if (engine != 2000) {
		settings.setValue(QLatin1String(RTP_DEFAULT_2003_KEY), folder);
	}
}

}

QStringList EditorSettings::rtpFolders() const {
	QSettings settings;
	moveRtpPath(settings);
	return settings.value(QLatin1String(RTP_FOLDERS_KEY)).toStringList();
}

void EditorSettings::setRtpFolders(const QStringList& folders) {
	QStringList list;
	for (const QString& folder : folders) {
		const QString clean = folder.trimmed().isEmpty() ? QString() : QDir::cleanPath(folder.trimmed());
		if (!clean.isEmpty() && !list.contains(clean)) {
			list << clean;
		}
	}
	// the games whose folder was taken out use their engine's default again
	QVariantMap projects = value(RTP_PROJECTS_KEY, QVariantMap()).toMap();
	for (auto it = projects.begin(); it != projects.end();) {
		it = list.contains(it.value().toString()) ? std::next(it) : projects.erase(it);
	}
	setValue(RTP_FOLDERS_KEY, list);
	setValue(RTP_PROJECTS_KEY, projects);
}

QString EditorSettings::rtpDefault(int engine) const {
	QSettings settings;
	moveRtpPath(settings);
	return settings.value(QLatin1String(rtpDefaultKey(engine))).toString();
}

void EditorSettings::setRtpDefault(int engine, const QString& folder) {
	setValue(rtpDefaultKey(engine), folder.isEmpty() ? QString() : QDir::cleanPath(folder));
}

QString EditorSettings::rtpActive(const QString& project) const {
	return value(RTP_PROJECTS_KEY, QVariantMap()).toMap().value(projectKey(project)).toString();
}

void EditorSettings::setRtpActive(const QString& project, const QString& folder) {
	QVariantMap projects = value(RTP_PROJECTS_KEY, QVariantMap()).toMap();
	if (folder.isEmpty()) {
		projects.remove(projectKey(project));
	} else {
		projects.insert(projectKey(project), QDir::cleanPath(folder));
	}
	setValue(RTP_PROJECTS_KEY, projects);
}

QStringList EditorSettings::rtpSearchPath(const QString& project, int engine) const {
	QStringList folders = rtpFolders();
	folders.removeIf([](const QString& folder) { return !QFileInfo(folder).isDir(); });
	// the game's own folder, else its engine's default: missing, the next choice
	QString first;
	if (!project.isEmpty()) {
		first = rtpActive(project);
		if (!folders.contains(first)) {
			first = rtpDefault(engine);
		}
	}
	if (folders.removeOne(first)) {
		folders.prepend(first);
	}
	return folders;
}

QString EditorSettings::projectKey(const QString& dir) {
	const QFileInfo info(dir);
	const QString canonical = info.canonicalFilePath();
	return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}

QStringList EditorSettings::rtpKeys() {
	return { QStringLiteral(RTP_KEY), QLatin1String(RTP_FOLDERS_KEY), QLatin1String(RTP_DEFAULT_2000_KEY),
		QLatin1String(RTP_DEFAULT_2003_KEY), QLatin1String(RTP_PROJECTS_KEY) };
}

int EditorSettings::rtpEngine(const QString& folder) {
	const QDir dir(folder);
	if (folder.isEmpty() || !dir.exists()) {
		return 0;
	}
	QSet<QString> names;
	for (const QString& name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		names.insert(name.toLower());
	}
	const auto has = [&names](const char* name) { return names.contains(QLatin1String(name)); };
	// most folders of an RTP: not a folder of a few extra files
	if (std::count_if(std::begin(rtp_folders), std::end(rtp_folders), has) < 6) {
		return 0;
	}
	return std::any_of(std::begin(rtp_2003_folders), std::end(rtp_2003_folders), has) ? 2003 : 2000;
}

bool EditorSettings::isEasyRpgRtp(const QString& folder) {
	// README.md in the repository, README or README.txt in its archives
	for (const QFileInfo& file : QDir(folder).entryInfoList({ QStringLiteral("README*") }, QDir::Files)) {
		QFile readme(file.filePath());
		if (readme.open(QIODevice::ReadOnly) && readme.read(512).contains("EasyRPG RTP")) {
			return true;
		}
	}
	return false;
}

QString EditorSettings::rtpTranslation(const QString& folder, int engine) {
	QSet<QString> names;
	for (const QFileInfo& dir : QDir(folder).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (dir.fileName().compare(QLatin1String(CHARSET), Qt::CaseInsensitive) == 0) {
			for (const QFileInfo& file : QDir(dir.filePath()).entryInfoList(QDir::Files)) {
				// the names of the file system (decomposed Hangul on macOS) as written here
				names.insert(file.completeBaseName().normalized(QString::NormalizationForm_C).toLower());
			}
		}
	}
	for (const RtpHero& hero : rtp_heroes) {
		if (hero.engine == engine && names.contains(QString::fromUtf8(hero.name))) {
			return QLatin1String(hero.translation);
		}
	}
	return QString();
}

QString EditorSettings::playerPath() const { return value(PLAYER_PATH_KEY, QString()).toString(); }
void EditorSettings::setPlayerPath(const QString& path) { setValue(PLAYER_PATH_KEY, path); }

namespace {
const char kScriptEditorKey[] = "script_editor";
}

QString EditorSettings::defaultScriptEditor() { return QStringLiteral("vscode://file/{file}:{line}"); }
QString EditorSettings::scriptEditor() const { return value(kScriptEditorKey, defaultScriptEditor()).toString(); }
void EditorSettings::setScriptEditor(const QString& editor) { setValue(kScriptEditorKey, editor); }

bool EditorSettings::reloadExternalChanges() const { return value(RELOAD_EXTERNAL_KEY, false).toBool(); }
void EditorSettings::setReloadExternalChanges(bool on) { setValue(RELOAD_EXTERNAL_KEY, on); }

bool EditorSettings::maniacMode() const { return value(MANIAC_MODE_KEY, false).toBool(); }
void EditorSettings::setManiacMode(bool on) { setValue(MANIAC_MODE_KEY, on); }

bool EditorSettings::animateTiles() const { return value(ANIMATE_TILES_KEY, true).toBool(); }
void EditorSettings::setAnimateTiles(bool on) { setValue(ANIMATE_TILES_KEY, on); }

bool EditorSettings::commandColors() const { return value(COMMAND_COLORS_KEY, true).toBool(); }
void EditorSettings::setCommandColors(bool on) { setValue(COMMAND_COLORS_KEY, on); }

bool EditorSettings::commandStripes() const { return value(COMMAND_STRIPES_KEY, true).toBool(); }
void EditorSettings::setCommandStripes(bool on) { setValue(COMMAND_STRIPES_KEY, on); }

bool EditorSettings::searchCache() const { return value(SEARCH_CACHE_KEY, false).toBool(); }
void EditorSettings::setSearchCache(bool on) { setValue(SEARCH_CACHE_KEY, on); }

bool EditorSettings::checkForUpdates() const { return value(CHECK_UPDATES_KEY, true).toBool(); }
void EditorSettings::setCheckForUpdates(bool on) { setValue(CHECK_UPDATES_KEY, on); }

void EditorSettings::resetLocalState() {
	QCoreApplication::instance()->setProperty("local_state_resetting", true);
	QCoreApplication::instance()->setProperty("local_state_revision", QCoreApplication::instance()->property("local_state_revision").toInt() + 1);
	QSettings settings;
	for (const QString& key : {QStringLiteral("window_states"), QStringLiteral("main_window_layout"), QStringLiteral("main_window_dock_layout"),
		QStringLiteral("dialog_sizes"), QStringLiteral("project_workspace"),
		QStringLiteral("database_views"), QStringLiteral("event_views"), QStringLiteral("main_window_geometry"),
		QStringLiteral("database_page"), QStringLiteral("event_dialog_geometry"), QStringLiteral("event_page_settings_width"),
		QStringLiteral("jukebox_geometry"), QStringLiteral("jukebox_tab"), QStringLiteral("resource_manager/geometry"),
		QStringLiteral("resource_manager/folder"), QStringLiteral("resource_manager/import_dir"),
		QStringLiteral("resource_manager/export_dir"), QStringLiteral("current_project"), QStringLiteral("default_dir")})
		settings.remove(key);
	settings.sync();
	emit localStateReset();
	QCoreApplication::instance()->setProperty("local_state_resetting", false);
}
