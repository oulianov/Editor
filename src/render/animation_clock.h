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
#include <QTimer>
#include <lcf/rpg/chipset.h>

/**
 * The clock of the water and of the animated tiles: a tick every tenth of
 * a second, as Core's timer. Every view reads its frames from it, so the
 * water of all maps moves together. A stopped clock is at its first tick:
 * the tiles show their first frame.
 */
class AnimationClock : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool running READ isRunning WRITE setRunning NOTIFY runningChanged)

public:
	/** The Player changes the animated tiles every 6 of its 60 frames */
	static constexpr int Interval = 100;

	/** A clock of its own, stopped (tests, stills) */
	explicit AnimationClock(QObject* parent = nullptr);

	/** The clock of the map views: it runs while the Animate tiles option
	 *  (EditorSettings::animateTiles) is on */
	static AnimationClock& shared();

	bool isRunning() const;
	void setRunning(bool running);

	/** Ticks since the clock started, 0 while it is stopped */
	unsigned tick() const;
	/** Shows the frames of a tick (tests, stills) */
	void setTick(unsigned tick);

	/** The frames at this tick, as Autotile computes them */
	int animatedTileFrame() const;
	int waterFrame(const lcf::rpg::Chipset& chipset) const;

signals:
	/** The tick changed: the frames may have */
	void ticked();
	void runningChanged(bool running);

private:
	QTimer m_timer;
	unsigned m_tick = 0;
};
