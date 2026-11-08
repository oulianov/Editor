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

#include "project_soundfont.h"
#include "common/logging.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QProcess>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>
#include <vector>

namespace {

/** Above it, the confirmation says the file ships with every copy of the game */
constexpr qint64 kLargeSoundfont = 50'000'000;

/** The end of a copy: the error, or cancelled */
struct CopyResult {
	QString error;
	bool cancelled = false;
};

/**
 * A short General MIDI file to hear a soundfont with, when the game has no
 * music: piano, strings, bass and drums for two bars (4 s at 120 BPM)
 */
QByteArray sampleSong() {
	struct Event {
		int tick;
		std::vector<uint8_t> bytes;
	};
	std::vector<Event> events;
	auto note = [&events](int channel, int key, int start, int length, int velocity) {
		events.push_back({start, {static_cast<uint8_t>(0x90 | channel), static_cast<uint8_t>(key), static_cast<uint8_t>(velocity)}});
		events.push_back({start + length, {static_cast<uint8_t>(0x80 | channel), static_cast<uint8_t>(key), 0}});
	};
	constexpr int beat = 480;
	// programs: piano, strings, fingered bass
	events.push_back({0, {0xC0, 0}});
	events.push_back({0, {0xC1, 48}});
	events.push_back({0, {0xC2, 33}});
	const int arpeggio[] = {60, 64, 67, 72};
	for (int i = 0; i < 4; ++i) {
		note(0, arpeggio[i], i * beat, beat, 90);
	}
	for (int key : {60, 64, 67, 72}) {
		note(0, key, 4 * beat, 4 * beat, 80);
	}
	for (int key : {48, 55, 64}) {
		note(1, key, 0, 8 * beat, 60);
	}
	for (int bar = 0; bar < 2; ++bar) {
		note(2, 36, bar * 4 * beat, 2 * beat, 90);
		note(2, 43, bar * 4 * beat + 2 * beat, 2 * beat, 90);
	}
	for (int i = 0; i < 16; ++i) {
		note(9, 42, i * beat / 2, beat / 4, 70);
		if (i % 4 == 0) {
			note(9, 36, i * beat / 2, beat / 4, 110);
		} else if (i % 4 == 2) {
			note(9, 38, i * beat / 2, beat / 4, 100);
		}
	}
	note(9, 49, 8 * beat, beat, 100);
	std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.tick < b.tick; });

	QByteArray track;
	auto variable = [&track](quint32 value) {
		QByteArray bytes(1, static_cast<char>(value & 0x7F));
		while (value >>= 7) {
			bytes.prepend(static_cast<char>((value & 0x7F) | 0x80));
		}
		track += bytes;
	};
	// tempo: 500000 us a beat
	track += QByteArray("\x00\xFF\x51\x03\x07\xA1\x20", 7);
	int last = 0;
	for (const Event& event : events) {
		variable(static_cast<quint32>(event.tick - last));
		last = event.tick;
		track += QByteArray(reinterpret_cast<const char*>(event.bytes.data()), static_cast<qsizetype>(event.bytes.size()));
	}
	track += QByteArray("\x00\xFF\x2F\x00", 4);
	QByteArray file("MThd\x00\x00\x00\x06\x00\x00\x00\x01\x01\xE0", 14);
	file += "MTrk";
	const quint32 size = static_cast<quint32>(track.size());
	file += static_cast<char>(size >> 24);
	file += static_cast<char>(size >> 16);
	file += static_cast<char>(size >> 8);
	file += static_cast<char>(size);
	return file + track;
}

/** Copies the file into a temporary file renamed once whole (QSaveFile): stopped, nothing changes */
CopyResult copySoundfont(const QString& from, const QString& to, std::atomic<bool>& cancelled, std::atomic<qint64>& written) {
	QFile in(from);
	if (!in.open(QIODevice::ReadOnly)) {
		return {QCoreApplication::translate("ProjectSettingsDialog", "%1 could not be read: %2").arg(QDir::toNativeSeparators(from), in.errorString())};
	}
	QSaveFile out(to);
	if (!out.open(QIODevice::WriteOnly)) {
		return {QCoreApplication::translate("ProjectSettingsDialog", "%1 could not be written: %2").arg(QDir::toNativeSeparators(to), out.errorString())};
	}
	QByteArray buffer(1 << 20, Qt::Uninitialized);
	while (!in.atEnd()) {
		if (cancelled) {
			out.cancelWriting();
			return {QString(), true};
		}
		const qint64 read = in.read(buffer.data(), buffer.size());
		if (read < 0 || out.write(buffer.constData(), read) != read) {
			const QString why = read < 0 ? in.errorString() : out.errorString();
			out.cancelWriting();
			return {QCoreApplication::translate("ProjectSettingsDialog", "%1 could not be written: %2").arg(QDir::toNativeSeparators(to), why)};
		}
		written += read;
	}
	if (!out.commit()) {
		return {QCoreApplication::translate("ProjectSettingsDialog", "%1 could not be written: %2").arg(QDir::toNativeSeparators(to), out.errorString())};
	}
	return {};
}

} // namespace

