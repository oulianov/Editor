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

// The lock of a modal window (ModalWindowLocks): held while a window
// blocking the others is shown, given back when it hides or goes, none for
// the other windows and those holding their own. The dialogs of the Editor
// themselves: tests/scripts/edit_locks.txt.

#include <QGuiApplication>
#include <QTest>
#include <QWindow>
#include <memory>
#include "controller/edit_locks.h"
#include "model/project_watcher.h"

class EditLocksTest : public QObject
{
	Q_OBJECT

private slots:
	void init();
	void cleanup();
	void modalWindow();
	void otherWindows();
	void deletedWhileShown();

private:
	std::unique_ptr<EditLocks> m_editLocks;
	std::unique_ptr<ModalWindowLocks> m_modal;
};

void EditLocksTest::init()
{
	m_editLocks = std::make_unique<EditLocks>();
	m_modal = std::make_unique<ModalWindowLocks>(*m_editLocks);
	qApp->installEventFilter(m_modal.get());
}

void EditLocksTest::cleanup()
{
	m_modal.reset();
	m_editLocks.reset();
}

void EditLocksTest::modalWindow()
{
	QWindow window;
	window.setModality(Qt::ApplicationModal);
	QVERIFY(m_editLocks->isEmpty());
	window.show();
	QCOMPARE(m_editLocks->locks().size(), 1);
	QCOMPARE(m_editLocks->locks().first().kind, EditLock::Dialog);
	// nothing is read again under it, the maps neither
	QVERIFY(!m_editLocks->canReload(ProjectWatcher::Database));
	QVERIFY(!m_editLocks->canReload(1));
	// shown again: still one
	window.show();
	QCOMPARE(m_editLocks->locks().size(), 1);
	window.hide();
	QVERIFY(m_editLocks->isEmpty());
	QVERIFY(m_editLocks->canReload(ProjectWatcher::Database));
	// modal to its parent only: it blocks the main window too
	window.setModality(Qt::WindowModal);
	window.show();
	QCOMPARE(m_editLocks->locks().size(), 1);
	window.close();
	QVERIFY(m_editLocks->isEmpty());
}

void EditLocksTest::otherWindows()
{
	// a window beside the others (the search, the guide)
	QWindow window;
	window.show();
	QVERIFY(m_editLocks->isEmpty());
	window.hide();
	// one holding its own lock (the event and database windows)
	QWindow own;
	own.setModality(Qt::ApplicationModal);
	own.setProperty(ModalWindowLocks::OwnLockProperty, true);
	own.show();
	QVERIFY(m_editLocks->isEmpty());
	own.hide();
	QVERIFY(m_editLocks->isEmpty());
}

void EditLocksTest::deletedWhileShown()
{
	auto window = std::make_unique<QWindow>();
	window->setModality(Qt::ApplicationModal);
	window->show();
	QCOMPARE(m_editLocks->locks().size(), 1);
	window.reset();
	QVERIFY(m_editLocks->isEmpty());
	// two at once, one over the other
	QWindow first, second;
	first.setModality(Qt::ApplicationModal);
	second.setModality(Qt::ApplicationModal);
	first.show();
	second.show();
	QCOMPARE(m_editLocks->locks().size(), 2);
	second.hide();
	QCOMPARE(m_editLocks->locks().size(), 1);
	first.hide();
	QVERIFY(m_editLocks->isEmpty());
}

QTEST_MAIN(EditLocksTest)
#include "edit_locks_test.moc"
