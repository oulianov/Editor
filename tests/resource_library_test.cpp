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

// The Resource Manager without its window (ResourceLibraryModel), on a copy
// of TestGame-2003 (EDITOR_TEST_PROJECT, skipped without it) and an RTP
// made here: the folders and their files, the RTP's hidden by the game's,
// the filter, what a file is, and Import, Export, Rename and Delete with
// their questions answered by a stand-in of the interface.

#include <QAbstractItemModelTester>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFont>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include "common/image_import.h"
#include "controller/dialog_service.h"
#include "model/resource_library.h"

using Kind = ResourceLibraryModel::Kind;
using Origin = ResourceLibraryModel::Origin;

namespace {
/** Answers the questions as told, and remembers what was asked and told */
class Answers : public DialogService
{
public:
	Answer answer = Answer::No;
	QStringList questions;
	QStringList informations;
	QStringList warnings;

	Answer question(const QString&, const QString& text, Buttons) override {
		questions << text;
		return answer;
	}
	void information(const QString&, const QString& text) override { informations << text; }
	void warning(const QString&, const QString& text) override { warnings << text; }
	void error(const QString&, const QString& text) override { warnings << text; }
};

/** The image choices of the window: colors as told, the transparent color kept */
class Choices : public ResourceImportChoices
{
public:
	Colors colors = Colors::Reduce;
	int askedColors = 0;
	int askedTransparent = 0;

	Colors askColors(const QString&, const QString&) override {
		++askedColors;
		return colors;
	}
	QImage chooseTransparent(const QImage& image, const QString&) override {
		++askedTransparent;
		return image;
	}
};

/** The game's files, without its tool folders (rpgsync's Scripts, hidden ones) */
bool copyGame(const QString& from, const QString& to)
{
	QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString source = it.next();
		const QString path = QDir(from).relativeFilePath(source);
		if (path.startsWith(QLatin1String("Scripts/")) || path.startsWith(QLatin1Char('.')) || path.contains(QLatin1String("/."))) {
			continue;
		}
		const QString target = QDir(to).filePath(path);
		if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(source, target)) {
			return false;
		}
	}
	return true;
}

/** A palette image of the size, two colors */
QImage paletteImage(int w, int h)
{
	QImage image(w, h, QImage::Format_Indexed8);
	image.setColorTable({ qRgb(255, 0, 255), qRgb(10, 20, 30) });
	image.fill(0);
	for (int x = 0; x < w / 2; ++x) {
		image.setPixel(x, 0, 1);
	}
	return image;
}

/** A true color image with more than 256 colors */
QImage manyColors(int w, int h)
{
	QImage image(w, h, QImage::Format_RGB32);
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			image.setPixel(x, y, qRgb(x % 256, y % 256, (x * y) % 256));
		}
	}
	return image;
}

bool touch(const QString& path, const QByteArray& data = QByteArray("RIFF"))
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
}

class ResourceLibraryTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void cleanup();
	void folders();
	void files();
	void filter();
	void sizeRules();
	void importImages();
	void importOther();
	void exportFile();
	void renameFile();
	void deleteFile();

private:
	QString game(const QString& path = QString()) const { return QDir(m_dir->filePath(QStringLiteral("game"))).filePath(path); }
	QString rtp(const QString& path = QString()) const { return QDir(m_dir->filePath(QStringLiteral("rtp"))).filePath(path); }
	QString source(const QString& name) const { return QDir(m_dir->filePath(QStringLiteral("import"))).filePath(name); }
	QStringList names() const;
	void show(const QString& folder);

	std::unique_ptr<QTemporaryDir> m_dir;
	QTemporaryDir m_settings;
	std::unique_ptr<ResourceLibraryModel> m_model;
	std::unique_ptr<QAbstractItemModelTester> m_tester;
	std::unique_ptr<QAbstractItemModelTester> m_folderTester;
	Answers m_answers;
	Choices m_choices;
};

