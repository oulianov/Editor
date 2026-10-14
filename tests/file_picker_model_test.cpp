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

// The resource pickers without their window (FilePickerModel): the files of
// a folder of a game and of an RTP made in a temporary folder, the file
// selected first, what is chosen with it. Nothing is played.

#include <QDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include "model/file_picker_model.h"

class FilePickerModelTest : public QObject {
	Q_OBJECT

private:
	QTemporaryDir m_root;
	QString m_game;
	QString m_rtp;

	void image(const QString& path, QSize size = QSize(24, 32)) {
		QDir().mkpath(QFileInfo(path).absolutePath());
		QImage picture(size, QImage::Format_Indexed8);
		picture.setColorCount(2);
		picture.setColor(0, qRgb(0, 128, 128));
		picture.setColor(1, qRgb(255, 255, 255));
		picture.fill(0);
		QVERIFY(picture.save(path));
	}
	void file(const QString& path) {
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile out(path);
		QVERIFY(out.open(QIODevice::WriteOnly));
		out.write("RIFF");
	}
	QStringList names(const FilePickerModel& model) {
		QStringList list;
		for (int row = 0; row < model.rowCount(); ++row) {
			list << model.index(row).data().toString();
		}
		return list;
	}

private slots:
	void initTestCase() {
		QVERIFY(m_root.isValid());
		m_game = m_root.filePath(QStringLiteral("game"));
		m_rtp = m_root.filePath(QStringLiteral("rtp"));
		// the game's CharSet folder in another case, as some games have it
		image(m_game + QStringLiteral("/Charset/b.png"));
		image(m_game + QStringLiteral("/Charset/A.png"));
		image(m_game + QStringLiteral("/Charset/a10.png"), QSize(288, 256));
		image(m_game + QStringLiteral("/Charset/a9.png"));
		file(m_game + QStringLiteral("/Charset/notes.txt"));
		image(m_rtp + QStringLiteral("/CharSet/Rtp1.png"));
		image(m_rtp + QStringLiteral("/CharSet/B.png"));
		// one name, two formats: the one the Player plays
		file(m_game + QStringLiteral("/Music/Theme.wav"));
		file(m_game + QStringLiteral("/Music/Theme.ogg"));
		file(m_game + QStringLiteral("/Music/Battle.mid"));
		QDir(m_game).mkpath(QStringLiteral("Panorama"));
	}

	void listsGameAndRtp() {
		FilePickerModel model(FilePickerModel::Preview::CharSet);
		QSignalSpy files(&model, &FilePickerModel::filesChanged);
		model.open(QStringLiteral("CharSet"), m_game, m_rtp, QStringLiteral("A"));
		QCOMPARE(files.count(), 1);
		QCOMPARE(model.fileType(), FileFinder::FileType::Image);
		// natural order, any case; the game's b hides the RTP's B; not the text file
		QCOMPARE(names(model), (QStringList{QStringLiteral("A.png"), QStringLiteral("a9.png"), QStringLiteral("a10.png"),
			QStringLiteral("b.png"), QStringLiteral("Rtp1.png (RTP)")}));
		QCOMPARE(model.index(3).data(FilePickerModel::RtpRole).toBool(), false);
		QCOMPARE(model.index(4).data(FilePickerModel::RtpRole).toBool(), true);
		QCOMPARE(model.index(4).data(FilePickerModel::NameRole).toString(), QStringLiteral("Rtp1"));
		QCOMPARE(model.index(2).data(FilePickerModel::FileNameRole).toString(), QStringLiteral("a10.png"));
	}

	void selectsTheCurrentFile() {
		FilePickerModel model;
		// any case; an RTP file too
		model.open(QStringLiteral("CharSet"), m_game, m_rtp, QStringLiteral("RTP1"));
		QCOMPARE(model.currentRow(), 4);
		QCOMPARE(model.currentName(), QStringLiteral("Rtp1"));
		QVERIFY(model.currentIsRtp());
		QVERIFY(model.info().startsWith(QStringLiteral("RTP file: Rtp1.png")));
		model.open(QStringLiteral("CharSet"), m_game, m_rtp, QStringLiteral("a10"));
		QCOMPARE(model.currentRow(), 2);
		QVERIFY(!model.currentIsRtp());
		QVERIFY(model.info().startsWith(QStringLiteral("Game file: a10.png")));
		QVERIFY(model.info().contains(QStringLiteral("288 x 256 pixels")));
		QCOMPARE(model.imageSize(), QSize(288, 256));
	}

