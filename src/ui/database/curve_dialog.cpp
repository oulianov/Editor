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

#include "curve_dialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#include "curve_editor.h"

void CurveGenerator::endpoints(std::vector<int16_t>& values, int levels, int first, int last, int growth) {
	// RPG Maker does not document its formula: a power curve between the two
	// values, 2^(growth / 2) as the exponent (0.25 for Early to 4 for Late)
	const double exponent = std::pow(2.0, growth / 2.0);
	levels = std::min(levels, static_cast<int>(values.size()));
	for (int i = 0; i < levels; ++i) {
		const double t = levels > 1 ? static_cast<double>(i) / (levels - 1) : 0.0;
		values[static_cast<size_t>(i)] = static_cast<int16_t>(std::lround(first + (last - first) * std::pow(t, exponent)));
	}
}

QColor CurveDialog::color(int curve) {
	// readable on the black background of the graphs
	static const std::array<QColor, 6> colors = {
		QColor(224, 48, 48), QColor(208, 64, 208), QColor(232, 200, 40),
		QColor(64, 192, 64), QColor(96, 112, 255), QColor(48, 192, 224)};
	return colors[static_cast<size_t>(std::clamp(curve, 0, 5))];
}

CurveDialog::CurveDialog(const lcf::rpg::Parameters& parameters, int max_level, int max_hp, int tab, QWidget* parent) :
	QDialog(parent),
	m_parameters(parameters),
	m_maxLevel(max_level)
{
	setWindowTitle(tr("Parameter Progression"));
	// the tab is the curve double-clicked, not the one shown last time
	setProperty("view_state_contextual", true);
	// a level without a value would read as 0 in the game
	m_parameters.Setup(max_level);
	m_curves = {{
		{tr("Max HP"), &m_parameters.maxhp, 1, max_hp},
		{tr("Max MP"), &m_parameters.maxsp, 0, 999},
		{tr("Attack"), &m_parameters.attack, 1, 999},
		{tr("Defense"), &m_parameters.defense, 1, 999},
		{tr("Intelligence"), &m_parameters.spirit, 1, 999},
		{tr("Agility"), &m_parameters.agility, 1, 999},
	}};

	// The tabs share one page, moved into the tab shown
	m_tabs = new QTabWidget(this);
	m_tabs->setObjectName(QStringLiteral("tabParameters"));
	const std::array<QString, 6> titles = {tr("Maximum HP"), tr("Maximum MP"), tr("Attack"), tr("Defense"),
		tr("Intelligence"), tr("Agility")};
	for (const auto& title : titles) {
		auto* page = new QWidget(m_tabs);
		auto* page_layout = new QVBoxLayout(page);
		page_layout->setContentsMargins(0, 0, 0, 0);
		m_tabs->addTab(page, title);
	}

	m_content = new QWidget(this);
	auto* content = new QHBoxLayout(m_content);
	m_graph = new CurveEditor(m_content);
	m_graph->setObjectName(QStringLiteral("graphParameter"));
	content->addWidget(m_graph, 1);

	auto* side = new QVBoxLayout();
	auto* level_group = new QGroupBox(tr("Level"), m_content);
	auto* level_layout = new QVBoxLayout(level_group);
	m_level = new QSpinBox(level_group);
	m_level->setObjectName(QStringLiteral("spinLevel"));
	m_level->setRange(1, max_level);
	level_layout->addWidget(m_level);
	side->addWidget(level_group);

	m_valueGroup = new QGroupBox(m_content);
	auto* value_layout = new QVBoxLayout(m_valueGroup);
	m_value = new QSpinBox(m_valueGroup);
	m_value->setObjectName(QStringLiteral("spinValue"));
	value_layout->addWidget(m_value);
	side->addWidget(m_valueGroup);

	m_table = new QTableWidget(max_level, 1, m_content);
	m_table->setObjectName(QStringLiteral("tableValues"));
	m_table->setHorizontalHeaderLabels({tr("Value")});
	m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	// the row headers are the levels, 1 to the maximum
	for (int row = 0; row < max_level; ++row) {
		m_table->setItem(row, 0, new QTableWidgetItem());
	}
	side->addWidget(m_table, 1);

	auto* quick = new QGroupBox(tr("QuickCurve"), m_content);
	auto* quick_layout = new QGridLayout(quick);
	const std::array<std::pair<QString, const char*>, 4> ranks = {{
		{tr("Outstanding"), "pushOutstanding"}, {tr("Above Average"), "pushAboveAverage"},
		{tr("Average"), "pushAverage"}, {tr("Below Average"), "pushBelowAverage"}}};
	for (int rank = 0; rank < 4; ++rank) {
		auto* button = new QPushButton(ranks[static_cast<size_t>(rank)].first, quick);
		button->setObjectName(QString::fromLatin1(ranks[static_cast<size_t>(rank)].second));
		connect(button, &QPushButton::clicked, this, [this, rank] { quickCurve(rank); });
		quick_layout->addWidget(button, rank / 2, rank % 2);
	}
	side->addWidget(quick);
	auto* endpoints = new QPushButton(tr("Specify Endpoints..."), m_content);
	endpoints->setObjectName(QStringLiteral("pushEndpoints"));
	connect(endpoints, &QPushButton::clicked, this, &CurveDialog::specifyEndpoints);
	side->addWidget(endpoints);
	content->addLayout(side);

	auto* layout = new QVBoxLayout(this);
	layout->addWidget(m_tabs, 1);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(m_tabs, &QTabWidget::currentChanged, this, &CurveDialog::showCurve);
	connect(m_level, &QSpinBox::valueChanged, this, [this](int level) {
		if (!m_updating) {
			setLevel(level);
		}
	});
	connect(m_value, &QSpinBox::valueChanged, this, [this](int value) {
		if (m_updating) {
			return;
		}
		(*m_curves[static_cast<size_t>(m_current)].values)[static_cast<size_t>(m_level->value()) - 1] = static_cast<int16_t>(value);
		refreshValues();
	});
	connect(m_table, &QTableWidget::currentCellChanged, this, [this](int row) {
		if (!m_updating && row >= 0) {
			setLevel(row + 1);
		}
	});
	connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
		if (m_updating) {
			return;
		}
		const auto& curve = m_curves[static_cast<size_t>(m_current)];
		const int value = std::clamp(item->data(Qt::EditRole).toInt(), curve.minimum, curve.maximum);
		(*curve.values)[static_cast<size_t>(item->row())] = static_cast<int16_t>(value);
		refreshValues();
		setLevel(item->row() + 1);
	});
	connect(m_graph, &CurveEditor::edited, this, [this](int level) {
		refreshValues();
		setLevel(level);
	});

	tab = std::clamp(tab, 0, 5);
	m_tabs->setCurrentIndex(tab);
	// no signal when the tab was already the current one
	showCurve(tab);
}