ProjectSoundfont::ProjectSoundfont(const QString& folder, QObject* parent) :
	QObject(parent), m_folder(folder)
{
	readGame();
}

ProjectSoundfont::~ProjectSoundfont()
{
	// a copy still running stops (its temporary file goes)
	if (m_copy) {
		m_copy->cancelled = true;
	}
}

void ProjectSoundfont::readGame()
{
	m_game = Soundfonts::gameSoundfont(m_folder);
	m_gameInfo = m_game.isEmpty() ? SoundfontInfo() : Soundfonts::instance().info(m_game);
}

bool ProjectSoundfont::present() const
{
	return m_choice ? !m_choice->isEmpty() : !m_game.isEmpty();
}

QString ProjectSoundfont::path() const
{
	return m_choice ? *m_choice : m_game;
}

QString ProjectSoundfont::displayName(const SoundfontInfo& info, const QString& original, const QString& file)
{
	if (!info.name.isEmpty()) {
		return info.name;
	}
	return original.isEmpty() ? QFileInfo(file).fileName() : original;
}

QString ProjectSoundfont::summary() const
{
	if (!present()) {
		return QCoreApplication::translate("ProjectSettingsDialog", "None — players hear their own soundfont, or the built-in synthesizer");
	}
	const SoundfontInfo& info = m_choice ? m_choiceInfo : m_gameInfo;
	const QString name = m_choice ? displayName(info, QString(), *m_choice)
		: displayName(info, Soundfonts::originalName(m_folder), m_game);
	if (!info.valid()) {
		return QCoreApplication::translate("ProjectSettingsDialog", "%1 — %2").arg(name, SoundfontInfo::sizeText(info.size));
	}
	return QCoreApplication::translate("ProjectSettingsDialog", "%1 — %2, %3").arg(name, SoundfontInfo::sizeText(info.size), QCoreApplication::translate("ProjectSettingsDialog", "%n presets", nullptr, info.presets));
}

QString ProjectSoundfont::warning() const
{
	if (!present()) {
		return {};
	}
	const SoundfontInfo& info = m_choice ? m_choiceInfo : m_gameInfo;
	return info.valid() ? info.warnings.join(QLatin1Char('\n')) : info.error;
}

bool ProjectSoundfont::invalid() const
{
	return present() && !(m_choice ? m_choiceInfo : m_gameInfo).valid();
}

QString ProjectSoundfont::pendingText() const
{
	if (!m_choice) {
		return {};
	}
	return m_choice->isEmpty() ? QCoreApplication::translate("ProjectSettingsDialog", "Moved to the trash on OK") : QCoreApplication::translate("ProjectSettingsDialog", "Copied as easyrpg.soundfont on OK");
}

QString ProjectSoundfont::check(const QString& file) const
{
	const SoundfontInfo info = SoundfontInfo::read(file);
	return info.valid() ? QString() : info.error;
}

QString ProjectSoundfont::chooseConfirmation(const QString& file) const
{
	QStringList lines;
	const bool replaces = !m_game.isEmpty() && !(m_choice && m_choice->isEmpty());
	if (replaces && QFileInfo(file) != QFileInfo(m_game)) {
		lines << QCoreApplication::translate("ProjectSettingsDialog", "Replace the game soundfont %1 with %2?")
			.arg(displayName(m_gameInfo, Soundfonts::originalName(m_folder), m_game), QFileInfo(file).fileName());
	}
	if (const qint64 size = QFileInfo(file).size(); size > kLargeSoundfont) {
		lines << QCoreApplication::translate("ProjectSettingsDialog", "%1 is %2: it ships with every copy of the game, and players download it with it.")
			.arg(QFileInfo(file).fileName(), SoundfontInfo::sizeText(size));
	}
	return lines.join(QStringLiteral("\n\n"));
}

void ProjectSoundfont::choose(const QString& file)
{
	if (m_busy || file.isEmpty()) {
		return;
	}
	if (!m_game.isEmpty() && QFileInfo(file) == QFileInfo(m_game)) {
		// the game's own: nothing to copy
		m_choice.reset();
	} else {
		m_choice = QFileInfo(file).absoluteFilePath();
		m_choiceInfo = SoundfontInfo::read(file);
	}
	emit changed();
}

QString ProjectSoundfont::removeConfirmation() const
{
	if (m_game.isEmpty()) {
		return {};
	}
	return QCoreApplication::translate("ProjectSettingsDialog", "Remove the game soundfont %1? It goes to the trash on OK: players then hear their own soundfont, "
		"or the built-in synthesizer.").arg(displayName(m_gameInfo, Soundfonts::originalName(m_folder), m_game));
}

void ProjectSoundfont::remove()
{
	if (m_busy) {
		return;
	}
	if (m_game.isEmpty()) {
		m_choice.reset();
	} else {
		m_choice = QString();
	}
	emit changed();
}

