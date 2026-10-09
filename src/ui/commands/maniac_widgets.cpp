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

#include "maniac_widgets.h"
#include "common/dbstring.h"
#include "model/maniac_expression.h"
#include "model/project_data.h"
#include "ui/common/rpg_combobox.h"
#include "ui/common/rpg_spinbox.h"
#include "ui/event/event_command_palette.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>

using Cmd = lcf::rpg::EventCommand::Code;

namespace {
constexpr int KindRole = Qt::UserRole + 1;

uint32_t maskOf(int shift, int bits) {
	return bits >= 32 ? 0xFFFFFFFFu : (((1u << bits) - 1u) << shift);
}

/** Selects the item of value, or adds it: a value of a newer patch is kept */
void selectData(QComboBox* combo, int value) {
	int index = combo->findData(value);
	if (index == -1) {
		combo->addItem(ManiacCommandWidget::tr("Unknown (%1)").arg(value), value);
		combo->setItemData(combo->count() - 1, static_cast<int>(ManiacCommandWidget::Kind::Number), KindRole);
		index = combo->count() - 1;
	}
	combo->setCurrentIndex(index);
}

RpgSpinBox* spinBox(QWidget* parent, const QString& name, int value, int minimum, int maximum) {
	auto* spin = new RpgSpinBox(parent);
	spin->setObjectName(name);
	// keep unusual values visible without rewriting them on load
	spin->setRange(std::min(minimum, value), std::max(maximum, value));
	spin->setValue(value);
	return spin;
}
}

ManiacCommandWidget::ManiacCommandWidget(ProjectData& project, QWidget* parent) :
	EventCommandBaseWidget(project, parent) {}

std::vector<ManiacCommandWidget::Mode> ManiacCommandWidget::numberModes() {
	return {{0, tr("Constant"), Kind::Number}, {1, tr("Variable"), Kind::Variable},
		{2, tr("Variable pointer"), Kind::Variable}};
}

std::vector<ManiacCommandWidget::Mode> ManiacCommandWidget::targetModes() {
	return {{0, tr("Variable"), Kind::Variable}, {1, tr("Variable pointer"), Kind::Variable}};
}

std::vector<ManiacCommandWidget::Mode> ManiacCommandWidget::stringModes() {
	return {{0, tr("Text"), Kind::Text}, {1, tr("String"), Kind::StringId},
		{2, tr("String pointer"), Kind::Variable}};
}

void ManiacCommandWidget::setData(lcf::rpg::EventCommand* cmd) {
	EventCommandBaseWidget::setData(cmd);
	setWindowTitle(EventCommandPalette::commandName(static_cast<Cmd>(cmd->code)));
	auto* layout = new QVBoxLayout(this);
	build(layout);
	layout->addStretch(1);
}

int ManiacCommandWidget::parameter(int index, int fallback) const {
	return index < static_cast<int>(m_cmd->parameters.size()) ? m_cmd->parameters[index] : fallback;
}

void ManiacCommandWidget::growParameters(int index) {
	resizeCommandList(index);
}

void ManiacCommandWidget::select(QComboBox* combo, int value) {
	selectData(combo, value);
}

ManiacCommandWidget::Kind ManiacCommandWidget::kindOf(QComboBox* mode) {
	return static_cast<Kind>(mode->currentData(KindRole).toInt());
}

bool ManiacCommandWidget::maniac() const {
	return EventCommandPalette::usesManiacPatch(m_project.project());
}

void ManiacCommandWidget::setParameter(int index, int value, int shift, int bits) {
	if (index >= static_cast<int>(m_cmd->parameters.size())) {
		growParameters(index);
	}
	const uint32_t mask = maskOf(shift, bits);
	const uint32_t old = static_cast<uint32_t>(m_cmd->parameters[index]);
	m_cmd->parameters[index] = static_cast<int32_t>((old & ~mask) | ((static_cast<uint32_t>(value) << shift) & mask));
}

int ManiacCommandWidget::bitsOf(int index, int shift, int bits) const {
	const uint32_t value = static_cast<uint32_t>(parameter(index));
	return bits >= 32 ? static_cast<int>(value) : static_cast<int>((value & maskOf(shift, bits)) >> shift);
}

QFormLayout* ManiacCommandWidget::section(QBoxLayout* layout, const QString& title) {
	auto* box = new QGroupBox(title, this);
	layout->addWidget(box);
	return new QFormLayout(box);
}

void ManiacCommandWidget::onCommit(QWidget* widget, std::function<void()> write) {
	m_commits.insert(widget, std::move(write));
}

void ManiacCommandWidget::commit(QWidget* widget) {
	auto it = m_commits.find(widget);
	if (it != m_commits.end()) {
		(*it)();
	}
}

QComboBox* ManiacCommandWidget::choice(const QString& name, int index, const Items& items, int shift, int bits) {
	auto* combo = new QComboBox(this);
	combo->setObjectName(name);
	for (const auto& item : items) {
		combo->addItem(item.second, item.first);
	}
	selectData(combo, bitsOf(index, shift, bits));
	auto write = [=, this] {
		setParameter(index, combo->currentData().toInt(), shift, bits);
	};
	connect(combo, &QComboBox::currentIndexChanged, this, write);
	onCommit(combo, write);
	return combo;
}

