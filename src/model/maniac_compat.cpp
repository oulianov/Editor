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

#include "maniac_compat.h"
#include "common/dbstring.h"
#include "common/filefinder.h"
#include "common/ini_file.h"
#include "defines.h"
#include "model/game_patches.h"
#include "model/maniac_expression.h"
#include "model/project.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <lcf/reader_util.h>
#include <lcf/rpg/map.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <set>

namespace {

using lcf::rpg::EventCommand;
using Cmd = EventCommand::Code;
using Group = ManiacCompat::Group;
using Finding = ManiacCompat::Finding;
using Commands = std::vector<EventCommand>;

/** Power Mode 2003's registers (RuntimePatches::PowerMode2003 of EasyRPG Player) */
constexpr int kCommandRegister = 1;
constexpr int kMouseX = 2;
constexpr int kMouseY = 3;
constexpr int kKey = 4;
constexpr int kFloat1 = 5;
constexpr int kFloat2 = 6;
constexpr int kFloatCode = 7;
constexpr int kRotations = 8;
/** Power Mode's decimals: a fraction times a million */
constexpr int kMillion = 1000000;

QString variable(int id) {
	return QStringLiteral("V[%1]").arg(id, 4, 10, QLatin1Char('0'));
}

EventCommand make(Cmd code, int indent, std::vector<int32_t> parameters, const std::string& text = {}) {
	EventCommand command;
	command.code = static_cast<int32_t>(code);
	command.indent = indent;
	command.string = lcf::DBString(text);
	command.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	return command;
}

/** Control Variables on one variable: operation, operand and its values */
EventCommand controlVariable(int indent, int id, int operation, int operand, std::vector<int32_t> values) {
	std::vector<int32_t> parameters = {0, id, id, operation, operand};
	parameters.insert(parameters.end(), values.begin(), values.end());
	// the operand's second value, written by RPG Maker even when unused
	if (parameters.size() < 7) {
		parameters.resize(7, 0);
	}
	return make(Cmd::ControlVars, indent, parameters);
}

/** Control Variables: id = a Maniac Patch expression (operand 21: its words after their count) */
EventCommand expression(int indent, int id, const std::string& text) {
	std::vector<int32_t> words;
	size_t error = 0;
	ManiacExpression::encode(text, words, error);
	std::vector<int32_t> parameters = {0, id, id, 0, 21, static_cast<int32_t>(words.size())};
	parameters.insert(parameters.end(), words.begin(), words.end());
	return make(Cmd::ControlVars, indent, parameters);
}

/** Key Input Processing Ex: whether a key (a virtual key code of Windows) is pressed, 1 or 0, into id */
EventCommand keyPressed(int indent, int id, int key) {
	return make(Cmd::Maniac_KeyInputProcEx, indent, {2, id, 0, key});
}

/** The name of a virtual key code of Windows (Power Mode's and Maniac's keys) */
QString keyName(int code) {
	if (code >= '0' && code <= '9') {
		return QString(QChar(code));
	}
	if (code >= 'A' && code <= 'Z') {
		return QString(QChar(code));
	}
	if (code >= 0x60 && code <= 0x69) {
		return QCoreApplication::translate("ManiacCompat", "Numpad %1").arg(code - 0x60);
	}
	if (code >= 0x70 && code <= 0x7B) {
		return QStringLiteral("F%1").arg(code - 0x70 + 1);
	}
	static const QHash<int, const char*> names = {
		{0x01, QT_TRANSLATE_NOOP("ManiacCompat", "Left mouse button")},
		{0x02, QT_TRANSLATE_NOOP("ManiacCompat", "Right mouse button")},
		{0x04, QT_TRANSLATE_NOOP("ManiacCompat", "Middle mouse button")},
		{0x08, QT_TRANSLATE_NOOP("ManiacCompat", "Backspace")},
		{0x09, QT_TRANSLATE_NOOP("ManiacCompat", "Tab")},
		{0x0D, QT_TRANSLATE_NOOP("ManiacCompat", "Enter")},
		{0x10, QT_TRANSLATE_NOOP("ManiacCompat", "Shift")},
		{0x11, QT_TRANSLATE_NOOP("ManiacCompat", "Ctrl")},
		{0x12, QT_TRANSLATE_NOOP("ManiacCompat", "Alt")},
		{0x1B, QT_TRANSLATE_NOOP("ManiacCompat", "Escape")},
		{0x20, QT_TRANSLATE_NOOP("ManiacCompat", "Space")},
		{0x21, QT_TRANSLATE_NOOP("ManiacCompat", "Page Up")},
		{0x22, QT_TRANSLATE_NOOP("ManiacCompat", "Page Down")},
		{0x23, QT_TRANSLATE_NOOP("ManiacCompat", "End")},
		{0x24, QT_TRANSLATE_NOOP("ManiacCompat", "Home")},
		{0x25, QT_TRANSLATE_NOOP("ManiacCompat", "Left")},
		{0x26, QT_TRANSLATE_NOOP("ManiacCompat", "Up")},
		{0x27, QT_TRANSLATE_NOOP("ManiacCompat", "Right")},
		{0x28, QT_TRANSLATE_NOOP("ManiacCompat", "Down")},
		{0x2D, QT_TRANSLATE_NOOP("ManiacCompat", "Insert")},
		{0x2E, QT_TRANSLATE_NOOP("ManiacCompat", "Delete")},
	};
	const auto found = names.find(code);
	return found != names.end() ? QCoreApplication::translate("ManiacCompat", *found)
		: QCoreApplication::translate("ManiacCompat", "code %1").arg(code);
}

/** The commands EasyRPG Player alone runs (lcf's EasyRpg_ codes) */
QString easyRpgCommandName(int code) {
	switch (static_cast<Cmd>(code)) {
	case Cmd::EasyRpg_TriggerEventAt: return QCoreApplication::translate("ManiacCompat", "Trigger Event At");
	case Cmd::EasyRpg_Pathfinder: return QCoreApplication::translate("ManiacCompat", "Pathfinder");
	case Cmd::EasyRpg_CallMovementAction: return QCoreApplication::translate("ManiacCompat", "Call Movement Action");
	case Cmd::EasyRpg_WaitForSingleMovement: return QCoreApplication::translate("ManiacCompat", "Wait for Single Movement");
	case Cmd::EasyRpg_AnimateVariable: return QCoreApplication::translate("ManiacCompat", "Animate Variable");
	case Cmd::EasyRpg_SetInterpreterFlag: return QCoreApplication::translate("ManiacCompat", "Set Interpreter Flag");
	case Cmd::EasyRpg_ProcessJson: return QCoreApplication::translate("ManiacCompat", "Process JSON");
	case Cmd::EasyRpg_CloneMapEvent: return QCoreApplication::translate("ManiacCompat", "Clone Map Event");
	case Cmd::EasyRpg_DestroyMapEvent: return QCoreApplication::translate("ManiacCompat", "Destroy Map Event");
	case Cmd::EasyRpg_StringPictureMenu: return QCoreApplication::translate("ManiacCompat", "String Picture Menu");
	default: return QCoreApplication::translate("ManiacCompat", "Command %1").arg(code);
	}
}

/** An action of a script of the Key Patch ([execute] and the sections of next=) */
struct ScriptAction {
	QByteArray name;
	QByteArray arg;
	QByteArray arg2;
};

/** A comment's words: @name a, "b c", d (DynRPG's syntax) */
struct CommentCall {
	QString name;
	QStringList args;
};

std::optional<CommentCall> parseCall(const QString& text) {
	if (!text.startsWith(u'@')) {
		return std::nullopt;
	}
	CommentCall call;
	int i = 1;
	while (i < text.size() && !text[i].isSpace() && text[i] != u',') {
		call.name += text[i++].toLower();
	}
	QString arg;
	bool quoted = false;
	bool any = false;
	for (; i < text.size(); ++i) {
		const QChar c = text[i];
		if (c == u'"') {
			quoted = !quoted;
			any = true;
		} else if (c == u',' && !quoted) {
			call.args << arg.trimmed();
			arg.clear();
			any = false;
		} else {
			arg += c;
			any = any || !c.isSpace();
		}
	}
	if (any || !call.args.isEmpty()) {
		call.args << arg.trimmed();
	}
	return call;
}

/**
 * Power Mode's decimal math (V7 = operation): V5 and V6 hold the result, a
 * fraction times a million, as EasyRPG Player computes it
 * (RuntimePatches::PowerMode2003). The division keeps V5 in V7 meanwhile,
 * then V7 gets the operation again.
 */
void floatMath(int operation, int indent, Commands& out) {
	const QString v5 = QStringLiteral("V[%1]").arg(kFloat1);
	const QString v6 = QStringLiteral("V[%1]").arg(kFloat2);
	const QString v7 = QStringLiteral("V[%1]").arg(kFloatCode);
	switch (operation) {
	case 1:
		// sin(V5) to V6, then cos(V5) to V5 (Maniac's operands 13, 14)
		out.push_back(controlVariable(indent, kFloat2, 0, 13, {kFloat1, kMillion, 1, 0}));
		out.push_back(controlVariable(indent, kFloat1, 0, 14, {kFloat1, kMillion, 1, 0}));
		break;
	case 2:
		out.push_back(expression(indent, kFloat2, QStringLiteral("muldiv(sin(%1, 0, %2), %2, cos(%1, 0, %2))")
			.arg(v5).arg(kMillion).toStdString()));
		break;
	case 3:
		// the fraction from the root times a million, then the whole part (operand 12)
		out.push_back(controlVariable(indent, kFloat2, 0, 12, {kFloat1, kMillion, 1}));
		out.push_back(controlVariable(indent, kFloat2, 5, 0, {kMillion}));
		out.push_back(controlVariable(indent, kFloat1, 0, 12, {kFloat1, 1, 1}));
		break;
	case 4:
		out.push_back(controlVariable(indent, kFloatCode, 0, 1, {kFloat1}));
		out.push_back(controlVariable(indent, kFloat1, 4, 1, {kFloat2}));
		out.push_back(expression(indent, kFloat2, (QStringLiteral("muldiv(") + v7 + QStringLiteral(" % ") + v6
			+ QStringLiteral(", %1, %2)").arg(kMillion).arg(v6)).toStdString()));
		out.push_back(controlVariable(indent, kFloatCode, 0, 0, {4}));
		break;
	}
}

class Scan {
	Q_DECLARE_TR_FUNCTIONS(ManiacCompat)
public:
	Scan(const Project& project, bool apply);

