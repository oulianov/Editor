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

#include "animation_clock.h"
#include <QCoreApplication>
#include <QPointer>
#include "common/editor_settings.h"
#include "model/autotile.h"

AnimationClock::AnimationClock(QObject* parent) :
	QObject(parent)
{
	m_timer.setInterval(Interval);
	connect(&m_timer, &QTimer::timeout, this, [this]() {
		m_tick++;
		emit ticked();
	});
}

AnimationClock& AnimationClock::shared()
{
	// owned by the application: its timer stops before the event loop goes
	static QPointer<AnimationClock> clock;
	if (!clock) {
		clock = new AnimationClock(QCoreApplication::instance());
		AnimationClock* self = clock;
		auto follow = [self]() { self->setRunning(EditorSettings::instance().animateTiles()); };
		connect(&EditorSettings::instance(), &EditorSettings::changed, self, follow);
		follow();
	}
	return *clock;
}

bool AnimationClock::isRunning() const
{
	return m_timer.isActive();
}

void AnimationClock::setRunning(bool running)
{
	if (running == isRunning())
		return;
	if (running) {
		m_timer.start();
	} else {
		m_timer.stop();
		// the first frame, as Core shows when the option is turned off
		setTick(0);
	}
	emit runningChanged(running);
}

unsigned AnimationClock::tick() const
{
	return m_tick;
}

void AnimationClock::setTick(unsigned tick)
{
	if (tick == m_tick)
		return;
	m_tick = tick;
	emit ticked();
}

int AnimationClock::animatedTileFrame() const
{
	return Autotile::animatedTileFrame(m_tick);
}

int AnimationClock::waterFrame(const lcf::rpg::Chipset& chipset) const
{
	return Autotile::waterFrame(chipset, m_tick);
}
