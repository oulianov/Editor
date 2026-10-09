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
#include "ui/common/rpg_spinbox.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
/** Number of characters (code points), as EasyRPG Player counts them */
int length(const QString& text) {
	return static_cast<int>(text.toUcs4().size());
}

QString middle(const QString& text, int position, int count) {
	const auto characters = text.toUcs4();
	if (position >= characters.size() || count <= 0) {
		return QString();
	}
	return QString::fromUcs4(reinterpret_cast<const char32_t*>(characters.constData()) + position,
		std::min<qsizetype>(count, characters.size() - position));
}

// The string ids of a String Picture's system graphic and font are written in
// its string, 5 bits per letter (ManiacPatch::DecodeStringToInt)
QString encodeId(uint32_t id) {
	QString result(QChar('A' + static_cast<int>(id & 31) - 1));
	for (id >>= 5; id > 0; id >>= 5) {
		result.prepend(QChar('a' + static_cast<int>(id & 31) - 1));
	}
	return result;
}

uint32_t decodeId(const QString& text) {
	if (text.isEmpty() || text.size() > 6) {
		return 0;
	}
	uint32_t result = text.back().unicode() - 'A' + 1;
	for (int i = 0; i < text.size() - 1; ++i) {
		const uint32_t digit = text[i].unicode() - 'a' + 1;
		if (digit >= 32) {
			return 0;
		}
		result += digit << ((text.size() - 1 - i) * 5);
	}
	return result < 32 * 32 * 32 * 32 * 32 * 32u ? result : 0;
}

enum class Arg { Str, Num, Var, VarPlain, StrOut, StrPlain, Switch, Database, Description, Dynamic, Encoding, Join };

struct Argument {
	Arg kind;
	const char* label;
};

#define ARG(text) QT_TRANSLATE_NOOP("ManiacControlStringsWidget", text)

/** The arguments (parameters 4..7) of an operation, as EasyRPG Player reads them */
std::vector<Argument> arguments(int operation, int function) {
	if (operation == 0 || operation == 1) {
		switch (function) {
			case 0: return {{Arg::Str, ARG("Text")}, {Arg::Num, ARG("Minimum length")}};
			case 1: return {{Arg::Num, ARG("Number")}, {Arg::Num, ARG("Minimum length")}};
			case 2: return {{Arg::Switch, ARG("Switch")}, {Arg::Num, ARG("Minimum length")}};
			case 3: return {{Arg::Database, ARG("Data")}, {Arg::Num, ARG("ID")}, {Arg::Dynamic, ARG("Value")}};
			case 4: return {{Arg::Description, ARG("Data")}, {Arg::Num, ARG("ID")}, {Arg::Dynamic, ARG("Value")}};
			case 6: return {{Arg::Str, ARG("Text 1")}, {Arg::Str, ARG("Text 2")}, {Arg::Str, ARG("Text 3")}};
			case 7: return {{Arg::Str, ARG("Text")}, {Arg::Num, ARG("Position")}, {Arg::Str, ARG("Inserted text")}};
			case 8:
			case 14: return {{Arg::Str, ARG("Text")}, {Arg::Str, ARG("Search")}, {Arg::Str, ARG("Replace with")}};
			case 9:
			case 13: return {{Arg::Str, ARG("Text")}, {Arg::Num, ARG("Start")}, {Arg::Num, ARG("Length")}};
			case 10: return {{Arg::Str, ARG("Delimiter")}, {Arg::Join, ARG("Join")}, {Arg::Num, ARG("Count")}};
			case 12: return {{Arg::Str, ARG("File name")}, {Arg::Encoding, ARG("Encoding")}};
			default: return {};
		}
	}
	switch (operation) {
		case 2:
		case 3: return {{Arg::VarPlain, ARG("Store in")}};
		case 4:
		case 9: return {{Arg::Str, ARG("Search")}, {Arg::Var, ARG("Position in")}, {Arg::Num, ARG("Start")}};
		case 5: return {{Arg::Str, ARG("Delimiter")}, {Arg::StrOut, ARG("Parts in strings from")}, {Arg::Var, ARG("Count in")}};
		case 7: return {{Arg::Str, ARG("File name")}, {Arg::Encoding, ARG("Encoding")}};
		case 8: return {{Arg::StrPlain, ARG("Line in string")}};
		case 10: return {{Arg::Str, ARG("Pattern")}, {Arg::Var, ARG("Position in")}, {Arg::Num, ARG("Start")},
			{Arg::StrPlain, ARG("Match in string")}};
		default: return {};
	}
}