	std::vector<Finding> findings;
	/** The places converted */
	int converted = 0;

	/** The commands of a page (converted in place); whether they changed */
	bool commands(Commands& list, const QString& where);
	/** The common events, the monster groups and the system; whether they changed */
	bool database(lcf::rpg::Database& db);
	/** The events of a map; whether they changed */
	bool map(std::vector<lcf::rpg::Event>& events, int id, const QString& name);

private:
	/** Converts the command at i (and the lines after it it takes): out
	 *  receives what replaces them; false: it stays, with the lines it
	 *  takes (those a conversion made after it, not read again) */
	bool command(const Commands& list, size_t i, const QString& place, Commands& out, size_t& taken);
	bool easyRpgComment(const Commands& list, size_t i, const QString& place, Commands& out, size_t& taken);
	/** Power Mode's registers written; a command already converted (the
	 *  conversion after it) stays as it is */
	bool powerVariables(const Commands& list, size_t i, const QString& place, Commands& out, size_t& taken);
	bool powerPicture(const EventCommand& com, const QString& place, Commands& out);
	bool keyScript(const Commands& list, size_t i, const QString& place, Commands& out, size_t& taken);
	/** A sound or music named *.link: the file it plays, in folder; empty if it cannot be played directly */
	QString linkTarget(const QString& name, const char* folder, QString* why) const;
	bool link(QString& name, const char* folder, const QString& place);

	std::vector<ScriptAction> script(const QString& name, bool* found);

	void found(Group group, const QString& what, const QString& becomes, const QString& place);

