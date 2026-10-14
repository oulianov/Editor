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

#include "animation_tools.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
/** Zero-based, in order and inside the frames; false without frames */
bool frameRange(const lcf::rpg::Animation& anim, int& first, int& last) {
	const int count = static_cast<int>(anim.frames.size());
	if (count == 0) {
		return false;
	}
	if (first > last) {
		std::swap(first, last);
	}
	first = std::clamp(first, 0, count - 1);
	last = std::clamp(last, 0, count - 1);
	return true;
}

/** A cel slot of a frame, added (with unused slots before it) when missing */
lcf::rpg::AnimationCellData& cellSlot(lcf::rpg::AnimationFrame& frame, int index) {
	auto& cells = frame.cells;
	while (static_cast<int>(cells.size()) <= index) {
		lcf::rpg::AnimationCellData unused;
		unused.ID = static_cast<int>(cells.size()) + 1;
		unused.valid = 0;
		cells.push_back(unused);
	}
	return cells[index];
}

const lcf::rpg::AnimationCellData* usedCell(const lcf::rpg::AnimationFrame& frame, int index) {
	if (index >= static_cast<int>(frame.cells.size()) || !frame.cells[index].valid) {
		return nullptr;
	}
	return &frame.cells[index];
}
}

int AnimationTools::maxCells(const lcf::rpg::Animation& anim) {
	size_t count = 1;
	for (const auto& frame : anim.frames) {
		count = std::max(count, frame.cells.size());
	}
	return static_cast<int>(count);
}

void AnimationTools::massClear(lcf::rpg::Animation& anim, int first, int last) {
	if (!frameRange(anim, first, last)) return;
	for (int i = first; i <= last; ++i) {
		anim.frames[i].cells.clear();
	}
}

void AnimationTools::massCopy(lcf::rpg::Animation& anim, int first, int last, int start) {
	if (!frameRange(anim, first, last)) return;
	const int count = static_cast<int>(anim.frames.size());
	// copied first: the ranges may overlap
	std::vector<std::vector<lcf::rpg::AnimationCellData>> source;
	for (int i = first; i <= last; ++i) {
		source.push_back(anim.frames[i].cells);
	}
	const int begin = std::clamp(start, 0, kMaxFrames - 1);
	const int end = std::min(kMaxFrames, begin + static_cast<int>(source.size()));
	if (end > count) {
		anim.frames.resize(end);
		for (int i = count; i < end; ++i) {
			anim.frames[i].ID = i + 1;
		}
	}
	for (int i = begin; i < end; ++i) {
		anim.frames[i].cells = source[i - begin];
	}
}

void AnimationTools::autocomplete(lcf::rpg::Animation& anim, int first, int last, int first_cel, int last_cel,
		const Carry& carry) {
	if (!frameRange(anim, first, last)) return;
	if (first_cel > last_cel) {
		std::swap(first_cel, last_cel);
	}
	first_cel = std::max(0, first_cel);
	for (int c = first_cel; c <= last_cel; ++c) {
		const auto* a = usedCell(anim.frames[first], c);
		const auto* b = usedCell(anim.frames[last], c);
		if (!a || !b) continue;
		// copies: cellSlot() may move the cels of the frames
		const lcf::rpg::AnimationCellData from = *a;
		const lcf::rpg::AnimationCellData to = *b;
		for (int f = first + 1; f < last; ++f) {
			const double t = double(f - first) / (last - first);
			auto between = [t](int x, int y) { return x + static_cast<int>(std::lround((y - x) * t)); };
			auto cell = from;
			cell.ID = c + 1;
			cell.x = between(from.x, to.x);
			cell.y = between(from.y, to.y);
			if (carry.pattern) cell.cell_id = between(from.cell_id, to.cell_id);
			if (carry.zoom) cell.zoom = between(from.zoom, to.zoom);
			if (carry.transparency) cell.transparency = between(from.transparency, to.transparency);
			cellSlot(anim.frames[f], c) = cell;
		}
	}
}

void AnimationTools::massModify(lcf::rpg::Animation& anim, int first, int last, const std::vector<bool>& cels,
		const CelValues& values) {
	if (!frameRange(anim, first, last)) return;
	for (int f = first; f <= last; ++f) {
		auto& frame_cells = anim.frames[f].cells;
		const size_t count = std::min(frame_cells.size(), cels.size());
		for (size_t c = 0; c < count; ++c) {
			auto& cell = frame_cells[c];
			if (!cell.valid || !cels[c]) continue;
			if (values.pattern) cell.cell_id = *values.pattern;
			if (values.zoom) cell.zoom = *values.zoom;
			if (values.transparency) cell.transparency = *values.transparency;
			if (values.tone) {
				cell.tone_red = (*values.tone)[0];
				cell.tone_green = (*values.tone)[1];
				cell.tone_blue = (*values.tone)[2];
				cell.tone_gray = (*values.tone)[3];
			}
		}
	}
}
