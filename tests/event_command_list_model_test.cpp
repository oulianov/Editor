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

// The command list of an event page as a list model, on a page in memory:
// its lines and roles, the blocks (a Conditional Branch with its Else and
// End lines) inserted, removed, copied and pasted whole, edits, undo, and
// the settings a new command starts from, with Qt's model checks watching
// every change. The texts of the commands come from a stand-in of the
// Stringizer (the real one reads the names of the project open, core());
// the keys and the mouse are the QML list's (tests/scripts/command_*.txt).

#include <QAbstractItemModelTester>
#include <QClipboard>
#include <QGuiApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>
#include "common/dbstring.h"
#include "model/command_colors.h"
#include "model/event_command_defaults.h"
#include "model/event_command_list_model.h"
#include "stringizer.h"

using Cmd = lcf::rpg::EventCommand::Code;
using Commands = EventCommandListModel::Commands;
using Range = EventCommandListModel::Range;

// The stand-in of the Stringizer: the name of the command
QString Stringizer::stringize(const lcf::rpg::EventCommand& cmd)
{
	switch (static_cast<Cmd>(cmd.code)) {
		case Cmd::ShowMessage: return QStringLiteral("Message: ") + ToQString(cmd.string);
		case Cmd::ConditionalBranch: return QStringLiteral("Branch");
		case Cmd::ElseBranch: return QStringLiteral("Else");
		case Cmd::EndBranch: return QStringLiteral("End");
		case Cmd::Wait: return QStringLiteral("Wait %1").arg(cmd.parameters.empty() ? 0 : cmd.parameters[0]);
		case Cmd::ControlVars: return QStringLiteral("Control Vars");
		default: return QStringLiteral("Command %1").arg(cmd.code);
	}
}

namespace {
lcf::DBArray<int32_t> parameters(std::vector<int32_t> values)
{
	return lcf::DBArray<int32_t>(values.begin(), values.end());
}

lcf::rpg::EventCommand line(Cmd code, int indent, const char* text = "", std::vector<int32_t> parameters = {})
{
	lcf::rpg::EventCommand cmd;
	cmd.code = static_cast<int32_t>(code);
	cmd.indent = indent;
	cmd.string = lcf::DBString(text);
	cmd.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	return cmd;
}

QList<int> codes(const Commands& commands)
{
	QList<int> list;
	for (const auto& cmd : commands) {
		list << cmd.code;
	}
	return list;
}

QList<int> indents(const Commands& commands)
{
	QList<int> list;
	for (const auto& cmd : commands) {
		list << cmd.indent;
	}
	return list;
}

constexpr int Message = static_cast<int>(Cmd::ShowMessage);
constexpr int Message2 = static_cast<int>(Cmd::ShowMessage_2);
constexpr int Branch = static_cast<int>(Cmd::ConditionalBranch);
constexpr int Else = static_cast<int>(Cmd::ElseBranch);
constexpr int EndBranch = static_cast<int>(Cmd::EndBranch);
constexpr int End = static_cast<int>(Cmd::END);
constexpr int Wait = static_cast<int>(Cmd::Wait);
constexpr int Vars = static_cast<int>(Cmd::ControlVars);
}

class EventCommandListModelTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void init();
	void rowsAndRoles();
	void indentsKeptByBlocks();
	void commandRanges();
	void insertBlock();
	void removeBlock();
	void copyPasteBlock();
	void cutAndPasteDeeper();
	void replaceAfterEdit();
	void editedInPlace();
	void undoRedo();
	void rememberedCommands();

private:
	QString text(int row, int role = Qt::DisplayRole) const;
	QVariant value(int row, int role) const;

	// a message of two lines, a branch with a wait and an else, a variable:
	// 0 Message: Hi / 1 : there / 2 Branch / 3 Wait / 4 <> / 5 Else / 6 <> /
	// 7 End / 8 Control Vars / 9 <> (the page end)
	Commands m_commands;
	std::unique_ptr<EventCommandListModel> m_model;
	std::unique_ptr<QAbstractItemModelTester> m_tester;
	QTemporaryDir m_settings;
};

