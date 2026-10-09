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

#include "convert_project_dialog.h"
#include "model/project.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QVBoxLayout>

namespace {

// the folders of the projects whose conversion was declined for good
constexpr auto kDeclinedKey = "convert_2003_declined";

QString projectKey(const Project& project) {
	return project.projectDir().absolutePath();
}

QLabel* makeLabel(const QString& text, QWidget* parent) {
	auto* label = new QLabel(text, parent);
	label->setWordWrap(true);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	return label;
}

}

ConvertProjectDialog::ConvertProjectDialog(const Project& project, bool at_open, QWidget* parent) :
	QDialog(parent)
{
	setObjectName(QStringLiteral("ConvertProjectDialog"));
	setWindowTitle(tr("Convert to RPG Maker 2003"));
	auto* layout = new QVBoxLayout(this);

	// RPG Maker 2003's own question
	auto* question = makeLabel(tr("Game data will be converted from RPG Maker 2000 format to RPG Maker 2003 format. "
		"This cannot be reversed. Are you sure you want to proceed?"), this);
	QFont bold = question->font();
	bold.setBold(true);
	question->setFont(bold);
	layout->addWidget(question);

	int maps = 0;
	for (const auto& info : project.treeMap().maps) {
		maps += info.ID > 0 && info.type == lcf::rpg::TreeMap::MapType_map;
	}
	auto* changes = makeLabel(QStringLiteral("<ul style=\"margin-left: 0px; -qt-list-indent: 1;\"><li>%1</li><li>%2</li><li>%3</li><li>%4</li></ul>").arg(
		tr("The database gets the data of RPG Maker 2003: the battle commands of RPG Maker 2000 "
		   "(Attack, Skill, Defend, Item, Escape) for every hero, the 32 battler animations of "
		   "RPG Maker 2003 (graphics of its RTP), a System2 graphic, the menu of RPG Maker 2000 "
		   "and the terms that RPG Maker 2000 does not have (Status, Row, Order...)."),
		tr("The game uses the battles of RPG Maker 2003 (side view, active time), its experience "
		   "curve and its level limit (99)."),
		tr("The database, the map tree and the maps (%1) are written in the format of RPG Maker 2003: "
		   "RPG Maker 2000 cannot open the project anymore.").arg(maps),
		tr("RPG_RT.exe, the runtime of RPG Maker 2000, cannot run the game anymore: it is moved to the "
		   "backup. Copy the RPG_RT.exe of RPG Maker 2003 into the game folder to play the game with it; "
		   "the EasyRPG Player plays it without.")), this);
	changes->setTextFormat(Qt::RichText);
	layout->addWidget(changes);

	auto* backup = new QGroupBox(tr("Backup"), this);
	auto* backup_layout = new QVBoxLayout(backup);
	const QString backup_path = project.projectDir().filePath(ProjectConvert::backupFolderName());
	backup_layout->addWidget(makeLabel(tr("Before the conversion, RPG_RT.ldb, RPG_RT.lmt and the maps are copied to a "
		"new folder of the game:\n%1").arg(QDir::toNativeSeparators(backup_path)), backup));
	layout->addWidget(backup);

	auto* player = new QGroupBox(tr("EasyRPG Player"), this);
	auto* player_layout = new QVBoxLayout(player);
	player_layout->addWidget(makeLabel(tr("Settings of the EasyRPG Player only: RPG_RT and RPG Maker 2003 ignore them."), player));
	m_keepBattle = new QCheckBox(tr("Keep the battles of RPG Maker 2000"), player);
	m_keepBattle->setObjectName(QStringLiteral("checkKeepBattle"));
	m_keepBattle->setToolTip(tr("Front view battles, with the Fight, Auto and Escape commands of the party"));
	player_layout->addWidget(m_keepBattle);
	m_keepExperience = new QCheckBox(tr("Keep the experience curve of RPG Maker 2000"), player);
	m_keepExperience->setObjectName(QStringLiteral("checkKeepExperience"));
	m_keepExperience->setToolTip(tr("The heroes need the experience of RPG Maker 2000 to reach each level"));
	player_layout->addWidget(m_keepExperience);
	layout->addWidget(player);

	if (at_open) {
		m_dontAsk = new QCheckBox(tr("Don't ask again for this project"), this);
		m_dontAsk->setObjectName(QStringLiteral("checkDontAsk"));
		m_dontAsk->setToolTip(tr("Game Project > Convert to RPG Maker 2003 converts it later"));
		layout->addWidget(m_dontAsk);
	}

	layout->addStretch();
	auto* buttons = new QDialogButtonBox(this);
	buttons->addButton(tr("Convert"), QDialogButtonBox::AcceptRole);
	if (at_open) {
		buttons->addButton(tr("Not Now"), QDialogButtonBox::RejectRole);
	} else {
		buttons->addButton(QDialogButtonBox::Cancel);
	}
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
	setMinimumWidth(520);
}

ProjectConvert::Options ConvertProjectDialog::options() const
{
	ProjectConvert::Options options;
	options.keep2kBattleSystem = m_keepBattle->isChecked();
	options.keep2kExperience = m_keepExperience->isChecked();
	return options;
}

bool ConvertProjectDialog::dontAskAgain() const
{
	return m_dontAsk && m_dontAsk->isChecked();
}

bool ConvertProjectDialog::askAtOpen(const Project& project)
{
	// asked once per session: the project is opened again after a failed conversion
	static QSet<QString> asked;
	const QString key = projectKey(project);
	if (!ProjectConvert::canConvert(project) || asked.contains(key)
			|| QSettings().value(kDeclinedKey).toStringList().contains(key)) {
		return false;
	}
	asked.insert(key);
	return true;
}

void ConvertProjectDialog::setAskAtOpen(const Project& project, bool ask)
{
	QSettings settings;
	QStringList declined = settings.value(kDeclinedKey).toStringList();
	const QString key = projectKey(project);
	declined.removeAll(key);
	if (!ask) {
		declined.append(key);
	}
	settings.setValue(kDeclinedKey, declined);
}

void ConvertProjectDialog::showResult(QWidget* parent, const QString& error, const QString& backup_path)
{
	if (!error.isEmpty()) {
		QMessageBox::critical(parent, tr("Convert to RPG Maker 2003"),
			tr("The project was not converted.\n\n%1").arg(error));
		return;
	}
	QMessageBox::information(parent, tr("Convert to RPG Maker 2003"),
		tr("The project is now an RPG Maker 2003 project.\n\n"
		   "The files of RPG Maker 2000 are in:\n%1").arg(QDir::toNativeSeparators(backup_path)));
}
