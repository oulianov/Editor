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

#include "event_command_list_model.h"
#include <QClipboard>
#include <QCryptographicHash>
#include <QDir>
#include <QGuiApplication>
#include <QSettings>
#include <QUndoStack>
#include <QVariant>
#include <algorithm>
#include <functional>
#include <limits>
#include "common/dbstring.h"
#include "model/command_colors.h"
#include "model/command_schema.h"
#include "model/event_command_defaults.h"
#include "stringizer.h"

namespace {
using Cmd = lcf::rpg::EventCommand::Code;
using Commands = EventCommandListModel::Commands;

Cmd codeOf(const lcf::rpg::EventCommand& cmd)
{
	return static_cast<Cmd>(cmd.code);
}

/** The next lines of a message or a comment */
bool isNextLine(Cmd code)
{
	return code == Cmd::ShowMessage_2 || code == Cmd::Comment_2;
}

// A change of the command list: the list before and after it
class CommandListChange : public QUndoCommand {
public:
	CommandListChange(std::function<void(const Commands&)> show, Commands before, Commands after) :
		m_show(std::move(show)), m_before(std::move(before)), m_after(std::move(after)) {}

	void undo() override { m_show(m_before); }
	void redo() override {
		// the change is already done when it is pushed
		if (!m_done) {
			m_done = true;
			return;
		}
		m_show(m_after);
	}

private:
	std::function<void(const Commands&)> m_show;
	Commands m_before, m_after;
	bool m_done = false;
};

/** The lines copied, pasted in any list */
Commands& clipboard()
{
	static Commands lines;
	return lines;
}

/** Where the last command accepted in the window of each kind is kept, for
 *  the game (by its folder), across sessions: a new one opens with the same
 *  settings. Blocks keep their shape (an "else" branch, battle handlers...)
 *  without the commands inside them. */
QString lastCommandKey(const QString& project_dir, Cmd code)
{
	const QByteArray game = QCryptographicHash::hash(QDir(project_dir).canonicalPath().toUtf8(), QCryptographicHash::Sha1).toHex();
	return QStringLiteral("last_commands/%1/%2").arg(QString::fromLatin1(game)).arg(static_cast<int>(code));
}

/** Commands of text start empty: a message is not written twice */
bool remembersSettings(Cmd code)
{
	return code != Cmd::ShowMessage && code != Cmd::Comment && code != Cmd::ShowChoice;
}

/** What starts the text of the first line of a command (a message, a comment) */
QString firstLineLabel(const lcf::rpg::EventCommand& cmd)
{
	return codeOf(cmd) == Cmd::ShowMessage ? Stringizer::tr("Message") : Stringizer::tr("Comment");
}

/** A line as Copy as text writes it, without its indent: "<>" before a
 *  command, ":" before the other lines of a command. text_column: where
 *  the text of the last message or comment starts, for its next lines */
QString textLine(const lcf::rpg::EventCommand& cmd, int& text_column)
{
	const Cmd code = codeOf(cmd);
	if (code == Cmd::END) {
		return QStringLiteral("<>");
	}
	if (isNextLine(code)) {
		// under the text of the first line
		return QString(text_column, QLatin1Char(' ')) + QStringLiteral(": ") + ToQString(cmd.string);
	}
	if (EventCommandListModel::hasEditor(code)) {
		text_column = 2 + static_cast<int>(firstLineLabel(cmd).size());
		return QStringLiteral("<>") + Stringizer::stringize(cmd);
	}
	return QStringLiteral(": ") + Stringizer::stringize(cmd);
}
}

EventCommandListModel::EventCommandListModel(QObject* parent) :
	QAbstractListModel(parent), m_undoStack(new QUndoStack(this))
{
	m_undoStack->setUndoLimit(100);
}

EventCommandListModel::~EventCommandListModel() = default;

void EventCommandListModel::setCommands(Commands* commands)
{
	beginResetModel();
	m_commands = commands;
	m_undoStack->clear();
	m_recorded.reset();
	endResetModel();
	if (!m_commands) {
		emit commandsChanged();
		return;
	}
	finishChange();
}

EventCommandListModel::Commands* EventCommandListModel::commands() const
{
	return m_commands;
}

int EventCommandListModel::lineCount() const
{
	return m_commands ? static_cast<int>(m_commands->size()) : 0;
}

int EventCommandListModel::rowCount(const QModelIndex& parent) const
{
	// the lines, then the end of the page
	return parent.isValid() || !m_commands ? 0 : lineCount() + 1;
}