void EventCommandListModelTest::initTestCase()
{
	// the remembered commands in a settings file of the test
	QVERIFY(m_settings.isValid());
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void EventCommandListModelTest::init()
{
	m_tester.reset();
	m_commands = {line(Cmd::ShowMessage, 0, "Hi"), line(Cmd::ShowMessage_2, 0, "there")};
	auto branch = EventCommandDefaults::make(Cmd::ConditionalBranch, 0);
	// a wait in the branch, at the wrong depth: the model sets it
	branch.insert(branch.begin() + 1, line(Cmd::Wait, 0, "", {5}));
	m_commands.insert(m_commands.end(), branch.begin(), branch.end());
	m_commands.push_back(line(Cmd::ControlVars, 0));
	m_model = std::make_unique<EventCommandListModel>();
	m_model->setCommands(&m_commands);
	m_tester = std::make_unique<QAbstractItemModelTester>(m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
}

QString EventCommandListModelTest::text(int row, int role) const
{
	return m_model->data(m_model->index(row), role).toString();
}

QVariant EventCommandListModelTest::value(int row, int role) const
{
	return m_model->data(m_model->index(row), role);
}

void EventCommandListModelTest::rowsAndRoles()
{
	using Model = EventCommandListModel;
	QCOMPARE(m_model->rowCount(), 10);
	QCOMPARE(codes(m_commands), (QList<int>{Message, Message2, Branch, Wait, End, Else, End, EndBranch, Vars}));
	// the list as shown, the page end after it
	QCOMPARE(text(0), QStringLiteral("Message: Hi"));
	QCOMPARE(text(1), QStringLiteral(": there"));
	QCOMPARE(text(3), QStringLiteral("Wait 5"));
	QCOMPARE(text(4), QStringLiteral("<>"));
	QCOMPARE(text(9), QStringLiteral("<>"));
	// as Copy as text writes it, with the indent
	QCOMPARE(text(1, Model::TextRole), QStringLiteral("         : there"));
	QCOMPARE(text(3, Model::TextRole), QStringLiteral("  <>Wait 5"));
	QCOMPARE(text(5, Model::TextRole), QStringLiteral(": Else"));
	QCOMPARE(text(6, Model::TextRole), QStringLiteral("  <>"));
	QCOMPARE(value(3, Model::IndentRole).toInt(), 1);
	QCOMPARE(value(8, Model::CodeRole).toInt(), Vars);
	QCOMPARE(value(9, Model::CodeRole).toInt(), End);
	QCOMPARE(value(3, Model::ColorKindRole).toInt(), static_cast<int>(CommandColors::kind(Wait)));
	QCOMPARE(value(5, Model::ColorKindRole).toInt(), static_cast<int>(CommandColors::Kind::Block));
	QCOMPARE(value(9, Model::ColorKindRole).toInt(), static_cast<int>(CommandColors::Kind::End));
	// the keys stop on the commands and the empty lines, not on the next
	// lines of a message nor on the lines of the structure of the branch
	QList<bool> starts, structural, ends;
	for (int row = 0; row < m_model->rowCount(); ++row) {
		starts << value(row, Model::CommandStartRole).toBool();
		structural << value(row, Model::StructuralRole).toBool();
		ends << value(row, Model::PageEndRole).toBool();
	}
	QCOMPARE(starts, (QList<bool>{true, false, true, true, true, false, true, false, true, true}));
	QCOMPARE(structural, (QList<bool>{false, false, false, false, false, true, false, true, false, false}));
	QCOMPARE(ends, (QList<bool>{false, false, false, false, false, false, false, false, false, true}));
	QVERIFY(!(m_model->flags(m_model->index(5)) & Qt::ItemIsSelectable));
	QVERIFY(m_model->flags(m_model->index(4)) & Qt::ItemIsSelectable);
	QCOMPARE(m_model->roleNames().value(Model::TextRole), QByteArray("text"));
	// showing the list is no change to undo
	QCOMPARE(m_model->undoStack()->count(), 0);
}

void EventCommandListModelTest::indentsKeptByBlocks()
{
	// set from the blocks the lines are in
	QCOMPARE(indents(m_commands), (QList<int>{0, 0, 0, 1, 1, 0, 1, 0, 0}));
	// an empty line at the root (imported pages) leaves the next ones there
	Commands odd = {line(Cmd::END, 0), line(Cmd::Wait, 3)};
	EventCommandListModel model;
	model.setCommands(&odd);
	QCOMPARE(indents(odd), (QList<int>{0, 0}));
}

void EventCommandListModelTest::commandRanges()
{
	// a message starts at its first line, with its next lines
	QCOMPARE(m_model->commandStart(1), 0);
	QCOMPARE(m_model->commandRange(1), Range(0, 2));
	// a branch with its block
	QCOMPARE(m_model->commandRange(2), Range(2, 8));
	QCOMPARE(m_model->commandRange(3), Range(3, 4));
	// the lines of its structure are not removed alone
	QCOMPARE(m_model->commandRange(5).first, m_model->commandRange(5).second);
	QCOMPARE(m_model->commandRange(7).first, m_model->commandRange(7).second);
	// selected lines grow to whole commands: the Else takes its branch
	QCOMPARE(m_model->wholeCommands(3, 6), Range(2, 8));
	QCOMPARE(m_model->wholeCommands(1, 3), Range(0, 8));
	QCOMPARE(m_model->wholeCommands(8, 10), Range(8, 9));
	// an empty line alone is no command
	const auto empty = m_model->wholeCommands(4, 5);
	QCOMPARE(empty.first, empty.second);
}

void EventCommandListModelTest::insertBlock()
{
	QSignalSpy inserted(m_model.get(), &QAbstractItemModel::rowsInserted);
	QSignalSpy changed(m_model.get(), &EventCommandListModel::commandsChanged);
	// a new branch in the branch, before its wait
	QCOMPARE(m_model->insertLines(3, EventCommandDefaults::make(Cmd::ConditionalBranch, 1)), 3);
	QCOMPARE(inserted.count(), 1);
	QCOMPARE(inserted[0][1].toInt(), 3);
	QCOMPARE(inserted[0][2].toInt(), 7);
	QCOMPARE(changed.count(), 1);
	QCOMPARE(m_model->rowCount(), 15);
	QCOMPARE(codes(m_commands).mid(2, 8), (QList<int>{Branch, Branch, End, Else, End, EndBranch, Wait, End}));
	QCOMPARE(indents(m_commands).mid(2, 8), (QList<int>{0, 1, 2, 1, 2, 1, 1, 1}));
	QCOMPARE(m_model->commandRange(2), Range(2, 13));
	QCOMPARE(m_model->undoStack()->count(), 1);

	// out of the list: at its end, after the last line
	QCOMPARE(m_model->insertLines(-1, {line(Cmd::Wait, 0)}), 14);
	QCOMPARE(m_commands.back().code, Wait);
	QCOMPARE(m_model->insertLines(100, {line(Cmd::Wait, 0)}), 15);
	// nothing inserted: no change
	QCOMPARE(m_model->insertLines(0, {}), 0);
	QCOMPARE(m_model->undoStack()->count(), 3);
}

void EventCommandListModelTest::removeBlock()
{
	QSignalSpy removed(m_model.get(), &QAbstractItemModel::rowsRemoved);
	m_model->removeLines(m_model->commandRange(2));
	QCOMPARE(removed.count(), 1);
	QCOMPARE(removed[0][1].toInt(), 2);
	QCOMPARE(removed[0][2].toInt(), 7);
	QCOMPARE(codes(m_commands), (QList<int>{Message, Message2, Vars}));
	QCOMPARE(m_model->undoStack()->count(), 1);

	// a line of the structure alone: nothing
	init();
	QSignalSpy changed(m_model.get(), &EventCommandListModel::commandsChanged);
	m_model->removeLines(m_model->commandRange(5));
	QCOMPARE(changed.count(), 0);
	QCOMPARE(m_commands.size(), size_t(9));
	QCOMPARE(m_model->undoStack()->count(), 0);
}

void EventCommandListModelTest::copyPasteBlock()
{
	// the Else line selected copies its branch
	m_model->copyLines(m_model->wholeCommands(5, 6));
	QVERIFY(EventCommandListModel::canPaste());
	QCOMPARE(m_commands.size(), size_t(9));
	// for text editors too
	const QString branch = QStringLiteral("<>Branch\n  <>Wait 5\n  <>\n: Else\n  <>\n: End");
	QCOMPARE(QGuiApplication::clipboard()->text(), branch);
	QCOMPARE(EventCommandListModel::commandsText(Commands(m_commands.begin() + 2, m_commands.begin() + 8)), branch);
	QCOMPARE(EventCommandListModel::commandsText(Commands(m_commands.begin(), m_commands.begin() + 2)),
		QStringLiteral("<>Message: Hi\n         : there"));

	// pasted before the message, then at the end of the page
	QCOMPARE(m_model->paste(0), 0);
	QCOMPARE(m_model->paste(-1), 15);
	QCOMPARE(codes(m_commands), (QList<int>{Branch, Wait, End, Else, End, EndBranch, Message, Message2,
		Branch, Wait, End, Else, End, EndBranch, Vars, Branch, Wait, End, Else, End, EndBranch}));
	QCOMPARE(m_model->rowCount(), 22);
	QCOMPARE(m_model->undoStack()->count(), 2);
}

void EventCommandListModelTest::cutAndPasteDeeper()
{
	QSignalSpy indentChanged(m_model.get(), &QAbstractItemModel::dataChanged);
	// Cut the variable, paste it in the else branch: one level deeper
	m_model->copyLines(m_model->commandRange(8), true);
	QCOMPARE(codes(m_commands), (QList<int>{Message, Message2, Branch, Wait, End, Else, End, EndBranch}));
	QCOMPARE(m_model->paste(6), 6);
	QCOMPARE(codes(m_commands), (QList<int>{Message, Message2, Branch, Wait, End, Else, Vars, End, EndBranch}));
	QCOMPARE(m_commands[6].indent, 1);
	// the views are told of the new indent
	QVERIFY(!indentChanged.isEmpty());
	QCOMPARE(indentChanged.last()[0].toModelIndex().row(), 6);
	// a cut and a paste: two steps
	QCOMPARE(m_model->undoStack()->count(), 2);
	m_model->undoStack()->undo();
	QCOMPARE(codes(m_commands), (QList<int>{Message, Message2, Branch, Wait, End, Else, End, EndBranch}));
	m_model->undoStack()->undo();
	QCOMPARE(codes(m_commands).last(), Vars);
	QCOMPARE(m_commands.back().indent, 0);
}

void EventCommandListModelTest::replaceAfterEdit()
{
	QSignalSpy changed(m_model.get(), &QAbstractItemModel::dataChanged);
	QSignalSpy inserted(m_model.get(), &QAbstractItemModel::rowsInserted);
	QSignalSpy removed(m_model.get(), &QAbstractItemModel::rowsRemoved);
	// the message edited on a copy: a line more
	m_model->replaceLines(m_model->commandRange(0), {line(Cmd::ShowMessage, 0, "Hello"),
		line(Cmd::ShowMessage_2, 0, "big"), line(Cmd::ShowMessage_2, 0, "world")});
	QCOMPARE(changed.count(), 1);
	QCOMPARE(inserted.count(), 1);
	QCOMPARE(inserted[0][1].toInt(), 2);
	QCOMPARE(text(0), QStringLiteral("Message: Hello"));
	QCOMPARE(text(2), QStringLiteral(": world"));
	QCOMPARE(m_model->commandRange(0), Range(0, 3));
	// then a line less
	m_model->replaceLines(m_model->commandRange(0), {line(Cmd::ShowMessage, 0, "Bye")});
	QCOMPARE(removed.count(), 1);
	QCOMPARE(removed[0][1].toInt(), 1);
	QCOMPARE(removed[0][2].toInt(), 2);
	QCOMPARE(codes(m_commands).mid(0, 2), (QList<int>{Message, Branch}));
	QCOMPARE(m_model->undoStack()->count(), 2);
	m_model->undoStack()->undo();
	QCOMPARE(text(1), QStringLiteral(": big"));
}

void EventCommandListModelTest::editedInPlace()
{
	// an editor changed the wait in place
	QSignalSpy changed(m_model.get(), &QAbstractItemModel::dataChanged);
	QSignalSpy reset(m_model.get(), &QAbstractItemModel::modelReset);
	m_commands[3].parameters = parameters({20});
	m_model->commandsEdited();
	QCOMPARE(changed.count(), 1);
	QCOMPARE(changed[0][0].toModelIndex().row(), 3);
	QCOMPARE(changed[0][1].toModelIndex().row(), 3);
	QCOMPARE(text(3), QStringLiteral("Wait 20"));
	QCOMPARE(m_model->undoStack()->count(), 1);
	// a message editor added a line: the rows are read again
	m_commands.insert(m_commands.begin() + 2, line(Cmd::ShowMessage_2, 0, "again"));
	m_model->commandsEdited();
	QCOMPARE(reset.count(), 1);
	QCOMPARE(m_model->rowCount(), 11);
	// accepted without a change: shown again, nothing to undo
	QSignalSpy shown(m_model.get(), &EventCommandListModel::commandsChanged);
	m_model->commandsEdited();
	QCOMPARE(shown.count(), 1);
	QCOMPARE(m_model->undoStack()->count(), 2);
	m_model->undoStack()->undo();
	m_model->undoStack()->undo();
	QCOMPARE(text(3), QStringLiteral("Wait 5"));
}

void EventCommandListModelTest::undoRedo()
{
	const Commands before = m_commands;
	m_model->removeLines(m_model->commandRange(2));
	const Commands after = m_commands;
	QSignalSpy reset(m_model.get(), &QAbstractItemModel::modelReset);
	QSignalSpy changed(m_model.get(), &EventCommandListModel::commandsChanged);
	m_model->undoStack()->undo();
	QVERIFY(m_commands == before);
	QCOMPARE(m_model->rowCount(), 10);
	m_model->undoStack()->redo();
	QVERIFY(m_commands == after);
	QCOMPARE(reset.count(), 2);
	QCOMPARE(changed.count(), 2);
	// undo and redo are no new steps
	QCOMPARE(m_model->undoStack()->count(), 1);
	// another list: a history of its own
	Commands other = {line(Cmd::Wait, 0)};
	m_model->setCommands(&other);
	QCOMPARE(m_model->undoStack()->count(), 0);
	QCOMPARE(m_model->rowCount(), 2);
	m_model->setCommands(nullptr);
	QCOMPARE(m_model->rowCount(), 0);
}

void EventCommandListModelTest::rememberedCommands()
{
	const QString game = m_settings.path();
	// nothing kept: the defaults of the command
	QVERIFY(EventCommandListModel::newCommand(game, Cmd::ConditionalBranch, 2) == EventCommandDefaults::make(Cmd::ConditionalBranch, 2));

	// a branch accepted: the next one has its settings and its shape,
	// without the commands inside it
	m_commands[2].parameters = parameters({1, 7, 0, 0, 0, 0});
	m_model->rememberCommand(2, game);
	const auto next = EventCommandListModel::newCommand(game, Cmd::ConditionalBranch, 1);
	QCOMPARE(codes(next), (QList<int>{Branch, End, Else, End, EndBranch}));
	QCOMPARE(indents(next), (QList<int>{1, 2, 1, 2, 1}));
	QVERIFY(next[0].parameters == m_commands[2].parameters);
	// another game keeps its own
	QTemporaryDir other;
	QVERIFY(EventCommandListModel::newCommand(other.path(), Cmd::ConditionalBranch, 0) == EventCommandDefaults::make(Cmd::ConditionalBranch, 0));
	// a message starts empty: its text is not written twice
	m_model->rememberCommand(0, game);
	QVERIFY(EventCommandListModel::newCommand(game, Cmd::ShowMessage, 0) == EventCommandDefaults::make(Cmd::ShowMessage, 0));
}

QTEST_MAIN(EventCommandListModelTest)
#include "event_command_list_model_test.moc"