void CurveDialog::showCurve(int index) {
	if (index < 0) {
		return;
	}
	m_current = index;
	m_tabs->widget(index)->layout()->addWidget(m_content);
	const auto& curve = m_curves[static_cast<size_t>(index)];
	m_graph->setCurve(curve.values, m_maxLevel, curve.minimum, curve.maximum, color(index));
	m_valueGroup->setTitle(curve.name);
	{
		QSignalBlocker blocker(m_value);
		m_value->setRange(curve.minimum, curve.maximum);
	}
	m_table->setHorizontalHeaderLabels({curve.name});
	refreshValues();
	setLevel(m_level->value());
}

void CurveDialog::setLevel(int level) {
	const auto& curve = m_curves[static_cast<size_t>(m_current)];
	m_updating = true;
	m_level->setValue(level);
	m_value->setValue((*curve.values)[static_cast<size_t>(level) - 1]);
	m_table->setCurrentCell(level - 1, 0);
	m_graph->setCurrentLevel(level);
	m_updating = false;
}

void CurveDialog::refreshValues() {
	const auto& curve = m_curves[static_cast<size_t>(m_current)];
	m_updating = true;
	for (int row = 0; row < m_maxLevel; ++row) {
		m_table->item(row, 0)->setData(Qt::EditRole, static_cast<int>((*curve.values)[static_cast<size_t>(row)]));
	}
	m_value->setValue((*curve.values)[static_cast<size_t>(m_level->value()) - 1]);
	m_graph->update();
	m_updating = false;
}

