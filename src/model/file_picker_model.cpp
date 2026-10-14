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

#include "file_picker_model.h"
#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <algorithm>
#include "common/audio_player.h"
#include "common/image_import.h"
#include "model/resource_library.h"

FilePickerModel::FilePickerModel(Preview preview, QObject* parent) :
	QAbstractListModel(parent), m_preview(preview)
{
}

FilePickerModel::~FilePickerModel() = default;

int FilePickerModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant FilePickerModel::data(const QModelIndex& index, int role) const
{
	if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
		return QVariant();
	}
	const Entry& entry = m_entries[index.row()];
	switch (role) {
	case Qt::DisplayRole:
		return entry.rtp ? QCoreApplication::translate("ResourceManagerDialog", "%1 (RTP)").arg(QFileInfo(entry.path).fileName())
			: QFileInfo(entry.path).fileName();
	case Qt::ToolTipRole:
		return QDir::toNativeSeparators(entry.path);
	case NameRole:
		return entry.name;
	case FileNameRole:
		return QFileInfo(entry.path).fileName();
	case PathRole:
		return entry.path;
	case RtpRole:
		return entry.rtp;
	default:
		return QVariant();
	}
}

QHash<int, QByteArray> FilePickerModel::roleNames() const
{
	QHash<int, QByteArray> names = QAbstractListModel::roleNames();
	names.insert(NameRole, "name");
	names.insert(FileNameRole, "fileName");
	names.insert(PathRole, "path");
	names.insert(RtpRole, "rtp");
	return names;
}

void FilePickerModel::open(const QString& folder, const QString& gamePath, const QString& rtpPath, const QString& current)
{
	stop();
	beginResetModel();
	m_folder = folder;
	const int known = ResourceLibraryModel::folderOf(folder);
	m_type = known >= 0 ? ResourceLibraryModel::folderType(known) : FileFinder::FileType::Image;
	m_entries.clear();
	m_current = -1;

	// as ResourceLibraryModel lists them: the game's files hide the RTP's of
	// the same name; in one folder, the extension the Player tries first wins
	const QStringList exts = ResourceLibraryModel::extensions(m_type);
	QHash<QString, int> seen;
	auto scan = [&](const QString& base, bool rtp) {
		if (base.isEmpty() || !QDir(base).exists()) {
			return;
		}
		// the folder whatever its case (Charset, CHARSET...)
		const QString dir = FileFinder::Find(QDir(base), folder, FileFinder::FileType::Default, QDir::Dirs);
		if (dir.isEmpty()) {
			return;
		}
		for (const QFileInfo& info : QDir(dir).entryInfoList(QDir::Files)) {
			const int rank = info.suffix().isEmpty() ? -1 : static_cast<int>(exts.indexOf(QLatin1Char('.') + info.suffix().toLower()));
			if (rank < 0) {
				continue;
			}
			const QString name = info.completeBaseName();
			const QString key = name.toLower();
			auto it = seen.constFind(key);
			if (it == seen.constEnd()) {
				seen.insert(key, static_cast<int>(m_entries.size()));
				m_entries.append({ name, info.absoluteFilePath(), rtp, rank });
			} else if (m_entries[*it].rtp == rtp && rank < m_entries[*it].rank) {
				m_entries[*it] = { name, info.absoluteFilePath(), rtp, rank };
			}
		}
	};
	scan(gamePath, false);
	scan(rtpPath, true);

	QCollator collator;
	collator.setNumericMode(true);
	collator.setCaseSensitivity(Qt::CaseInsensitive);
	std::sort(m_entries.begin(), m_entries.end(), [&](const Entry& a, const Entry& b) {
		return collator.compare(a.name, b.name) < 0;
	});
	// a file is always selected: the current one, else the first
	m_current = rowOf(current);
	if (m_current < 0 && !m_entries.isEmpty()) {
		m_current = 0;
	}
	endResetModel();
	emit filesChanged();
	updateCurrent();
}

void FilePickerModel::setCurrentRow(int row)
{
	if (row < 0 || row >= m_entries.size() || row == m_current) {
		return;
	}
	m_current = row;
	updateCurrent();
	// like RPG Maker: while listening, the file chosen is played
	if (playing()) {
		play();
	}
}

int FilePickerModel::rowOf(const QString& name) const
{
	if (name.isEmpty()) {
		return -1;
	}
	for (int i = 0; i < m_entries.size(); ++i) {
		if (m_entries[i].name.compare(name, Qt::CaseInsensitive) == 0) {
			return i;
		}
	}
	return -1;
}

