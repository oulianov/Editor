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

#include "rename_project_dialog.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QVBoxLayout>

RenameProjectDialog::RenameProjectDialog(const QDir& folder, const QString& title, QWidget* parent) :
	QDialog(parent), m_folder(folder)
{
	setObjectName(QStringLiteral("RenameProjectDialog"));
	setWindowTitle(tr("Rename Project"));
	auto* layout = new QVBoxLayout(this);
	auto* form = new QFormLayout();
	m_folderName = new QLineEdit(folder.dirName(), this);
	m_folderName->setObjectName(QStringLiteral("lineFolderName"));
	form->addRow(tr("Directory Name"), m_folderName);
	m_title = new QLineEdit(title, this);
	m_title->setObjectName(QStringLiteral("lineGameTitle"));
	form->addRow(tr("Game Name"), m_title);
	layout->addLayout(form);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
	setMinimumWidth(360);
}

QString RenameProjectDialog::folderName() const
{
	return m_folderName->text().trimmed();
}

QString RenameProjectDialog::gameTitle() const
{
	return m_title->text();
}

void RenameProjectDialog::accept()
{
	const QString name = folderName();
	// a folder name usable on every system, and not taken
	static const QRegularExpression invalid(QStringLiteral(R"([<>:"/\\|?*]|^\.+$)"));
	if (name.isEmpty() || name.contains(invalid)) {
		QMessageBox::warning(this, windowTitle(), tr("\"%1\" is not a valid folder name.").arg(name));
		return;
	}
	if (name != m_folder.dirName()) {
		QDir parent = m_folder;
		parent.cdUp();
		if (parent.exists(name)) {
			QMessageBox::warning(this, windowTitle(),
				tr("The folder %1 already exists. Choose another folder name.").arg(name));
			return;
		}
	}
	QDialog::accept();
}