#undef ARG

/** The literal texts of these functions follow each other in the string, the arguments are their lengths */
bool multipleTexts(int operation, int function) {
	return (operation == 0 || operation == 1) && (function == 6 || function == 7 || function == 8 || function == 14);
}
}

ManiacControlStringsWidget::ManiacControlStringsWidget(ProjectData& project, QWidget* parent) :
	ManiacCommandWidget(project, parent) {}

void ManiacControlStringsWidget::build(QBoxLayout* layout) {
	auto* target = section(layout, tr("String"));
	auto* targetMode = modeCombo(QStringLiteral("stringTargetMode"), 0, 0, {{0, tr("Single"), Kind::StringId},
		{1, tr("Range"), Kind::StringId}, {2, tr("Pointer"), Kind::Variable}, {3, tr("Pointer range"), Kind::Variable}});
	target->addRow(tr("Target"), targetMode);
	target->addRow(tr("String"), valueField(QStringLiteral("stringFirst"), targetMode, 1, 1, 99999));
	auto* last = valueField(QStringLiteral("stringLast"), targetMode, 2, 1, 99999);
	target->addRow(tr("To"), last);
	showWhen(target, last, targetMode, {1, 3});

	auto* operation = section(layout, tr("Operation"));
	m_operation = choice(QStringLiteral("stringOperation"), 3, {{0, tr("Set")}, {1, tr("Append")},
		{2, tr("To number")}, {3, tr("Get length")}, {4, tr("Find")}, {5, tr("Split")}, {7, tr("Save to file")},
		{8, tr("Remove first line")}, {9, tr("Find (regular expression)")}, {10, tr("Match (regular expression)")}}, 0, 8);
	m_function = choice(QStringLiteral("stringFunction"), 3, {{0, tr("Text")}, {1, tr("Number")}, {2, tr("Switch")},
		{3, tr("Database name")}, {4, tr("Database description")}, {6, tr("Concatenation")}, {7, tr("Insertion")},
		{8, tr("Replacement")}, {9, tr("Part of a text")}, {10, tr("Strings joined")}, {12, tr("File contents")},
		{13, tr("Text without a part")}, {14, tr("Replacement (regular expression)")}}, 8, 8);
	operation->addRow(tr("Operation"), m_operation);
	operation->addRow(tr("Value"), m_function);
	showWhen(operation, m_function, m_operation, {0, 1});
	operation->addRow(flag(QStringLiteral("stringExtract"), tr("Evaluate the control codes (\\V, \\T...)"), 3, 18));
	operation->addRow(flag(QStringLiteral("stringHex"), tr("Hexadecimal"), 3, 17));

	m_arguments = section(layout, tr("Arguments"));

	// the literal texts of the arguments
	const int op = bitsOf(3, 0, 8);
	const int fn = bitsOf(3, 8, 8);
	const QString string = ToQString(m_cmd->string);
	int position = 0;
	const auto initial = arguments(op, fn);
	for (int i = 0; i < static_cast<int>(initial.size()); ++i) {
		if (initial[i].kind != Arg::Str) {
			continue;
		}
		if (!multipleTexts(op, fn)) {
			m_texts.append(string);
		} else if (bitsOf(0, 4 + 4 * i, 4) == 0) {
			m_texts.append(middle(string, position, parameter(4 + i)));
			position += parameter(4 + i);
		} else {
			m_texts.append(QString());
		}
	}
	rebuildArguments(false);
	connect(m_operation, &QComboBox::currentIndexChanged, this, [this] { rebuildArguments(true); });
	connect(m_function, &QComboBox::currentIndexChanged, this, [this] { rebuildArguments(true); });
}

