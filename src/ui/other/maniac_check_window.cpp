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

#include "maniac_check_window.h"
#include "model/project_convert.h"
#include "qmlbinding/qml_dialog_service.h"
#include "qmlbinding/qml_host.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QVariantList>

namespace {

/** RPG_RT.exe is in the folder (any case) */
bool hasExe(const QString& folder)
{
	return !QDir(folder).entryList({QStringLiteral("RPG_RT.exe")}, QDir::Files).isEmpty();
}

} // namespace

bool ManiacCheckWindow::ask(QWindow* over, const ManiacCompat::Report& report, const QString& backup)
{
	// one line per finding, under the title of its group with the group's count
	QHash<int, int> totals;
	for (const auto& finding : report.findings) {
		totals[static_cast<int>(finding.group)] += finding.places.size();
	}
	QVariantList findings;
	bool power_mode = false;
	for (const auto& finding : report.findings) {
		power_mode = power_mode || finding.group == ManiacCompat::Group::PowerMode;
		findings.append(QVariantMap{
			{QStringLiteral("section"), QStringLiteral("%1 (%2)").arg(ManiacCompat::groupName(finding.group))
				.arg(totals.value(static_cast<int>(finding.group)))},
			{QStringLiteral("what"), finding.what},
			{QStringLiteral("count"), finding.places.size()},
			{QStringLiteral("becomes"), finding.becomes},
			{QStringLiteral("places"), finding.places},
		});
	}
	const int result = QmlHost::exec(QStringLiteral("ManiacCheckWindow"), over, {
		{QStringLiteral("findings"), findings},
		{QStringLiteral("convertible"), report.convertible()},
		{QStringLiteral("remaining"), report.remaining()},
		{QStringLiteral("rpg2000"), report.rpg2000},
		{QStringLiteral("exe2000"), report.rpg2000 && hasExe(QFileInfo(backup).absolutePath())},
		{QStringLiteral("backup2000Path"), QDir::toNativeSeparators(
			QDir(QFileInfo(backup).absolutePath()).filePath(ProjectConvert::backupFolderName()))},
		{QStringLiteral("powerMode"), power_mode},
		{QStringLiteral("backupPath"), QDir::toNativeSeparators(backup)},
	});
	return result == 1 && report.convertible() > 0;
}

void ManiacCheckWindow::result(QWindow* over, const QString& error, int converted, int remaining, const QString& backup,
	const QString& backup2000)
{
	QmlDialogService dialogs(over);
	const QString title = QCoreApplication::translate("ManiacCheckDialog", "Maniac Patch Compatibility");
	QStringList text;
	if (!backup2000.isEmpty()) {
		text << QCoreApplication::translate("ManiacCheckDialog", "The game is now an RPG Maker 2003 game. The files of RPG Maker 2000 are in:\n%1")
			.arg(QDir::toNativeSeparators(backup2000));
		if (hasExe(backup2000)) {
			// moved by the conversion to RPG Maker 2003: said, it is not lost
			text << QCoreApplication::translate("ManiacCheckDialog", "RPG_RT.exe of RPG Maker 2000 was moved there: it would "
				"play the game as an RPG Maker 2000 game. Copy the RPG_RT.exe of the Maniac Patch into the game folder to "
				"play the game with it; the EasyRPG Player plays it without.");
		}
	}
	if (!error.isEmpty()) {
		text.prepend(QCoreApplication::translate("ManiacCheckDialog", "The game was not converted to the Maniac Patch.\n\n%1").arg(error));
		dialogs.error(title, text.join(QStringLiteral("\n\n")));
		return;
	}
	if (converted > 0) {
		text << QCoreApplication::translate("ManiacCheckDialog", "Converted to commands of the Maniac Patch: %1.").arg(converted);
	}
	text << QCoreApplication::translate("ManiacCheckDialog", "The Maniac Patch is on in EasyRPG.ini.");
	if (!backup.isEmpty()) {
		text << QCoreApplication::translate("ManiacCheckDialog", "The files as they were are in:\n%1").arg(QDir::toNativeSeparators(backup));
	}
	if (remaining > 0) {
		text << QCoreApplication::translate("ManiacCheckDialog", "Left as they were, without an equivalent in the Maniac Patch: "
			"%1 (Game Project > Check Maniac Compatibility lists them).").arg(remaining);
	}
	dialogs.information(title, text.join(QStringLiteral("\n\n")));
}
