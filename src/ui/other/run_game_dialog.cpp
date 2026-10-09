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

#include "core.h"
#include "common/dbstring.h"
#include "run_game_dialog.h"
#include "common/editor_settings.h"
#include "ui_run_game_dialog.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>

namespace {
std::function<bool()> before_launch;

/** The program inside a macOS application bundle, or the path itself */
QString executableOf(const QString& path) {
	QFileInfo info(QDir::cleanPath(path));
	if (info.isDir() && info.suffix().compare(QStringLiteral("app"), Qt::CaseInsensitive) == 0) {
		QDir macos(info.absoluteFilePath() + QStringLiteral("/Contents/MacOS"));
		const QStringList files = macos.entryList(QDir::Files | QDir::Executable);
		if (files.contains(info.completeBaseName())) {
			return macos.filePath(info.completeBaseName());
		}
		return files.isEmpty() ? QString() : macos.filePath(files.first());
	}
	return info.isFile() && info.isExecutable() ? info.absoluteFilePath() : QString();
}
}

RunGameDialog::RunGameDialog(ProjectData& project, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::RunGameDialog),
	m_project(project)
{
	ui->setupUi(this);

	// The Player has no command line option for these
	ui->checkDisableEncounters->setEnabled(false);
	ui->checkDisableEncounters->setToolTip(tr("Not supported by the EasyRPG Player"));
	ui->frame_4->setEnabled(false);
	ui->frame_4->setToolTip(tr("The battle test uses the levels and equipment of the database (System)"));
	ui->checkSfx->setToolTip(tr("The Player can only turn off all the audio"));
	ui->checkMusic->setToolTip(ui->checkSfx->toolTip());

	ui->comboDisplayMode->setCurrentIndex(QSettings().value(TESTPLAY_FULLSCREEN_KEY, false).toBool() ? 1 : 0);
	on_comboMode_currentIndexChanged(0);

	UpdateModels();
}

RunGameDialog::~RunGameDialog()
{
	delete ui;
}

void RunGameDialog::runHere(int map_id, int x, int y)
{
	// Straight into the map, without the title screen
	launch(QApplication::activeWindow(), {QStringLiteral("--new-game"),
		QStringLiteral("--start-map-id"), QString::number(map_id),
		QStringLiteral("--start-position"), QString::number(x), QString::number(y)});
}

void RunGameDialog::runBattle(int troop_id)
{
	launch(QApplication::activeWindow(), {QStringLiteral("--battle-test"), QString::number(troop_id)});
}

void RunGameDialog::setBeforeLaunch(std::function<bool()> callback)
{
	before_launch = std::move(callback);
}

QString RunGameDialog::findPlayer()
{
	QStringList candidates;
	candidates << QSettings().value(PLAYER_PATH_KEY).toString();
	// next to the Editor, as in the release archives
	const QString app_dir = QCoreApplication::applicationDirPath();
	candidates << app_dir + "/" + PLAYER << app_dir + "/EasyRPG Player";
#ifdef Q_OS_MACOS
	// the Editor's bundle next to the Player's bundle
	candidates << QDir::cleanPath(app_dir + "/../../../EasyRPG Player.app");
#endif
	candidates << QStandardPaths::findExecutable(PLAYER);
#ifdef Q_OS_MACOS
	candidates << QStringLiteral("/Applications/EasyRPG Player.app")
		<< QDir::homePath() + "/Applications/EasyRPG Player.app";
#endif
	for (const QString& candidate : candidates) {
		if (candidate.isEmpty()) {
			continue;
		}
		QString program = executableOf(candidate);
		if (!program.isEmpty()) {
			return program;
		}
	}
	return QString();
}

QString RunGameDialog::choosePlayer(QWidget* parent)
{
	QString path = QFileDialog::getOpenFileName(parent, tr("Locate the EasyRPG Player"),
		QFileInfo(findPlayer()).absolutePath());
	if (path.isEmpty()) {
		return QString();
	}
	QString program = executableOf(path);
	if (program.isEmpty()) {
		QMessageBox::warning(parent, tr("Test Play"), tr("%1 is not a program.").arg(QDir::toNativeSeparators(path)));
		return QString();
	}
	EditorSettings::instance().setPlayerPath(program);
	return program;
}