void ManiacControlStringsWidget::rebuildArguments(bool user) {
	while (m_arguments->rowCount() > 0) {
		m_arguments->removeRow(0);
	}
	m_textEdits.clear();
	m_textArguments.clear();

	const int op = m_operation->currentData().toInt();
	const int fn = m_function->currentData().toInt();
	m_multipleTexts = multipleTexts(op, fn);
	const auto list = arguments(op, fn);
	QList<QWidget*> rows;
	for (int i = 0; i < static_cast<int>(list.size()); ++i) {
		const QString name = QStringLiteral("stringArgument%1").arg(i + 1);
		const int index = 4 + i;
		const int slot = i + 1;
		QWidget* row = nullptr;
		switch (list[i].kind) {
			case Arg::Str: {
				auto* edit = new QLineEdit(m_texts.value(m_textEdits.size()), this);
				edit->setObjectName(name + QStringLiteral("Text"));
				const int ordinal = static_cast<int>(m_textEdits.size());
				m_textEdits.append(edit);
				m_textArguments.append(i);
				connect(edit, &QLineEdit::textEdited, this, [this, ordinal](const QString& text) {
					while (m_texts.size() <= ordinal) {
						m_texts.append(QString());
					}
					m_texts[ordinal] = text;
					writeTexts();
				});
				row = new QWidget(this);
				auto* rowLayout = new QHBoxLayout(row);
				rowLayout->setContentsMargins(0, 0, 0, 0);
				auto* mode = modeCombo(name + QStringLiteral("Mode"), 0, slot, stringModes());
				auto* field = valueField(name, mode, index, 0, 99999, edit);
				rowLayout->addWidget(mode);
				rowLayout->addWidget(field, 1);
				// a text that becomes a string id (or the reverse) changes the string
				connect(mode, &QComboBox::currentIndexChanged, this, &ManiacControlStringsWidget::writeTexts);
				onCommit(row, [=, this] {
					commit(mode);
					commit(field);
				});
				break;
			}
			case Arg::Num:
				row = value(name, 0, slot, index);
				break;
			case Arg::Var:
				row = value(name, 0, slot, index, 1, 99999999, targetModes());
				break;
			case Arg::VarPlain:
				row = variable(name, index);
				break;
			case Arg::StrOut:
				row = value(name, 0, slot, index, 1, 99999,
					{{0, tr("String"), Kind::StringId}, {1, tr("String pointer"), Kind::Variable}});
				break;
			case Arg::StrPlain:
				row = number(name, index, 0, 99999);
				break;
			case Arg::Switch:
				row = value(name, 0, slot, index, 1, 99999,
					{{0, tr("Switch"), Kind::Number}, {1, tr("Switch pointer"), Kind::Variable}});
				break;
			case Arg::Database:
				row = choice(name, index, {{0, tr("Hero")}, {1, tr("Skill")}, {2, tr("Item")}, {3, tr("Enemy")},
					{4, tr("Troop")}, {5, tr("Terrain")}, {6, tr("Attribute")}, {7, tr("Condition")},
					{8, tr("Battle animation")}, {9, tr("Chipset")}, {10, tr("Switch")}, {11, tr("Variable")},
					{12, tr("String")}, {13, tr("Common event")}, {14, tr("Class")}, {15, tr("Battler animation")},
					{16, tr("Map")}, {17, tr("Map event")}, {18, tr("Party member")}});
				break;
			case Arg::Description:
				row = choice(name, index, {{0, tr("Hero (title)")}, {1, tr("Skill")}, {2, tr("Item")},
					{18, tr("Party member (title)")}});
				break;
			case Arg::Dynamic:
				row = choice(name, index, {{0, tr("From the database")}, {1, tr("Current (changed in the game)")}});
				break;
			case Arg::Encoding:
				row = choice(name, index, {{0, tr("ANSI (code page of the game)")}, {1, tr("UTF-8")}});
				break;
			case Arg::Join:
				row = value(name, 0, slot, index, 1, 99999999, {{0, tr("Strings from"), Kind::StringId},
					{1, tr("Strings from pointer"), Kind::Variable}, {2, tr("Variables from"), Kind::Variable},
					{3, tr("Variables from pointer"), Kind::Variable}});
				break;
		}
		m_arguments->addRow(tr(list[i].label), row);
		rows.append(row);
	}
	if ((op == 0 || op == 1) && fn == 14) {
		auto* first = flag(QStringLiteral("stringFirstOnly"), tr("Replace the first match only"), 3, 19);
		m_arguments->addRow(first);
		rows.append(first);
	}

	// the new arguments read the parameters another way: store what they show
	if (user) {
		for (auto* row : rows) {
			commit(row);
		}
		writeTexts();
	}
}

