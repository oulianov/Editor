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

#include "ui/event/event_command_base_widget.h"
#include <QHash>
#include <QList>
#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

class QBoxLayout;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QLineEdit;
class QPlainTextEdit;
class RpgSpinBox;

/**
 * Editor of the Maniac Patch commands (codes 3001-3032). The layout of each
 * command is the one EasyRPG Player reads (game_interpreter.cpp,
 * game_interpreter_battle.cpp): most values have a "mode" (a number, a
 * variable, a variable pointer...) packed 4 bits per value in another
 * parameter. The controls only write the parameters the user changes.
 * Also the base of the editors of standard commands that Maniac Patch
 * extends (Control Variables, Conditional Branch, pictures), for its helpers.
 */
class ManiacCommandWidget : public EventCommandBaseWidget {
	Q_OBJECT
public:
	ManiacCommandWidget(ProjectData& project, QWidget* parent = nullptr);
	void setData(lcf::rpg::EventCommand* cmd) override;

	/** The control that edits a value, after its mode */
	enum class Kind { Number, Variable, StringId, Text };
	struct Mode {
		int mode;
		QString label;
		Kind kind;
	};
	using Items = QList<std::pair<int, QString>>;

	/** A number: constant, V[x] or V[V[x]] */
	static std::vector<Mode> numberModes();
	/** A variable id: V[x] or V[V[x]] */
	static std::vector<Mode> targetModes();
	/** A string: text of the command, T[x] or T[V[x]] */
	static std::vector<Mode> stringModes();

protected:
	/** Adds the controls of the command */
	virtual void build(QBoxLayout* layout);

	int parameter(int index, int fallback = 0) const;
	/** Writes value in the bits [shift, shift + bits) of a parameter */
	void setParameter(int index, int value, int shift = 0, int bits = 32);
	int bitsOf(int index, int shift, int bits) const;
	/** Makes room for parameter index: commands whose newer parameters have defaults override it */
	virtual void growParameters(int index);
	/** Selects the item of value, or adds it as "Unknown (n)": a value of a newer patch is kept */
	static void select(QComboBox* combo, int value);
	/** The Kind of the current mode of a mode combo box */
	static Kind kindOf(QComboBox* mode);
	/** True when the open game uses Maniac Patch */
	bool maniac() const;

	/** A titled section with a form */
	QFormLayout* section(QBoxLayout* layout, const QString& title);
	/** A choice stored in the bits of a parameter; unknown values are kept */
	QComboBox* choice(const QString& name, int index, const Items& items, int shift = 0, int bits = 32);
	QCheckBox* flag(const QString& name, const QString& label, int index, int bit);
	/** A check box for a whole parameter: 0 or 1 */
	QCheckBox* boolean(const QString& name, const QString& label, int index);
	RpgSpinBox* number(const QString& name, int index, int minimum, int maximum, int shift = 0, int bits = 32);
	QWidget* variable(const QString& name, int index);
	/** The mode of a value: nibble slot of the parameter, -1 for the whole parameter */
	QComboBox* modeCombo(const QString& name, int index, int slot, const std::vector<Mode>& modes);
	/** The mode of a value in the bits [shift, shift + bits) of the parameter */
	QComboBox* modeComboAt(const QString& name, int index, int shift, int bits, const std::vector<Mode>& modes);
	/** A mode combo box bound to nothing, for the modes kept in the string */
	QComboBox* modeChooser(const QString& name, int mode, const std::vector<Mode>& modes);
	/** The value edited after the current mode of mode; text is the control of Kind::Text */
	QWidget* valueField(const QString& name, QComboBox* mode, int index, int minimum, int maximum, QWidget* text = nullptr);
	/** A mode and its value on one row */
	QWidget* value(const QString& name, int modeIndex, int slot, int index, int minimum = -99999999,
		int maximum = 99999999, const std::vector<Mode>& modes = numberModes());
	/** A text field for the string of the command */
	QLineEdit* textField(const QString& name);
	/** A Maniac Patch expression (bytecode words) edited as text; write gets the words of a valid text */
	QWidget* expressionField(const QString& name, std::span<const int32_t> words,
		std::function<void(const std::vector<int32_t>&)> write);
	/** Shows a row of form only when combo has one of values */
	void showWhen(QFormLayout* form, QWidget* field, QComboBox* combo, const QList<int>& values);
	/** Writes what a control shows, for a control made visible again */
	void commit(QWidget* widget);
	void onCommit(QWidget* widget, std::function<void()> write);

private:
	QHash<QWidget*, std::function<void()>> m_commits;
};

/** Control Strings (3020): the arguments depend on the operation */
class ManiacControlStringsWidget : public ManiacCommandWidget {
	Q_OBJECT
public:
	ManiacControlStringsWidget(ProjectData& project, QWidget* parent = nullptr);

protected:
	void build(QBoxLayout* layout) override;

private:
	void rebuildArguments(bool user);
	void writeTexts();

	QFormLayout* m_arguments = nullptr;
	QComboBox* m_operation = nullptr;
	QComboBox* m_function = nullptr;
	/** The literal texts of the arguments, in their order in the string */
	QStringList m_texts;
	QList<QLineEdit*> m_textEdits;
	QList<int> m_textArguments;
	bool m_multipleTexts = false;
};

/** Show String Picture (3007): a window with a text shown as a picture */
class ManiacShowStringPictureWidget : public ManiacCommandWidget {
	Q_OBJECT
public:
	ManiacShowStringPictureWidget(ProjectData& project, QWidget* parent = nullptr);

protected:
	void build(QBoxLayout* layout) override;

private:
	void writeString();

	// the three parts of the string: text, system graphic, font; mode 0 is a
	// literal, 1 and 2 a string id (system graphic and font: encoded in the part)
	int m_modes[3] = {0, 0, 0};
	QString m_parts[3];
};