bool RunGameDialog::launch(QWidget* parent, const QStringList& options, bool use_settings)
{
	const auto& project = core().project();
	if (!project) {
		return false;
	}
	// the Player reads the files: unsaved maps would not be seen
	if (before_launch && !before_launch()) {
		return false;
	}

	QString player = findPlayer();
	if (player.isEmpty()) {
		QMessageBox box(QMessageBox::Warning, tr("Test Play"),
			tr("The EasyRPG Player was not found.\n\n"
			   "Put it next to the Editor or in the PATH, or locate its program."),
			QMessageBox::Cancel, parent);
		QPushButton* locate = box.addButton(tr("Locate..."), QMessageBox::AcceptRole);
		box.exec();
		if (box.clickedButton() != locate) {
			return false;
		}
		player = choosePlayer(parent);
		if (player.isEmpty()) {
			return false;
		}
	}

	const QString project_dir = project->projectDir().absolutePath();
	QStringList args{QStringLiteral("--project-path"), project_dir, QStringLiteral("--test-play")};
	if (use_settings) {
		QSettings settings;
		args << (settings.value(TESTPLAY_FULLSCREEN_KEY, false).toBool() ? QStringLiteral("--fullscreen") : QStringLiteral("--window"));
		if (!settings.value(TESTPLAY_SHOW_TITLE_KEY, true).toBool()) {
			args << QStringLiteral("--hide-title");
		}
	}
	// the RTP the Editor uses
	const QString rtp = QSettings().value(RTP_KEY).toString();
	if (!rtp.isEmpty() && QFileInfo(rtp).isDir() && !options.contains(QStringLiteral("--no-rtp"))) {
		args << QStringLiteral("--rtp-path") << rtp;
	}
	args << options;

	qint64 pid = 0;
	if (!QProcess::startDetached(player, args, project_dir, &pid)) {
		QMessageBox::warning(parent, tr("Test Play"),
			tr("The EasyRPG Player could not be started:\n%1").arg(QDir::toNativeSeparators(player)));
		return false;
	}
#ifdef Q_OS_MACOS
	activatePlayer(pid);
#endif
	return true;
}

void RunGameDialog::accept()
{
	QStringList args;
	args << (ui->comboDisplayMode->currentIndex() == 1 ? QStringLiteral("--fullscreen") : QStringLiteral("--window"));

	const int mode = ui->comboMode->currentIndex();
	if (mode == 1) {
		args << QStringLiteral("--new-game");
	} else if (mode == 2) {
		args << QStringLiteral("--load-game-id") << QString::number(ui->spinSave->value());
	} else if (mode == 3) {
		// the first entry is the test troop of the database, then the troops
		args << QStringLiteral("--battle-test") << QString::number(std::max(1, ui->comboTroop->currentIndex()));
		if (ui->groupPartyFormation->isEnabled()) {
			int formation = lcf::rpg::System::BattleFormation_terrain;
			if (ui->radioLooseFormation->isChecked()) {
				formation = lcf::rpg::System::BattleFormation_loose;
			} else if (ui->radioTightFormation->isChecked()) {
				formation = lcf::rpg::System::BattleFormation_tight;
			}
			args << QString::number(formation) << QString::number(ui->comboBattleCondition->currentIndex())
				<< QString::number(ui->comboCustomFormation->currentIndex() + 1);
		}
	}

	if (mode != 2 && mode != 3 && ui->groupInitialPosition->isChecked() && ui->comboMapId->currentIndex() >= 0) {
		args << QStringLiteral("--start-map-id") << ui->comboMapId->currentData().toString()
			<< QStringLiteral("--start-position") << QString::number(ui->spinX->value()) << QString::number(ui->spinY->value());
	}
	if (mode != 2 && ui->groupInitialParty->isChecked()) {
		QStringList party;
		for (int row = 0; row < ui->tableInitialParty->rowCount(); row++) {
			auto* combo = qobject_cast<QComboBox*>(ui->tableInitialParty->cellWidget(row, 0));
			if (combo && combo->currentData().toInt() > 0) {
				party << combo->currentData().toString();
			}
		}
		if (!party.isEmpty()) {
			args << QStringLiteral("--start-party") << party;
		}
	}
	if (ui->checkSfx->isChecked() || ui->checkMusic->isChecked()) {
		args << QStringLiteral("--no-audio");
	}
	if (ui->checkRTP->isChecked()) {
		args << QStringLiteral("--no-rtp");
	}

	if (launch(this, args, false)) {
		QDialog::accept();
	}
}