	const Project& m_project;
	const bool m_apply;
	bool m_powerMode = false;
	bool m_dynRpg = false;
	/** The id enableMouseSupport gives the mouse position (none: no script, or several ids) */
	std::optional<int> m_mousePrefix;
	QHash<QString, int> m_index;
	QHash<QString, std::vector<ScriptAction>> m_scripts;
};

Scan::Scan(const Project& project, bool apply) : m_project(project), m_apply(apply) {
	const auto values = GamePatches::read(project).values;
	// Power Mode's game, also once converted: warp.dll, or the patch written
	// off next to the Maniac Patch on (as the conversion leaves EasyRPG.ini)
	m_powerMode = values.value(QStringLiteral("PowerMode2003")) > 0 || !project.findFile(QStringLiteral("warp.dll")).isEmpty();
	if (values.value(QStringLiteral("Maniac")) > 0) {
		for (const auto& [key, value] : IniFile::read(project.findFileOrDefault(QStringLiteral("EasyRPG.ini")), "Patch")) {
			m_powerMode = m_powerMode || key.compare("PowerMode2003", Qt::CaseInsensitive) == 0;
		}
	}
	m_dynRpg = values.value(QStringLiteral("DynRPG")) > 0;
	// the mouse of the Key Patch: the id its scripts turn it on with
	const QString sounds = project.findDirectory(QStringLiteral(SOUND));
	QSet<int> ids;
	if (!sounds.isEmpty()) {
		for (const QString& file : QDir(sounds).entryList({QStringLiteral("*.script"), QStringLiteral("*.script.*")}, QDir::Files)) {
			bool exists = false;
			for (const auto& action : script(file.left(file.lastIndexOf(QLatin1String(".script")) + 7), &exists)) {
				if (action.name == "enablemousesupport" && action.arg.trimmed().toLower() == "true") {
					ids.insert(action.arg2.trimmed().toInt());
				}
			}
		}
	}
	if (ids.size() == 1) {
		m_mousePrefix = *ids.begin();
	}
}

void Scan::found(Group group, const QString& what, const QString& becomes, const QString& place) {
	const QString key = QStringLiteral("%1\n%2\n%3").arg(static_cast<int>(group)).arg(what, becomes);
	auto it = m_index.find(key);
	if (it == m_index.end()) {
		it = m_index.insert(key, static_cast<int>(findings.size()));
		findings.push_back({group, what, becomes, {}});
	}
	findings[*it].places << place;
	if (!becomes.isEmpty()) {
		++converted;
	}
}

std::vector<ScriptAction> Scan::script(const QString& name, bool* found) {
	const QString key = name.toLower();
	// as it is, else as RPG Maker finds a sound: with an extension of audio (name.script.wav)
	QString path = FileFinder::Find(m_project.projectDir(), QStringLiteral(SOUND), name);
	if (path.isEmpty()) {
		path = FileFinder::Find(m_project.projectDir(), QStringLiteral(SOUND), name, FileFinder::FileType::Sound);
	}
	*found = !path.isEmpty();
	if (!*found) {
		return {};
	}
	auto cached = m_scripts.find(key);
	if (cached != m_scripts.end()) {
		return *cached;
	}
	// as Game_Ineluki::Parse: [execute], then the section each one names in next=
	std::vector<ScriptAction> actions;
	QSet<QByteArray> seen;
	QByteArray section = "execute";
	while (!section.isEmpty() && !seen.contains(section.toLower())) {
		seen.insert(section.toLower());
		QHash<QByteArray, QByteArray> keys;
		for (const auto& [entry, value] : IniFile::read(path, section)) {
			keys.insert(entry.toLower(), value.trimmed());
		}
		ScriptAction action;
		action.name = keys.value("action").toLower();
		if (action.name == "writetolog") {
			action.arg = keys.value("text");
		} else if (action.name == "execprogram" || action.name == "mcicommand") {
			action.arg = keys.value("command");
		} else if (action.name == "miditickfunction") {
			action.arg = keys.value("command", keys.value("value"));
		} else if (action.name == "addoutput" || action.name == "setmouseasreturn" || action.name == "setmousewheelaskeys") {
			action.arg = keys.value("value");
		} else if (action.name == "enablekeysupport") {
			action.arg = keys.value("enable");
		} else if (action.name == "registerkeydownevent" || action.name == "registerkeyupevent") {
			action.arg = keys.value("key");
			action.arg2 = keys.value("value");
		} else if (action.name == "enablemousesupport") {
			action.arg = keys.value("enable");
			action.arg2 = keys.value("id");
		} else if (action.name == "registercheatevent") {
			action.arg = keys.value("cheat");
			action.arg2 = keys.value("value");
		}
		if (!action.name.isEmpty()) {
			actions.push_back(action);
		}
		section = keys.value("next");
	}
	m_scripts.insert(key, actions);
	return actions;
}

bool Scan::commands(Commands& list, const QString& where) {
	Commands result;
	bool changed = false;
	for (size_t i = 0; i < list.size();) {
		const QString place = tr("%1, line %2").arg(where).arg(i + 1);
		Commands out;
		size_t taken = 1;
		if (command(list, i, place, out, taken)) {
			changed = true;
			result.insert(result.end(), out.begin(), out.end());
			i += taken;
			continue;
		}
		// as it is, with the lines a conversion made after it (taken)
		const size_t kept = std::min(taken, list.size() - i);
		result.insert(result.end(), list.begin() + static_cast<std::ptrdiff_t>(i), list.begin() + static_cast<std::ptrdiff_t>(i + kept));
		i += kept;
	}
	if (changed && m_apply) {
		list = std::move(result);
	}
	return changed;
}

bool Scan::command(const Commands& list, size_t i, const QString& place, Commands& out, size_t& taken) {
	const EventCommand& com = list[i];
	const auto parameter = [&com](size_t index) {
		return index < com.parameters.size() ? com.parameters[index] : 0;
	};
	if (com.code >= 2000 && com.code < 3000) {
		found(Group::EasyRpg, tr("%1 command").arg(easyRpgCommandName(com.code)), {}, place);
		return false;
	}
	switch (static_cast<Cmd>(com.code)) {
	case Cmd::Comment:
		return easyRpgComment(list, i, place, out, taken);
	case Cmd::Comment_2:
		// a line of a comment calling EasyRPG Player (not the first): it stays
		if (!m_dynRpg && ToQString(com.string).startsWith(QLatin1String("@easyrpg_"))) {
			found(Group::EasyRpg, tr("Comment %1").arg(ToQString(com.string).section(u' ', 0, 0)), {}, place);
		}
		return false;
	case Cmd::OpenSaveMenu:
		// EasyRPG Player's screens of the main menu (Call System Functions 200-209)
		if (parameter(0) >= 200 && parameter(0) < 210) {
			found(Group::EasyRpg, tr("Call System Functions %1: a screen of EasyRPG Player").arg(parameter(0)), {}, place);
		}
		return false;
	case Cmd::ControlVars:
		return m_powerMode && powerVariables(list, i, place, out, taken);
	case Cmd::ConditionalBranch:
	case Cmd::ConditionalBranch_B:
		// variable 1 compared: whether a save file exists, as Power Mode keeps it
		if (m_powerMode && parameter(0) == 1 && parameter(1) == kCommandRegister) {
			found(Group::PowerMode, tr("Condition on %1: whether a save file exists").arg(variable(kCommandRegister)), {}, place);
		}
		return false;
	case Cmd::Maniac_KeyInputProcEx:
		// a key of Power Mode converted: V4 = whether pressed, times the key
		if (m_powerMode && parameter(0) == 2 && parameter(1) == kKey && i + 1 < list.size()
				&& list[i + 1] == controlVariable(com.indent, kKey, 3, 0, {parameter(3)})) {
			taken = 2;
		}
		return false;
	case Cmd::ShowPicture:
	case Cmd::MovePicture:
		return m_powerMode && powerPicture(com, place, out);
	case Cmd::PlaySound: {
		const QString name = ToQString(com.string);
		if (name.endsWith(QLatin1String(".script"), Qt::CaseInsensitive)) {
			return keyScript(list, i, place, out, taken);
		}
		if (QString target = name; name.endsWith(QLatin1String(".link"), Qt::CaseInsensitive) && link(target, SOUND, place)) {
			EventCommand changed = com;
			changed.string = ToDBString(target);
			out = {changed};
			return true;
		}
		return false;
	}
	case Cmd::PlayBGM:
	case Cmd::ChangeSystemBGM:
	case Cmd::ChangeSystemSFX: {
		if (!ToQString(com.string).endsWith(QLatin1String(".link"), Qt::CaseInsensitive)) {
			return false;
		}
		QString target = ToQString(com.string);
		if (link(target, com.code == static_cast<int>(Cmd::ChangeSystemSFX) ? SOUND : MUSIC, place)) {
			EventCommand changed = com;
			changed.string = ToDBString(target);
			out = {changed};
			return true;
		}
		return false;
	}
	default:
		return false;
	}
}

bool Scan::easyRpgComment(const Commands& list, size_t i, const QString& place, Commands& out, size_t& taken) {
	const EventCommand& com = list[i];
	QString text = ToQString(com.string);
	if (!text.startsWith(QLatin1String("@easyrpg_")) && !text.startsWith(QLatin1String("@call easyrpg_"))) {
		return false;
	}
	// its next lines, as EasyRPG Player reads them (Game_Interpreter::HandleDynRpgScript)
	size_t lines = 1;
	bool whole = true;
	for (size_t next = i + 1; next < list.size() && list[next].code == static_cast<int>(Cmd::Comment_2); ++next) {
		const QString line = ToQString(list[next].string);
		if (m_dynRpg) {
			text += QStringLiteral("\r\n") + line;
		} else if (line.isEmpty() || line.startsWith(u'@')) {
			// a line of the comment that is not part of the call: it would stay alone
			whole = false;
			break;
		} else {
			text += line;
		}
		++lines;
	}
	const auto call = parseCall(text);
	QString name = QStringLiteral("@") + (call ? call->name : QString());
	if (call && call->name == QLatin1String("call") && !call->args.isEmpty()) {
		name += u' ' + call->args.front();
	}
	auto integer = [](const QString& arg, qint64& value) {
		bool ok = false;
		value = arg.toLongLong(&ok);
		return ok;
	};
	if (whole && call && call->name == QLatin1String("easyrpg_add") && call->args.size() >= 2) {
		// the variable of the first argument gets the sum of the others
		qint64 target = 0;
		qint64 sum = 0;
		bool numbers = integer(call->args[0], target) && target > 0;
		for (int a = 1; numbers && a < call->args.size(); ++a) {
			qint64 value = 0;
			numbers = integer(call->args[a], value);
			sum += value;
		}
		if (numbers && sum >= INT32_MIN && sum <= INT32_MAX) {
			found(Group::EasyRpg, tr("Comment %1").arg(name),
				tr("Control Variables: %1 = the sum").arg(variable(static_cast<int>(target))), place);
			out = {controlVariable(com.indent, static_cast<int>(target), 0, 0, {static_cast<int32_t>(sum)})};
			taken = lines;
			return true;
		}
	}
	if (whole && call && call->name == QLatin1String("easyrpg_exit") && call->args.isEmpty()) {
		found(Group::EasyRpg, tr("Comment %1").arg(name), tr("Exit Game"), place);
		out = {make(Cmd::ExitGame, com.indent, {})};
		taken = lines;
		return true;
	}
	found(Group::EasyRpg, tr("Comment %1").arg(name), {}, place);
	return false;
}

bool Scan::powerVariables(const Commands& list, size_t i, const QString& place, Commands& out, size_t& taken) {
	const EventCommand& com = list[i];
	const auto parameter = [&com](size_t index) {
		return index < com.parameters.size() ? com.parameters[index] : 0;
	};
	int first = 0;
	int last = 0;
	switch (parameter(0)) {
	case 0:
		first = last = parameter(1);
		break;
	case 1:
		first = std::min(parameter(1), parameter(2));
		last = std::max(parameter(1), parameter(2));
		break;
	default:
		// a variable given by another: which one is known only while playing
		return false;
	}
	if (last < kCommandRegister || first > kRotations) {
		return false;
	}
	const int indent = com.indent;
	const bool single = first == last;
	// the value the register gets: known for "set to a number"
	const bool known = parameter(3) == 0 && parameter(4) == 0;
	const int value = parameter(5);
	auto has = [&](int id) {
		return first <= id && id <= last;
	};
	// what Power Mode does once the command has set the registers, and the
	// findings, told once it is known whether the command was converted already
	Commands added;
	std::vector<std::pair<QString, QString>> notes;
	auto note = [&notes](const QString& what, const QString& becomes = {}) {
		notes.emplace_back(what, becomes);
	};
	// a single register whose value Power Mode replaces at once: the command goes
	bool replaced = false;
	bool converts = false;

	if (has(kCommandRegister)) {
		if (known && value == 254) {
			note(tr("%1 = 254: quits the game").arg(variable(kCommandRegister)), tr("Exit Game"));
			added.push_back(make(Cmd::ExitGame, indent, {}));
			replaced = single;
			converts = true;
		} else if (known && value == 255) {
			note(tr("%1 = 255: opens the load menu").arg(variable(kCommandRegister)),
				tr("Call System Functions: Load menu"));
			added.push_back(make(Cmd::OpenSaveMenu, indent, {1, 0}));
			replaced = single;
			converts = true;
		} else {
			note(tr("%1 changed: becomes whether a save file exists (254 quits, 255 loads)")
				.arg(variable(kCommandRegister)));
		}
	}
	if (has(kMouseY)) {
		// any change of V3 reads the mouse into V2 and V3, whatever the value
		note(tr("%1 changed: the mouse position into %2, %3").arg(variable(kMouseY), variable(kMouseX), variable(kMouseY)),
			tr("Get Mouse Position: %1, %2").arg(variable(kMouseX), variable(kMouseY)));
		added.push_back(make(Cmd::Maniac_GetMousePosition, indent, {kMouseX, kMouseY}));
		replaced = single;
		converts = true;
	}
	if (has(kKey)) {
		if (known && value >= 1 && value <= 255) {
			// the code stays when the key is pressed, else 0: 1 or 0, times the code
			note(tr("%1 = %2: whether %3 is pressed").arg(variable(kKey)).arg(value).arg(keyName(value)),
				tr("Key Input Processing Ex: %1").arg(keyName(value)));
			added.push_back(keyPressed(indent, kKey, value));
			if (value != 1) {
				added.push_back(controlVariable(indent, kKey, 3, 0, {value}));
			}
			replaced = single;
			converts = true;
		} else if (known && value == 0) {
			note(tr("%1 = 0: the code of any key pressed").arg(variable(kKey)));
		} else if (known) {
			// not a key: Power Mode sets it to 0
			note(tr("%1 = %2: not a key code, set to 0").arg(variable(kKey)).arg(value),
				tr("Control Variables: %1 = 0").arg(variable(kKey)));
			added.push_back(controlVariable(indent, kKey, 0, 0, {0}));
			replaced = single;
			converts = true;
		} else {
			note(tr("%1 changed: whether the key of its value is pressed (0: any key)").arg(variable(kKey)));
		}
	}
	if (has(kFloatCode)) {
		// the operation (1 to 4) on V5 and V6, the fractions times a million;
		// the command stays before it (V7 keeps the operation)
		static const char* const operations[] = {
			QT_TRANSLATE_NOOP("ManiacCompat", "sine and cosine of %1"),
			QT_TRANSLATE_NOOP("ManiacCompat", "tangent of %1"),
			QT_TRANSLATE_NOOP("ManiacCompat", "square root of %1"),
			QT_TRANSLATE_NOOP("ManiacCompat", "%1 divided by %2"),
		};
		if (known && value >= 1 && value <= 4) {
			note(tr("%1 = %2: %3").arg(variable(kFloatCode)).arg(value)
				.arg(tr(operations[value - 1]).arg(variable(kFloat1), variable(kFloat2))),
				tr("Control Variables (Maniac Patch functions)"));
			floatMath(value, indent, added);
			converts = true;
		} else if (!known) {
			// its value known while playing: one condition per operation
			note(tr("%1 changed: decimal math on %2, %3 by its value")
				.arg(variable(kFloatCode), variable(kFloat1), variable(kFloat2)),
				tr("Conditional Branches on %1, Control Variables").arg(variable(kFloatCode)));
			for (int operation = 1; operation <= 4; ++operation) {
				added.push_back(make(Cmd::ConditionalBranch, indent, {1, kFloatCode, 0, operation, 0, 0}));
				floatMath(operation, indent + 1, added);
				added.push_back(make(Cmd::END, indent + 1, {}));
				added.push_back(make(Cmd::EndBranch, indent, {}));
			}
			converts = true;
		}
	}
	if (has(kRotations)) {
		if (known && value > 10) {
			note(tr("%1 = %2: pictures 1 to 50 turned by %3 to %4").arg(variable(kRotations)).arg(value)
				.arg(variable(value), variable(value + 49)));
		} else if (!known) {
			note(tr("%1 changed: above 10, pictures 1 to 50 turned by the variables from it")
				.arg(variable(kRotations)));
		}
	}
	// a range, V7: the command first, as Power Mode reads the values it set
	out.clear();
	if (!replaced) {
		out.push_back(com);
	}
	out.insert(out.end(), added.begin(), added.end());
	// kept and followed by its conversion: converted already
	const bool done = converts && !replaced && i + out.size() <= list.size()
		&& std::equal(out.begin() + 1, out.end(), list.begin() + static_cast<std::ptrdiff_t>(i) + 1);
	for (const auto& [what, becomes] : notes) {
		if (!done || becomes.isEmpty()) {
			found(Group::PowerMode, what, becomes, place);
		}
	}
	if (done) {
		// its conversion is not read again (it sets V7 for the division)
		taken = out.size();
		out.clear();
		return false;
	}
	return converts;
}

bool Scan::powerPicture(const EventCommand& com, const QString& place, Commands& out) {
	// Power Mode turns a rotating picture counter-clockwise when its bottom
	// transparency is 50% or more (it shows no bottom transparency)
	const bool show = com.code == static_cast<int>(Cmd::ShowPicture);
	const size_t bottom_index = show ? 14 : 16;
	if (com.parameters.size() <= std::max<size_t>(bottom_index, 13) || com.parameters[12] != 1) {
		return false;
	}
	const int bottom = show ? (com.parameters[bottom_index] & 0xFF) : com.parameters[bottom_index];
	const int top = com.parameters.size() > 6 ? com.parameters[6] : 0;
	// converted already: a negative speed (RPG Maker gives none) with the top's transparency
	if (bottom < 50 || (com.parameters[13] < 0 && bottom == top)) {
		return false;
	}
	found(Group::PowerMode, show ? tr("Show Picture rotating, bottom transparency 50% or more: turns counter-clockwise")
			: tr("Move Picture rotating, bottom transparency 50% or more: turns counter-clockwise"),
		tr("Negative rotation speed, bottom transparency as the top"), place);
	EventCommand changed = com;
	changed.parameters[13] = -com.parameters[13];
	changed.parameters[bottom_index] = show ? ((com.parameters[bottom_index] & ~0xFF) | (top & 0xFF)) : top;
	out = {changed};
	return true;
}

bool Scan::keyScript(const Commands& list, size_t i, const QString& place, Commands& out, size_t& taken) {
	const EventCommand& com = list[i];
	const QString name = ToQString(com.string);
	if (name.compare(QLatin1String("saves.script"), Qt::CaseInsensitive) == 0) {
		found(Group::KeyPatch, tr("Play Sound Effect %1: counts the save files").arg(name), {}, place);
		return false;
	}
	bool exists = false;
	const auto actions = script(name, &exists);
	if (!exists) {
		found(Group::KeyPatch, tr("Play Sound Effect %1: a script missing in Sound").arg(name), {}, place);
		return false;
	}
	// what has a Maniac equivalent: quitting, the mouse position; logging does nothing
	static const QHash<QByteArray, const char*> doings = {
		{"enablekeysupport", QT_TRANSLATE_NOOP("ManiacCompat", "turns the keys on or off")},
		{"registerkeydownevent", QT_TRANSLATE_NOOP("ManiacCompat", "registers keys to read")},
		{"registerkeyupevent", QT_TRANSLATE_NOOP("ManiacCompat", "registers keys to read")},
		{"registercheatevent", QT_TRANSLATE_NOOP("ManiacCompat", "registers a cheat code")},
		{"enablemousesupport", QT_TRANSLATE_NOOP("ManiacCompat", "turns the mouse on or off")},
		{"setmouseasreturn", QT_TRANSLATE_NOOP("ManiacCompat", "makes the mouse the decision key")},
		{"setmousewheelaskeys", QT_TRANSLATE_NOOP("ManiacCompat", "makes the mouse wheel keys")},
		{"miditickfunction", QT_TRANSLATE_NOOP("ManiacCompat", "chooses what MIDI ticks read")},
		{"addoutput", QT_TRANSLATE_NOOP("ManiacCompat", "adds a value MIDI ticks read")},
		{"execprogram", QT_TRANSLATE_NOOP("ManiacCompat", "runs a program")},
		{"mcicommand", QT_TRANSLATE_NOOP("ManiacCompat", "plays through MCI")},
	};
	bool exits = false;
	bool mouse = false;
	QStringList others;
	for (const auto& action : actions) {
		if (action.name == "writetolog" || action.name == "setdebuglevel") {
			continue;
		}
		if (action.name == "execprogram" && (action.arg.toLower().startsWith("exitgame") || action.arg.toLower().startsWith("taskkill"))) {
			exits = true;
		} else if (action.name == "getmouseposition") {
			mouse = true;
		} else {
			const char* doing = doings.value(action.name);
			const QString text = doing ? tr(doing) : QString::fromLatin1(action.name);
			if (!others.contains(text)) {
				others << text;
			}
		}
	}
	if (exits && mouse) {
		others.prepend(tr("reads the mouse"));
		others.prepend(tr("quits the game"));
	}
	if (!others.isEmpty() || (!exits && !mouse)) {
		found(Group::KeyPatch, tr("Play Sound Effect %1: %2").arg(name, others.isEmpty() ? tr("writes to the log")
			: others.join(QStringLiteral(", "))), {}, place);
		return false;
	}
	if (exits) {
		found(Group::KeyPatch, tr("Play Sound Effect %1: quits the game").arg(name), tr("Exit Game"), place);
		out = {make(Cmd::ExitGame, com.indent, {})};
		return true;
	}
	// the mouse: its four values read right after, by Control Variables
	// (Other: MIDI ticks): the id of enableMouseSupport, X, Y, the buttons
	// (1 left, 2 right, 3 both)
	std::vector<int> reads;
	for (size_t next = i + 1; next < list.size() && reads.size() < 4; ++next) {
		const auto& p = list[next].parameters;
		if (list[next].code != static_cast<int>(Cmd::ControlVars) || list[next].indent != com.indent || p.size() < 6
				|| p[0] != 0 || p[3] != 0 || p[4] != 7 || p[5] != 8) {
			break;
		}
		reads.push_back(p[1]);
	}
	const std::set<int> distinct(reads.begin(), reads.end());
	if (!m_mousePrefix) {
		found(Group::KeyPatch, tr("Play Sound Effect %1: the mouse, its ID not given by one EnableMouseSupport").arg(name), {}, place);
		return false;
	}
	if (reads.size() != 4 || distinct.size() != 4) {
		found(Group::KeyPatch, tr("Play Sound Effect %1: the mouse, not read into four variables at once").arg(name), {}, place);
		return false;
	}
	const int id = reads[0];
	const int x = reads[1];
	const int y = reads[2];
	const int buttons = reads[3];
	found(Group::KeyPatch, tr("Play Sound Effect %1: the mouse position and buttons").arg(name),
		tr("Get Mouse Position, Key Input Processing Ex"), place);
	const int indent = com.indent;
	out = {
		make(Cmd::Maniac_GetMousePosition, indent, {x, y}),
		// the right button in the id's variable for a moment, then the buttons
		keyPressed(indent, id, 2),
		keyPressed(indent, buttons, 1),
		expression(indent, buttons, QStringLiteral("V[%1] + V[%2] * 2").arg(buttons).arg(id).toStdString()),
		controlVariable(indent, id, 0, 0, {*m_mousePrefix}),
	};
	taken = 5;
	return true;
}

QString Scan::linkTarget(const QString& name, const char* folder, QString* why) const {
	// the link as it is, else as RPG Maker finds a sound: with an extension of audio (name.link.wav)
	const bool sound = QLatin1String(folder) == QLatin1String(SOUND);
	QString path = FileFinder::Find(m_project.projectDir(), QString::fromLatin1(folder), name);
	if (path.isEmpty()) {
		path = FileFinder::Find(m_project.projectDir(), QString::fromLatin1(folder), name,
			sound ? FileFinder::FileType::Sound : FileFinder::FileType::Music);
	}
	if (path.isEmpty()) {
		*why = tr("missing in %1").arg(QString::fromLatin1(folder));
		return {};
	}
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly) || file.size() >= 500) {
		*why = tr("not a link");
		return {};
	}
	// its first line: the file played, from the link's folder; a second
	// line "loop" plays a sound effect in a loop
	const QByteArray line = file.readLine().trimmed();
	if (sound && file.readLine().trimmed().toLower() == "loop") {
		*why = tr("played in a loop");
		return {};
	}
	const std::string decoded = lcf::ReaderUtil::Recode(line.toStdString(), m_project.encoding().toStdString());
	const QString target = QString::fromStdString(decoded).replace(u'\\', u'/');
	const QDir folder_dir(QFileInfo(path).absolutePath());
	const QString played = QDir::cleanPath(folder_dir.filePath(target));
	if (target.isEmpty() || QFileInfo(played).absolutePath().compare(folder_dir.absolutePath(), Qt::CaseInsensitive) != 0) {
		*why = tr("plays %1, outside %2").arg(target, QString::fromLatin1(folder));
		return {};
	}
	if (FileFinder::Find(folder_dir, QFileInfo(played).fileName()).isEmpty()) {
		*why = tr("plays %1, which is missing").arg(target);
		return {};
	}
	return QFileInfo(played).completeBaseName();
}

