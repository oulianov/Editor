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

#include "project_settings.h"
#include "core.h"
#include "common/exfont.h"
#include "common/game_font.h"
#include "common/rpg_rt_exe.h"
#include "common/script_link.h"
#include "controller/dialog_service.h"
#include "model/game_patches.h"
#include "model/project.h"
#include "model/project_convert.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QVariantMap>
#include <algorithm>
#include <lcf/rpg/system.h>
#include <utility>
#include <vector>

namespace {

/** The code pages of RPG Maker games, as EasyRPG Player reads them */
const std::vector<std::pair<int, const char*>>& codePages() {
	static const std::vector<std::pair<int, const char*>> pages = {
		{1252, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Western European (English, French, German...)")},
		{1250, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Central European (Polish, Czech...)")},
		{1251, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Cyrillic (Russian...)")},
		{1253, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Greek")},
		{1254, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Turkish")},
		{1255, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Hebrew")},
		{1256, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Arabic")},
		{1257, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Baltic")},
		{1258, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Vietnamese")},
		{874, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Thai")},
		{932, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Japanese (Shift-JIS)")},
		{936, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Simplified Chinese (GBK)")},
		{949, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Korean")},
		{950, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Traditional Chinese (Big5)")},
	};
	return pages;
}

bool knownCodePage(int page) {
	return std::any_of(codePages().begin(), codePages().end(), [page](const auto& known) { return known.first == page; });
}

QString codePageName(int number, const char* name) {
	return QStringLiteral("%1 - %2").arg(number).arg(QCoreApplication::translate("ProjectSettingsDialog", name));
}

QVariantMap choice(const QVariant& value, const QString& name) {
	return {{QStringLiteral("value"), value}, {QStringLiteral("name"), name}};
}

/** The extensions of the fonts EasyRPG Player reads (FileFinder FONTS_TYPES) */
const QStringList& fontTypes() {
	static const QStringList types = {QStringLiteral("fon"), QStringLiteral("fnt"), QStringLiteral("bdf"),
		QStringLiteral("ttf"), QStringLiteral("ttc"), QStringLiteral("otf"), QStringLiteral("woff2"), QStringLiteral("woff")};
	return types;
}

/** Font/Font.ttf (font 1) and Font/Font2.ttf (font 2) of the game, as the Player finds them */
QStringList fontFiles(const QDir& game, int font) {
	QDir dir = game;
	QStringList found;
	const QString base = font == 0 ? QStringLiteral("font") : QStringLiteral("font2");
	for (const QString& sub : game.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (sub.compare(QLatin1String("Font"), Qt::CaseInsensitive) != 0) {
			continue;
		}
		dir.cd(sub);
		for (const QFileInfo& file : dir.entryInfoList(QDir::Files)) {
			if (file.completeBaseName().compare(base, Qt::CaseInsensitive) == 0
					&& fontTypes().contains(file.suffix().toLower())) {
				found << file.absoluteFilePath();
			}
		}
		break;
	}
	return found;
}

bool validFont(int font) {
	return font == 0 || font == 1;
}

} // namespace

ProjectSettings::ProjectSettings(Project& project, bool open, QObject* parent) :
	QObject(parent), m_project(project), m_open(open), m_folder(project.projectDir()), m_folderName(m_folder.dirName()),
	m_initialTitle(project.gameTitle()), m_title(m_initialTitle),
	m_encodingEditable(project.projectType() != FileFinder::ProjectType::EasyRpg)
{
	// [EasyRPG] Encoding of RPG_RT.ini: a code page of the list, else as written
	if (m_encodingEditable) {
		m_encodingSetting = project.encodingSetting();
		if (!m_encodingSetting.isEmpty()) {
			const int page = codePageOf(m_encodingSetting);
			m_encoding = knownCodePage(page) ? QString::number(page) : m_encodingSetting;
		}
	}
	m_canConvert = open && ProjectConvert::canConvert(project);
	if (open) {
		m_textFont = project.database().system.font_id;
	}
	const ExFont::Sheet sheet = ExFont::load(project);
	m_symbols = sheet.image;
	m_symbolsSource = sheet.source;

	// the patches as EasyRPG.ini chooses them, else detected
	const GamePatches::State state = GamePatches::read(project);
	m_detected = GamePatches::detect(project);
	m_written = state.chosen ? state.values : m_detected;
	m_chosen = m_written;
	m_others = state.others;
	m_patchMode = !state.chosen ? Detect : state.others.isEmpty() ? Choose : Refer;
	readIni();
}

QString ProjectSettings::folderName() const
{
	return m_folderName;
}

void ProjectSettings::setFolderName(const QString& name)
{
	if (name == m_folderName) {
		return;
	}
	m_folderName = name;
	emit folderNameChanged();
}

bool ProjectSettings::renamesFolder() const
{
	return m_folderName.trimmed() != m_folder.dirName();
}

QString ProjectSettings::renameFolder() const
{
	const QString name = m_folderName.trimmed();
	QDir parent = m_folder;
	parent.cdUp();
	return parent.rename(m_folder.dirName(), name) ? parent.filePath(name) : QString();
}

QString ProjectSettings::gameTitle() const
{
	return m_title;
}

void ProjectSettings::setGameTitle(const QString& title)
{
	if (title == m_title) {
		return;
	}
	m_title = title;
	emit gameTitleChanged();
}

QString ProjectSettings::encoding() const
{
	return m_encoding;
}

void ProjectSettings::setEncoding(const QString& encoding)
{
	if (!m_encodingEditable || encoding == m_encoding) {
		return;
	}
	m_encoding = encoding;
	emit encodingChanged();
}

bool ProjectSettings::encodingModified() const
{
	return m_encodingEditable && m_encoding != m_encodingSetting;
}

QVariantList ProjectSettings::encodingChoices() const
{
	if (!m_encodingEditable) {
		return {};
	}
	QVariantList choices;
	// detected: what the Editor found reading the game
	const QString found = m_encodingSetting.isEmpty() ? m_project.encoding() : QString();
	choices << choice(QString(), found.isEmpty() ? QCoreApplication::translate("ProjectSettingsDialog", "Detect automatically")
		: QCoreApplication::translate("ProjectSettingsDialog", "Detect automatically (%1)").arg(describeEncoding(found)));
	for (const auto& [number, name] : codePages()) {
		choices << choice(QString::number(number), codePageName(number, name));
	}
	if (!m_encodingSetting.isEmpty() && !knownCodePage(codePageOf(m_encodingSetting))) {
		// as written, though the Player may not read it
		choices << choice(m_encodingSetting, m_encodingSetting);
	}
	return choices;
}

QString ProjectSettings::engineName() const
{
	if (!m_open) {
		return {};
	}
	if (m_project.projectType() == FileFinder::ProjectType::EasyRpg) {
		return QCoreApplication::translate("ProjectSettingsDialog", "EasyRPG (XML files)");
	}
	return m_project.database().system.ldb_id == 2003 ? QCoreApplication::translate("ProjectSettingsDialog", "RPG Maker 2003")
		: QCoreApplication::translate("ProjectSettingsDialog", "RPG Maker 2000");
}

void ProjectSettings::setConvertRequested(bool convert)
{
	if (convert == m_convert) {
		return;
	}
	m_convert = convert;
	emit convertRequestedChanged();
}

void ProjectSettings::setTextFont(int font)
{
	if (font == m_textFont) {
		return;
	}
	m_textFont = font;
	emit textFontChanged();
}

QVariantList ProjectSettings::textFontChoices() const
{
	return {
		choice(static_cast<int>(lcf::rpg::System::Font_gothic), QCoreApplication::translate("ProjectSettingsDialog", "Font 1 (MS Gothic)")),
		choice(static_cast<int>(lcf::rpg::System::Font_mincho), QCoreApplication::translate("ProjectSettingsDialog", "Font 2 (MS Mincho)")),
	};
}

QImage ProjectSettings::fontPreview(const QColor& color) const
{
	// the font chosen, as the Player draws it, its pixels doubled
	const int font = m_textFont == lcf::rpg::System::Font_mincho ? 1 : 0;
	const QImage text = GameFont::render(QCoreApplication::translate("ProjectSettingsDialog", "The quick brown fox jumps over the lazy dog."),
		font, playerFont(font), cjkGame(), color);
	return text.scaled(text.size() * 4, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

QString ProjectSettings::symbolsSource() const
{
	return m_symbolsSource == QLatin1String("EasyRPG") ? QCoreApplication::translate("ProjectSettingsDialog", "EasyRPG Player's")
		: m_symbolsSource;
}

QImage ProjectSettings::symbolsImage(const QColor& color) const
{
	// the glyphs as messages show them: $A to $Z, then $a to $z, crisp at twice the pixels
	const int columns = ExFont::Count / 2;
	QImage preview(columns * ExFont::GlyphSize * 2, 2 * ExFont::GlyphSize * 2, QImage::Format_ARGB32_Premultiplied);
	preview.fill(Qt::transparent);
	QPainter painter(&preview);
	for (int index = 0; index < ExFont::Count; ++index) {
		const QImage glyph = ExFont::glyph(m_symbols, index, color);
		painter.drawImage(QRect((index % columns) * ExFont::GlyphSize * 2, (index / columns) * ExFont::GlyphSize * 2,
			ExFont::GlyphSize * 2, ExFont::GlyphSize * 2), glyph);
	}
	painter.end();
	return preview;
}

void ProjectSettings::setPatchMode(PatchMode mode)
{
	if (mode == m_patchMode) {
		return;
	}
	m_patchMode = mode;
	emit patchModeChanged();
	emit patchesChanged();
}

QVariantList ProjectSettings::patchModeChoices() const
{
	auto mode = [](PatchMode value, const QString& name, bool enabled) {
		QVariantMap item = choice(static_cast<int>(value), name);
		item.insert(QStringLiteral("enabled"), enabled);
		return item;
	};
	return {
		mode(Detect, QCoreApplication::translate("ProjectSettingsDialog", "Detect from the game files"), detectAllowed()),
		mode(Choose, QCoreApplication::translate("ProjectSettingsDialog", "Choose"), true),
		mode(Refer, QCoreApplication::translate("ProjectSettingsDialog", "As written in EasyRPG.ini"), true),
	};
}

QVariantList ProjectSettings::patches() const
{
	QVariantList list;
	for (const auto& patch : GamePatches::list()) {
		list << QVariantMap{
			{QStringLiteral("key"), QString::fromLatin1(patch.key)},
			{QStringLiteral("name"), QCoreApplication::translate("GamePatches", patch.name)},
			{QStringLiteral("explanation"), GamePatches::explain(patch)},
		};
	}
	return list;
}

const QMap<QString, int>& ProjectSettings::patchValues() const
{
	// detected: what the files tell; as written: what EasyRPG.ini says; chosen: the boxes
	return m_patchMode == Detect ? m_detected : m_patchMode == Choose ? m_chosen : m_written;
}

bool ProjectSettings::patchOn(const QString& key) const
{
	return patchValues().value(key) > 0;
}

void ProjectSettings::setPatchOn(const QString& key, bool on)
{
	if (m_patchMode != Choose) {
		return;
	}
	// the Maniac Patch keeps its version
	const int was = m_chosen.value(key);
	m_chosen[key] = on ? std::max(was, 1) : 0;
	if (m_chosen[key] != was) {
		emit patchesChanged();
	}
}

QString ProjectSettings::patchExplanation(const QString& key) const
{
	for (const auto& patch : GamePatches::list()) {
		if (key == QLatin1String(patch.key)) {
			return GamePatches::explain(patch);
		}
	}
	return {};
}

QString ProjectSettings::playerFont(int font) const
{
	if (!validFont(font)) {
		return {};
	}
	if (m_fontChoice[font]) {
		return *m_fontChoice[font];
	}
	return fontFiles(m_folder, font).value(0);
}

QString ProjectSettings::playerFontText(int font) const
{
	if (!validFont(font)) {
		return {};
	}
	const QString file = playerFont(font);
	if (file.isEmpty()) {
		const bool cjk = cjkGame();
		return QCoreApplication::translate("ProjectSettingsDialog", "Built-in (%1)").arg(cjk
			? (font == 0 ? QStringLiteral("Shinonome Gothic") : QStringLiteral("Shinonome Mincho"))
			: (font == 0 ? QStringLiteral("ttyp0") : QStringLiteral("RMG2000")));
	}
	if (m_fontChoice[font]) {
		return QCoreApplication::translate("ProjectSettingsDialog", "%1 (copied on OK)").arg(QFileInfo(file).fileName());
	}
	return m_folder.relativeFilePath(file);
}

void ProjectSettings::choosePlayerFont(int font, const QString& file)
{
	if (!validFont(font) || file.isEmpty()) {
		return;
	}
	m_fontChoice[font] = file;
	emit playerFontsChanged();
}

void ProjectSettings::useBuiltinPlayerFont(int font)
{
	if (!validFont(font)) {
		return;
	}
	m_fontChoice[font] = QString();
	emit playerFontsChanged();
}

QStringList ProjectSettings::fontPatterns()
{
	QStringList patterns;
	for (const QString& type : fontTypes()) {
		patterns << QStringLiteral("*.") + type;
	}
	return patterns;
}

bool ProjectSettings::cjkGame() const
{
	// Font::DefaultBitmapFont: by the code page of the game
	const int page = codePageOf(m_project.encoding());
	return page == 932 || page == 936 || page == 949 || page == 950;
}

void ProjectSettings::readIni()
{
	// the engine EasyRPG Player is told, else the one it reads in the program
	const QString exe = GamePatches::engineExe(m_project);
	const QString exe_setting = GamePatches::enginePathSetting(m_project);
	const RpgRtExe::Info info = exe.isEmpty() ? RpgRtExe::Info() : GamePatches::exeInfo(exe);
	// the players in the folder of the game: RPG_RT.exe (RPG Maker's, of which version, or
	// EasyRPG Player renamed), and EasyRPG Player under its own name
	auto version = [](QString text) {
		while (text.endsWith(QLatin1String(".0")) && text.count(QLatin1Char('.')) > 1) {
			text.chop(2);
		}
		return text;
	};
	auto player = [](const QString& number) {
		return number.isEmpty() ? QCoreApplication::translate("ProjectSettingsDialog", "EasyRPG Player")
			: QCoreApplication::translate("ProjectSettingsDialog", "EasyRPG Player %1").arg(number);
	};
	auto describe = [&](const RpgRtExe::Info& info) {
		QString text;
		if (info.easyrpgPlayer) {
			text = player(version(info.version));
		} else if (!info.program || !info.engine) {
			text = QCoreApplication::translate("ProjectSettingsDialog", "unknown version");
		} else if (info.version.isEmpty()) {
			text = RpgRtExe::engineName(info.engine);
		} else {
			text = QStringLiteral("%1 %2").arg(info.engine & RpgRtExe::EngineRpg2k3
				? QCoreApplication::translate("ProjectSettingsDialog", "RPG Maker 2003")
				: QCoreApplication::translate("ProjectSettingsDialog", "RPG Maker 2000"), info.version);
		}
		if (info.maniac) {
			text += QStringLiteral(", ") + (info.maniac > 1 ? QCoreApplication::translate("ProjectSettingsDialog", "Maniac Patch %1").arg(info.maniac)
				: QCoreApplication::translate("ProjectSettingsDialog", "Maniac Patch"));
		}
		return text;
	};
	QStringList players;
	if (!exe.isEmpty()) {
		players << QStringLiteral("%1: %2").arg(QFileInfo(exe).fileName(), describe(info));
	} else if (!exe_setting.isEmpty()) {
		players << QCoreApplication::translate("ProjectSettingsDialog", "%1: not found").arg(exe_setting);
	}
	for (const QFileInfo& file : m_folder.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
		const QString name = file.fileName();
		if (file.absoluteFilePath() == QFileInfo(exe).absoluteFilePath()) {
			continue;
		}
		if (file.isFile() && file.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0 && file.size() < 64 * 1024 * 1024) {
			const RpgRtExe::Info other = GamePatches::exeInfo(file.absoluteFilePath());
			if (other.easyrpgPlayer) {
				players << QStringLiteral("%1: %2").arg(name, describe(other));
			}
		} else if (file.isDir() && file.suffix() == QLatin1String("app") && name.contains(QLatin1String("EasyRPG"), Qt::CaseInsensitive)) {
			// macOS: the version of its bundle
			const QSettings plist(file.absoluteFilePath() + QStringLiteral("/Contents/Info.plist"), QSettings::NativeFormat);
			players << QStringLiteral("%1: %2").arg(name, player(plist.value(QStringLiteral("CFBundleShortVersionString")).toString()));
		} else if (file.isFile() && name.compare(QLatin1String("easyrpg-player"), Qt::CaseInsensitive) == 0) {
			players << QStringLiteral("%1: %2").arg(name, player(QString()));
		}
	}
	m_runtime = players.isEmpty() ? QCoreApplication::translate("ProjectSettingsDialog", "None in the game folder")
		: players.join(QLatin1Char('\n'));
	m_exe = exe;

	const QStringList names = exe.isEmpty() || info.easyrpgPlayer ? QStringList() : RpgRtExe::fontNames(exe);
	m_exeFonts = names.isEmpty() ? QCoreApplication::translate("ProjectSettingsDialog", "Unknown") : names.join(QStringLiteral(", "));

	// the patches as EasyRPG.ini says now
	const GamePatches::State state = GamePatches::read(m_project);
	m_detected = GamePatches::detect(m_project);
	m_written = state.chosen ? state.values : m_detected;
	m_others = state.others;
	emit iniChanged();
	if (!m_others.isEmpty()) {
		// any key of [Patch] stops the detection of the Player
		if (m_patchMode == Detect) {
			setPatchMode(Refer);
		}
	}
	emit patchesChanged();
}

bool ProjectSettings::openIni(DialogService& dialogs)
{
	const QString title = QCoreApplication::translate("ProjectSettingsDialog", "Project Settings");
	const QString path = m_project.findFileOrDefault(QStringLiteral("EasyRPG.ini"));
	if (!QFileInfo::exists(path)) {
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly) || file.write("[Game]\r\n\r\n[Patch]\r\n") < 0) {
			dialogs.warning(title, QCoreApplication::translate("ProjectSettingsDialog", "%1 could not be created.")
				.arg(QDir::toNativeSeparators(path)));
			return false;
		}
	}
	const bool opened = ScriptLink::openAt(path, 1);
	if (!opened) {
		dialogs.warning(title, QCoreApplication::translate("ProjectSettingsDialog", "The script editor could not open %1.")
			.arg(QDir::toNativeSeparators(path)));
	}
	if (m_patchMode == Detect) {
		// edited by hand from now on
		setPatchMode(Refer);
	}
	return opened;
}

bool ProjectSettings::openIni()
{
	return openIni(DialogService::instance());
}

QString ProjectSettings::validate() const
{
	const QString name = m_folderName.trimmed();
	// a folder name usable on every system, and not taken
	static const QRegularExpression invalid(QStringLiteral(R"([<>:"/\\|?*]|^\.+$)"));
	if (name.isEmpty() || name.contains(invalid)) {
		return QCoreApplication::translate("ProjectSettingsDialog", "\"%1\" is not a valid folder name.").arg(name);
	}
	if (name != m_folder.dirName()) {
		QDir parent = m_folder;
		parent.cdUp();
		if (parent.exists(name)) {
			return QCoreApplication::translate("ProjectSettingsDialog", "The folder %1 already exists. Choose another folder name.").arg(name);
		}
	}
	return {};
}

QString ProjectSettings::write(Project& project) const
{
	// the font of the game: in the database
	if (m_open && m_textFont != project.database().system.font_id) {
		// another program (rpgsync...) changed the database since it was
		// read: the font goes in its version, never over it. Outside the
		// Database window the Editor has no unsaved database edits to lose.
		if (core().watcher().isStale(project.databaseFile()) && !project.reloadDatabase()) {
			return QCoreApplication::translate("ProjectSettingsDialog", "The database changed on disk and could not be read again.");
		}
		project.database().system.font_id = m_textFont;
		if (!project.saveDatabase()) {
			return QCoreApplication::translate("ProjectSettingsDialog", "The database could not be saved.");
		}
	}
	// the code page first: the title is written in it
	if (encodingModified() && !project.saveEncodingSetting(m_encoding)) {
		return QCoreApplication::translate("ProjectSettingsDialog", "The encoding could not be written in RPG_RT.ini.");
	}
	// refer: EasyRPG.ini as it is
	const GamePatches::State state = GamePatches::read(project);
	const bool patches_changed = m_patchMode == Choose ? (!state.chosen || state.values != m_chosen)
		: m_patchMode == Detect && state.chosen;
	if (patches_changed && !GamePatches::write(project, m_patchMode == Choose ? std::optional(m_chosen) : std::nullopt)) {
		return QCoreApplication::translate("ProjectSettingsDialog", "The patches could not be written in EasyRPG.ini.");
	}
	// the fonts of the Player: the old file to the trash, the new one copied
	for (int font = 0; font < 2; ++font) {
		if (!m_fontChoice[font]) {
			continue;
		}
		const QString chosen = m_fontChoice[font]->isEmpty() ? QString() : QFileInfo(*m_fontChoice[font]).absoluteFilePath();
		const QStringList old = fontFiles(project.projectDir(), font);
		if (old.size() == 1 && old.front() == chosen) {
			continue;
		}
		for (const QString& file : old) {
			if (file != chosen && !QFile::moveToTrash(file)) {
				return QCoreApplication::translate("ProjectSettingsDialog", "%1 could not be moved to the trash.").arg(QDir::toNativeSeparators(file));
			}
		}
		if (chosen.isEmpty()) {
			continue;
		}
		QDir game = project.projectDir();
		QString dir = QStringLiteral("Font");
		for (const QString& sub : game.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			if (sub.compare(dir, Qt::CaseInsensitive) == 0) {
				dir = sub;
			}
		}
		const QString target = game.filePath(QStringLiteral("%1/%2.%3").arg(dir,
			font == 0 ? QStringLiteral("Font") : QStringLiteral("Font2"), QFileInfo(chosen).suffix().toLower()));
		if (target == chosen) {
			continue;
		}
		if (!game.mkpath(dir) || !QFile::copy(chosen, target)) {
			return QCoreApplication::translate("ProjectSettingsDialog", "The font could not be copied in %1.").arg(QDir::toNativeSeparators(game.filePath(dir)));
		}
	}
	// as typed: an encoding changed reads the title again, unless it was typed
	if (m_title != m_initialTitle && !project.saveGameTitle(m_title)) {
		return QCoreApplication::translate("ProjectSettingsDialog", "The game title could not be written.");
	}
	return {};
}

QString ProjectSettings::apply() const
{
	return write(m_project);
}

int ProjectSettings::codePageOf(const QString& encoding)
{
	bool ok = false;
	const int number = encoding.trimmed().toInt(&ok);
	if (ok) {
		return number;
	}
	// liblcf's names (ReaderUtil::CodepageToEncoding, DetectEncodings)
	if (encoding.startsWith(QLatin1String("ibm-943"))) {
		return 932;
	}
	if (encoding.startsWith(QLatin1String("ibm-5348"))) {
		return 1252;
	}
	static const QRegularExpression windows(QStringLiteral("^windows-(\\d+)"));
	const auto match = windows.match(encoding);
	return match.hasMatch() ? match.captured(1).toInt() : 0;
}

QString ProjectSettings::describeEncoding(const QString& encoding)
{
	const int page = codePageOf(encoding);
	for (const auto& [number, name] : codePages()) {
		if (number == page) {
			return codePageName(number, name);
		}
	}
	return encoding;
}