void RunGameDialog::on_comboMode_currentIndexChanged(int index)
{
	switch (index)
	{
	case 0:
	case 1:
		ui->groupSave->setEnabled(false);
		ui->groupInitialPosition->setEnabled(true);
		ui->groupBattleOptions->setEnabled(false);
		ui->groupInitialParty->setEnabled(true);
		break;
	case 2:
		ui->groupSave->setEnabled(true);
		ui->groupInitialPosition->setEnabled(false);
		ui->groupBattleOptions->setEnabled(false);
		ui->groupInitialParty->setEnabled(false);
		break;
	case 3:
		ui->groupSave->setEnabled(false);
		ui->groupInitialPosition->setEnabled(false);
		ui->groupBattleOptions->setEnabled(true);
		ui->groupInitialParty->setEnabled(true);
		break;
	}
}

void RunGameDialog::UpdateModels()
{
	// Maps
	auto& tree = m_project.treeMap();
	ui->comboMapId->clear();
	for (size_t i = 1; i < tree.maps.size(); i++)
	{
		if (tree.maps[i].type == lcf::rpg::TreeMap::MapType_map)
		{
			ui->comboMapId->addItem(ToQString(tree.maps[i].name),
									tree.maps[i].ID);
		}
	}

	// Troops
	auto& database = m_project.database();
	ui->comboTroop->clear();
	ui->comboTroop->addItem(tr("<Test Troop>"));
	for (size_t i = 0; i < database.troops.size(); i++)
		ui->comboTroop->addItem(ToQString(database.troops[i].name));
	if (!database.troops.empty())
		ui->comboTroop->setCurrentIndex(1);

	bool auto_placement = static_cast<bool>(database.battlecommands.placement);
	ui->groupPartyFormation->setEnabled(auto_placement);
	ui->comboBattleCondition->clear();
	QStringList conditions;
	conditions << tr("No condition") << tr("Initiative") << tr("Back attack");
	if (auto_placement)
		conditions << tr("Surround attack") << tr("Pincers attack");
	ui->comboBattleCondition->addItems(conditions);

	ui->comboCustomFormation->clear();
	for (size_t i = 0; i < database.terrains.size(); i++)
		ui->comboCustomFormation->addItem(ToQString(database.terrains[i].name));

	// Initial party: four places, filled with the battle test party
	const auto& battletest_data = database.system.battletest_data;
	ui->tableInitialParty->setColumnCount(1);
	ui->tableInitialParty->setRowCount(4);
	ui->tableInitialParty->horizontalHeader()->setStretchLastSection(true);
	for (int row = 0; row < 4; row++) {
		auto* combo = new QComboBox(ui->tableInitialParty);
		combo->addItem(tr("<None>"), 0);
		for (const auto& actor : database.actors)
			combo->addItem(ToQString(actor.name), actor.ID);
		if (row < static_cast<int>(battletest_data.size()))
			combo->setCurrentIndex(std::max(0, combo->findData(battletest_data[static_cast<size_t>(row)].actor_id)));
		ui->tableInitialParty->setCellWidget(row, 0, combo);
	}
}