bool Scan::link(QString& name, const char* folder, const QString& place) {
	const QString link = name;
	QString why;
	const QString target = linkTarget(link, folder, &why);
	if (target.isEmpty()) {
		found(Group::KeyPatch, tr("%1: a link, %2").arg(link, why), {}, place);
		return false;
	}
	found(Group::KeyPatch, tr("%1: a link to %2").arg(link, target), tr("Plays %1 itself").arg(target), place);
	name = target;
	return true;
}

bool Scan::database(lcf::rpg::Database& db) {
	bool changed = false;
	for (auto& event : db.commonevents) {
		changed = commands(event.event_commands, tr("Common event %1: %2")
			.arg(event.ID, 4, 10, QLatin1Char('0')).arg(ToQString(event.name))) || changed;
	}
	for (auto& troop : db.troops) {
		for (size_t page = 0; page < troop.pages.size(); ++page) {
			changed = commands(troop.pages[page].event_commands, tr("Monster group %1: %2, page %3")
				.arg(troop.ID, 4, 10, QLatin1Char('0')).arg(ToQString(troop.name)).arg(page + 1)) || changed;
		}
	}
	// the music and sounds of the system, linked by the Key Patch
	const QString system = tr("Database: System");
	auto& s = db.system;
	for (lcf::rpg::Music* music : {&s.title_music, &s.battle_music, &s.battle_end_music, &s.inn_music, &s.boat_music,
			&s.ship_music, &s.airship_music, &s.gameover_music}) {
		if (QString name = QString::fromStdString(music->name); name.endsWith(QLatin1String(".link"), Qt::CaseInsensitive)
				&& link(name, MUSIC, system)) {
			changed = true;
			if (m_apply) {
				music->name = name.toStdString();
			}
		}
	}
	for (lcf::rpg::Sound* sound : {&s.cursor_se, &s.decision_se, &s.cancel_se, &s.buzzer_se, &s.battle_se, &s.escape_se,
			&s.enemy_attack_se, &s.enemy_damaged_se, &s.actor_damaged_se, &s.dodge_se, &s.enemy_death_se, &s.item_se}) {
		if (QString name = QString::fromStdString(sound->name); name.endsWith(QLatin1String(".link"), Qt::CaseInsensitive)
				&& link(name, SOUND, system)) {
			changed = true;
			if (m_apply) {
				sound->name = name.toStdString();
			}
		}
	}
	// the options of the database only EasyRPG Player reads
	QStringList options;
	if (s.easyrpg_use_rpg2k_battle_system) {
		options << tr("the battles of RPG Maker 2000");
	}
	if (s.easyrpg_use_rpg2k_battle_commands) {
		options << tr("the battle commands of RPG Maker 2000");
	}
	if (s.easyrpg_alternative_exp != 0) {
		options << tr("another experience curve");
	}
	if (s.easyrpg_max_level != -1 || s.easyrpg_max_actor_hp != -1 || s.easyrpg_max_actor_sp != -1 || s.easyrpg_max_enemy_hp != -1
			|| s.easyrpg_max_enemy_sp != -1 || s.easyrpg_max_damage != -1 || s.easyrpg_max_exp != -1 || s.easyrpg_max_item_count != -1
			|| s.easyrpg_max_stat_base_value != -1 || s.easyrpg_max_stat_battle_value != -1) {
		options << tr("other limits");
	}
	if (!options.isEmpty()) {
		found(Group::EasyRpg, tr("Database options: %1").arg(options.join(QStringLiteral(", "))), {}, system);
	}
	return changed;
}

