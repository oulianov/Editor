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

// Game Project > Project Settings without its window (ProjectSettings):
// what it reads of TestGame-2003 (EDITOR_TEST_PROJECT, skipped without it),
// the choices it offers, the folder names it accepts, and what it writes in
// a copy of the files of the game: the title, the code page, the font of
// the database, the patches of EasyRPG.ini and a font of the Player. The
// window itself is left to tests/scripts/project_settings.txt; the font
// files sent to the trash are not tested (it is the user's trash).

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <lcf/rpg/system.h>
#include "model/game_patches.h"
#include "model/project.h"
#include "model/project_settings.h"

namespace {

QString canonical(const QString& path)
{
	return QFileInfo(path).canonicalFilePath();
}

QVariantMap at(const QVariantList& list, int i)
{
	return list.value(i).toMap();
}

} // namespace

class ProjectSettingsTest : public QObject
{
	Q_OBJECT

private slots:
	void init();
	void cleanup();
	void readsTheGame();
	void choices();
	void codePages();
	void properties();
	void validate();
	void renameFolder();
	void titleAndEncoding();
	void encodingNotListed();
	void textFont();
	void patches();
	void playerFonts();
	void closedProject();

private:
	/** The settings of the copy of the game, as the Editor opens them */
	std::unique_ptr<ProjectSettings> settings(bool open = true);
	/** The copy as its files are now */
	std::shared_ptr<Project> reload() const;

	std::unique_ptr<QTemporaryDir> m_dir;
	QString m_game;
	std::shared_ptr<Project> m_project;
};

void ProjectSettingsTest::init()
{
	const QString source = qEnvironmentVariable("EDITOR_TEST_PROJECT");
	if (source.isEmpty()) {
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	}
	m_dir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_dir->isValid());
	// the files the settings read and write, in a folder of its own (it is renamed)
	QDir dir(m_dir->path());
	QVERIFY(dir.mkdir(QStringLiteral("Game")));
	m_game = dir.filePath(QStringLiteral("Game"));
	for (const char* name : {"RPG_RT.ini", "EasyRPG.ini", "RPG_RT.ldb", "RPG_RT.lmt", "RPG_RT.exe"}) {
		const QString target = QDir(m_game).filePath(QString::fromLatin1(name));
		QVERIFY2(QFile::copy(QDir(source).filePath(QString::fromLatin1(name)), target), name);
		QFile::setPermissions(target, QFile::permissions(target) | QFile::WriteOwner);
	}
	m_project = Project::load(m_game);
	QVERIFY(m_project);
	QVERIFY(m_project->loadDatabaseAndMapTree());
}

void ProjectSettingsTest::cleanup()
{
	m_project.reset();
	m_dir.reset();
}

std::unique_ptr<ProjectSettings> ProjectSettingsTest::settings(bool open)
{
	return std::make_unique<ProjectSettings>(*m_project, open);
}

std::shared_ptr<Project> ProjectSettingsTest::reload() const
{
	return Project::load(m_game);
}