QCheckBox* ManiacCommandWidget::flag(const QString& name, const QString& label, int index, int bit) {
	auto* check = new QCheckBox(label, this);
	check->setObjectName(name);
	check->setChecked(bitsOf(index, bit, 1) != 0);
	auto write = [=, this] {
		setParameter(index, check->isChecked() ? 1 : 0, bit, 1);
	};
	connect(check, &QCheckBox::toggled, this, write);
	onCommit(check, write);
	return check;
}

QCheckBox* ManiacCommandWidget::boolean(const QString& name, const QString& label, int index) {
	auto* check = new QCheckBox(label, this);
	check->setObjectName(name);
	check->setChecked(parameter(index) != 0);
	auto write = [=, this] {
		setParameter(index, check->isChecked() ? 1 : 0);
	};
	connect(check, &QCheckBox::toggled, this, write);
	onCommit(check, write);
	return check;
}

RpgSpinBox* ManiacCommandWidget::number(const QString& name, int index, int minimum, int maximum, int shift, int bits) {
	auto* spin = spinBox(this, name, bitsOf(index, shift, bits), minimum, maximum);
	auto write = [=, this] {
		setParameter(index, spin->value(), shift, bits);
	};
	connect(spin, &QSpinBox::valueChanged, this, write);
	onCommit(spin, write);
	return spin;
}

QWidget* ManiacCommandWidget::variable(const QString& name, int index) {
	auto* combo = new VariableRpgComboBox(this);
	combo->makeModel(m_project);
	combo->comboBox()->setObjectName(name);
	combo->setCurrentIndex(parameter(index) - 1);
	auto write = [=, this] {
		if (combo->currentIndex() >= 0) {
			setParameter(index, combo->currentIndex() + 1);
		}
	};
	connect(combo->comboBox(), &QComboBox::currentIndexChanged, this, write);
	onCommit(combo, write);
	return combo;
}

QComboBox* ManiacCommandWidget::modeChooser(const QString& name, int mode, const std::vector<Mode>& modes) {
	auto* combo = new QComboBox(this);
	combo->setObjectName(name);
	for (const auto& item : modes) {
		combo->addItem(item.label, item.mode);
		combo->setItemData(combo->count() - 1, static_cast<int>(item.kind), KindRole);
	}
	selectData(combo, mode);
	return combo;
}

QComboBox* ManiacCommandWidget::modeCombo(const QString& name, int index, int slot, const std::vector<Mode>& modes) {
	return slot < 0 ? modeComboAt(name, index, 0, 32, modes) : modeComboAt(name, index, slot * 4, 4, modes);
}

QComboBox* ManiacCommandWidget::modeComboAt(const QString& name, int index, int shift, int bits, const std::vector<Mode>& modes) {
	auto* combo = modeChooser(name, bitsOf(index, shift, bits), modes);
	auto write = [=, this] {
		setParameter(index, combo->currentData().toInt(), shift, bits);
	};
	connect(combo, &QComboBox::currentIndexChanged, this, write);
	onCommit(combo, write);
	return combo;
}

QWidget* ManiacCommandWidget::valueField(const QString& name, QComboBox* mode, int index, int minimum, int maximum, QWidget* text) {
	const int value = parameter(index);
	// one page per Kind, in the order of the enum
	auto* pages = new QStackedWidget(this);
	auto* spin = spinBox(pages, name, value, minimum, maximum);
	auto* variables = new VariableRpgComboBox(pages);
	variables->makeModel(m_project);
	variables->comboBox()->setObjectName(name + QStringLiteral("Variable"));
	variables->setCurrentIndex(value - 1);
	auto* strings = spinBox(pages, name + QStringLiteral("String"), value, 0, 99999);
	strings->setPrefix(tr("String "));
	pages->addWidget(spin);
	pages->addWidget(variables);
	pages->addWidget(strings);
	pages->addWidget(text ? text : new QWidget(pages));

	auto kind = [mode] { return static_cast<Kind>(mode->currentData(KindRole).toInt()); };
	auto write = [=, this] {
		switch (kind()) {
			case Kind::Number: setParameter(index, spin->value()); break;
			case Kind::StringId: setParameter(index, strings->value()); break;
			case Kind::Variable:
				if (variables->currentIndex() >= 0) {
					setParameter(index, variables->currentIndex() + 1);
				}
				break;
			case Kind::Text: break;
		}
	};
	pages->setCurrentIndex(static_cast<int>(kind()));
	connect(mode, &QComboBox::currentIndexChanged, this, [=] {
		pages->setCurrentIndex(static_cast<int>(kind()));
		write();
	});
	connect(spin, &QSpinBox::valueChanged, this, [=] { if (kind() == Kind::Number) write(); });
	connect(strings, &QSpinBox::valueChanged, this, [=] { if (kind() == Kind::StringId) write(); });
	connect(variables->comboBox(), &QComboBox::currentIndexChanged, this, [=] { if (kind() == Kind::Variable) write(); });
	onCommit(pages, write);
	return pages;
}