bool Scan::map(std::vector<lcf::rpg::Event>& events, int id, const QString& name) {
	bool changed = false;
	for (auto& event : events) {
		for (size_t page = 0; page < event.pages.size(); ++page) {
			changed = commands(event.pages[page].event_commands, tr("Map %1: %2, event %3: %4, page %5")
				.arg(id, 4, 10, QLatin1Char('0')).arg(name).arg(event.ID, 4, 10, QLatin1Char('0'))
				.arg(ToQString(event.name)).arg(page + 1)) || changed;
		}
	}
	return changed;
}

} // namespace

int ManiacCompat::Report::convertible() const {
	int count = 0;
	for (const auto& finding : findings) {
		count += finding.becomes.isEmpty() ? 0 : finding.places.size();
	}
	return count;
}

int ManiacCompat::Report::remaining() const {
	int count = 0;
	for (const auto& finding : findings) {
		count += finding.becomes.isEmpty() ? finding.places.size() : 0;
	}
	return count;
}

QString ManiacCompat::Report::fingerprint() const {
	QCryptographicHash hash(QCryptographicHash::Sha1);
	for (const auto& finding : findings) {
		hash.addData(QStringLiteral("%1\n%2\n%3\n%4\n").arg(static_cast<int>(finding.group))
			.arg(finding.what, finding.becomes, finding.places.join(u'\n')).toUtf8());
	}
	return QString::fromLatin1(hash.result().toHex());
}