void ProjectSettingsTest::readsTheGame()
{
	const auto s = settings();
	QVERIFY(s->isOpen());
	QCOMPARE(s->folderName(), QStringLiteral("Game"));
	QVERIFY(!s->renamesFolder());
	QCOMPARE(s->gameTitle(), QStringLiteral("RPG Maker 2003 Test suite"));
	// no [EasyRPG] Encoding: detected
	QVERIFY(s->encodingEditable());
	QCOMPARE(s->encoding(), QString());
	QVERIFY(!s->encodingModified());
	QCOMPARE(s->engineName(), QStringLiteral("RPG Maker 2003"));
	QVERIFY(!s->canConvert());
	QVERIFY(!s->convertRequested());
	// its RPG_RT.exe is a stub
	QVERIFY2(s->runtime().contains(QStringLiteral("RPG_RT.exe: unknown version")), qPrintable(s->runtime()));
	QCOMPARE(canonical(s->runtimeExe()), canonical(QDir(m_game).filePath(QStringLiteral("RPG_RT.exe"))));
	QCOMPARE(s->exeFonts(), QStringLiteral("Unknown"));
	QCOMPARE(s->textFont(), m_project->database().system.font_id);
	QVERIFY(!s->symbolsSource().isEmpty());
	// $A to $Z, then $a to $z, at twice the pixels
	QCOMPARE(s->symbolsImage(Qt::black).size(), QSize(26 * 24, 2 * 24));
	QVERIFY(!s->fontPreview(Qt::black).isNull());

	// EasyRPG.ini chooses patches, with keys the Editor does not show: as written
	QCOMPARE(s->patchMode(), ProjectSettings::Refer);
	QVERIFY(!s->detectAllowed());
	QVERIFY(s->otherPatches().contains(QStringLiteral("AntiLagSwitch")));
	QVERIFY(s->otherPatches().contains(QStringLiteral("DirectMenu")));
	QVERIFY(s->patchOn(QStringLiteral("DynRPG")));
	QVERIFY(s->patchOn(QStringLiteral("KeyPatch")));
	QVERIFY(!s->patchOn(QStringLiteral("Maniac")));
	QVERIFY(!s->patchOn(QStringLiteral("NoSuchPatch")));

	// no Font folder: the Player's own fonts, those of western code pages
	QCOMPARE(s->playerFont(0), QString());
	QVERIFY(!s->cjkGame());
	QCOMPARE(s->playerFontText(0), QStringLiteral("Built-in (ttyp0)"));
	QCOMPARE(s->playerFontText(1), QStringLiteral("Built-in (RMG2000)"));
	QCOMPARE(s->playerFontText(2), QString());

	// nothing changed: nothing to write
	QCOMPARE(s->validate(), QString());
	QFile ini(QDir(m_game).filePath(QStringLiteral("EasyRPG.ini")));
	QVERIFY(ini.open(QIODevice::ReadOnly));
	const QByteArray before = ini.readAll();
	ini.close();
	QCOMPARE(s->apply(), QString());
	QVERIFY(ini.open(QIODevice::ReadOnly));
	QCOMPARE(ini.readAll(), before);
}

void ProjectSettingsTest::choices()
{
	const auto s = settings();
	// detect, then the code pages of RPG Maker games
	const QVariantList encodings = s->encodingChoices();
	QCOMPARE(encodings.size(), 15);
	QCOMPARE(at(encodings, 0).value(QStringLiteral("value")).toString(), QString());
	QVERIFY(at(encodings, 0).value(QStringLiteral("name")).toString().startsWith(QStringLiteral("Detect automatically")));
	QCOMPARE(at(encodings, 1).value(QStringLiteral("value")).toString(), QStringLiteral("1252"));
	QCOMPARE(at(encodings, 1).value(QStringLiteral("name")).toString(), QStringLiteral("1252 - Western European (English, French, German...)"));
	QCOMPARE(at(encodings, 11).value(QStringLiteral("name")).toString(), QStringLiteral("932 - Japanese (Shift-JIS)"));

	const QVariantList fonts = s->textFontChoices();
	QCOMPARE(fonts.size(), 2);
	QCOMPARE(at(fonts, 0).value(QStringLiteral("value")).toInt(), static_cast<int>(lcf::rpg::System::Font_gothic));
	QCOMPARE(at(fonts, 1).value(QStringLiteral("name")).toString(), QStringLiteral("Font 2 (MS Mincho)"));

	// Detect is greyed: keys of [Patch] stop the detection
	const QVariantList modes = s->patchModeChoices();
	QCOMPARE(modes.size(), 3);
	for (int i = 0; i < modes.size(); ++i) {
		QCOMPARE(at(modes, i).value(QStringLiteral("value")).toInt(), i);
	}
	QCOMPARE(at(modes, 0).value(QStringLiteral("enabled")).toBool(), false);
	QCOMPARE(at(modes, 1).value(QStringLiteral("enabled")).toBool(), true);
	QCOMPARE(at(modes, 2).value(QStringLiteral("name")).toString(), QStringLiteral("As written in EasyRPG.ini"));

	const QVariantList patches = s->patches();
	QCOMPARE(patches.size(), static_cast<qsizetype>(GamePatches::list().size()));
	for (const QVariant& item : patches) {
		const QVariantMap patch = item.toMap();
		const QString key = patch.value(QStringLiteral("key")).toString();
		QVERIFY(!key.isEmpty());
		QVERIFY(!patch.value(QStringLiteral("name")).toString().isEmpty());
		QCOMPARE(patch.value(QStringLiteral("explanation")).toString(), s->patchExplanation(key));
		QVERIFY(!s->patchExplanation(key).isEmpty());
	}
	QCOMPARE(s->patchExplanation(QString()), QString());
	QVERIFY(ProjectSettings::fontPatterns().contains(QStringLiteral("*.ttf")));
	QVERIFY(ProjectSettings::fontPatterns().contains(QStringLiteral("*.fon")));
}

