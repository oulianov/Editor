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

#include "player_download.h"
#include "controller/dialog_service.h"
#include "model/game_runner.h"
#include "model/launcher_download.h"
#include <QCoreApplication>

PlayerDownload::PlayerDownload(QObject* parent) : QObject(parent)
{
}

PlayerDownload::~PlayerDownload()
{
	if (m_download) {
		// nobody listens any more
		m_download->disconnect(this);
		m_download->cancel();
	}
}

bool PlayerDownload::isBusy() const
{
	return !m_download.isNull();
}

int PlayerDownload::progress() const
{
	return m_progress;
}

int PlayerDownload::progressMaximum() const
{
	return m_progressMaximum;
}

QString PlayerDownload::version() const
{
	return Launchers::version();
}

QString PlayerDownload::fileName() const
{
	return Launchers::url(Launchers::hostPlatform()).fileName();
}

void PlayerDownload::setDialogService(DialogService* dialogs)
{
	m_dialogs = dialogs;
}

DialogService& PlayerDownload::dialogs() const
{
	return m_dialogs ? *m_dialogs : DialogService::instance();
}

void PlayerDownload::setProgress(int value, int maximum)
{
	if (value != m_progress || maximum != m_progressMaximum) {
		m_progress = value;
		m_progressMaximum = maximum;
		emit progressChanged();
	}
}

QString PlayerDownload::downloadedProgram()
{
	const QString launcher = Launchers::downloaded(Launchers::hostPlatform());
	return launcher.isEmpty() ? QString() : GameRunner::executableOf(launcher);
}

bool PlayerDownload::start()
{
	if (m_download) {
		return false;
	}
	// downloaded before: nothing to fetch
	if (const QString program = downloadedProgram(); !program.isEmpty()) {
		emit finished(program);
		return true;
	}
	const ExportPlatform platform = Launchers::hostPlatform();
	if (Launchers::url(platform).isEmpty()) {
		return false;
	}
	m_cancelled = false;
	m_download = new LauncherDownload(platform, this);
	connect(m_download, &LauncherDownload::progress, this, [this](qint64 received, qint64 total) {
		// in KB: the bars count with int
		setProgress(static_cast<int>(received / 1024), total > 0 ? static_cast<int>(total / 1024) : 0);
	});
	connect(m_download, &LauncherDownload::finished, this, [this, platform](const QString& launcher, QString error) {
		m_download->deleteLater();
		m_download = nullptr;
		QString program = launcher.isEmpty() ? QString() : GameRunner::executableOf(launcher);
		if (!launcher.isEmpty() && program.isEmpty()) {
			error = QCoreApplication::translate("LauncherDownload", "The archive has no EasyRPG Player for %1.")
				.arg(ExportPreset::name(platform));
		}
		setProgress(program.isEmpty() ? 0 : 1, 1);
		emit busyChanged();
		if (program.isEmpty() && !m_cancelled) {
			dialogs().warning(QCoreApplication::translate("ExportDialog", "Download"), error);
		}
		emit finished(program);
	});
	setProgress(0, 0);
	emit busyChanged();
	m_download->start();
	return true;
}

void PlayerDownload::cancel()
{
	if (m_download) {
		m_cancelled = true;
		m_download->cancel();
	}
}
