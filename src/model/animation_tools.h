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

#include <array>
#include <optional>
#include <vector>
#include <lcf/rpg/animation.h>

/**
 * The frame tools of RPG Maker's Animations page, on the data alone: the
 * page asks for the ranges and the values, then calls these. Frames and
 * cels are zero-based; a range may be given either way round, and is kept
 * inside the frames of the animation.
 */
namespace AnimationTools {

/** The highest frame count, as the Number of Frames of the page */
constexpr int kMaxFrames = 999;
/** RPG Maker's limits of the zoom of a cel */
constexpr int kMinZoom = 20;
constexpr int kMaxZoom = 800;

/** The most cels a frame of the animation has, at least 1 */
int maxCells(const lcf::rpg::Animation& anim);

/** Mass Clear: the frames from first to last lose their cels */
void massClear(lcf::rpg::Animation& anim, int first, int last);

/**
 * Mass Copy: the cels of the frames from first to last go to the frames
 * from start on. The ranges may overlap; frames are added past the end, up
 * to kMaxFrames.
 */
void massCopy(lcf::rpg::Animation& anim, int first, int last, int start);

/** What Autocomplete changes from the first frame to the last, besides the position */
struct Carry
{
	bool pattern = true;
	bool zoom = true;
	bool transparency = true;
};

/**
 * Autocomplete: each cel from first_cel to last_cel used in both the first
 * and the last frame is placed in the frames between, moving in a straight
 * line. What is not carried (and the color) stays as in the first frame.
 */
void autocomplete(lcf::rpg::Animation& anim, int first, int last, int first_cel, int last_cel,
		const Carry& carry = {});

/** The values Mass Modify gives, each only when set */
struct CelValues
{
	/** zero-based, like cell_id */
	std::optional<int> pattern;
	std::optional<int> zoom;
	std::optional<int> transparency;
	/** red, green, blue and saturation */
	std::optional<std::array<int, 4>> tone;
};

/**
 * Mass Modify: the used cels of the frames from first to last whose slot is
 * chosen (cels[i] for the cel i, past its end not chosen) get the values.
 * A frame without one of these cels is left as it is.
 */
void massModify(lcf::rpg::Animation& anim, int first, int last, const std::vector<bool>& cels,
		const CelValues& values);

}