void ProjectSettingsTest::codePages()
{
	QCOMPARE(ProjectSettings::codePageOf(QStringLiteral("1252")), 1252);
	QCOMPARE(ProjectSettings::codePageOf(QStringLiteral(" 932 ")), 932);
	QCOMPARE(ProjectSettings::codePageOf(QStringLiteral("windows-1251")), 1251);
	// liblcf's names
	QCOMPARE(ProjectSettings::codePageOf(QStringLiteral("ibm-943_P15A-2003")), 932);
	QCOMPARE(ProjectSettings::codePageOf(QStringLiteral("ibm-5348_P100-1997")), 1252);
	QCOMPARE(ProjectSettings::codePageOf(QStringLiteral("UTF-8")), 0);
	QCOMPARE(ProjectSettings::describeEncoding(QStringLiteral("windows-949")), QStringLiteral("949 - Korean"));
	QCOMPARE(ProjectSettings::describeEncoding(QStringLiteral("UTF-8")), QStringLiteral("UTF-8"));
}

void ProjectSettingsTest::properties()
{
	// what a QML window binds to
	const auto s = settings();
	QCOMPARE(s->property("gameTitle").toString(), QStringLiteral("RPG Maker 2003 Test suite"));
	QCOMPARE(s->property("patchMode").toInt(), static_cast<int>(ProjectSettings::Refer));
	QVERIFY(s->setProperty("patchMode", static_cast<int>(ProjectSettings::Choose)));
	QCOMPARE(s->patchMode(), ProjectSettings::Choose);
	QVERIFY(s->setProperty("folderName", QStringLiteral("Other")));
	QCOMPARE(s->folderName(), QStringLiteral("Other"));
	QVariantList encodings;
	QVERIFY(QMetaObject::invokeMethod(s.get(), "encodingChoices", Q_RETURN_ARG(QVariantList, encodings)));
	QCOMPARE(encodings.size(), 15);
	bool on = false;
	QVERIFY(QMetaObject::invokeMethod(s.get(), "patchOn", Q_RETURN_ARG(bool, on), Q_ARG(QString, QStringLiteral("DynRPG"))));
	QVERIFY(on);

	QSignalSpy convert(s.get(), &ProjectSettings::convertRequestedChanged);
	s->setConvertRequested(true);
	s->setConvertRequested(true);
	QCOMPARE(convert.count(), 1);
	QVERIFY(s->convertRequested());
}

void ProjectSettingsTest::validate()
{
	const auto s = settings();
	QSignalSpy names(s.get(), &ProjectSettings::folderNameChanged);
	// spaces around are not part of it
	s->setFolderName(QStringLiteral("  Game "));
	QCOMPARE(names.count(), 1);
	QCOMPARE(s->validate(), QString());
	QVERIFY(!s->renamesFolder());
	// usable on every system
	for (const char* name : {"", "   ", "a/b", "a\\b", "a:b", "what?", "a*", "<a>", "a|b", "\"a\"", ".", ".."}) {
		s->setFolderName(QString::fromLatin1(name));
		QVERIFY2(!s->validate().isEmpty(), name);
	}
	// not taken
	QVERIFY(QDir(m_dir->path()).mkdir(QStringLiteral("Taken")));
	s->setFolderName(QStringLiteral("Taken"));
	QCOMPARE(s->validate(), QStringLiteral("The folder Taken already exists. Choose another folder name."));
	s->setFolderName(QStringLiteral("My Game 2"));
	QCOMPARE(s->validate(), QString());
	QVERIFY(s->renamesFolder());
	const int count = names.count();
	s->setFolderName(QStringLiteral("My Game 2"));
	QCOMPARE(names.count(), count);
}

void ProjectSettingsTest::renameFolder()
{
	const auto s = settings();
	// taken: not renamed
	QDir dir(m_dir->path());
	QVERIFY(dir.mkpath(QStringLiteral("Taken/Inside")));
	s->setFolderName(QStringLiteral("Taken"));
	QCOMPARE(s->renameFolder(), QString());
	QVERIFY(QFileInfo::exists(QDir(m_game).filePath(QStringLiteral("RPG_RT.ldb"))));

	s->setFolderName(QStringLiteral(" Renamed "));
	QVERIFY(s->renamesFolder());
	const QString path = s->renameFolder();
	QCOMPARE(path, dir.filePath(QStringLiteral("Renamed")));
	QVERIFY(QFileInfo::exists(QDir(path).filePath(QStringLiteral("RPG_RT.ldb"))));
	QVERIFY(!QFileInfo::exists(m_game));
}

