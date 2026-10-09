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

#include "experience_dialog.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

#include "model/project_data.h"

ExperienceCurve ExperienceCurve::of(ProjectData& project, int base, int inflation, int correction) {
	const auto& system = project.database().system;
	const bool is2k3 = system.ldb_id == 2003;
	ExperienceCurve curve;
	curve.base = base;
	curve.inflation = inflation;
	curve.correction = correction;
	// EasyRPG can give a game the formula of the other engine
	curve.rpg2k_formula = system.easyrpg_alternative_exp > 0 ? system.easyrpg_alternative_exp == 1 : !is2k3;
	curve.max_exp = system.easyrpg_max_exp == -1 ? (is2k3 ? 9999999 : 999999) : system.easyrpg_max_exp;
	return curve;
}

int ExperienceCurve::total(int level) const {
	// Player's CalculateExp(n) is the total to go from level 1 to level n + 1
	const int n = level - 1;
	if (n < 1) {
		return 0;
	}
	// 64 bits: large settings must reach the maximum, not wrap around
	int64_t result = 0;
	if (rpg2k_formula) {
		double b = base;
		double rate = 1.5 + inflation * 0.01;
		for (int i = n; i >= 1 && result < max_exp; --i) {
			result += static_cast<int64_t>(correction + b);
			b = b * rate;
			rate = ((n + 1) * 0.002 + 0.8) * (rate - 1) + 1;
		}
	} else {
		for (int i = 1; i <= n && result < max_exp; ++i) {
			result += base + static_cast<int64_t>(i) * inflation + correction;
		}
	}
	return static_cast<int>(std::min<int64_t>(result, max_exp));
}

namespace {
struct Setting {
	const char* name2k;
	const char* name2k3;
	int min2k, max2k;
	int min2k3, max2k3;
};
// RPG Maker 2000: 10-50, 10-50 and 0-40. RPG Maker 2003: its help gives
// 1-2000 for Primary; the same maximum is used for the two others.
const std::array<Setting, 3> kSettings = {{
	{QT_TRANSLATE_NOOP("ExperienceDialog", "Base Value"), QT_TRANSLATE_NOOP("ExperienceDialog", "Primary"), 10, 50, 1, 2000},
	{QT_TRANSLATE_NOOP("ExperienceDialog", "Extra Value"), QT_TRANSLATE_NOOP("ExperienceDialog", "Secondary"), 10, 50, 0, 2000},
	{QT_TRANSLATE_NOOP("ExperienceDialog", "Correction"), QT_TRANSLATE_NOOP("ExperienceDialog", "Tertiary"), 0, 40, 0, 2000},
}};
const std::array<const char*, 3> kNames = {"ExpBase", "ExpInflation", "ExpCorrection"};
}