QVariant EventCommandListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || !checkIndex(index, CheckIndexOption::ParentIsInvalid)) {
		return {};
	}
	const int row = index.row();
	const bool page_end = row == lineCount();
	const lcf::rpg::EventCommand* cmd = page_end ? nullptr : &(*m_commands)[static_cast<size_t>(row)];
	switch (role) {
		case Qt::DisplayRole:
			return lineText(row);
		case TextRole: {
			if (page_end) {
				return QStringLiteral("<>");
			}
			// the next lines of a message under its text, as Copy as text writes them
			const auto& start = (*m_commands)[static_cast<size_t>(commandStart(row))];
			int text_column = hasEditor(codeOf(start)) ? 2 + static_cast<int>(firstLineLabel(start).size()) : 0;
			return QString(2 * std::max(0, cmd->indent), QLatin1Char(' ')) + textLine(*cmd, text_column);
		}
		case IndentRole:
			return page_end ? 0 : cmd->indent;
		case CodeRole:
			return page_end ? static_cast<int>(Cmd::END) : cmd->code;
		case ColorKindRole:
			return static_cast<int>(page_end ? CommandColors::Kind::End : CommandColors::kind(cmd->code));
		case CommandStartRole:
			return isCommandLine(row);
		case StructuralRole:
			return !isSelectable(row);
		case PageEndRole:
			return page_end;
		default:
			return {};
	}
}

Qt::ItemFlags EventCommandListModel::flags(const QModelIndex& index) const
{
	if (!index.isValid() || !checkIndex(index, CheckIndexOption::ParentIsInvalid)) {
		return Qt::NoItemFlags;
	}
	Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
	if (isSelectable(index.row())) {
		flags |= Qt::ItemIsSelectable;
	}
	return flags;
}

QHash<int, QByteArray> EventCommandListModel::roleNames() const
{
	auto names = QAbstractListModel::roleNames();
	names.insert(TextRole, "text");
	names.insert(IndentRole, "indent");
	names.insert(CodeRole, "code");
	names.insert(ColorKindRole, "colorKind");
	names.insert(CommandStartRole, "commandStart");
	names.insert(StructuralRole, "structural");
	names.insert(PageEndRole, "pageEnd");
	return names;
}

bool EventCommandListModel::hasEditor(Code code)
{
	return CommandSchema::hasEditor(code);
}

QString EventCommandListModel::lineText(int index) const
{
	if (index < 0 || index > lineCount() || !m_commands) {
		return {};
	}
	if (index == lineCount()) {
		return QStringLiteral("<>");
	}
	const auto& cmd = (*m_commands)[static_cast<size_t>(index)];
	if (isNextLine(codeOf(cmd))) {
		// a view puts it under the text of the first line
		return QStringLiteral(": ") + ToQString(cmd.string);
	}
	if (codeOf(cmd) == Cmd::END) {
		// where commands are inserted
		return QStringLiteral("<>");
	}
	return Stringizer::stringize(cmd);
}

bool EventCommandListModel::isSelectable(int index) const
{
	if (index < 0 || index > lineCount() || !m_commands) {
		return false;
	}
	if (index == lineCount()) {
		return true;
	}
	const Cmd code = codeOf((*m_commands)[static_cast<size_t>(index)]);
	return code == Cmd::END || hasEditor(code);
}

bool EventCommandListModel::isCommandLine(int index) const
{
	if (!isSelectable(index)) {
		return false;
	}
	if (index <= 0 || index >= lineCount()) {
		return true;
	}
	return !isNextLine(codeOf((*m_commands)[static_cast<size_t>(index)]));
}

int EventCommandListModel::commandStart(int index) const
{
	while (index > 0 && index < lineCount()) {
		if (!isNextLine(codeOf((*m_commands)[static_cast<size_t>(index)]))) {
			break;
		}
		--index;
	}
	return index;
}

EventCommandListModel::Range EventCommandListModel::commandRange(int index) const
{
	if (!m_commands) {
		return {index, index};
	}
	return EventCommandDefaults::range(*m_commands, commandStart(index));
}

EventCommandListModel::Range EventCommandListModel::wholeCommands(int first, int last) const
{
	if (!m_commands) {
		return {0, 0};
	}
	const auto& commands = *m_commands;
	first = std::max(first, 0);
	last = std::min(last, lineCount());
	auto is_end = [&](int i) { return codeOf(commands[static_cast<size_t>(i)]) == Cmd::END; };

	// Grow until every line goes with its whole command, like RPG Maker:
	// a line of the structure of a block (else, an option, its end) takes
	// the block with it. The empty lines of a block are left for the end.
	for (bool grown = true; grown;) {
		grown = false;
		for (int i = first; i < last && !grown; ++i) {
			if (is_end(i)) {
				continue;
			}
			auto range = EventCommandDefaults::range(commands, commandStart(i));
			if (range.first == range.second) {
				// the nearest command before it whose block goes past it
				for (int k = i - 1; k >= 0; --k) {
					if (commands[static_cast<size_t>(k)].indent > commands[static_cast<size_t>(i)].indent) {
						continue;
					}
					auto block = EventCommandDefaults::range(commands, k);
					if (block.first < block.second && block.second > i) {
						range = block;
						break;
					}
				}
			}
			if (range.first < range.second && (range.first < first || range.second > last)) {
				first = std::min(first, range.first);
				last = std::max(last, range.second);
				grown = true;
			}
		}
	}
	// An empty line at an edge is outside of the blocks selected: not taken
	while (first < last && is_end(first)) {
		++first;
	}
	while (first < last && is_end(last - 1)) {
		--last;
	}
	return {first, last};
}