void ProjectSettingsTest::titleAndEncoding()
{
	const auto s = settings();
	QSignalSpy titles(s.get(), &ProjectSettings::gameTitleChanged);
	QSignalSpy encodings(s.get(), &ProjectSettings::encodingChanged);
	s->setGameTitle(QStringLiteral("Écrit en 1252"));
	s->setEncoding(QStringLiteral("1252"));
	s->setEncoding(QStringLiteral("1252"));
	QCOMPARE(titles.count(), 1);
	QCOMPARE(encodings.count(), 1);
	QVERIFY(s->encodingModified());
	QCOMPARE(s->apply(), QString());
	// the title in the code page written
	auto read = reload();
	QCOMPARE(read->encodingSetting(), QStringLiteral("1252"));
	QCOMPARE(read->gameTitle(), QStringLiteral("Écrit en 1252"));

	// the settings of the game as written
	const auto again = settings();
	QCOMPARE(again->encoding(), QStringLiteral("1252"));
	QVERIFY(!again->encodingModified());
	QVERIFY(at(again->encodingChoices(), 0).value(QStringLiteral("name")).toString() == QStringLiteral("Detect automatically"));
	// detected again
	again->setEncoding(QString());
	QVERIFY(again->encodingModified());
	QCOMPARE(again->apply(), QString());
	read = reload();
	QCOMPARE(read->encodingSetting(), QString());
	QCOMPARE(read->gameTitle(), QStringLiteral("Écrit en 1252"));
}

void ProjectSettingsTest::encodingNotListed()
{
	// as written, though the Player may not read it
	QVERIFY(m_project->saveEncodingSetting(QStringLiteral("65001")));
	const auto s = settings();
	QCOMPARE(s->encoding(), QStringLiteral("65001"));
	QVERIFY(!s->encodingModified());
	const QVariantList encodings = s->encodingChoices();
	QCOMPARE(encodings.size(), 16);
	QCOMPARE(at(encodings, 15).value(QStringLiteral("value")).toString(), QStringLiteral("65001"));
	QCOMPARE(at(encodings, 15).value(QStringLiteral("name")).toString(), QStringLiteral("65001"));
	// a name of liblcf: the code page of the list
	QVERIFY(m_project->saveEncodingSetting(QStringLiteral("windows-1251")));
	const auto named = settings();
	QCOMPARE(named->encoding(), QStringLiteral("1251"));
	QCOMPARE(named->encodingChoices().size(), 15);
}

void ProjectSettingsTest::textFont()
{
	const auto s = settings();
	const int font = m_project->database().system.font_id;
	const int other = font == lcf::rpg::System::Font_mincho ? lcf::rpg::System::Font_gothic : lcf::rpg::System::Font_mincho;
	QSignalSpy fonts(s.get(), &ProjectSettings::textFontChanged);
	s->setTextFont(other);
	QCOMPARE(fonts.count(), 1);
	QVERIFY(!s->fontPreview(Qt::white).isNull());
	QCOMPARE(s->apply(), QString());
	QCOMPARE(m_project->database().system.font_id, other);
	// saved in the database
	const auto read = reload();
	QVERIFY(read->loadDatabaseAndMapTree());
	QCOMPARE(read->database().system.font_id, other);
}