void ResourceLibraryTest::initTestCase()
{
	// the remembered folders in a settings file of the test
	QVERIFY(m_settings.isValid());
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());

	const QString test_game = qEnvironmentVariable("EDITOR_TEST_PROJECT");
	if (test_game.isEmpty()) {
		QSKIP("EDITOR_TEST_PROJECT (TestGame-2003) is not set");
	}
	m_dir = std::make_unique<QTemporaryDir>();
	QVERIFY(m_dir->isValid());
	QVERIFY(copyGame(test_game, game()));
	QVERIFY(QFile::exists(game(QStringLiteral("CharSet/Actor1.png"))));

	// an RTP: a charset the game replaces, one of its own
	QVERIFY(QDir().mkpath(rtp(QStringLiteral("CharSet"))));
	QVERIFY(paletteImage(288, 256).save(rtp(QStringLiteral("CharSet/Actor1.png"))));
	QVERIFY(paletteImage(288, 256).save(rtp(QStringLiteral("CharSet/People1.png"))));

	// the files imported
	QVERIFY(QDir().mkpath(m_dir->filePath(QStringLiteral("import"))));
	QVERIFY(paletteImage(288, 256).save(source(QStringLiteral("Hero.png"))));
	QVERIFY(paletteImage(10, 10).save(source(QStringLiteral("Small.png"))));
	QVERIFY(manyColors(288, 256).save(source(QStringLiteral("Big.png"))));
	QVERIFY(touch(source(QStringLiteral("Song.wav"))));
	QVERIFY(touch(source(QStringLiteral("Notes.txt")), "notes"));
}