QWidget* ManiacCommandWidget::value(const QString& name, int modeIndex, int slot, int index, int minimum, int maximum,
	const std::vector<Mode>& modes) {
	auto* row = new QWidget(this);
	auto* layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	auto* mode = modeCombo(name + QStringLiteral("Mode"), modeIndex, slot, modes);
	auto* field = valueField(name, mode, index, minimum, maximum);
	layout->addWidget(mode);
	layout->addWidget(field, 1);
	onCommit(row, [=, this] {
		commit(mode);
		commit(field);
	});
	return row;
}

QLineEdit* ManiacCommandWidget::textField(const QString& name) {
	auto* edit = new QLineEdit(ToQString(m_cmd->string), this);
	edit->setObjectName(name);
	connect(edit, &QLineEdit::textEdited, this, [this](const QString& text) {
		m_cmd->string = ToDBString(text);
	});
	return edit;
}

QWidget* ManiacCommandWidget::expressionField(const QString& name, std::span<const int32_t> words,
	std::function<void(const std::vector<int32_t>&)> write) {
	auto* field = new QWidget(this);
	auto* layout = new QVBoxLayout(field);
	layout->setContentsMargins(0, 0, 0, 0);
	auto* edit = new QLineEdit(field);
	edit->setObjectName(name);
	edit->setToolTip(tr("Variables V[1], switches S[1], pointers V[V[1]], ranges V[1..5], arrays [1, 2][V[1]], "
		"C operators (+ - * / % & | ^ << >> == != < <= > >= && || ! ~ ?:), assignments (V[1] = 2, +=...) "
		"and functions: rand, item, event, actor, party, enemy, misc, pow, sqrt, sin, cos, atan2, min, max, "
		"abs, clamp, muldiv, divmul, between, lerp, arraysum, arraymin, arraymax."));
	auto* status = new QLabel(field);
	status->setWordWrap(true);
	status->setVisible(false);
	layout->addWidget(edit);
	layout->addWidget(status);
	// the words written: the original ones until a valid text replaces them
	auto current = std::make_shared<std::vector<int32_t>>(words.begin(), words.end());
	std::string text;
	if (words.empty() || ManiacExpression::decode(words, text)) {
		edit->setText(QString::fromStdString(text));
	} else {
		edit->setReadOnly(true);
		status->setText(tr("This expression cannot be read: it is kept as it is, use Edit raw to change it."));
		status->setVisible(true);
	}
	connect(edit, &QLineEdit::textEdited, this, [=](const QString& value) {
		std::vector<int32_t> encoded;
		size_t error = 0;
		if (!ManiacExpression::encode(value.toStdString(), encoded, error)) {
			status->setText(tr("Not a valid expression (at character %1): it is not saved.").arg(error + 1));
			status->setVisible(true);
			return;
		}
		status->setVisible(false);
		*current = encoded;
		write(encoded);
	});
	onCommit(field, [=] {
		if (!current->empty()) {
			write(*current);
		}
	});
	return field;
}

void ManiacCommandWidget::showWhen(QFormLayout* form, QWidget* field, QComboBox* combo, const QList<int>& values) {
	auto update = [=, this](bool user) {
		const bool visible = values.contains(combo->currentData().toInt());
		form->setRowVisible(field, visible);
		// the row shows a parameter another row may have changed: what is
		// shown is what is stored
		if (user && visible) {
			commit(field);
		}
	};
	connect(combo, &QComboBox::currentIndexChanged, this, [=] { update(true); });
	update(false);
}

