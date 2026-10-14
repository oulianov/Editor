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

#include "test_play.h"
#include "core.h"
#include "model/game_runner.h"
#include "model/project.h"
#include "qml_dialog_service.h"
#include "qml_host.h"
#include "test_battle_party_model.h"

#include <QCoreApplication>
#include <QDir>
#include <QQuickWindow>

namespace {
QString runText(const char* text) {
	return QCoreApplication::translate("RunGameDialog", text);
}
}

bool TestPlay::launch(QWindow* over, const QStringList& options, bool use_settings)
{
	GameRunner::Result result = GameRunner::launch(options, use_settings);
	QString player;
	if (result == GameRunner::Result::PlayerMissing) {
		const int accepted = QmlHost::exec(QStringLiteral("PlayerMissingWindow"), over, {}, [&player](QQuickWindow* window) {
			player = window->property("player").toString();
		});
		if (!accepted || player.isEmpty()) {
			return false;
		}
		result = GameRunner::launch(options, use_settings, player);
	}
	if (result == GameRunner::Result::StartFailed) {
		QmlDialogService(over).warning(runText(QT_TRANSLATE_NOOP("RunGameDialog", "Test Play")),
			runText(QT_TRANSLATE_NOOP("RunGameDialog", "The EasyRPG Player could not be started:\n%1")).arg(QDir::toNativeSeparators(player.isEmpty() ? GameRunner::findPlayer() : player)));
	}
	return result == GameRunner::Result::Started;
}

bool TestPlay::runHere(QWindow* over, int map_id, int x, int y)
{
	return launch(over, GameRunner::hereOptions(map_id, x, y));
}

bool TestPlay::runBattle(QWindow* over, int troop_id, int formation, int condition, int terrain_id)
{
	return launch(over, GameRunner::battleOptions(troop_id, formation, condition, terrain_id));
}

QmlTestPlay::QmlTestPlay(QObject* parent) :
	QObject(parent)
{
}

QWindow* QmlTestPlay::window() const
{
	return m_window;
}

void QmlTestPlay::setWindow(QWindow* window)
{
	if (window != m_window) {
		m_window = window;
		emit windowChanged();
	}
}

bool QmlTestPlay::launch(const QStringList& options, bool use_settings)
{
	return TestPlay::launch(m_window, options, use_settings);
}

bool QmlTestPlay::saveBattleParty(TestBattlePartyModel* party)
{
	Project* project = core().project().get();
	if (!project || !party) {
		return false;
	}
	QmlDialogService dialogs(m_window);
	// another program's changes to the file are not replaced silently
	if (core().watcher().isStale(project->databaseFile())
			&& dialogs.askStale(QCoreApplication::translate("RunGameDialog", "The database"), false, {}) != StaleChoice::Overwrite) {
		return false;
	}
	party->save();
	// saveDatabase counts the save
	if (!project->saveDatabase()) {
		dialogs.error(QCoreApplication::translate("RunGameDialog", "Test Battle"), QCoreApplication::translate("RunGameDialog", "The database could not be written."));
		return false;
	}
	return true;
}
