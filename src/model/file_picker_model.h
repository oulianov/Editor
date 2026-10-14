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

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QSize>
#include <QString>
#include "common/filefinder.h"

class AudioPlayer;

/**
 * A resource picker without its window (the "..." of a graphic, a music, a
 * movie): the files of a folder of the game and of the RTP, one per name
 * (the game's hide the RTP's, the extension the Player tries first wins),
 * sorted as the file windows of the systems (natural, any case).
 *
 * A file of the list is always selected: the current one, else the first
 * of the list (no file yet, or a missing one).
 *
 * It keeps what is chosen with the file (the part of the image, the
 * settings of a sound) and plays the music and sounds with those settings.
 */
class FilePickerModel : public QAbstractListModel
{
	Q_OBJECT
	/** The folder of the game listed (CharSet...) */
	Q_PROPERTY(QString folder READ folder NOTIFY filesChanged)
	Q_PROPERTY(Preview preview READ preview CONSTANT)
	Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentChanged)
	/** The file selected, without its extension (as the data names it); empty when none */
	Q_PROPERTY(QString currentName READ currentName NOTIFY currentChanged)
	Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentChanged)
	Q_PROPERTY(bool currentIsRtp READ currentIsRtp NOTIFY currentChanged)
	/** What the file selected is, line by line (ResourceLibraryModel::fileInfo) */
	Q_PROPERTY(QString info READ info NOTIFY currentChanged)
	/** The size of the image selected (empty: none, or not an image) */
	Q_PROPERTY(QSize imageSize READ imageSize NOTIFY currentChanged)

	// what is chosen with the file
	/** The character of a CharSet (0 to 7), the face of a FaceSet (0 to 15) */
	Q_PROPERTY(int spriteIndex READ spriteIndex WRITE setSpriteIndex NOTIFY choiceChanged)
	/** 0 up, 1 right, 2 down, 3 left */
	Q_PROPERTY(int direction READ direction WRITE setDirection NOTIFY choiceChanged)
	/** 0 left, 1 middle, 2 right */
	Q_PROPERTY(int pattern READ pattern WRITE setPattern NOTIFY choiceChanged)
	/** The color alteration of a monster, 0 to 359 degrees */
	Q_PROPERTY(int hue READ hue WRITE setHue NOTIFY choiceChanged)
	/** Of a music, in milliseconds */
	Q_PROPERTY(int fadeIn READ fadeIn WRITE setFadeIn NOTIFY choiceChanged)
	Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY choiceChanged)
	Q_PROPERTY(int tempo READ tempo WRITE setTempo NOTIFY choiceChanged)
	Q_PROPERTY(int balance READ balance WRITE setBalance NOTIFY choiceChanged)

	/** The music or sound selected is being played */
	Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
	/** What the player says: the file played, or why it cannot */
	Q_PROPERTY(QString playStatus READ playStatus NOTIFY playingChanged)

public:
	/** What the picker shows of the file selected, besides its name */
	enum class Preview {
		/** The whole image (backdrops, chipsets, animations...) */
		Image,
		/** A character of a CharSet: index */
		CharSet,
		/** A pose of a character of a CharSet: index, direction and pattern */
		CharSetPose,
		/** A face of a FaceSet: index */
		FaceSet,
		/** A monster, with its hue */
		Monster,
		/** Played: fade-in, volume, tempo, balance */
		Music,
		/** Played: volume, tempo, balance */
		Sound,
		/** Nothing (movies) */
		None,
	};
	Q_ENUM(Preview)

	enum Role {
		/** Without its extension ("Actor1") */
		NameRole = Qt::UserRole + 1,
		/** With its extension ("Actor1.png"; DisplayRole: "Actor1.png (RTP)" for the RTP's) */
		FileNameRole,
		PathRole,
		/** A file of the RTP, not of the game */
		RtpRole,
	};

	explicit FilePickerModel(Preview preview = Preview::Image, QObject* parent = nullptr);
	~FilePickerModel() override;

	// QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	/**
	 * Lists a folder and selects a file (by its name, any case), else the
	 * first file of the list
	 *
	 * @param folder The folder of the game (CharSet...), which says the kind of files
	 * @param gamePath The folder of the game (empty: none)
	 * @param rtpPath The folder of the RTP (empty: none)
	 * @param current The file of the data, without its extension
	 */
	void open(const QString& folder, const QString& gamePath, const QString& rtpPath, const QString& current);

	QString folder() const { return m_folder; }
	Preview preview() const { return m_preview; }
	/** The kind of files listed (from the folder) */
	FileFinder::FileType fileType() const { return m_type; }

	int currentRow() const { return m_current; }
	void setCurrentRow(int row);
	/** The row of a file (any case), -1 when it is not listed */
	Q_INVOKABLE int rowOf(const QString& name) const;
	/** The first row from the row from on (wrapping) whose file starts with text (any case), -1 when none */
	Q_INVOKABLE int rowStartingWith(const QString& text, int from) const;
	QString currentName() const;
	QString currentPath() const;
	bool currentIsRtp() const;
	QString info() const { return m_info; }
	QSize imageSize() const { return m_imageSize; }

	int spriteIndex() const { return m_index; }
	void setSpriteIndex(int index);
	int direction() const { return m_direction; }
	void setDirection(int direction);
	int pattern() const { return m_pattern; }
	void setPattern(int pattern);
	int hue() const { return m_hue; }
	void setHue(int hue);
	int fadeIn() const { return m_fadeIn; }
	void setFadeIn(int ms);
	int volume() const { return m_volume; }
	void setVolume(int volume);
	int tempo() const { return m_tempo; }
	void setTempo(int tempo);
	int balance() const { return m_balance; }
	void setBalance(int balance);

	/** Plays the music or sound selected with the settings chosen */
	Q_INVOKABLE void play();
	Q_INVOKABLE void stop();
	bool playing() const;
	QString playStatus() const { return m_playStatus; }

	/** Apply: the choice is given to the data, the picker stays (applied) */
	Q_INVOKABLE void apply();

signals:
	void filesChanged();
	void currentChanged();
	void choiceChanged();
	void playingChanged();
	/** Apply was pressed */
	void applied();

private:
	struct Entry {
		QString name;
		QString path;
		bool rtp = false;
		/** The extension's place in the Player's order */
		int rank = 0;
	};

	void updateCurrent();
	AudioPlayer* player();

	Preview m_preview;
	FileFinder::FileType m_type = FileFinder::FileType::Image;
	QString m_folder;
	QList<Entry> m_entries;
	int m_current = -1;
	QString m_info;
	QSize m_imageSize;

	int m_index = 0;
	int m_direction = 2;
	int m_pattern = 1;
	int m_hue = 0;
	int m_fadeIn = 0;
	int m_volume = 100;
	int m_tempo = 100;
	int m_balance = 50;

	/** Made when something is played first */
	AudioPlayer* m_player = nullptr;
	QString m_playStatus;
};
