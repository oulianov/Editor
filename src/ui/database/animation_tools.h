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

#include <QCoreApplication>
#include <lcf/rpg/animation.h>

class QWidget;

// The frame tools of RPG Maker's Animations page. Each opens its dialog,
// starting on the frame shown (zero-based), and changes the animation when
// accepted: true when it did.
class AnimationTools {
	Q_DECLARE_TR_FUNCTIONS(AnimationTools)

public:
	// The highest frame count, as the Number of Frames of the page
	static constexpr int kMaxFrames = 999;

	// Mass Copy/Clear: copies the cels of a range of frames to another
	// starting frame (frames are added past the end), or removes them
	static bool massCopyClear(QWidget* parent, lcf::rpg::Animation& anim, int current);

	// Autocomplete: the cels of a range present in the first and last frames
	// of a range are placed in the frames between, moving in a straight line;
	// the pattern, zoom and transparency change too when checked
	static bool autocomplete(QWidget* parent, lcf::rpg::Animation& anim, int current);

	// Mass Modify: gives the checked values to the chosen cels of a range of
	// frames; patterns go from 1 to pattern_count
	static bool massModify(QWidget* parent, lcf::rpg::Animation& anim, int current, int pattern_count);
};