	void selectsTheFirstFileOtherwise() {
		FilePickerModel model;
		// a file is always selected: none yet, or a missing one, is the first
		model.open(QStringLiteral("CharSet"), m_game, m_rtp, QString());
		QCOMPARE(model.currentRow(), 0);
		QCOMPARE(model.currentName(), QStringLiteral("A"));
		model.open(QStringLiteral("CharSet"), m_game, m_rtp, QStringLiteral("Missing"));
		QCOMPARE(model.currentRow(), 0);
		// without the RTP, its file is missing too
		model.open(QStringLiteral("CharSet"), m_game, QString(), QStringLiteral("Rtp1"));
		QCOMPARE(model.rowCount(), 4);
		QCOMPARE(model.currentName(), QStringLiteral("A"));
	}

	void emptyFolder() {
		FilePickerModel model;
		// no file to give: nothing selected
		model.open(QStringLiteral("Panorama"), m_game, m_rtp, QStringLiteral("Sky"));
		QCOMPARE(model.rowCount(), 0);
		QCOMPARE(model.currentRow(), -1);
		QVERIFY(model.currentName().isEmpty());
		QVERIFY(model.info().isEmpty());
		model.open(QStringLiteral("Backdrop"), m_game, QString(), QString());
		QCOMPARE(model.currentRow(), -1);
	}

	void choosesAnotherFile() {
		FilePickerModel model;
		model.open(QStringLiteral("CharSet"), m_game, m_rtp, QStringLiteral("A"));
		QSignalSpy current(&model, &FilePickerModel::currentChanged);
		model.setCurrentRow(3);
		QCOMPARE(current.count(), 1);
		QCOMPARE(model.currentName(), QStringLiteral("b"));
		// out of the list: the file stays
		model.setCurrentRow(9);
		model.setCurrentRow(-1);
		QCOMPARE(model.currentRow(), 3);
		QCOMPARE(current.count(), 1);
	}

	void prefersThePlayersFormat() {
		FilePickerModel model(FilePickerModel::Preview::Music);
		model.open(QStringLiteral("Music"), m_game, m_rtp, QStringLiteral("theme"));
		QCOMPARE(model.fileType(), FileFinder::FileType::Music);
		QCOMPARE(names(model), (QStringList{QStringLiteral("Battle.mid"), QStringLiteral("Theme.ogg")}));
		QCOMPARE(model.currentName(), QStringLiteral("Theme"));
		QVERIFY(model.currentPath().endsWith(QStringLiteral("Theme.ogg")));
		QVERIFY(model.info().contains(QStringLiteral("Game file: Theme.ogg")));
	}

	void keepsTheChoice() {
		FilePickerModel model(FilePickerModel::Preview::CharSetPose);
		QSignalSpy choice(&model, &FilePickerModel::choiceChanged);
		model.setSpriteIndex(5);
		model.setDirection(7);
		model.setPattern(-1);
		model.setHue(400);
		model.setFadeIn(20000);
		model.setVolume(120);
		model.setTempo(10);
		model.setBalance(-5);
		// within RPG Maker's ranges
		QCOMPARE(model.spriteIndex(), 5);
		QCOMPARE(model.direction(), 3);
		QCOMPARE(model.pattern(), 0);
		QCOMPARE(model.hue(), 359);
		QCOMPARE(model.fadeIn(), 10000);
		QCOMPARE(model.volume(), 100);
		QCOMPARE(model.tempo(), 50);
		QCOMPARE(model.balance(), 0);
		// the volume was already at its maximum
		QCOMPARE(choice.count(), 7);
		model.setVolume(100);
		QCOMPARE(choice.count(), 7);
	}

	void applies() {
		FilePickerModel model;
		QSignalSpy applied(&model, &FilePickerModel::applied);
		model.apply();
		QCOMPARE(applied.count(), 1);
		// nothing to play: it says so, nothing plays
		model.open(QStringLiteral("Panorama"), m_game, m_rtp, QString());
		model.play();
		QVERIFY(!model.playing());
		QVERIFY(!model.playStatus().isEmpty());
	}
};

QTEST_MAIN(FilePickerModelTest)
#include "file_picker_model_test.moc"