int EventCommandListModel::insertLines(int index, const Commands& lines)
{
	if (!m_commands) {
		return -1;
	}
	// Out of the list: at its end
	if (index < 0 || index > lineCount()) {
		index = lineCount();
	}
	if (lines.empty()) {
		return index;
	}
	beginInsertRows(QModelIndex(), index, index + static_cast<int>(lines.size()) - 1);
	m_commands->insert(m_commands->begin() + index, lines.begin(), lines.end());
	endInsertRows();
	finishChange();
	return index;
}

void EventCommandListModel::removeLines(Range range)
{
	if (!m_commands) {
		return;
	}
	range.first = std::max(range.first, 0);
	range.second = std::min(range.second, lineCount());
	if (range.first >= range.second) {
		// a line of the structure of a block: the block goes instead
		return;
	}
	beginRemoveRows(QModelIndex(), range.first, range.second - 1);
	m_commands->erase(m_commands->begin() + range.first, m_commands->begin() + range.second);
	endRemoveRows();
	finishChange();
}

void EventCommandListModel::replaceLines(Range range, const Commands& lines)
{
	if (!m_commands) {
		return;
	}
	range.first = std::clamp(range.first, 0, lineCount());
	range.second = std::clamp(range.second, range.first, lineCount());
	// The lines in place change, the others come or go
	const int old_size = range.second - range.first;
	const int new_size = static_cast<int>(lines.size());
	const int kept = std::min(old_size, new_size);
	std::copy(lines.begin(), lines.begin() + kept, m_commands->begin() + range.first);
	if (kept > 0) {
		emit dataChanged(index(range.first), index(range.first + kept - 1));
	}
	if (new_size > kept) {
		beginInsertRows(QModelIndex(), range.first + kept, range.first + new_size - 1);
		m_commands->insert(m_commands->begin() + range.first + kept, lines.begin() + kept, lines.end());
		endInsertRows();
	} else if (old_size > kept) {
		beginRemoveRows(QModelIndex(), range.first + kept, range.second - 1);
		m_commands->erase(m_commands->begin() + range.first + kept, m_commands->begin() + range.second);
		endRemoveRows();
	}
	finishChange();
}

void EventCommandListModel::commandsEdited()
{
	if (!m_commands) {
		return;
	}
	// The lines changed already: the views read again those that differ
	// from the list they last read (the one recorded)
	if (!m_recorded || m_recorded->size() != m_commands->size()) {
		beginResetModel();
		endResetModel();
	} else {
		int first = -1;
		int last = -1;
		for (int i = 0; i < lineCount(); ++i) {
			if (!((*m_recorded)[static_cast<size_t>(i)] == (*m_commands)[static_cast<size_t>(i)])) {
				first = first < 0 ? i : first;
				last = i;
			}
		}
		if (first >= 0) {
			emit dataChanged(index(first), index(last));
		}
	}
	finishChange();
}

QUndoStack* EventCommandListModel::undoStack() const
{
	return m_undoStack;
}

void EventCommandListModel::updateIndents()
{
	int first = -1;
	int last = -1;
	int32_t indent = 0;
	for (size_t i = 0; i < m_commands->size(); ++i) {
		auto& cmd = (*m_commands)[i];
		if (i > 0) {
			switch (codeOf((*m_commands)[i - 1])) {
				case Cmd::ConditionalBranch:
				case Cmd::ElseBranch:
				case Cmd::ConditionalBranch_B:
				case Cmd::ElseBranch_B:
				case Cmd::Loop:
				case Cmd::ShowChoiceOption:
				case Cmd::VictoryHandler:
				case Cmd::EscapeHandler:
				case Cmd::DefeatHandler:
				case Cmd::Transaction:
				case Cmd::NoTransaction:
				case Cmd::Stay:
				case Cmd::NoStay:
					indent++;
					break;
				case Cmd::END:
					// Some imported pages retain an END at the root: the
					// lines after it stay at the root, never below it
					indent = std::max(0, indent - 1);
					break;
				default:
					break;
			}
		}
		if (cmd.indent != indent) {
			cmd.indent = indent;
			first = first < 0 ? static_cast<int>(i) : first;
			last = static_cast<int>(i);
		}
	}
	if (first >= 0) {
		emit dataChanged(index(first), index(last), {IndentRole, TextRole});
	}
}