QString ManiacCompat::groupName(Group group) {
	switch (group) {
	case Group::Engine: return tr("RPG Maker 2000");
	case Group::EasyRpg: return tr("EasyRPG Player");
	case Group::PowerMode: return tr("Power Mode 2003");
	case Group::KeyPatch: return tr("Ineluki Key Patch (harmony.dll)");
	}
	return {};
}

QString ManiacCompat::backupFolderName() {
	return QStringLiteral("Backup (Maniac Patch)");
}

ManiacCompat::Report ManiacCompat::check(const Project& project, const OpenMap& open_map) {
	Report report;
	Scan scan(project, false);
	if (project.database().system.ldb_id != 2003) {
		// ProjectConvert converts RPG Maker's files, not the EasyRPG format
		report.rpg2000 = project.projectType() == FileFinder::ProjectType::Legacy;
		scan.findings.push_back({Group::Engine, tr("An RPG Maker 2000 game: the Maniac Patch runs RPG Maker 2003 games"),
			report.rpg2000 ? tr("An RPG Maker 2003 game (Convert to RPG Maker 2003)") : QString(),
			{tr("The database, the map tree and the maps")}});
	}
	lcf::rpg::Database db = project.database();
	scan.database(db);
	for (const auto& info : project.treeMap().maps) {
		if (info.ID <= 0 || info.type != lcf::rpg::TreeMap::MapType_map) {
			continue;
		}
		const lcf::rpg::Map* open = open_map ? open_map(info.ID) : nullptr;
		std::unique_ptr<lcf::rpg::Map> read;
		if (!open) {
			read = project.loadMap(info.ID);
			if (!read) {
				continue;
			}
		}
		// the events only: a check changes nothing
		std::vector<lcf::rpg::Event> events = open ? open->events : read->events;
		scan.map(events, info.ID, ToQString(info.name));
	}
	// an RPG Maker 2000 game that stays one (the EasyRPG format): nothing converts
	if (project.database().system.ldb_id != 2003 && !report.rpg2000) {
		for (auto& finding : scan.findings) {
			finding.becomes.clear();
		}
	}
	// by group: the engine, EasyRPG Player, the patches
	std::stable_sort(scan.findings.begin(), scan.findings.end(), [](const Finding& a, const Finding& b) {
		return a.group < b.group;
	});
	report.findings = std::move(scan.findings);
	return report;
}

