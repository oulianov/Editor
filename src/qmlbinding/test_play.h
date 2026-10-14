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

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QWindow>
#include <QtQmlIntegration>

class TestBattlePartyModel;

/**
 * Test play from the windows of the Editor: the EasyRPG Player started
 * (GameRunner), and when there is none, the window offering to download
 * it or locate it (PlayerMissingWindow); a Player that cannot start is
 * told. Over the window given.
 */
namespace TestPlay {
	/** Starts the Player with the options, with the Full Screen and Show Title settings unless use_settings is false */
	bool launch(QWindow* over, const QStringList& options, bool use_settings = true);
	/** From a map position (Start Game Here) */
	bool runHere(QWindow* over, int map_id, int x, int y);
	/** A battle test against a monster group (GameRunner::battleOptions) */
	bool runBattle(QWindow* over, int troop_id, int formation = -1, int condition = 0, int terrain_id = 1);
}

/**
 * TestPlay for a QML window (Ez.TestPlay), over its window:
 *
 *   Ez.TestPlay { id: testPlay; window: root }
 *   onAccepting: check => check.ok = testPlay.launch(options.arguments(), false)
 */
class QmlTestPlay : public QObject
{
	Q_OBJECT
	QML_NAMED_ELEMENT(TestPlay)
	Q_PROPERTY(QWindow* window READ window WRITE setWindow NOTIFY windowChanged)

public:
	explicit QmlTestPlay(QObject* parent = nullptr);

	QWindow* window() const;
	void setWindow(QWindow* window);

	/** TestPlay::launch: true once the Player started */
	Q_INVOKABLE bool launch(const QStringList& options, bool use_settings = true);
	/**
	 * Before a battle test: the party written into the project's database
	 * and the database saved, since the Player reads them from its file.
	 * Asks first when another program changed the file. false: not saved
	 * (cancelled, or failed and told)
	 */
	Q_INVOKABLE bool saveBattleParty(TestBattlePartyModel* party);

signals:
	void windowChanged();

private:
	QPointer<QWindow> m_window;
};
