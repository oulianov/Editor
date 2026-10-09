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
#include <QImage>
#include "common/filefinder.h"

class QTreeWidgetItem;

namespace Ui {
class ResourceManagerDialog;
}

/**
 * Tools > Resource Manager, as in RPG Maker: the files of each folder of the
 * game and of the RTP, with a preview, Import (images get their transparent
 * color chosen and their size checked), Export and Delete (game files only).
 */
class ResourceManagerDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ResourceManagerDialog(QWidget *parent = nullptr);
	~ResourceManagerDialog() override;

	/**
	 * AUDIO HOOK: plays a Music or Sound file of the preview, an empty path
	 * stops. The shared audio player is connected here (see the .cpp); until
	 * then it only emits audioPreviewRequested.
	 */
	void previewAudio(const QString& path);

signals:
	/** Emitted by previewAudio(): a path to play, empty to stop */
	void audioPreviewRequested(const QString& path);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private slots:
	void folderChanged();
	void fileChanged();
	void applyFilter();
	void importFiles();
	void exportFile();
	void deleteFile();

private:
	class AudioPlayer* m_player = nullptr;
	struct Folder {
		const char* name;
		FileFinder::FileType type;
		/** palette index 0 is transparent in the game */
		bool transparent;
		const char* description;
	};
	static const Folder folders[];

	const Folder* currentFolder() const;
	QString gameFolderPath(const Folder& folder) const;
	QString rtpFolderPath(const Folder& folder) const;
	static QStringList extensions(FileFinder::FileType type);

	void refreshFolders();
	void refreshFiles(const QString& select = QString());
	void updateButtons();
	void showPreview(const QString& path, bool rtp);

	QString currentPath() const;
	bool currentIsRtp() const;

	bool importImage(const Folder& folder, const QString& source, const QString& dir);
	bool importFile(const Folder& folder, const QString& source, const QString& dir);
	/** Asks before replacing a game file of the same name; removes the old ones */
	bool makeRoom(const Folder& folder, const QString& dir, const QString& name);
	static QString sizeRule(const Folder& folder, QSize size);

	Ui::ResourceManagerDialog *ui;
};