void ResourceLibraryTest::init()
{
	m_answers = Answers();
	m_choices = Choices();
	DialogService::install(&m_answers);
	m_model = std::make_unique<ResourceLibraryModel>();
	m_model->setUseTrash(false);
	m_model->setImportChoices(&m_choices);
	m_tester = std::make_unique<QAbstractItemModelTester>(m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
	m_folderTester = std::make_unique<QAbstractItemModelTester>(m_model->folders(), QAbstractItemModelTester::FailureReportingMode::QtTest);
	m_model->setGamePath(game());
	m_model->setRtpPath(rtp());
}

void ResourceLibraryTest::cleanup()
{
	m_tester.reset();
	m_folderTester.reset();
	m_model.reset();
	DialogService::install(nullptr);
}

QStringList ResourceLibraryTest::names() const
{
	QStringList list;
	for (int row = 0; row < m_model->rowCount(); ++row) {
		list << m_model->index(row).data(ResourceLibraryModel::NameRole).toString();
	}
	return list;
}

void ResourceLibraryTest::show(const QString& folder)
{
	const int index = ResourceLibraryModel::folderOf(folder);
	QVERIFY(index >= 0);
	// another folder first: the files of this one are listed again
	m_model->setFolder(index == 0 ? 1 : 0);
	m_model->setFolder(index);
	QCOMPARE(m_model->folderName(), folder);
}

void ResourceLibraryTest::folders()
{
	QCOMPARE(ResourceLibraryModel::folderCount(), 19);
	QCOMPARE(m_model->folders()->rowCount(), 19);
	const int music = ResourceLibraryModel::folderOf(QStringLiteral("Music"));
	const int movie = ResourceLibraryModel::folderOf(QStringLiteral("Movie"));
	const int charset = ResourceLibraryModel::folderOf(QStringLiteral("CharSet"));
	QVERIFY(music >= 0 && movie >= 0 && charset >= 0);
	QCOMPARE(ResourceLibraryModel::folderOf(QStringLiteral("Nothing")), -1);
	QCOMPARE(ResourceLibraryModel::folderKind(music), Kind::Music);
	QCOMPARE(ResourceLibraryModel::folderKind(movie), Kind::Movie);
	QVERIFY(ResourceLibraryModel::folderTransparent(charset));
	QVERIFY(!ResourceLibraryModel::folderTransparent(music));

	// the test game has no movies: the folder is shown in italic, Import creates it
	ResourceFolderModel* folders = m_model->folders();
	const QModelIndex movies = folders->index(movie);
	QCOMPARE(movies.data(Qt::DisplayRole).toString(), QStringLiteral("Movie"));
	QVERIFY(movies.data(ResourceFolderModel::MissingRole).toBool());
	QVERIFY(movies.data(Qt::FontRole).value<QFont>().italic());
	QVERIFY(movies.data(Qt::ToolTipRole).toString().contains(QStringLiteral("no Movie folder")));
	QVERIFY(!folders->index(charset).data(ResourceFolderModel::MissingRole).toBool());
	QVERIFY(!folders->index(charset).data(Qt::FontRole).isValid());
	QCOMPARE(folders->index(charset).data(ResourceFolderModel::PathRole).toString(), game(QStringLiteral("CharSet")));

	m_model->setFolder(movie);
	QCOMPARE(m_model->rowCount(), 0);
	QVERIFY(m_model->createFolder().ok);
	QVERIFY(QDir(game(QStringLiteral("Movie"))).exists());
	QVERIFY(!movies.data(ResourceFolderModel::MissingRole).toBool());

	// remembered for the next window
	QCOMPARE(ResourceLibraryModel().folder(), movie);
}

void ResourceLibraryTest::files()
{
	show(QStringLiteral("CharSet"));
	// the game's Actor1 hides the RTP's, People1 is the RTP's
	QCOMPARE(names(), QStringList({ "Actor1", "Actor2", "Actor3", "Actor4", "People1", "Vehicles" }));
	QCOMPARE(m_model->countText(), QStringLiteral("Files: 6, from the RTP: 1"));
	const QModelIndex actor = m_model->index(0);
	const QModelIndex people = m_model->index(4);
	QCOMPARE(actor.data(ResourceLibraryModel::PathRole).toString(), game(QStringLiteral("CharSet/Actor1.png")));
	QCOMPARE(actor.data(ResourceLibraryModel::OriginRole).value<Origin>(), Origin::Game);
	QCOMPARE(actor.data(ResourceLibraryModel::FormatRole).toString(), QStringLiteral("PNG"));
	QCOMPARE(actor.data(ResourceLibraryModel::SizeRole).toLongLong(), QFileInfo(game(QStringLiteral("CharSet/Actor1.png"))).size());
	QCOMPARE(people.data(ResourceLibraryModel::OriginRole).value<Origin>(), Origin::Rtp);
	QCOMPARE(people.data(Qt::DisplayRole).toString(), QStringLiteral("People1 (RTP)"));
	QVERIFY(people.data(Qt::ForegroundRole).isValid());

	// nothing shown at first
	QCOMPARE(m_model->currentRow(), -1);
	QVERIFY(m_model->info().isEmpty());
	QVERIFY(m_model->canImport());
	QVERIFY(!m_model->canExport());
	QVERIFY(!m_model->canDelete());

	// a game file: what it is, as the game shows it
	QSignalSpy current(m_model.get(), &ResourceLibraryModel::currentChanged);
	m_model->setCurrentRow(0);
	QCOMPARE(current.count(), 1);
	QCOMPARE(m_model->currentName(), QStringLiteral("Actor1"));
	QVERIFY(!m_model->currentIsRtp());
	const QStringList info = m_model->info();
	QCOMPARE(info.value(0), QStringLiteral("Game file: Actor1.png"));
	QVERIFY(info.contains(QStringLiteral("288 x 256 pixels")));
	QVERIFY(info.contains(QStringLiteral("Shown as in the game: palette color 0 is transparent.")));
	QVERIFY(std::none_of(info.cbegin(), info.cend(), [](const QString& line) { return line.startsWith(QStringLiteral("RPG Maker's")); }));
	const QImage preview = m_model->previewImage();
	QCOMPARE(preview.size(), QSize(288, 256));
	QCOMPARE(preview.format(), QImage::Format_ARGB32);
	QVERIFY(m_model->canDelete());
	QVERIFY(m_model->previewNote().isEmpty());
	QCOMPARE(m_model->openFolderPath(), game(QStringLiteral("CharSet")));

	// the RTP's: shown, not deleted
	QVERIFY(m_model->selectName(QStringLiteral("people1")));
	QCOMPARE(m_model->currentRow(), 4);
	QVERIFY(m_model->currentIsRtp());
	QCOMPARE(m_model->info().value(0), QStringLiteral("RTP file: People1.png"));
	QVERIFY(m_model->canExport());
	QVERIFY(!m_model->canDelete());

	// listed again: the same file stays shown
	m_model->refresh();
	QCOMPARE(m_model->currentName(), QStringLiteral("People1"));

	// sounds: no image, how to listen to them
	show(QStringLiteral("Sound"));
	QVERIFY(m_model->rowCount() > 0);
	QCOMPARE(m_model->kind(), Kind::Sound);
	m_model->setCurrentRow(0);
	QVERIFY(m_model->previewImage().isNull());
	QVERIFY(m_model->info().last().startsWith(QStringLiteral("Format: ")));
	QVERIFY(!m_model->previewNote().isEmpty());

	// no game: the RTP only, and says so
	m_model->setGamePath(QString());
	show(QStringLiteral("CharSet"));
	QCOMPARE(names(), QStringList({ "Actor1", "People1" }));
	QCOMPARE(m_model->info(), QStringList(QStringLiteral("No project is open.")));
	QVERIFY(!m_model->canImport());
	m_model->setRtpPath(QString());
	QCOMPARE(m_model->rowCount(), 0);
	QCOMPARE(m_model->countText(), QStringLiteral("Files: 0, no RTP path set"));
}

void ResourceLibraryTest::filter()
{
	show(QStringLiteral("CharSet"));
	QVERIFY(m_model->selectName(QStringLiteral("Vehicles")));
	QSignalSpy reset(m_model.get(), &QAbstractItemModel::modelReset);
	m_model->setFilter(QStringLiteral("actor"));
	QCOMPARE(reset.count(), 1);
	QCOMPARE(names(), QStringList({ "Actor1", "Actor2", "Actor3", "Actor4" }));
	// the file shown stays shown, out of the rows
	QCOMPARE(m_model->currentRow(), -1);
	QCOMPARE(m_model->currentName(), QStringLiteral("Vehicles"));
	QVERIFY(m_model->canExport());

	// kept for another folder
	show(QStringLiteral("System"));
	QCOMPARE(m_model->rowCount(), 0);
	show(QStringLiteral("CharSet"));
	QCOMPARE(m_model->rowCount(), 4);
	m_model->setFilter(QString());
	QCOMPARE(m_model->rowCount(), 6);
}

void ResourceLibraryTest::sizeRules()
{
	QVERIFY(ResourceLibraryModel::sizeRule(QStringLiteral("CharSet"), QSize(288, 256)).isEmpty());
	QVERIFY(ResourceLibraryModel::sizeRule(QStringLiteral("CharSet"), QSize(100, 100)).contains(QStringLiteral("288 x 256")));
	QVERIFY(ResourceLibraryModel::sizeRule(QStringLiteral("Battle"), QSize(192, 96)).isEmpty());
	QVERIFY(!ResourceLibraryModel::sizeRule(QStringLiteral("Battle"), QSize(100, 96)).isEmpty());
	QVERIFY(ResourceLibraryModel::sizeRule(QStringLiteral("Picture"), QSize(640, 1)).isEmpty());
	QVERIFY(!ResourceLibraryModel::sizeRule(QStringLiteral("Picture"), QSize(641, 10)).isEmpty());
	QVERIFY(ResourceLibraryModel::sizeRule(QStringLiteral("Music"), QSize(1, 1)).isEmpty());

	const QStringList filters = [] {
		ResourceLibraryModel model;
		model.setFolder(ResourceLibraryModel::folderOf(QStringLiteral("Sound")));
		return model.importFilters();
	}();
	QCOMPARE(filters.size(), 2);
	QVERIFY(filters.first().startsWith(QStringLiteral("Sound files (")));
	QVERIFY(filters.first().contains(QStringLiteral("*.wav")));
}

void ResourceLibraryTest::importImages()
{
	show(QStringLiteral("CharSet"));
	// a palette image of the right size: its transparent color chosen, nothing asked
	ResourceResult result = m_model->importFile(source(QStringLiteral("Hero.png")));
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(result.name, QStringLiteral("Hero"));
	QCOMPARE(m_choices.askedTransparent, 1);
	QVERIFY(m_answers.questions.isEmpty());
	QVERIFY(QFile::exists(game(QStringLiteral("CharSet/Hero.png"))));
	m_model->refresh(result.name);
	QCOMPARE(m_model->currentName(), QStringLiteral("Hero"));
	QCOMPARE(m_model->rowCount(), 7);

	// again, as XYZ: asked before replacing, No keeps the old one
	m_model->setImportXyz(true);
	QVERIFY(ResourceLibraryModel().importXyz());
	result = m_model->importFile(source(QStringLiteral("Hero.png")));
	QVERIFY(result.cancelled);
	QCOMPARE(m_answers.questions.size(), 1);
	QVERIFY(QFile::exists(game(QStringLiteral("CharSet/Hero.png"))));
	m_answers.answer = DialogService::Answer::Yes;
	result = m_model->importFile(source(QStringLiteral("Hero.png")));
	QVERIFY2(result.ok, qPrintable(result.error));
	QVERIFY(!QFile::exists(game(QStringLiteral("CharSet/Hero.png"))));
	const QImage xyz = ImageImport::loadImage(game(QStringLiteral("CharSet/Hero.xyz")));
	QCOMPARE(xyz.format(), QImage::Format_Indexed8);
	QCOMPARE(xyz.size(), QSize(288, 256));
	m_model->setImportXyz(false);

	// more than 256 colors: the window chooses, Cancel stops
	m_choices.colors = ResourceImportChoices::Colors::Cancel;
	result = m_model->importFile(source(QStringLiteral("Big.png")));
	QVERIFY(result.cancelled);
	QCOMPARE(m_choices.askedColors, 1);
	QVERIFY(!QFile::exists(game(QStringLiteral("CharSet/Big.png"))));
	m_choices.colors = ResourceImportChoices::Colors::Reduce;
	result = m_model->importFile(source(QStringLiteral("Big.png")));
	QVERIFY2(result.ok, qPrintable(result.error));
	const QImage reduced = ImageImport::loadImage(game(QStringLiteral("CharSet/Big.png")));
	QCOMPARE(reduced.format(), QImage::Format_Indexed8);
	QVERIFY(ImageImport::paletteSize(reduced) <= 256);

	// a size RPG Maker does not use: asked
	m_answers = Answers();
	result = m_model->importFile(source(QStringLiteral("Small.png")));
	QVERIFY(result.cancelled);
	QCOMPARE(m_answers.questions.size(), 1);
	QVERIFY(m_answers.questions.first().contains(QStringLiteral("288 x 256")));

	// a file of the game itself, a file that is no image
	result = m_model->importFile(game(QStringLiteral("CharSet/Actor2.png")));
	QVERIFY(!result.ok && !result.notice.isEmpty() && result.error.isEmpty());
	result = m_model->importFile(source(QStringLiteral("Notes.txt")));
	QVERIFY(!result.ok && !result.error.isEmpty());

	// without the windows of the interface: not imported
	m_model->setImportChoices(nullptr);
	m_answers.answer = DialogService::Answer::Yes;
	result = m_model->importFile(source(QStringLiteral("Hero.png")));
	QVERIFY(result.cancelled);
}

void ResourceLibraryTest::importOther()
{
	show(QStringLiteral("Sound"));
	const int count = m_model->rowCount();
	ResourceResult result = m_model->importFile(source(QStringLiteral("Notes.txt")));
	QVERIFY(!result.ok);
	QVERIFY(result.error.contains(QStringLiteral(".wav")));

	// the steps at once, as a QML window calls them: the file imported is shown
	m_answers.answer = DialogService::Answer::Yes;
	const QVariantList results = m_model->importFiles({ source(QStringLiteral("Song.wav")) });
	QCOMPARE(results.size(), 1);
	QVERIFY(results.first().value<ResourceResult>().ok);
	QVERIFY(QFile::exists(game(QStringLiteral("Sound/Song.wav"))));
	QCOMPARE(m_model->rowCount(), count + 1);
	QCOMPARE(m_model->currentName(), QStringLiteral("Song"));
	QCOMPARE(m_model->importDirectory(), m_dir->filePath(QStringLiteral("import")));
	QVERIFY(m_answers.questions.isEmpty());
}

void ResourceLibraryTest::exportFile()
{
	show(QStringLiteral("CharSet"));
	QVERIFY(!m_model->exportCurrent(m_dir->filePath(QStringLiteral("out/copy"))).ok);
	QVERIFY(m_model->selectName(QStringLiteral("Actor2")));
	QVERIFY(QDir().mkpath(m_dir->filePath(QStringLiteral("out"))));
	// the extension of the file when none is given
	ResourceResult result = m_model->exportCurrent(m_dir->filePath(QStringLiteral("out/copy")));
	QVERIFY2(result.ok, qPrintable(result.error));
	QVERIFY(QFile::exists(m_dir->filePath(QStringLiteral("out/copy.png"))));
	QCOMPARE(m_model->exportDirectory(), m_dir->filePath(QStringLiteral("out")));
	// replaced, and onto itself: nothing to do
	QVERIFY(m_model->exportCurrent(m_dir->filePath(QStringLiteral("out/copy.png"))).ok);
	QVERIFY(m_model->exportCurrent(m_model->currentPath()).ok);
	QVERIFY(QFile::exists(m_model->currentPath()));
}

void ResourceLibraryTest::renameFile()
{
	show(QStringLiteral("CharSet"));
	QVERIFY(m_model->selectName(QStringLiteral("Actor3")));
	QSignalSpy removed(m_model.get(), &ResourceLibraryModel::aboutToRemoveFile);
	ResourceResult result = m_model->renameCurrent(QStringLiteral("Villain"));
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(removed.count(), 1);
	QVERIFY(QFile::exists(game(QStringLiteral("CharSet/Villain.png"))));
	QVERIFY(!QFile::exists(game(QStringLiteral("CharSet/Actor3.png"))));
	QCOMPARE(m_model->currentName(), QStringLiteral("Villain"));

	// a name the folder has, a name that is no file name
	QVERIFY(!m_model->renameCurrent(QStringLiteral("actor1")).error.isEmpty());
	QVERIFY(!m_model->renameCurrent(QStringLiteral("a/b")).error.isEmpty());
	QVERIFY(!m_model->renameCurrent(QStringLiteral(" ")).error.isEmpty());
	QVERIFY(m_model->renameCurrent(QStringLiteral("Villain")).ok);

	// the RTP's are not renamed
	QVERIFY(m_model->selectName(QStringLiteral("People1")));
	QVERIFY(m_model->renameCurrent(QStringLiteral("Other")).cancelled);
}

void ResourceLibraryTest::deleteFile()
{
	show(QStringLiteral("CharSet"));
	QVERIFY(m_model->selectName(QStringLiteral("Actor1")));
	// asked first: No keeps it
	ResourceResult result = m_model->deleteCurrent();
	QVERIFY(result.cancelled);
	QCOMPARE(m_answers.questions.size(), 1);
	QVERIFY(QFile::exists(game(QStringLiteral("CharSet/Actor1.png"))));

	// the RTP's of the same name shows again
	m_answers.answer = DialogService::Answer::Yes;
	QSignalSpy removed(m_model.get(), &ResourceLibraryModel::aboutToRemoveFile);
	result = m_model->deleteCurrent();
	QVERIFY2(result.ok, qPrintable(result.error));
	QCOMPARE(removed.count(), 1);
	QVERIFY(!QFile::exists(game(QStringLiteral("CharSet/Actor1.png"))));
	QCOMPARE(m_model->currentName(), QStringLiteral("Actor1"));
	QVERIFY(m_model->currentIsRtp());
	QVERIFY(m_model->deleteCurrent().cancelled);

	// else the next one
	QVERIFY(m_model->selectName(QStringLiteral("Actor2")));
	const int row = m_model->currentRow();
	const QString next = m_model->index(row + 1).data(ResourceLibraryModel::NameRole).toString();
	QVERIFY(m_model->deleteCurrent().ok);
	QCOMPARE(m_model->currentName(), next);
	QCOMPARE(m_model->currentRow(), row);
}

QTEST_MAIN(ResourceLibraryTest)
#include "resource_library_test.moc"