void ProjectSoundfont::revert()
{
	if (!m_busy && m_choice) {
		m_choice.reset();
		emit changed();
	}
}

void ProjectSoundfont::reveal() const
{
	const QString file = path();
	if (file.isEmpty()) {
		return;
	}
#if defined(Q_OS_MACOS)
	QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), file});
#elif defined(Q_OS_WIN)
	QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + QDir::toNativeSeparators(file)});
#else
	// the file managers show the folder; the file itself needs their D-Bus interface
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).absolutePath()));
#endif
}

QString ProjectSoundfont::sampleMidi() const
{
	// the smallest MIDI file of the game's music: often a short jingle
	const QDir game(m_folder);
	for (const QString& sub : game.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (sub.compare(QLatin1String("Music"), Qt::CaseInsensitive) != 0) {
			continue;
		}
		const QFileInfoList songs = QDir(game.filePath(sub)).entryInfoList({QStringLiteral("*.mid"), QStringLiteral("*.midi")},
			QDir::Files, QDir::Size | QDir::Reversed);
		for (const QFileInfo& song : songs) {
			// past the header and a track
			if (song.size() > 64) {
				return song.absoluteFilePath();
			}
		}
	}
	const QString sample = QDir::temp().filePath(QStringLiteral("easyrpg-editor-soundfont-sample.mid"));
	const QByteArray song = sampleSong();
	QFile file(sample);
	if (QFileInfo(sample).size() != song.size() && (!file.open(QIODevice::WriteOnly) || file.write(song) != song.size())) {
		qCWarning(lcAudio).noquote() << "The sample song could not be written:" << sample;
		return {};
	}
	return sample;
}

QString ProjectSoundfont::readOnlyError() const
{
	const QFileInfo folder(m_folder);
	if (!folder.isWritable() || (!m_game.isEmpty() && !QFileInfo(m_game).isWritable())) {
		return QCoreApplication::translate("ProjectSettingsDialog", "The game folder is read-only: the soundfont was not changed.");
	}
	return {};
}

QString ProjectSoundfont::apply()
{
	if (m_busy) {
		return {};
	}
	if (!m_choice) {
		QTimer::singleShot(0, this, [this] { done(QString(), false); });
		return {};
	}
	if (const QString error = readOnlyError(); !error.isEmpty()) {
		return error;
	}
	if (m_choice->isEmpty()) {
		// to the trash, else deleted where the system has none
		if (!QFile::moveToTrash(m_game) && QFileInfo::exists(m_game) && !QFile::remove(m_game)) {
			return QCoreApplication::translate("ProjectSettingsDialog", "%1 could not be removed.").arg(QDir::toNativeSeparators(m_game));
		}
		qCInfo(lcAudio).noquote() << "Game soundfont removed:" << m_game;
		Soundfonts::setOriginalName(m_folder, QString());
		QTimer::singleShot(0, this, [this] { done(QString(), false); });
		return {};
	}
	const QString from = *m_choice;
	if (const QString error = check(from); !error.isEmpty()) {
		return error;
	}
	const QString to = Soundfonts::gameSoundfontTarget(m_folder);
	m_copy = std::make_shared<Copy>();
	m_copy->size = QFileInfo(from).size();
	m_progress = 0;
	emit progressChanged();
	setBusy(true);
	auto* poll = new QTimer(this);
	connect(poll, &QTimer::timeout, this, [this, poll] {
		if (!m_copy) {
			poll->deleteLater();
			return;
		}
		m_progress = m_copy->size > 0 ? static_cast<double>(m_copy->written) / m_copy->size : 1;
		emit progressChanged();
	});
	poll->start(100);
	QtConcurrent::run([copy = m_copy, from, to] {
		return copySoundfont(from, to, copy->cancelled, copy->written);
	}).then(this, [this, from, to](CopyResult result) {
		m_copy.reset();
		if (result.error.isEmpty() && !result.cancelled) {
			qCInfo(lcAudio).noquote() << "Game soundfont:" << from << "copied as" << to;
			Soundfonts::setOriginalName(m_folder, QFileInfo(from).fileName());
		}
		done(result.error, result.cancelled);
	});
	return {};
}

void ProjectSoundfont::cancel()
{
	if (m_copy) {
		m_copy->cancelled = true;
	}
}

QString ProjectSoundfont::applyAndWait()
{
	QString error;
	QEventLoop loop;
	connect(this, &ProjectSoundfont::finished, &loop, [&](const QString& why, bool) {
		error = why;
		loop.quit();
	});
	if (const QString refused = apply(); !refused.isEmpty()) {
		return refused;
	}
	loop.exec();
	return error;
}

void ProjectSoundfont::setBusy(bool busy)
{
	if (busy != m_busy) {
		m_busy = busy;
		emit busyChanged();
	}
}

void ProjectSoundfont::done(const QString& error, bool cancelled)
{
	setBusy(false);
	if (error.isEmpty() && !cancelled) {
		m_choice.reset();
	}
	readGame();
	emit changed();
	emit finished(error, cancelled);
}