QString FilePickerModel::currentName() const
{
	return m_current >= 0 ? m_entries[m_current].name : QString();
}

QString FilePickerModel::currentPath() const
{
	return m_current >= 0 ? m_entries[m_current].path : QString();
}

bool FilePickerModel::currentIsRtp() const
{
	return m_current >= 0 && m_entries[m_current].rtp;
}

void FilePickerModel::updateCurrent()
{
	m_info.clear();
	m_imageSize = QSize();
	const QString path = currentPath();
	if (!path.isEmpty()) {
		const QImage image = m_type == FileFinder::FileType::Image ? ImageImport::loadImage(path) : QImage();
		m_imageSize = image.size();
		m_info = ResourceLibraryModel::fileInfo(path, currentIsRtp(), image).join(QLatin1Char('\n'));
	}
	emit currentChanged();
}

void FilePickerModel::setSpriteIndex(int index)
{
	if (index != m_index) {
		m_index = index;
		emit choiceChanged();
	}
}

void FilePickerModel::setDirection(int direction)
{
	direction = std::clamp(direction, 0, 3);
	if (direction != m_direction) {
		m_direction = direction;
		emit choiceChanged();
	}
}

void FilePickerModel::setPattern(int pattern)
{
	pattern = std::clamp(pattern, 0, 2);
	if (pattern != m_pattern) {
		m_pattern = pattern;
		emit choiceChanged();
	}
}

void FilePickerModel::setHue(int hue)
{
	hue = std::clamp(hue, 0, 359);
	if (hue != m_hue) {
		m_hue = hue;
		emit choiceChanged();
	}
}

void FilePickerModel::setFadeIn(int ms)
{
	ms = std::clamp(ms, 0, 10000);
	if (ms != m_fadeIn) {
		m_fadeIn = ms;
		emit choiceChanged();
	}
}

// the settings are heard at once
void FilePickerModel::setVolume(int volume)
{
	volume = std::clamp(volume, 0, 100);
	if (volume != m_volume) {
		m_volume = volume;
		if (m_player) {
			m_player->setVolume(volume);
		}
		emit choiceChanged();
	}
}

void FilePickerModel::setTempo(int tempo)
{
	tempo = std::clamp(tempo, 50, 150);
	if (tempo != m_tempo) {
		m_tempo = tempo;
		if (m_player) {
			m_player->setTempo(tempo);
		}
		emit choiceChanged();
	}
}

void FilePickerModel::setBalance(int balance)
{
	balance = std::clamp(balance, 0, 100);
	if (balance != m_balance) {
		m_balance = balance;
		if (m_player) {
			m_player->setBalance(balance);
		}
		emit choiceChanged();
	}
}

AudioPlayer* FilePickerModel::player()
{
	if (!m_player) {
		m_player = new AudioPlayer(this);
		connect(m_player, &AudioPlayer::stateChanged, this, [this](AudioPlayer::State state) {
			if (state == AudioPlayer::State::Stopped && m_player->errorString().isEmpty()) {
				m_playStatus.clear();
			}
			emit playingChanged();
		});
		connect(m_player, &AudioPlayer::errorOccurred, this, [this](const QString& message) {
			m_playStatus = message;
			emit playingChanged();
		});
	}
	return m_player;
}

void FilePickerModel::play()
{
	const QString path = currentPath();
	if (path.isEmpty() || !QFileInfo(path).isFile()) {
		m_playStatus = QCoreApplication::translate("PickerAudioWidget", "Choose a file to play.");
		emit playingChanged();
		return;
	}
	AudioPlayer* audio = player();
	audio->setVolume(m_volume);
	audio->setTempo(m_tempo);
	audio->setBalance(m_balance);
	const bool music = m_type == FileFinder::FileType::Music;
	audio->setFadeIn(music ? m_fadeIn : 0);
	audio->setLooping(music);
	if (audio->play(path)) {
		m_playStatus = QCoreApplication::translate("PickerAudioWidget", "Playing %1").arg(QFileInfo(path).fileName());
	}
	emit playingChanged();
}

void FilePickerModel::stop()
{
	if (m_player) {
		m_player->stop();
	}
}

bool FilePickerModel::playing() const
{
	return m_player && m_player->state() != AudioPlayer::State::Stopped;
}

void FilePickerModel::apply()
{
	emit applied();
}