QString ManiacCompat::convert(Project& project, int* converted, QString* backup_path) {
	if (project.database().system.ldb_id != 2003) {
		return tr("The game is not an RPG Maker 2003 game.");
	}
	// 1. Everything converted in memory: nothing is written when a file cannot be read
	Scan scan(project, true);
	lcf::rpg::Database db = project.database();
	const bool database_changed = scan.database(db);
	struct ChangedMap {
		int id;
		std::unique_ptr<lcf::rpg::Map> map;
	};
	std::vector<ChangedMap> maps;
	for (const auto& info : project.treeMap().maps) {
		if (info.ID <= 0 || info.type != lcf::rpg::TreeMap::MapType_map) {
			continue;
		}
		auto map = project.loadMap(info.ID);
		if (map && scan.map(map->events, info.ID, ToQString(info.name))) {
			maps.push_back({info.ID, std::move(map)});
		}
	}

	// 2. The backup: a new folder with a copy of each file written
	const QDir dir = project.projectDir();
	QString backup_name = backupFolderName();
	for (int n = 2; dir.exists(backup_name); ++n) {
		backup_name = QStringLiteral("%1 %2").arg(backupFolderName()).arg(n);
	}
	if (!dir.mkdir(backup_name)) {
		return tr("The backup folder %1 cannot be created.").arg(QDir::toNativeSeparators(dir.filePath(backup_name)));
	}
	QDir backup(dir.filePath(backup_name));
	if (backup_path) {
		*backup_path = backup.absolutePath();
	}
	auto copy = [&backup](const QString& file) {
		return !QFileInfo::exists(file) || QFile::copy(file, backup.filePath(QFileInfo(file).fileName()));
	};
	const QString ini = project.findFileOrDefault(QStringLiteral("EasyRPG.ini"));
	QStringList files;
	if (database_changed) {
		files << project.databaseFile();
	}
	for (const auto& changed : maps) {
		files << project.mapFile(changed.id);
	}
	files << ini;
	for (const QString& file : files) {
		if (!copy(file)) {
			backup.removeRecursively();
			return tr("%1 cannot be copied to the backup folder.").arg(QFileInfo(file).fileName());
		}
	}
	// nothing was there to copy (no EasyRPG.ini yet): no backup
	if (backup.isEmpty()) {
		dir.rmdir(backup_name);
		if (backup_path) {
			backup_path->clear();
		}
	}

	// 3. The files: the maps, then the database, then the patches
	const QString kept = backup.exists() ? QStringLiteral("\n") + tr("The files as they were are in %1.")
		.arg(QDir::toNativeSeparators(backup.absolutePath())) : QString();
	for (auto& changed : maps) {
		if (!project.saveMap(*changed.map, changed.id)) {
			return tr("%1 cannot be written.").arg(QFileInfo(project.mapFile(changed.id)).fileName()) + kept;
		}
	}
	if (database_changed) {
		project.database() = std::move(db);
		if (!project.saveDatabase()) {
			return tr("The database cannot be written.") + kept;
		}
	}
	// the patches as the Player reads them now, chosen from then on: the
	// Maniac Patch on (its version kept), Power Mode off
	auto values = GamePatches::read(project).values;
	values[QStringLiteral("Maniac")] = std::max(1, values.value(QStringLiteral("Maniac")));
	values[QStringLiteral("PowerMode2003")] = 0;
	if (!GamePatches::write(project, values)) {
		return tr("The patches cannot be written in EasyRPG.ini.") + kept;
	}
	if (converted) {
		*converted = scan.converted;
	}
	return {};
}
