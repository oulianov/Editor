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

#include <QDialog>

class AudioPlayer;
class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

/**
 * Tools > Jukebox: listens to the music and sound effects of the game and of
 * the RTP, like RPG Maker's music and sound test windows. Not modal: it can
 * stay open while editing; it stops playing when closed.
 */
class JukeboxDialog : public QDialog {
	Q_OBJECT

public:
	explicit JukeboxDialog(QWidget* parent = nullptr);
	~JukeboxDialog() override;

	/** Lists the files of the project open (and of the RTP) again */
	void refresh();

protected:
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	enum class Kind { Music, Sound };

	QTreeWidget* createList(const QString& name);
	void fillList(Kind kind);
	void applyFilter();
	QTreeWidget* currentList() const;
	Kind currentKind() const;

	void tabChanged();
	void itemSelected(QTreeWidgetItem* item);
	void playItem(QTreeWidgetItem* item);
	void playSelected();
	void pauseOrResume();
	void stop();

	void updateButtons();
	void updatePosition(qint64 ms);
	void updateDuration(qint64 ms);
	void showStatus(const QString& text, bool error = false);
	QString timeText(qint64 ms) const;

	AudioPlayer* m_player = nullptr;
	// the file shown as playing, for the status, and its tab
	QString m_playingName;
	Kind m_playingKind = Kind::Music;

	QLineEdit* m_filter = nullptr;
	QTabWidget* m_tabs = nullptr;
	QTreeWidget* m_listMusic = nullptr;
	QTreeWidget* m_listSound = nullptr;

	QPushButton* m_play = nullptr;
	QPushButton* m_pause = nullptr;
	QPushButton* m_stop = nullptr;
	QCheckBox* m_loop = nullptr;
	QSlider* m_position = nullptr;
	QLabel* m_time = nullptr;
	QLabel* m_status = nullptr;
	bool m_updatingPosition = false;

	QGroupBox* m_groupFadeIn = nullptr;
	QSlider* m_fadeIn = nullptr;
	QSlider* m_volume = nullptr;
	QSlider* m_tempo = nullptr;
	QSlider* m_balance = nullptr;

	// loop on for music, off for sounds; remembered per tab
	bool m_loopMusic = true;
	bool m_loopSound = false;
};