void CurveDialog::quickCurve(int rank) {
	// RPG Maker makes a random curve of the chosen rank and does not document
	// how: here the values of an average hero, scaled by the rank and varied a
	// little, joined by a slightly accelerating curve
	const bool is2k3 = m_maxLevel > 50;
	// first and last level of an average hero, per parameter
	static const std::array<std::pair<int, int>, 6> average2k3 = {{{60, 3000}, {25, 600}, {20, 300}, {20, 300}, {20, 300}, {20, 300}}};
	static const std::array<std::pair<int, int>, 6> average2k = {{{40, 600}, {20, 300}, {15, 200}, {15, 200}, {15, 200}, {15, 200}}};
	static const std::array<double, 4> scale = {1.5, 1.25, 1.0, 0.75};
	const auto& [first, last] = (is2k3 ? average2k3 : average2k)[static_cast<size_t>(m_current)];
	auto* random = QRandomGenerator::global();
	auto vary = [&](int value) {
		return static_cast<int>(std::lround(value * scale[static_cast<size_t>(rank)] * (0.95 + random->bounded(0.1))));
	};
	const auto& curve = m_curves[static_cast<size_t>(m_current)];
	const int start = std::clamp(vary(first), curve.minimum, curve.maximum);
	const int end = std::clamp(vary(last), start, curve.maximum);
	CurveGenerator::endpoints(*curve.values, m_maxLevel, start, end, 1);
	refreshValues();
}

void CurveDialog::specifyEndpoints() {
	const auto& curve = m_curves[static_cast<size_t>(m_current)];
	auto& values = *curve.values;

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Specify Endpoints"));
	auto* layout = new QVBoxLayout(&dialog);
	auto endpoint = [&](const QString& title, const char* name, int value) {
		auto* group = new QGroupBox(title, &dialog);
		auto* group_layout = new QVBoxLayout(group);
		auto* spin = new QSpinBox(group);
		spin->setObjectName(QString::fromLatin1(name));
		spin->setRange(curve.minimum, curve.maximum);
		spin->setValue(value);
		group_layout->addWidget(spin);
		return std::pair{group, spin};
	};
	auto [first_group, first] = endpoint(tr("L1 Endpoint"), "spinFirstLevel", values.front());
	auto [last_group, last] = endpoint(tr("L%1 Endpoint").arg(m_maxLevel), "spinLastLevel",
		values[static_cast<size_t>(m_maxLevel) - 1]);
	auto* row = new QHBoxLayout();
	row->addWidget(first_group);
	row->addWidget(last_group);
	layout->addLayout(row);

	auto* growth_group = new QGroupBox(tr("Accelerate Growth"), &dialog);
	auto* growth_layout = new QGridLayout(growth_group);
	auto* growth = new QSlider(Qt::Horizontal, growth_group);
	growth->setObjectName(QStringLiteral("sliderGrowth"));
	growth->setRange(-4, 4);
	growth->setTickPosition(QSlider::TicksBelow);
	growth->setTickInterval(1);
	growth->setPageStep(1);
	growth_layout->addWidget(growth, 0, 0, 1, 3);
	growth_layout->addWidget(new QLabel(tr("Early"), growth_group), 1, 0, Qt::AlignLeft);
	growth_layout->addWidget(new QLabel(tr("Equal"), growth_group), 1, 1, Qt::AlignHCenter);
	growth_layout->addWidget(new QLabel(tr("Late"), growth_group), 1, 2, Qt::AlignRight);
	layout->addWidget(growth_group);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	CurveGenerator::endpoints(values, m_maxLevel, first->value(), last->value(), growth->value());
	refreshValues();
}
