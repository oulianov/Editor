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

#include "open_project_dialog.h"
#include "ui_open_project_dialog.h"
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QSettings>
#include <QStyle>
#include <QtConcurrentRun>
#include "core.h"
#include "model/project.h"
#include "project_settings_dialog.h"

OpenProjectDialog::OpenProjectDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::OpenProjectDialog)
{
	setModal(true);
	ui->setupUi(this);
	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
	ui->tableProjects->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

OpenProjectDialog::~OpenProjectDialog()
{
	delete ui;
}

void OpenProjectDialog::setDefaultDir(const QString& n_defDir)
{
	m_defaultDir = n_defDir;
	ui->lineProjectPath->setText(n_defDir);
	refreshProjectList();
}

QString OpenProjectDialog::getDefaultDir()
{
	return m_defaultDir;
}

std::shared_ptr<Project> OpenProjectDialog::getProject() {
	const int row = ui->tableProjects->currentRow();
	if (row >= 0 && static_cast<size_t>(row) < prjList.size()) {
		return prjList[static_cast<size_t>(row)];
	}

	return nullptr;
}

void OpenProjectDialog::refreshProjectList()
{
	prjList.clear();
	ui->tableProjects->setRowCount(0);
	ui->tableProjects->setEnabled(false);
	ui->tableProjects->setCursor(Qt::BusyCursor);
	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

	auto* scan = new QFutureWatcher<Project::ProjectList>(this);
	m_projectScan = scan;
	connect(scan, &QFutureWatcher<Project::ProjectList>::finished, this, [this, scan] {
		scan->deleteLater();
		// A path change can start another scan before this one finishes.
		if (m_projectScan != scan) {
			return;
		}
		m_projectScan = nullptr;
		prjList = scan->result();
		ui->tableProjects->setEnabled(true);
		ui->tableProjects->unsetCursor();

		for (const auto& prj: prjList) {
			ui->tableProjects->insertRow(ui->tableProjects->rowCount());
			QTableWidgetItem* item = new QTableWidgetItem(prj->projectDir().dirName());
			item->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
			item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			ui->tableProjects->setItem(ui->tableProjects->rowCount() - 1, 0, item);

			item = new QTableWidgetItem(prj->gameTitle());
			item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
			ui->tableProjects->setItem(ui->tableProjects->rowCount() - 1, 1, item);
		}

		if (ui->tableProjects->rowCount() > 0) {
			ui->tableProjects->setCurrentItem(ui->tableProjects->item(0, 1));
		}
	});
	// Only filesystem work runs in the worker; widgets are updated above.
	scan->setFuture(QtConcurrent::run(&Project::enumerate, QDir(m_defaultDir)));
}

void OpenProjectDialog::on_toolProjectPath_clicked()
{
	auto* dialog = new QFileDialog(this, tr("Select destination folder"), m_defaultDir);
	dialog->setFileMode(QFileDialog::Directory);
	dialog->setOption(QFileDialog::ShowDirsOnly);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	ui->toolProjectPath->setEnabled(false);
	connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
		ui->toolProjectPath->setEnabled(true);
		if (result != QDialog::Accepted) {
			return;
		}
		const QString path = dialog->selectedFiles().value(0);
		if (!path.isEmpty()) {
			setDefaultDir(path + "/");
		}
	});
	// A native picker inside this modal window must not start another
	// blocking event loop (in particular on macOS).
	dialog->open();
}

std::shared_ptr<Project> OpenProjectDialog::closedProject()
{
	auto project = getProject();
	if (!project) {
		return nullptr;
	}
	const auto& open = core().project();
	if (open && QDir(open->projectDir().absolutePath()) == QDir(project->projectDir().absolutePath())) {
		QMessageBox::information(this, windowTitle(),
			tr("%1 is the project open in the Editor: change it with Game Project > Project Settings, "
				"or close it to delete it.").arg(project->projectDir().dirName()));
		return nullptr;
	}
	return project;
}

void OpenProjectDialog::on_pushRename_clicked()
{
	auto project = closedProject();
	if (!project) {
		return;
	}
	// the title and the code page as the game is read
	project->detectEncoding();
	ProjectSettingsDialog dialog(*project, false, this);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	const QString error = dialog.writeSettings(*project);
	if (!error.isEmpty()) {
		QMessageBox::warning(this, windowTitle(), error);
	}
	if (dialog.folderName() != project->projectDir().dirName()) {
		QDir parent = project->projectDir();
		parent.cdUp();
		if (!parent.rename(project->projectDir().dirName(), dialog.folderName())) {
			QMessageBox::warning(this, windowTitle(), tr("The folder could not be renamed."));
		}
	}
	refreshProjectList();
}

void OpenProjectDialog::on_pushImport_clicked()
{
	// the main window imports, in the folder of the projects
	done(ImportRequested);
}

void OpenProjectDialog::on_pushDelete_clicked()
{
	auto project = closedProject();
	if (!project) {
		return;
	}
	const QString path = project->projectDir().absolutePath();
	if (QMessageBox::warning(this, windowTitle(),
			tr("Move the project %1 (%2) and all its files to the trash?").arg(project->gameTitle(), QDir::toNativeSeparators(path)),
			QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
		return;
	}
	// to the trash, never erased: RPG Maker asks the same, the files stay recoverable
	if (!QFile::moveToTrash(path)) {
		QMessageBox::warning(this, windowTitle(), tr("%1 could not be moved to the trash.").arg(QDir::toNativeSeparators(path)));
	}
	refreshProjectList();
}

void OpenProjectDialog::on_tableProjects_cellDoubleClicked(int row, int column)
{
	Q_UNUSED(row)
	Q_UNUSED(column)
	if (getProject()) {
		emit ui->buttonBox->accepted();
	}
}

void OpenProjectDialog::on_tableProjects_itemSelectionChanged()
{
	const bool selected = !ui->tableProjects->selectedItems().empty();
	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(selected);
	ui->pushRename->setEnabled(selected);
	ui->pushDelete->setEnabled(selected);
}
