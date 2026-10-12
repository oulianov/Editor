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

#include "picker_dialog.h"
#include "ui/common/help.h"
#include "ui_picker_dialog.h"
#include "model/project.h"
#include "common/image_loader.h"
#include "picker_child_widget.h"
#include "ui/common/resource_info.h"
#include "ui/common/preview_zoom.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <algorithm>
#include <QCollator>
#include <QDir>
#include <QFileSystemModel>
#include <algorithm>
#include <QGraphicsScene>
#include <QAbstractButton>
#include <QDialogButtonBox>

PickerDialog::PickerDialog(ProjectData &project, FileFinder::FileType file_type, PickerChildWidget* wrappedWidget, QWidget *parent) :
		QDialog(parent),
		ui(new Ui::PickerDialog),
		m_project(project),
		m_file_type(file_type) {
	ui->setupUi(this);
	// the separator gives the preview more room; the list takes what the window gains
	ui->splitterPicker->setStretchFactor(0, 1);
	ui->splitterPicker->setStretchFactor(1, 0);
	wrappedWidget->setParent(this);
	ui->wrappedWidget = wrappedWidget;
	ui->sidebar->insertWidget(1, wrappedWidget);
	// what the file is, under its preview (as in the Resource Manager)
	m_info = new QLabel(this);
	m_info->setObjectName(QStringLiteral("labelInfo"));
	m_info->setWordWrap(true);
	m_info->setTextInteractionFlags(Qt::TextSelectableByMouse);

	ui->filesystemView->setRootIsDecorated(false);
	ui->filesystemView->setHeaderHidden(true);

	m_model = new QFileSystemModel(this);
	m_model->setReadOnly(true);
	// only the files of the kind asked (no desktop.ini, Thumbs.db...)
	if (file_type != FileFinder::FileType::Default) {
		QStringList filters;
		for (const QString& extension : FileFinder::GetFiltersForType(file_type)) {
			filters << QStringLiteral("*") + extension;
		}
		m_model->setFilter(QDir::Files | QDir::NoDotAndDotDot);
		m_model->setNameFilters(filters);
		m_model->setNameFilterDisables(false);
	}
	ui->filesystemView->setModel(m_model);
	// the preview follows the file shown, with the mouse or the arrow keys
	QObject::connect(ui->filesystemView->selectionModel(), &QItemSelectionModel::currentChanged,
		this, [this](const QModelIndex& index) { showFile(index); });

	// Hide columns except name
	for (int i = 1; i <= 3; ++i) {
		ui->filesystemView->setColumnHidden(i, true);
	}

	QObject::connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &PickerDialog::buttonClicked);

	if (file_type == FileFinder::FileType::Image) {
		ui->wrappedWidget->setView(ui->graphicsView);
		QObject::connect(ui->graphicsView, &RpgGraphicsViewBase::clicked, this, &PickerDialog::viewClicked);
		// a closer look, as when importing an image
		m_zoom = new PreviewZoom(ui->graphicsView, this);
		ui->sidebar->insertLayout(1, PreviewZoom::infoRow(m_info, m_zoom));
	} else {
		ui->sidebar->insertWidget(1, m_info);
		ui->graphicsView->setVisible(false);
	}
}

void PickerDialog::showInfo() {
	const QString path = m_currentFile.absoluteFilePath();
	if (m_currentFile.fileName().isEmpty() || !m_currentFile.isFile()) {
		m_info->clear();
		return;
	}
	// the RTP's files are outside the game
	const bool rtp = !path.startsWith(m_project.project().projectDir().absolutePath() + QLatin1Char('/'));
	const QImage image = m_file_type == FileFinder::FileType::Image ? ImportImageDialog::loadImage(path) : QImage();
	m_info->setText(resourceInfo(path, rtp, image).join(QLatin1Char('\n')));
}

PickerDialog::~PickerDialog() {
	delete ui;
}

void PickerDialog::buttonClicked(QAbstractButton* button) {
	switch (ui->buttonBox->standardButton(button)) {
		case QDialogButtonBox::Ok:
			emit fileSelected(m_currentFile.baseName());
			QDialog::accept();
			break;
		case QDialogButtonBox::Cancel:
		case QDialogButtonBox::Close:
			QDialog::reject();
			break;
		case QDialogButtonBox::Apply:
			emit fileSelected(m_currentFile.baseName());
			break;
		case QDialogButtonBox::Help:
			Help::open(this, QStringLiteral("resource-picker"));
			break;
		default:
			break;
	}
}

void PickerDialog::setDirectoryAndFile(const QString &dir, const QString& initialFile) {
	m_dir.setPath(dir);
	// the folder chosen in (Panorama, CharSet...), unless the caller named it
	if (windowTitle().isEmpty()) {
		setWindowTitle(tr("Select from %1").arg(QFileInfo(dir).fileName()));
	}
	QString path = m_project.project().findDirectory(dir);
	QString file = m_project.project().findFile(dir, initialFile, m_file_type);
	m_model->setRootPath(path);
	ui->filesystemView->setRootIndex(m_model->index(path));

	QModelIndex index = m_model->index(file);
	ui->filesystemView->setCurrentIndex(index);

	m_currentFile = QFileInfo(file);
	showInfo();

	ui->wrappedWidget->fileChanged(m_currentFile.absoluteFilePath());

	if (m_file_type == FileFinder::FileType::Image) {
		QPixmap image = ImageLoader::Load(m_currentFile.absoluteFilePath());
		ui->wrappedWidget->imageChanged(image);
	}
}

QString PickerDialog::firstFile(const QString& dir) const {
	const QString path = m_project.project().findDirectory(dir);
	if (path.isEmpty()) {
		return {};
	}
	QStringList files = QDir(path).entryList(m_model->nameFilters(), QDir::Files | QDir::NoDotAndDotDot);
	// sorted as QFileSystemModel does: natural, case-insensitive
	QCollator collator;
	collator.setNumericMode(true);
	collator.setCaseSensitivity(Qt::CaseInsensitive);
	std::sort(files.begin(), files.end(), collator);
	return files.isEmpty() ? QString() : QFileInfo(files.front()).completeBaseName();
}

void PickerDialog::showFile(const QModelIndex &index) {
	if (!index.isValid()) {
		return;
	}
	m_currentFile = m_model->fileInfo(index);
	showInfo();
	ui->wrappedWidget->fileChanged(m_currentFile.absoluteFilePath());
	if (m_file_type == FileFinder::FileType::Image) {
		QPixmap image = ImageLoader::Load(m_currentFile.absoluteFilePath());
		ui->wrappedWidget->imageChanged(image);
	}
}

void PickerDialog::viewClicked(const QPointF& pos) {
	ui->wrappedWidget->clicked(pos);
}
