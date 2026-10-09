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
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
// RPG Maker's limits of a cel
constexpr int min_zoom = 20;
constexpr int max_zoom = 800;

QSpinBox* makeSpin(QWidget* parent, const QString& name, int min, int max, int value, const QString& suffix = QString()) {
	auto* box = new QSpinBox(parent);
	box->setObjectName(name);
	box->setRange(min, max);
	box->setValue(std::clamp(value, min, max));
	box->setSuffix(suffix);
	return box;
}

/** A group with "first - last", the spin boxes named name + "From" and name + "To" */
std::pair<QSpinBox*, QSpinBox*> makeRange(QWidget* parent, QVBoxLayout* layout, const QString& title,
		const QString& name, int max, int first, int last) {
	auto* box = new QGroupBox(title, parent);
	auto* row = new QHBoxLayout(box);
	auto* from = makeSpin(box, name + QStringLiteral("From"), 1, max, first);
	auto* to = makeSpin(box, name + QStringLiteral("To"), 1, max, last);
	row->addWidget(from, 1);
	row->addWidget(new QLabel(QStringLiteral("-"), box));
	row->addWidget(to, 1);
	layout->addWidget(box);
	return {from, to};
}

/** The OK and Cancel buttons; true when accepted */
bool run(QDialog& dialog, QVBoxLayout* layout) {
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);
	return dialog.exec() == QDialog::Accepted;
}

/** Zero-based, in order, whatever way the range was entered */
std::pair<int, int> ordered(const std::pair<QSpinBox*, QSpinBox*>& range) {
	const int a = range.first->value() - 1;
	const int b = range.second->value() - 1;
	return {std::min(a, b), std::max(a, b)};
}