ExperienceDialog::ExperienceDialog(ProjectData& project, int32_t base, int32_t inflation, int32_t correction,
	QWidget* parent) :
	QDialog(parent),
	m_project(project)
{
	setWindowTitle(tr("Experience Curve"));
	m_maxLevel = project.database().system.ldb_id == 2003 ? 99 : 50;
	const bool rpg2k = ExperienceCurve::of(project, 0, 0, 0).rpg2k_formula;

	auto makeTable = [this](const QString& title, const char* name) {
		auto* table = new QTableWidget(m_maxLevel, 1, this);
		table->setObjectName(QString::fromLatin1(name));
		table->setEditTriggers(QAbstractItemView::NoEditTriggers);
		table->setHorizontalHeaderLabels({title});
		table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
		// the row headers are the levels, 1 to the maximum
		return table;
	};
	m_total = makeTable(tr("Experience"), "tableTotalExperience");
	m_perLevel = makeTable(tr("To the next level"), "tableExperiencePerLevel");
	auto* tabs = new QTabWidget(this);
	tabs->setObjectName(QStringLiteral("tabExperience"));
	tabs->addTab(m_total, tr("Total Experience"));
	tabs->addTab(m_perLevel, tr("Experience Per Level"));

	auto* settings = new QVBoxLayout();
	const std::array<int32_t, 3> values = {base, inflation, correction};
	for (size_t i = 0; i < kSettings.size(); ++i) {
		const auto& s = kSettings[i];
		// a stored value outside RPG Maker's range is kept, not clipped
		const int minimum = std::min(rpg2k ? s.min2k : s.min2k3, values[i]);
		const int maximum = std::max(rpg2k ? s.max2k : s.max2k3, values[i]);
		auto* group = new QGroupBox(tr(rpg2k ? s.name2k : s.name2k3), this);
		auto* row = new QHBoxLayout(group);
		auto* slider = new QSlider(Qt::Horizontal, group);
		slider->setObjectName(QStringLiteral("slider") + kNames[i]);
		slider->setRange(minimum, maximum);
		slider->setMinimumWidth(160);
		auto* spin = new QSpinBox(group);
		spin->setObjectName(QStringLiteral("spin") + kNames[i]);
		spin->setRange(minimum, maximum);
		row->addWidget(slider, 1);
		row->addWidget(spin);
		settings->addWidget(group);
		m_spins[i] = spin;

		spin->setValue(values[i]);
		slider->setValue(values[i]);
		connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
		connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
		connect(spin, &QSpinBox::valueChanged, this, &ExperienceDialog::refresh);
	}
	settings->addStretch();

	auto* columns = new QHBoxLayout();
	columns->addWidget(tabs, 1);
	columns->addLayout(settings);
	auto* layout = new QVBoxLayout(this);
	layout->addLayout(columns, 1);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	refresh();
}

int32_t ExperienceDialog::base() const {
	return m_spins[0]->value();
}

int32_t ExperienceDialog::inflation() const {
	return m_spins[1]->value();
}

int32_t ExperienceDialog::correction() const {
	return m_spins[2]->value();
}

void ExperienceDialog::refresh() {
	const auto curve = ExperienceCurve::of(m_project, base(), inflation(), correction());
	const QColor capped = palette().color(QPalette::Disabled, QPalette::Text);
	auto set = [](QTableWidget* table, int row, const QString& text, const QColor& color) {
		auto* item = table->item(row, 0);
		if (!item) {
			item = new QTableWidgetItem();
			item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
			table->setItem(row, 0, item);
		}
		item->setText(text);
		item->setData(Qt::ForegroundRole, color.isValid() ? QVariant(color) : QVariant());
	};
	int previous = 0;
	for (int level = 1; level <= m_maxLevel; ++level) {
		const int total = curve.total(level);
		// RPG Maker draws the levels at the maximum dotted: they cannot be reached
		const QColor color = level > 1 && total >= curve.max_exp ? capped : QColor();
		set(m_total, level - 1, QString::number(total), color);
		if (level > 1) {
			set(m_perLevel, level - 2, QString::number(total - previous), color);
		}
		previous = total;
	}
	set(m_perLevel, m_maxLevel - 1, QStringLiteral("-"), QColor());
}

QString ExperienceDialog::summary(ProjectData& project, int32_t base, int32_t inflation, int32_t correction) {
	const auto curve = ExperienceCurve::of(project, base, inflation, correction);
	const int max_level = project.database().system.ldb_id == 2003 ? 99 : 50;
	return tr("%1: %2, %3: %4, %5: %6 (level %7: %8)")
		.arg(tr(curve.rpg2k_formula ? kSettings[0].name2k : kSettings[0].name2k3)).arg(base)
		.arg(tr(curve.rpg2k_formula ? kSettings[1].name2k : kSettings[1].name2k3)).arg(inflation)
		.arg(tr(curve.rpg2k_formula ? kSettings[2].name2k : kSettings[2].name2k3)).arg(correction)
		.arg(max_level).arg(curve.total(max_level));
}

bool ExperienceDialog::edit(QWidget* parent, ProjectData& project, int32_t& base, int32_t& inflation, int32_t& correction) {
	ExperienceDialog dialog(project, base, inflation, correction, parent);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}
	base = dialog.base();
	inflation = dialog.inflation();
	correction = dialog.correction();
	return true;
}