void EventCommandListModel::finishChange()
{
	updateIndents();
	// Every change of the list ends here: remember the list as it was
	if (m_recorded && *m_recorded != *m_commands) {
		m_undoStack->push(new CommandListChange([this](const Commands& commands) { restore(commands); },
			std::move(*m_recorded), *m_commands));
	}
	m_recorded = *m_commands;
	emit commandsChanged();
}

void EventCommandListModel::restore(const Commands& commands)
{
	beginResetModel();
	*m_commands = commands;
	m_recorded = commands;
	endResetModel();
	finishChange();
}

void EventCommandListModel::copyLines(Range range, bool cut)
{
	if (!m_commands) {
		return;
	}
	range.first = std::max(range.first, 0);
	range.second = std::min(range.second, lineCount());
	if (range.first >= range.second) {
		return;
	}
	clipboard().assign(m_commands->begin() + range.first, m_commands->begin() + range.second);
	// as text too, for text editors (the command lists paste the commands themselves)
	if (qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
		QGuiApplication::clipboard()->setText(commandsText(clipboard()));
	}
	if (cut) {
		removeLines(range);
	}
}

int EventCommandListModel::paste(int index)
{
	if (!m_commands || clipboard().empty()) {
		return -1;
	}
	return insertLines(index, clipboard());
}

bool EventCommandListModel::canPaste()
{
	return !clipboard().empty();
}

QString EventCommandListModel::commandsText(const Commands& commands)
{
	// the lines of the list, as RPG Maker shows them: "<>" before a command,
	// ":" before the other lines of a command, indented by their depth
	int base = std::numeric_limits<int>::max();
	for (const auto& cmd : commands) {
		base = std::min(base, cmd.indent);
	}
	QStringList lines;
	int text_column = 0;
	for (const auto& cmd : commands) {
		const QString indent(2 * std::max(0, cmd.indent - base), QLatin1Char(' '));
		lines << indent + textLine(cmd, text_column);
	}
	return lines.join(QLatin1Char('\n'));
}

EventCommandListModel::Commands EventCommandListModel::newCommand(const QString& project_dir, Code code, int indent)
{
	const QVariantList saved = QSettings().value(lastCommandKey(project_dir, code)).toList();
	Commands lines;
	for (const QVariant& value : saved) {
		const QVariantMap map = value.toMap();
		lcf::rpg::EventCommand line;
		line.code = map.value(QStringLiteral("code")).toInt();
		line.indent = map.value(QStringLiteral("indent")).toInt() + indent;
		line.string = lcf::DBString(map.value(QStringLiteral("string")).toByteArray().toStdString());
		std::vector<int32_t> parameters;
		for (const QVariant& parameter : map.value(QStringLiteral("parameters")).toList()) {
			parameters.push_back(parameter.toInt());
		}
		line.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
		lines.push_back(line);
	}
	// nothing kept, or not this command (settings of another version)
	if (lines.empty() || lines.front().code != static_cast<int>(code)) {
		return EventCommandDefaults::make(code, indent);
	}
	return lines;
}

void EventCommandListModel::rememberCommand(int index, const QString& project_dir) const
{
	const auto& commands = *m_commands;
	const auto code = codeOf(commands[static_cast<size_t>(index)]);
	if (!remembersSettings(code)) {
		return;
	}
	const int base = commands[static_cast<size_t>(index)].indent;
	const auto [start, end] = EventCommandDefaults::range(commands, index);
	Commands shape;
	for (int i = start; i < std::max(end, start + 1); ++i) {
		auto line = commands[static_cast<size_t>(i)];
		// the command, the lines of its block, the end of each of its lists
		if (i == start || line.indent == base || (line.indent == base + 1 && codeOf(line) == Cmd::END)) {
			line.indent -= base;
			shape.push_back(line);
		}
	}
	QVariantList lines;
	for (const auto& line : shape) {
		QVariantList parameters;
		for (const int32_t parameter : line.parameters) {
			parameters << parameter;
		}
		lines << QVariantMap{
			{QStringLiteral("code"), line.code},
			{QStringLiteral("indent"), line.indent},
			{QStringLiteral("string"), QByteArray(line.string.data(), static_cast<qsizetype>(line.string.size()))},
			{QStringLiteral("parameters"), parameters},
		};
	}
	QSettings().setValue(lastCommandKey(project_dir, code), lines);
}