void ManiacCommandWidget::build(QBoxLayout* layout) {
	const auto pictureModes = numberModes();
	const Items battleTargets = {{0, tr("Hero")}, {1, tr("Party member")}, {2, tr("Entire party")},
		{3, tr("Enemy")}, {4, tr("All enemies")}};
	const Items layers = {{0, tr("Lower layer")}, {1, tr("Upper layer")}};
	// x, y, width and height, with their modes in the nibbles first.. of modeIndex
	auto rectangle = [&](QFormLayout* form, const QString& prefix, int modeIndex, int first, int index) {
		QList<QWidget*> rows;
		const QStringList labels = {tr("X"), tr("Y"), tr("Width"), tr("Height")};
		const QStringList names = {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Width"), QStringLiteral("Height")};
		for (int i = 0; i < 4; ++i) {
			auto* row = value(prefix + names[i], modeIndex, first + i, index + i);
			form->addRow(labels[i], row);
			rows.append(row);
		}
		return rows;
	};

	switch (static_cast<Cmd>(m_cmd->code)) {
		case Cmd::Maniac_GetSaveInfo: {
			auto* save = section(layout, tr("Save File"));
			// EasyRPG Player reads the whole parameter 0 as the mode
			save->addRow(tr("Save slot"), value(QStringLiteral("saveSlot"), 0, -1, 1, 1, 9999));
			auto* store = section(layout, tr("Store In"));
			store->addRow(tr("Date (YYMMDD)"), variable(QStringLiteral("saveDate"), 2));
			store->addRow(tr("Time (HHMMSS)"), variable(QStringLiteral("saveTime"), 3));
			store->addRow(tr("Level"), variable(QStringLiteral("saveLevel"), 4));
			store->addRow(tr("HP"), variable(QStringLiteral("saveHp"), 5));
			auto* faces = section(layout, tr("Faces as Pictures (0: none)"));
			// one mode for the four picture numbers
			auto* mode = modeCombo(QStringLiteral("saveFaceMode"), 7, -1, pictureModes);
			faces->addRow(tr("Picture numbers"), mode);
			for (int i = 0; i < 4; ++i) {
				faces->addRow(tr("Face %1").arg(i + 1), valueField(QStringLiteral("saveFace%1").arg(i + 1), mode, 8 + i, 0, 9999));
			}
			break;
		}
		case Cmd::Maniac_Save:
		case Cmd::Maniac_Load: {
			const bool save = m_cmd->code == static_cast<int>(Cmd::Maniac_Save);
			auto* form = section(layout, tr("Save File"));
			form->addRow(tr("Save slot"), value(QStringLiteral("saveSlot"), 0, -1, 1, 1, 9999));
			if (save) {
				auto* result = boolean(QStringLiteral("saveStoreResult"), tr("Store the result in a variable"), 2);
				auto* variableRow = variable(QStringLiteral("saveResult"), 3);
				form->addRow(result);
				form->addRow(tr("Result"), variableRow);
				variableRow->setEnabled(result->isChecked());
				connect(result, &QCheckBox::toggled, variableRow, &QWidget::setEnabled);
			} else {
				form->addRow(boolean(QStringLiteral("loadSkipCheck"), tr("Do not check that the save exists"), 2));
			}
			break;
		}
		case Cmd::Maniac_GetMousePosition: {
			auto* form = section(layout, tr("Store In"));
			form->addRow(tr("X"), variable(QStringLiteral("mouseX"), 0));
			form->addRow(tr("Y"), variable(QStringLiteral("mouseY"), 1));
			break;
		}
		case Cmd::Maniac_SetMousePosition: {
			// not run by EasyRPG Player: layout of rpgsync (one mode for both)
			auto* form = section(layout, tr("Position"));
			auto* mode = modeCombo(QStringLiteral("mouseMode"), 0, 0, numberModes());
			form->addRow(tr("Given as"), mode);
			form->addRow(tr("X"), valueField(QStringLiteral("mouseX"), mode, 1, -9999, 9999));
			form->addRow(tr("Y"), valueField(QStringLiteral("mouseY"), mode, 2, -9999, 9999));
			break;
		}
		case Cmd::Maniac_GetPictureInfo: {
			auto* form = section(layout, tr("Picture"));
			form->addRow(tr("Picture No."), value(QStringLiteral("pictureId"), 0, 0, 3, 1, 9999));
			auto* type = choice(QStringLiteral("pictureInfo"), 1, {{0, tr("Position and original size")},
				{1, tr("Current position and size")}, {2, tr("Target position and size")}, {3, tr("Pixels")}});
			form->addRow(tr("Information"), type);
			auto* origin = choice(QStringLiteral("pictureOrigin"), 2, {{0, tr("Center")}, {1, tr("Top left")},
				{2, tr("Edges (left, top, right, bottom)")}});
			form->addRow(tr("Position of"), origin);
			showWhen(form, origin, type, {0, 1, 2});
			auto* effects = flag(QStringLiteral("pictureEffects"), tr("With the effects (tone, flash...)"), 2, 0);
			auto* alpha = flag(QStringLiteral("pictureIgnoreAlpha"), tr("Ignore alpha"), 2, 1);
			form->addRow(effects);
			form->addRow(alpha);
			showWhen(form, effects, type, {3});
			showWhen(form, alpha, type, {3});

			auto* store = section(layout, tr("Store In"));
			const QStringList labels = {tr("X"), tr("Y"), tr("Width"), tr("Height")};
			for (int i = 0; i < 4; ++i) {
				auto* row = variable(QStringLiteral("pictureVariable%1").arg(i + 1), 4 + i);
				store->addRow(labels[i], row);
				showWhen(store, row, type, {0, 1, 2});
			}
			// pixels: an area of the picture, written from a variable on
			for (auto* row : rectangle(store, QStringLiteral("pixel"), 0, 1, 4)) {
				showWhen(store, row, type, {3});
			}
			auto* first = value(QStringLiteral("pixelVariable"), 0, 5, 8, 1, 99999999, targetModes());
			store->addRow(tr("First variable"), first);
			showWhen(store, first, type, {3});
			break;
		}
		case Cmd::Maniac_ControlBattle: {
			auto* form = section(layout, tr("Battle Event"));
			form->addRow(tr("When"), choice(QStringLiteral("battleHook"), 0, {{0, tr("ATB gauge increases")},
				{1, tr("Damage pops up")}, {2, tr("Targets are chosen")}, {3, tr("A condition changes")},
				{4, tr("A parameter changes")}}));
			form->addRow(tr("Common event"), value(QStringLiteral("battleCommonEvent"), 1, -1, 2, 0, 9999));
			form->addRow(tr("First variable"), variable(QStringLiteral("battleVariable"), 3));
			break;
		}
		case Cmd::Maniac_ControlAtbGauge: {
			auto* form = section(layout, tr("Target"));
			auto* target = choice(QStringLiteral("atbTarget"), 0, battleTargets);
			form->addRow(tr("Target"), target);
			auto* id = value(QStringLiteral("atbTargetId"), 1, -1, 2, 0, 9999);
			form->addRow(tr("ID or position"), id);
			showWhen(form, id, target, {0, 1, 3});
			auto* operation = section(layout, tr("Operation"));
			operation->addRow(tr("Operation"), choice(QStringLiteral("atbOperation"), 3,
				{{0, tr("Set")}, {1, tr("Increase")}, {2, tr("Decrease")}}));
			operation->addRow(tr("Value"), value(QStringLiteral("atbValue"), 5, -1, 6));
			operation->addRow(tr("Unit"), choice(QStringLiteral("atbUnit"), 4, {{0, tr("Points")}, {1, tr("Percent of the gauge")}}));
			break;
		}
		case Cmd::Maniac_ChangeBattleCommandEx: {
			auto* heroes = section(layout, tr("Hero Commands"));
			heroes->addRow(boolean(QStringLiteral("battleNoRow"), tr("Remove the Row command"), 0));
			auto* party = section(layout, tr("Party Commands"));
			party->addRow(flag(QStringLiteral("battleNoFight"), tr("Remove Fight"), 1, 0));
			party->addRow(flag(QStringLiteral("battleNoAuto"), tr("Remove Auto Battle"), 1, 1));
			party->addRow(flag(QStringLiteral("battleNoEscape"), tr("Remove Escape"), 1, 2));
			party->addRow(flag(QStringLiteral("battleWin"), tr("Add Win"), 1, 3));
			party->addRow(flag(QStringLiteral("battleLose"), tr("Add Lose"), 1, 4));
			break;
		}
		case Cmd::Maniac_GetBattleInfo: {
			auto* form = section(layout, tr("Target"));
			auto* target = choice(QStringLiteral("battleTarget"), 0, battleTargets);
			form->addRow(tr("Target"), target);
			auto* id = value(QStringLiteral("battleTargetId"), 2, -1, 3, 0, 9999);
			form->addRow(tr("ID or position"), id);
			showWhen(form, id, target, {0, 1, 3});
			auto* single = choice(QStringLiteral("battleInfo"), 1, {{0, tr("Parameter changes")},
				{1, tr("Conditions")}, {2, tr("Attribute resistances")}, {3, tr("Position and state")}});
			auto* group = choice(QStringLiteral("battleGroupInfo"), 1, {{0, tr("Number of members")},
				{1, tr("Number of members alive")}, {2, tr("Number of members able to act")}});
			form->addRow(tr("Information"), single);
			form->addRow(tr("Information"), group);
			showWhen(form, single, target, {0, 1, 3});
			showWhen(form, group, target, {2, 4});
			form->addRow(tr("First variable"), variable(QStringLiteral("battleVariable"), 4));
			break;
		}
		case Cmd::Maniac_ControlVarArray: {
			auto* form = section(layout, tr("Variable Arrays"));
			auto* operation = choice(QStringLiteral("arrayOperation"), 0, {{0, tr("Copy A to B")}, {1, tr("Swap")},
				{2, tr("Sort ascending")}, {3, tr("Sort descending")}, {4, tr("Shuffle")}, {5, tr("Enumerate")},
				{6, tr("Add B")}, {7, tr("Subtract B")}, {8, tr("Multiply by B")}, {9, tr("Divide by B")},
				{10, tr("Modulo B")}, {11, tr("Bitwise OR")}, {12, tr("Bitwise AND")}, {13, tr("Bitwise XOR")},
				{14, tr("Shift left")}, {15, tr("Shift right")}});
			form->addRow(tr("Operation"), operation);
			form->addRow(tr("Array A from"), value(QStringLiteral("arrayA"), 1, 0, 2, 1, 99999999, targetModes()));
			form->addRow(tr("Length"), value(QStringLiteral("arrayLength"), 1, 1, 3, 1));
			auto* arrayB = value(QStringLiteral("arrayB"), 1, 2, 4, 1, 99999999, targetModes());
			auto* start = value(QStringLiteral("arrayStart"), 1, 2, 4);
			form->addRow(tr("Array B from"), arrayB);
			form->addRow(tr("First value"), start);
			showWhen(form, arrayB, operation, {0, 1, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
			showWhen(form, start, operation, {5});
			break;
		}
		case Cmd::Maniac_KeyInputProcEx: {
			auto* form = section(layout, tr("Keys"));
			auto* operation = choice(QStringLiteral("keyOperation"), 0, {{0, tr("All keys (keyboard)")},
				{1, tr("All keys (keyboard and joypad)")}, {2, tr("One key")}, {3, tr("Joypad")}});
			form->addRow(tr("Read"), operation);
			auto* key = value(QStringLiteral("keyCode"), 2, -1, 3, 0, 255);
			form->addRow(tr("Virtual key code"), key);
			showWhen(form, key, operation, {2});
			form->addRow(tr("Store in (from)"), variable(QStringLiteral("keyVariable"), 1));
			break;
		}
		case Cmd::Maniac_RewriteMap: {
			auto* form = section(layout, tr("Tiles"));
			form->addRow(tr("Layer"), choice(QStringLiteral("mapLayer"), 2, layers));
			form->addRow(tr("Tile"), value(QStringLiteral("mapTile"), 0, 0, 3, 0));
			form->addRow(boolean(QStringLiteral("mapTilesFromVariables"), tr("One tile per cell, from the variables from the tile value"), 1));
			form->addRow(boolean(QStringLiteral("mapNoAutotile"), tr("No autotile update"), 8));
			rectangle(section(layout, tr("Area")), QStringLiteral("map"), 0, 1, 4);
			break;
		}
		case Cmd::Maniac_ControlGlobalSave: {
			auto* form = section(layout, tr("Global Save"));
			auto* operation = choice(QStringLiteral("globalOperation"), 0, {{0, tr("Open")}, {1, tr("Close")},
				{2, tr("Save")}, {3, tr("Save and close")}, {4, tr("Copy to the game")}, {5, tr("Copy from the game")}});
			form->addRow(tr("Operation"), operation);
			auto* type = choice(QStringLiteral("globalType"), 2, {{0, tr("Switches")}, {1, tr("Variables")}});
			auto* game = value(QStringLiteral("globalGameId"), 1, 0, 3, 1);
			auto* global = value(QStringLiteral("globalId"), 1, 1, 4, 1);
			auto* count = value(QStringLiteral("globalCount"), 1, 2, 5, 1);
			form->addRow(tr("Data"), type);
			form->addRow(tr("Game from"), game);
			form->addRow(tr("Global save from"), global);
			form->addRow(tr("Count"), count);
			for (auto* row : {static_cast<QWidget*>(type), game, global, count}) {
				showWhen(form, row, operation, {4, 5});
			}
			break;
		}
		case Cmd::Maniac_ChangePictureId: {
			// not run by EasyRPG Player: layout of rpgsync
			auto* form = section(layout, tr("Pictures"));
			form->addRow(tr("Operation"), choice(QStringLiteral("pictureOperation"), 0,
				{{0, tr("Move")}, {1, tr("Swap")}, {2, tr("Slide")}}));
			form->addRow(tr("First picture"), value(QStringLiteral("pictureFirst"), 1, 0, 2, 1, 9999));
			form->addRow(tr("Count"), value(QStringLiteral("pictureCount"), 1, 1, 3, 1, 9999));
			form->addRow(tr("Target picture"), value(QStringLiteral("pictureTarget"), 1, 2, 4, 1, 9999));
			form->addRow(boolean(QStringLiteral("pictureIgnoreRange"), tr("Ignore out of range"), 5));
			break;
		}
		case Cmd::Maniac_SetGameOption: {
			auto* form = section(layout, tr("Option"));
			auto* option = choice(QStringLiteral("gameOption"), 1, {{0, tr("Run when inactive")}, {1, tr("Game speed")},
				{2, tr("Picture limit")}, {3, tr("Frame skip")}, {4, tr("Mouse in messages")}, {5, tr("Battle origin")},
				{6, tr("Battle animation limit")}, {7, tr("Face size")}});
			form->addRow(tr("Option"), option);
			// the mode of parameter 0 applies to both values
			auto* mode = modeCombo(QStringLiteral("gameValueMode"), 0, -1, numberModes());
			form->addRow(tr("Value given as"), mode);
			showWhen(form, mode, option, {0, 1, 2, 4, 6, 7});
			auto field = [&](const QString& label, const QString& name, int index, const QList<int>& options) {
				auto* row = valueField(name, mode, index, -99999999, 99999999);
				form->addRow(label, row);
				showWhen(form, row, option, options);
			};
			field(tr("Run when inactive (1: yes)"), QStringLiteral("gameInactive"), 2, {0});
			field(tr("Frames per second"), QStringLiteral("gameFps"), 2, {1});
			field(tr("Pictures"), QStringLiteral("gamePictures"), 2, {2});
			field(tr("No mouse in messages (1: yes)"), QStringLiteral("gameMouse"), 2, {4});
			field(tr("Animations"), QStringLiteral("gameAnimations"), 2, {6});
			field(tr("Width"), QStringLiteral("gameFaceWidth"), 2, {7});
			field(tr("Height"), QStringLiteral("gameFaceHeight"), 3, {7});
			auto* testPlay = choice(QStringLiteral("gameTestPlay"), 3, {{0, tr("Keep")}, {1, tr("Off")}, {2, tr("On")}}, 0, 2);
			auto* fastText = flag(QStringLiteral("gameFastText"), tr("Fast forward text"), 3, 4);
			form->addRow(tr("Test play"), testPlay);
			form->addRow(fastText);
			showWhen(form, testPlay, option, {1});
			showWhen(form, fastText, option, {1});
			auto* skip = choice(QStringLiteral("gameFrameSkip"), 2, {{0, tr("Every frame")}, {1, tr("1/5")},
				{2, tr("1/3")}, {3, tr("1/2")}});
			form->addRow(tr("Frames drawn"), skip);
			showWhen(form, skip, option, {3});
			auto* origin = choice(QStringLiteral("gameBattleOrigin"), 2, {{0, tr("Center")}, {1, tr("Top left")},
				{2, tr("Bottom left")}, {3, tr("Top right")}, {4, tr("Bottom right")}, {5, tr("Top")},
				{6, tr("Bottom")}, {7, tr("Left")}, {8, tr("Right")}});
			form->addRow(tr("Position"), origin);
			showWhen(form, origin, option, {5});
			break;
		}
		case Cmd::Maniac_CallCommand: {
			auto* form = section(layout, tr("Command"));
			form->addRow(tr("Command code"), value(QStringLiteral("callCode"), 0, 0, 1, 0, 99999));
			// the string of the called command: this command's string, or a string
			auto* textMode = modeCombo(QStringLiteral("callStringMode"), 0, 3, stringModes());
			auto* row = new QWidget(this);
			auto* rowLayout = new QHBoxLayout(row);
			rowLayout->setContentsMargins(0, 0, 0, 0);
			rowLayout->addWidget(textMode);
			rowLayout->addWidget(valueField(QStringLiteral("callString"), textMode, 4, 0, 99999,
				textField(QStringLiteral("callText"))), 1);
			form->addRow(tr("Text"), row);

			auto* parameters = section(layout, tr("Parameters"));
			const int source = bitsOf(0, 4, 4);
			if (source == 0 || source == 1) {
				// the values of the variables from a variable id (or a pointer)
				parameters->addRow(tr("From"), value(QStringLiteral("callFirst"), 0, 1, 2, 1, 99999999,
					{{0, tr("Variable"), Kind::Variable}, {1, tr("Variable pointer"), Kind::Variable}}));
				parameters->addRow(tr("Count"), value(QStringLiteral("callCount"), 0, 2, 3, 0, 9999));
			} else {
				auto* note = new QLabel(tr("Values written in the command (inline values or expressions): kept as they are, use Edit raw to change them."), this);
				note->setWordWrap(true);
				parameters->addRow(note);
			}
			break;
		}
		case Cmd::Maniac_GetGameInfo: {
			auto* form = section(layout, tr("Information"));
			auto* type = choice(QStringLiteral("gameInfo"), 1, {{0, tr("Map size")}, {1, tr("Tiles")},
				{2, tr("Window size")}, {3, tr("Screen pixels")}, {4, tr("Event processing state")},
				{5, tr("Chipset ID")}, {6, tr("Face graphic")}, {7, tr("Character graphic")},
				{8, tr("Screen position")}, {9, tr("Screen shake")}, {10, tr("Current BGM")}});
			form->addRow(tr("Information"), type);
			auto row = [&](const QString& label, QWidget* field, const QList<int>& types) {
				form->addRow(label, field);
				showWhen(form, field, type, types);
			};
			row(tr("Layer"), choice(QStringLiteral("gameInfoLayer"), 2, layers), {1});
			row(QString(), flag(QStringLiteral("gameInfoIgnoreAlpha"), tr("Ignore alpha"), 2, 0), {3});
			for (auto* area : rectangle(form, QStringLiteral("gameInfo"), 0, 0, 3)) {
				showWhen(form, area, type, {1, 3});
			}
			row(tr("Levels up"), value(QStringLiteral("gameInfoNest"), 0, 2, 4, 0, 99), {4});
			row(tr("Of"), choice(QStringLiteral("gameInfoFace"), 4, {{0, tr("Hero")}, {1, tr("Message")}}), {6});
			row(tr("Of"), choice(QStringLiteral("gameInfoCharacter"), 4, {{0, tr("Hero")}, {1, tr("Event")}}), {7});
			row(tr("ID"), value(QStringLiteral("gameInfoId"), 0, 0, 5, 0, 99999), {6, 7});
			row(tr("Graphic"), choice(QStringLiteral("gameInfoDynamic"), 6, {{0, tr("Original")}, {1, tr("Current")}}), {6, 7});
			row(tr("Store in"), variable(QStringLiteral("gameInfoVariable"), 2), {0, 2, 4, 5, 8, 9});
			row(tr("Store from"), variable(QStringLiteral("gameInfoFirst"), 7), {1, 3});
			row(tr("File name in"), number(QStringLiteral("gameInfoString"), 2, 0, 99999), {6, 7, 10});
			row(tr("Index in"), variable(QStringLiteral("gameInfoIndex"), 3), {6, 7});
			row(tr("Fade, volume, tempo, balance from"), variable(QStringLiteral("gameInfoBgm"), 3), {10});
			break;
		}
		case Cmd::Maniac_EditPicture: {
			auto* form = section(layout, tr("Picture"));
			form->addRow(tr("Picture No."), value(QStringLiteral("pictureId"), 0, 0, 1, 1, 9999));
			rectangle(section(layout, tr("Area")), QStringLiteral("picture"), 0, 1, 2);
			auto* pixels = section(layout, tr("Pixels"));
			pixels->addRow(tr("From variable"), value(QStringLiteral("pictureVariable"), 0, 5, 6, 1, 99999999, targetModes()));
			pixels->addRow(flag(QStringLiteral("pictureOpaque"), tr("Opaque (replace the pixels)"), 7, 0));
			pixels->addRow(flag(QStringLiteral("pictureSkipTransparent"), tr("Skip transparent pixels"), 7, 1));
			break;
		}
		case Cmd::Maniac_WritePicture: {
			auto* form = section(layout, tr("Image"));
			auto* target = choice(QStringLiteral("writeTarget"), 1, {{0, tr("Screen")}, {1, tr("Picture")}});
			form->addRow(tr("Save"), target);
			auto* picture = value(QStringLiteral("writePicture"), 0, 0, 2, 1, 9999);
			auto* dynamic = flag(QStringLiteral("writeDynamic"), tr("With the effects (tone, flash...)"), 4, 0);
			auto* opaque = flag(QStringLiteral("writeOpaque"), tr("Opaque"), 4, 1);
			form->addRow(tr("Picture No."), picture);
			form->addRow(dynamic);
			form->addRow(opaque);
			for (auto* row : {picture, static_cast<QWidget*>(dynamic), static_cast<QWidget*>(opaque)}) {
				showWhen(form, row, target, {1});
			}
			auto* file = section(layout, tr("File (PNG)"));
			auto* mode = modeCombo(QStringLiteral("writeFileMode"), 0, 1, stringModes());
			file->addRow(tr("Name given as"), mode);
			file->addRow(tr("Name"), valueField(QStringLiteral("writeFile"), mode, 3, 0, 99999, textField(QStringLiteral("writeFileName"))));
			break;
		}
		case Cmd::Maniac_EditTile: {
			// not run by EasyRPG Player: layout of rpgsync
			auto* form = section(layout, tr("Tiles"));
			form->addRow(tr("Picture No."), value(QStringLiteral("tilePicture"), 0, 0, 1, 1, 9999));
			form->addRow(tr("Chipset"), value(QStringLiteral("tileChipset"), 0, 6, 10, 0, 9999));
			form->addRow(tr("Layer"), choice(QStringLiteral("tileLayer"), 9, layers));
			form->addRow(tr("Tile"), value(QStringLiteral("tileId"), 0, 5, 6, 0));
			form->addRow(tr("Pattern"), value(QStringLiteral("tilePattern"), 0, 7, 11, 0));
			form->addRow(boolean(QStringLiteral("tileFromVariables"), tr("One tile per cell, from the variables from the tile value"), 8));
			form->addRow(flag(QStringLiteral("tileNoAutotile"), tr("No autotile update"), 7, 0));
			form->addRow(flag(QStringLiteral("tileClear"), tr("Clear the area"), 7, 1));
			rectangle(section(layout, tr("Area")), QStringLiteral("tile"), 0, 1, 2);
			break;
		}
		case Cmd::Maniac_ControlTextProcessing: {
			// not run by EasyRPG Player: layout of rpgsync
			auto* form = section(layout, tr("Common Event"));
			auto* source = choice(QStringLiteral("textSource"), 0, {{0, tr("ID")}, {1, tr("Variable")},
				{2, tr("Variable pointer")}, {3, tr("Name")}, {4, tr("String")}, {5, tr("String pointer")}});
			form->addRow(tr("Given as"), source);
			auto* id = number(QStringLiteral("textEvent"), 2, 0, 99999999);
			auto* name = textField(QStringLiteral("textName"));
			form->addRow(tr("Value"), id);
			form->addRow(tr("Name"), name);
			showWhen(form, id, source, {0, 1, 2, 4, 5});
			showWhen(form, name, source, {3});
			auto* when = section(layout, tr("Called"));
			when->addRow(flag(QStringLiteral("textOnMarker"), tr("On a marker"), 1, 0));
			when->addRow(flag(QStringLiteral("textOnOpen"), tr("When the message opens"), 1, 1));
			when->addRow(flag(QStringLiteral("textOnClose"), tr("When the message closes"), 1, 2));
			when->addRow(flag(QStringLiteral("textOnCharacter"), tr("On each character"), 1, 3));
			auto* data = section(layout, tr("Data"));
			data->addRow(tr("Information in"), variable(QStringLiteral("textInfoVariable"), 3));
			data->addRow(tr("Text in string"), number(QStringLiteral("textInfoString"), 4, 0, 99999));
			data->addRow(tr("Number arguments"), number(QStringLiteral("textNumbers"), 5, 0, 99999));
			data->addRow(tr("String arguments"), number(QStringLiteral("textStrings"), 6, 0, 99999));
			break;
		}
		case Cmd::Maniac_Zoom: {
			// not run by EasyRPG Player: layout of rpgsync
			auto* form = section(layout, tr("Zoom"));
			form->addRow(tr("Center X"), value(QStringLiteral("zoomX"), 0, 0, 1, -99999, 99999));
			form->addRow(tr("Center Y"), value(QStringLiteral("zoomY"), 0, 1, 2, -99999, 99999));
			form->addRow(tr("Magnification (%)"), value(QStringLiteral("zoomScale"), 0, 2, 3, 0, 99999));
			form->addRow(tr("Duration (frames)"), value(QStringLiteral("zoomDuration"), 0, 3, 4, 0, 99999));
			form->addRow(tr("Layer"), value(QStringLiteral("zoomLayer"), 0, 4, 5, 0, 99));
			form->addRow(boolean(QStringLiteral("zoomWait"), tr("Wait for completion"), 6));
			break;
		}
		default:
			break;
	}
}