int maxCells(const lcf::rpg::Animation& anim) {
	size_t count = 1;
	for (const auto& frame : anim.frames) {
		count = std::max(count, frame.cells.size());
	}
	return static_cast<int>(count);
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

bool AnimationTools::massCopyClear(QWidget* parent, lcf::rpg::Animation& anim, int current) {
	const int count = static_cast<int>(anim.frames.size());
	if (count == 0) return false;
	QDialog dialog(parent);
	dialog.setObjectName(QStringLiteral("dialogMassCopyClear"));
	dialog.setWindowTitle(tr("Mass Copy/Clear"));
	auto* layout = new QVBoxLayout(&dialog);
	const auto range = makeRange(&dialog, layout, tr("Frame Range"), QStringLiteral("spinFrame"), count, current + 1, current + 1);

	auto* action_box = new QGroupBox(tr("Action"), &dialog);
	auto* action_layout = new QVBoxLayout(action_box);
	auto* action = new QButtonGroup(&dialog);
	action->setObjectName(QStringLiteral("groupAction"));
	auto* copy = new QRadioButton(tr("Copy"), action_box);
	copy->setObjectName(QStringLiteral("radioCopy"));
	auto* clear = new QRadioButton(tr("Clear"), action_box);
	clear->setObjectName(QStringLiteral("radioClear"));
	action->addButton(copy, 0);
	action->addButton(clear, 1);
	copy->setChecked(true);
	action_layout->addWidget(copy);
	auto* start_form = new QFormLayout();
	// the copy may go past the last frame: frames are added
	auto* start = makeSpin(action_box, QStringLiteral("spinStartFrame"), 1, kMaxFrames, std::min(current + 2, kMaxFrames));
	start_form->addRow(tr("Starting Frame"), start);
	action_layout->addLayout(start_form);
	action_layout->addWidget(clear);
	layout->addWidget(action_box);
	QObject::connect(copy, &QRadioButton::toggled, start, &QSpinBox::setEnabled);

	if (!run(dialog, layout)) return false;
	const auto [from, to] = ordered(range);
	if (clear->isChecked()) {
		for (int i = from; i <= to; ++i) {
			anim.frames[i].cells.clear();
		}
		return true;
	}
	// copied first: the ranges may overlap
	std::vector<std::vector<lcf::rpg::AnimationCellData>> source;
	for (int i = from; i <= to; ++i) {
		source.push_back(anim.frames[i].cells);
	}
	const int first = start->value() - 1;
	const int last = std::min(kMaxFrames, first + static_cast<int>(source.size()));
	if (last > count) {
		anim.frames.resize(last);
		for (int i = count; i < last; ++i) {
			anim.frames[i].ID = i + 1;
		}
	}
	for (int i = first; i < last; ++i) {
		anim.frames[i].cells = source[i - first];
	}
	return true;
}

bool AnimationTools::autocomplete(QWidget* parent, lcf::rpg::Animation& anim, int current) {
	const int count = static_cast<int>(anim.frames.size());
	if (count == 0) return false;
	QDialog dialog(parent);
	dialog.setObjectName(QStringLiteral("dialogAutocomplete"));
	dialog.setWindowTitle(tr("Autocomplete"));
	auto* layout = new QVBoxLayout(&dialog);
	const auto frames = makeRange(&dialog, layout, tr("Frame Range"), QStringLiteral("spinFrame"), count,
		current + 1, count);
	const int cell_count = maxCells(anim);
	const auto cells = makeRange(&dialog, layout, tr("Cel Range"), QStringLiteral("spinCel"), cell_count, 1, cell_count);

	auto* carry_box = new QGroupBox(tr("Carry Through"), &dialog);
	carry_box->setToolTip(tr("Besides the position, what changes from the first frame to the last"));
	auto* carry = new QVBoxLayout(carry_box);
	auto check = [carry_box, carry](const QString& text, const QString& name) {
		auto* box = new QCheckBox(text, carry_box);
		box->setObjectName(name);
		box->setChecked(true);
		carry->addWidget(box);
		return box;
	};
	auto* pattern = check(tr("Image Section"), QStringLiteral("checkImageSection"));
	auto* zoom = check(tr("Magnification"), QStringLiteral("checkMagnification"));
	auto* transparency = check(tr("Transparency"), QStringLiteral("checkTransparency"));
	layout->addWidget(carry_box);

	if (!run(dialog, layout)) return false;
	const auto [first, last] = ordered(frames);
	const auto [cell_first, cell_last] = ordered(cells);
	for (int c = cell_first; c <= cell_last; ++c) {
		const auto* a = usedCell(anim.frames[first], c);
		const auto* b = usedCell(anim.frames[last], c);
		if (!a || !b) continue;
		// copies: cellSlot() may move the cels of the frames
		const lcf::rpg::AnimationCellData start = *a;
		const lcf::rpg::AnimationCellData end = *b;
		for (int f = first + 1; f < last; ++f) {
			const double t = double(f - first) / (last - first);
			auto between = [t](int x, int y) { return x + static_cast<int>(std::lround((y - x) * t)); };
			// what is not carried through (and the color) stays as in the first frame
			auto cell = start;
			cell.ID = c + 1;
			cell.x = between(start.x, end.x);
			cell.y = between(start.y, end.y);
			if (pattern->isChecked()) cell.cell_id = between(start.cell_id, end.cell_id);
			if (zoom->isChecked()) cell.zoom = between(start.zoom, end.zoom);
			if (transparency->isChecked()) cell.transparency = between(start.transparency, end.transparency);
			cellSlot(anim.frames[f], c) = cell;
		}
	}
	return true;
}

bool AnimationTools::massModify(QWidget* parent, lcf::rpg::Animation& anim, int current, int pattern_count) {
	const int count = static_cast<int>(anim.frames.size());
	if (count == 0) return false;
	QDialog dialog(parent);
	dialog.setObjectName(QStringLiteral("dialogMassModify"));
	dialog.setWindowTitle(tr("Mass Modify"));
	auto* layout = new QVBoxLayout(&dialog);
	const auto frames = makeRange(&dialog, layout, tr("Affected Frame"), QStringLiteral("spinFrame"), count, current + 1, count);

	// RPG Maker has 8; more when frames of the animation have more cels
	auto* cells_box = new QGroupBox(tr("Affected Cels"), &dialog);
	auto* cells_grid = new QGridLayout(cells_box);
	std::vector<QCheckBox*> cells;
	const int cell_count = std::max(8, maxCells(anim));
	for (int i = 0; i < cell_count; ++i) {
		auto* box = new QCheckBox(QString::number(i + 1), cells_box);
		box->setObjectName(QStringLiteral("checkCel%1").arg(i + 1));
		box->setChecked(true);
		cells_grid->addWidget(box, i / 8, i % 8);
		cells.push_back(box);
	}
	layout->addWidget(cells_box);

	// each value is given only when checked
	auto* values_box = new QGroupBox(tr("Cel Properties"), &dialog);
	auto* values = new QGridLayout(values_box);
	auto row = [values_box, values](const QString& text, const QString& name, const std::vector<QSpinBox*>& boxes) {
		auto* check = new QCheckBox(text, values_box);
		check->setObjectName(name);
		const int r = values->rowCount();
		values->addWidget(check, r, 0);
		auto* fields = new QHBoxLayout();
		for (auto* box : boxes) {
			box->setEnabled(false);
			fields->addWidget(box);
			QObject::connect(check, &QCheckBox::toggled, box, &QSpinBox::setEnabled);
		}
		fields->addStretch(1);
		values->addLayout(fields, r, 1);
		return check;
	};
	const QString percent = QStringLiteral("%");
	auto* pattern = makeSpin(values_box, QStringLiteral("spinImageSection"), 1, std::max(1, pattern_count), 1);
	auto* zoom = makeSpin(values_box, QStringLiteral("spinMagnification"), min_zoom, max_zoom, 100, percent);
	auto* transparency = makeSpin(values_box, QStringLiteral("spinTransparency"), 0, 100, 0, percent);
	QSpinBox* tone[4];
	const QString tone_names[] = {QStringLiteral("spinRed"), QStringLiteral("spinGreen"), QStringLiteral("spinBlue"), QStringLiteral("spinSaturation")};
	const QString tone_tips[] = {tr("Red"), tr("Green"), tr("Blue"), tr("Saturation")};
	for (int i = 0; i < 4; ++i) {
		tone[i] = makeSpin(values_box, tone_names[i], 0, 200, 100, percent);
		tone[i]->setToolTip(tone_tips[i]);
	}
	auto* use_pattern = row(tr("Image Section"), QStringLiteral("checkImageSection"), {pattern});
	auto* use_zoom = row(tr("Magnification"), QStringLiteral("checkMagnification"), {zoom});
	auto* use_transparency = row(tr("Transparency"), QStringLiteral("checkTransparency"), {transparency});
	auto* use_tone = row(tr("Color Cast"), QStringLiteral("checkColorCast"), {tone[0], tone[1], tone[2], tone[3]});
	layout->addWidget(values_box);

	if (!run(dialog, layout)) return false;
	const auto [first, last] = ordered(frames);
	for (int f = first; f <= last; ++f) {
		auto& frame_cells = anim.frames[f].cells;
		for (int c = 0; c < static_cast<int>(frame_cells.size()) && c < cell_count; ++c) {
			auto& cell = frame_cells[c];
			// a frame without this cel is left as it is
			if (!cell.valid || !cells[c]->isChecked()) continue;
			if (use_pattern->isChecked()) cell.cell_id = pattern->value() - 1;
			if (use_zoom->isChecked()) cell.zoom = zoom->value();
			if (use_transparency->isChecked()) cell.transparency = transparency->value();
			if (use_tone->isChecked()) {
				cell.tone_red = tone[0]->value();
				cell.tone_green = tone[1]->value();
				cell.tone_blue = tone[2]->value();
				cell.tone_gray = tone[3]->value();
			}
		}
	}
	return true;
}