void ManiacControlStringsWidget::writeTexts() {
	QString string;
	for (int ordinal = 0; ordinal < m_textEdits.size(); ++ordinal) {
		const int argument = m_textArguments[ordinal];
		if (bitsOf(0, 4 + 4 * (argument + 1), 4) != 0) {
			continue;
		}
		const QString text = m_textEdits[ordinal]->text();
		if (m_multipleTexts) {
			string += text;
			setParameter(4 + argument, length(text));
		} else {
			string = text;
		}
	}
	m_cmd->string = ToDBString(string);
}

ManiacShowStringPictureWidget::ManiacShowStringPictureWidget(ProjectData& project, QWidget* parent) :
	ManiacCommandWidget(project, parent) {}

void ManiacShowStringPictureWidget::writeString() {
	QString string;
	for (int i = 0; i < 3; ++i) {
		string += QChar(m_modes[i] + 1) + m_parts[i];
	}
	m_cmd->string = ToDBString(string);
}

void ManiacShowStringPictureWidget::build(QBoxLayout* layout) {
	// "\x01text\x01system\x01font": each separator is the mode of the part plus one
	const QString string = ToQString(m_cmd->string);
	QStringList parts = {QString()};
	QList<int> modes;
	for (const QChar c : string) {
		if (c.unicode() >= 1 && c.unicode() <= 3) {
			modes.append(c.unicode() - 1);
			parts.append(QString());
		} else {
			parts.back() += c;
		}
	}
	if (parts.size() >= 4) {
		for (int i = 0; i < 3; ++i) {
			m_modes[i] = modes[i];
			m_parts[i] = parts[i + 1];
		}
	} else {
		m_parts[0] = string;
	}

	auto* tabs = new QTabWidget(this);
	tabs->setObjectName(QStringLiteral("stringPictureTabs"));
	layout->addWidget(tabs);
	// a tab of two columns
	auto page = [&](const QString& title) {
		auto* widget = new QWidget(tabs);
		auto* columns = new QHBoxLayout(widget);
		auto* left = new QVBoxLayout();
		auto* right = new QVBoxLayout();
		columns->addLayout(left, 1);
		columns->addLayout(right, 1);
		tabs->addTab(widget, title);
		return std::make_pair(left, right);
	};

	// Text
	auto [textPage, fontPage] = page(tr("Text"));
	auto* text = section(textPage, tr("Text"));
	auto* textMode = modeChooser(QStringLiteral("stringPictureTextMode"), m_modes[0], stringModes());
	auto* textEdit = new QPlainTextEdit(m_parts[0], this);
	textEdit->setObjectName(QStringLiteral("stringPictureText"));
	text->addRow(tr("Given as"), textMode);
	text->addRow(valueField(QStringLiteral("stringPictureTextId"), textMode, 22, 0, 99999, textEdit));
	connect(textMode, &QComboBox::currentIndexChanged, this, [=, this] {
		m_modes[0] = textMode->currentData().toInt();
		writeString();
	});
	connect(textEdit, &QPlainTextEdit::textChanged, this, [=, this] {
		m_parts[0] = textEdit->toPlainText();
		writeString();
	});

	auto* font = section(fontPage, tr("Font"));
	// the system graphic and the font: a name, or the id of a string written in the part
	auto namePart = [&](QFormLayout* form, int part, const QString& name, const QString& label) {
		auto* mode = modeChooser(name + QStringLiteral("Mode"), m_modes[part], stringModes());
		auto* pages = new QStackedWidget(this);
		auto* edit = new QLineEdit(m_modes[part] == 0 ? m_parts[part] : QString(), pages);
		edit->setObjectName(name);
		auto* id = new RpgSpinBox(pages);
		id->setObjectName(name + QStringLiteral("Id"));
		id->setRange(0, 99999);
		id->setValue(m_modes[part] == 0 ? 0 : static_cast<int>(decodeId(m_parts[part])));
		pages->addWidget(edit);
		pages->addWidget(id);
		pages->setCurrentIndex(m_modes[part] == 0 ? 0 : 1);
		auto write = [=, this] {
			m_modes[part] = mode->currentData().toInt();
			pages->setCurrentIndex(m_modes[part] == 0 ? 0 : 1);
			m_parts[part] = m_modes[part] == 0 ? edit->text() : encodeId(static_cast<uint32_t>(id->value()));
			writeString();
		};
		connect(mode, &QComboBox::currentIndexChanged, this, write);
		connect(edit, &QLineEdit::textEdited, this, write);
		connect(id, &QSpinBox::valueChanged, this, write);
		auto* row = new QWidget(this);
		auto* rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		rowLayout->addWidget(mode);
		rowLayout->addWidget(pages, 1);
		form->addRow(label, row);
	};
	namePart(font, 2, QStringLiteral("stringPictureFont"), tr("Font (empty: default)"));
	font->addRow(tr("Size"), value(QStringLiteral("stringPictureFontSize"), 0, 5, 20, 0, 999));
	font->addRow(tr("Letter spacing"), number(QStringLiteral("stringPictureLetterSpacing"), 14, 0, 255, 8, 8));
	font->addRow(tr("Line spacing"), number(QStringLiteral("stringPictureLineSpacing"), 14, 0, 255, 16, 8));
	font->addRow(flag(QStringLiteral("stringPictureBold"), tr("Bold"), 13, 15));
	font->addRow(flag(QStringLiteral("stringPictureNoShadow"), tr("No shadow"), 13, 14));
	font->addRow(flag(QStringLiteral("stringPictureNoGradient"), tr("No gradient"), 13, 13));

	fontPage->addStretch(1);

	// Window
	auto [windowPage, windowRight] = page(tr("Window"));
	auto* window = section(windowPage, tr("Window"));
	namePart(window, 1, QStringLiteral("stringPictureSystem"), tr("System graphic (empty: default)"));
	auto* sizeMode = modeCombo(QStringLiteral("stringPictureSizeMode"), 0, 4, numberModes());
	window->addRow(tr("Size given as"), sizeMode);
	window->addRow(tr("Width (0: fit the text)"), valueField(QStringLiteral("stringPictureWidth"), sizeMode, 18, 0, 9999));
	window->addRow(tr("Height (0: fit the text)"), valueField(QStringLiteral("stringPictureHeight"), sizeMode, 19, 0, 9999));
	window->addRow(tr("Background"), choice(QStringLiteral("stringPictureBackground"), 13,
		{{0, tr("None")}, {1, tr("Stretched")}, {2, tr("Tiled")}}, 8, 4));
	window->addRow(flag(QStringLiteral("stringPictureTranslucent"), tr("Translucent background"), 14, 0));
	window->addRow(flag(QStringLiteral("stringPictureNoFrame"), tr("No frame"), 13, 12));
	window->addRow(flag(QStringLiteral("stringPictureNoPadding"), tr("No padding"), 13, 16));
	windowPage->addStretch(1);
	windowRight->addStretch(1);

	// Picture
	auto [left, right] = page(tr("Picture"));
	auto* picture = section(left, tr("Picture"));
	picture->addRow(tr("Picture No."), value(QStringLiteral("stringPictureId"), 0, 0, 1, 1, 9999));
	auto* positionMode = modeCombo(QStringLiteral("stringPicturePositionMode"), 0, 1, numberModes());
	picture->addRow(tr("Position given as"), positionMode);
	picture->addRow(tr("X"), valueField(QStringLiteral("stringPictureX"), positionMode, 2, -9999, 9999));
	picture->addRow(tr("Y"), valueField(QStringLiteral("stringPictureY"), positionMode, 3, -9999, 9999));
	picture->addRow(tr("Origin"), choice(QStringLiteral("stringPictureOrigin"), 0, {{0, tr("Center")},
		{1, tr("Top left")}, {2, tr("Bottom left")}, {3, tr("Top right")}, {4, tr("Bottom right")},
		{5, tr("Top")}, {6, tr("Bottom")}, {7, tr("Left")}, {8, tr("Right")}}, 24, 4));
	picture->addRow(flag(QStringLiteral("stringPictureScroll"), tr("Scrolls with the map"), 13, 0));
	picture->addRow(tr("Map layer"), number(QStringLiteral("stringPictureMapLayer"), 15, 0, 10));
	picture->addRow(tr("Battle layer"), number(QStringLiteral("stringPictureBattleLayer"), 16, 0, 10));

	auto* size = section(left, tr("Magnification and Transparency"));
	size->addRow(tr("Magnification (%)"), value(QStringLiteral("stringPictureMagnification"), 0, 2, 4, 0, 2000));
	auto* separate = flag(QStringLiteral("stringPictureSeparateHeight"), tr("Height magnified apart"), 0, 28);
	size->addRow(separate);
	// the mode of the height is in the 3 highest bits, after this flag
	auto* heightMode = modeComboAt(QStringLiteral("stringPictureHeightMagnificationMode"), 0, 29, 3, numberModes());
	auto* height = new QWidget(this);
	auto* heightLayout = new QHBoxLayout(height);
	heightLayout->setContentsMargins(0, 0, 0, 0);
	heightLayout->addWidget(heightMode);
	heightLayout->addWidget(valueField(QStringLiteral("stringPictureHeightMagnification"), heightMode, 23, 0, 2000), 1);
	size->addRow(tr("Height (%)"), height);
	height->setEnabled(separate->isChecked());
	connect(separate, &QCheckBox::toggled, height, &QWidget::setEnabled);
	size->addRow(tr("Transparency (%)"), value(QStringLiteral("stringPictureTransparency"), 0, 3, 5, 0, 100));

	auto* options = section(right, tr("Options"));
	options->addRow(flag(QStringLiteral("stringPictureEraseOnMapChange"), tr("Erased when the map changes"), 17, 0));
	options->addRow(flag(QStringLiteral("stringPictureEraseOnBattleEnd"), tr("Erased when the battle ends"), 17, 1));
	options->addRow(flag(QStringLiteral("stringPictureTint"), tr("Affected by Tint Screen"), 17, 4));
	options->addRow(flag(QStringLiteral("stringPictureFlash"), tr("Affected by Flash Screen"), 17, 5));
	options->addRow(flag(QStringLiteral("stringPictureShake"), tr("Affected by Shake Screen"), 17, 6));
	left->addStretch(1);
	right->addStretch(1);

	// Effects
	auto [colorPage, effectPage] = page(tr("Effects"));
	auto* color = section(colorPage, tr("Color"));
	color->addRow(tr("Red (%)"), number(QStringLiteral("stringPictureRed"), 6, 0, 200));
	color->addRow(tr("Green (%)"), number(QStringLiteral("stringPictureGreen"), 7, 0, 200));
	color->addRow(tr("Blue (%)"), number(QStringLiteral("stringPictureBlue"), 8, 0, 200));
	color->addRow(tr("Saturation (%)"), number(QStringLiteral("stringPictureSaturation"), 9, 0, 200));
	color->addRow(tr("Blending"), choice(QStringLiteral("stringPictureBlend"), 12,
		{{0, tr("Normal")}, {1, tr("Multiply")}, {2, tr("Additive")}, {3, tr("Overlay")}}, 0, 4));
	color->addRow(flag(QStringLiteral("stringPictureFlipX"), tr("Flip horizontally"), 12, 4));
	color->addRow(flag(QStringLiteral("stringPictureFlipY"), tr("Flip vertically"), 12, 5));

	auto* effects = section(effectPage, tr("Effect"));
	auto* effect = choice(QStringLiteral("stringPictureEffect"), 10,
		{{0, tr("None")}, {1, tr("Rotation")}, {2, tr("Wave")}, {3, tr("Fixed angle")}});
	effects->addRow(tr("Effect"), effect);
	auto* power = number(QStringLiteral("stringPicturePower"), 11, -9999, 9999);
	auto* angle = value(QStringLiteral("stringPictureAngle"), 13, 6, 11);
	auto* divisor = value(QStringLiteral("stringPictureDivisor"), 13, 7, 21);
	effects->addRow(tr("Power"), power);
	effects->addRow(tr("Angle"), angle);
	effects->addRow(tr("Divided by"), divisor);
	showWhen(effects, power, effect, {1, 2});
	showWhen(effects, angle, effect, {3});
	showWhen(effects, divisor, effect, {3});
	colorPage->addStretch(1);
	effectPage->addStretch(1);
}
