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
#include <QDir>
#include <QMouseEvent>

#include "common/filefinder.h"

class ProjectData;
class QFileSystemModel;
class QLabel;
class QAbstractButton;
class PickerChildWidget;
class PreviewZoom;

namespace Ui {
class PickerDialog;
}

class PickerDialog : public QDialog {
	Q_OBJECT

public:
	explicit PickerDialog(ProjectData& project, FileFinder::FileType file_type = FileFinder::FileType::Default, PickerChildWidget* wrappedWidget = nullptr, QWidget *parent = nullptr);
	~PickerDialog() override;

	void setDirectoryAndFile(const QString& dir, const QString& initialFile);
	/** The first file of the folder of the game, in the order of the list (none: empty) */
	QString firstFile(const QString& dir) const;

signals:
	void fileSelected(QString baseName);

private slots:
	/** The file shown (a click, the arrow keys): its preview */
	void showFile(const QModelIndex &index);
	void buttonClicked(QAbstractButton* button);
	void viewClicked(const QPointF& pos);

protected:
	Ui::PickerDialog *ui;

private:
	ProjectData& m_project;
	QDir m_dir;
	QFileInfo m_currentFile;
	QFileSystemModel* m_model = nullptr;
	/** What the file shown is (resourceInfo) */
	QLabel* m_info = nullptr;
	/** The zoom of the preview, as when importing an image */
	PreviewZoom* m_zoom = nullptr;
	void showInfo();
	FileFinder::FileType m_file_type;
};