void ProjectSettingsTest::patches()
{
	const auto s = settings();
	QSignalSpy modes(s.get(), &ProjectSettings::patchModeChanged);
	QSignalSpy shown(s.get(), &ProjectSettings::patchesChanged);
	// as written: they are not checked by hand
	s->setPatchOn(QStringLiteral("Maniac"), true);
	QVERIFY(!s->patchOn(QStringLiteral("Maniac")));
	QCOMPARE(shown.count(), 0);

	// chosen, from what is written
	s->setPatchMode(ProjectSettings::Choose);
	QCOMPARE(modes.count(), 1);
	QCOMPARE(shown.count(), 1);
	QVERIFY(s->patchOn(QStringLiteral("DynRPG")));
	s->setPatchOn(QStringLiteral("Maniac"), true);
	s->setPatchOn(QStringLiteral("DynRPG"), false);
	QCOMPARE(shown.count(), 3);
	QVERIFY(s->patchOn(QStringLiteral("Maniac")));
	QVERIFY(!s->patchOn(QStringLiteral("DynRPG")));
	QCOMPARE(s->apply(), QString());
	GamePatches::State state = GamePatches::read(*m_project);
	QVERIFY(state.chosen);
	QCOMPARE(state.values.value(QStringLiteral("Maniac")), 1);
	QCOMPARE(state.values.value(QStringLiteral("DynRPG")), 0);
	QCOMPARE(state.values.value(QStringLiteral("KeyPatch")), 1);
	// the keys the Editor does not show stay
	QVERIFY(state.others.contains(QStringLiteral("AntiLagSwitch")));

	// detected: no [Patch] left, the Player detects them
	s->setPatchMode(ProjectSettings::Detect);
	QCOMPARE(s->apply(), QString());
	state = GamePatches::read(*m_project);
	QVERIFY(!state.chosen);
	QSignalSpy ini(s.get(), &ProjectSettings::iniChanged);
	s->readIni();
	QCOMPARE(ini.count(), 1);
	QVERIFY(s->detectAllowed());
	QVERIFY(s->otherPatches().isEmpty());
	QCOMPARE(s->patchMode(), ProjectSettings::Detect);
	QCOMPARE(at(s->patchModeChoices(), 0).value(QStringLiteral("enabled")).toBool(), true);

	// EasyRPG.ini edited by hand meanwhile: a key the Editor does not show
	// stops the detection, its patches are as written
	QFile file(QDir(m_game).filePath(QStringLiteral("EasyRPG.ini")));
	QVERIFY(file.open(QIODevice::Append));
	file.write("\r\n[Patch]\r\nAntiLagSwitch=1000\r\n");
	file.close();
	s->readIni();
	QCOMPARE(s->patchMode(), ProjectSettings::Refer);
	QVERIFY(!s->detectAllowed());
	QCOMPARE(s->otherPatches(), QStringList{QStringLiteral("AntiLagSwitch")});
	// written as it is
	QVERIFY(file.open(QIODevice::ReadOnly));
	const QByteArray before = file.readAll();
	file.close();
	QCOMPARE(s->apply(), QString());
	QVERIFY(file.open(QIODevice::ReadOnly));
	QCOMPARE(file.readAll(), before);
}

void ProjectSettingsTest::playerFonts()
{
	const auto s = settings();
	QSignalSpy fonts(s.get(), &ProjectSettings::playerFontsChanged);
	const QString chosen = QDir(m_dir->path()).filePath(QStringLiteral("Chosen.TTF"));
	QFile file(chosen);
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.write("not really a font");
	file.close();
	s->choosePlayerFont(1, chosen);
	QCOMPARE(fonts.count(), 1);
	QCOMPARE(s->playerFont(1), chosen);
	QCOMPARE(s->playerFontText(1), QStringLiteral("Chosen.TTF (copied on OK)"));
	// none chosen (the file window cancelled), or no such font: unchanged
	s->choosePlayerFont(1, QString());
	s->choosePlayerFont(2, chosen);
	QCOMPARE(fonts.count(), 1);
	QCOMPARE(s->playerFont(0), QString());

	// copied in the Font folder, under the name the Player looks for
	QCOMPARE(s->apply(), QString());
	const QString copied = QDir(m_game).filePath(QStringLiteral("Font/Font2.ttf"));
	QVERIFY(QFileInfo::exists(copied));
	const auto again = settings();
	QCOMPARE(canonical(again->playerFont(1)), canonical(copied));
	QCOMPARE(again->playerFontText(1), QStringLiteral("Font/Font2.ttf"));
	QCOMPARE(again->playerFontText(0), QStringLiteral("Built-in (ttyp0)"));
	// the Player's own font: its file goes to the trash on OK (not written here)
	again->useBuiltinPlayerFont(1);
	QCOMPARE(again->playerFont(1), QString());
	QCOMPARE(again->playerFontText(1), QStringLiteral("Built-in (RMG2000)"));
}

void ProjectSettingsTest::closedProject()
{
	// as Open Project reads it
	m_project->detectEncoding();
	const auto s = settings(false);
	QVERIFY(!s->isOpen());
	QCOMPARE(s->engineName(), QString());
	QVERIFY(!s->canConvert());
	QCOMPARE(s->gameTitle(), QStringLiteral("RPG Maker 2003 Test suite"));
	// the font of the database is only the open project's
	const int font = m_project->database().system.font_id;
	s->setTextFont(font == lcf::rpg::System::Font_mincho ? lcf::rpg::System::Font_gothic : lcf::rpg::System::Font_mincho);
	s->setGameTitle(QStringLiteral("Closed"));
	QCOMPARE(s->apply(), QString());
	const auto read = reload();
	QCOMPARE(read->gameTitle(), QStringLiteral("Closed"));
	QVERIFY(read->loadDatabaseAndMapTree());
	QCOMPARE(read->database().system.font_id, font);
}

QTEST_MAIN(ProjectSettingsTest)

#include "project_settings_test.moc"
