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

#include <QObject>
#include <map>

/** What an open editor holds: data it points into, or shows */
struct EditLock {
	enum Kind {
		/** An event of a map edited (the event dialog): it follows the
		 *  changes of the maps, but shows names from the database and the
		 *  map tree */
		Event,
		/** The database edited */
		Database,
		/** Another dialog in front, which can keep pointers into any data
		 *  (Map Properties, a question...) */
		Dialog,
	};
	Kind kind = Dialog;
	/** The map of an Event lock */
	int map_id = 0;
};

/**
 * The editors open, and what another program's changes can replace while
 * they are: a file changed on disk is read again only when no lock holds
 * its data (ProjectWatcher waits and tries again). The interface takes a
 * lock while an editor is open.
 */
class EditLocks : public QObject
{
	Q_OBJECT

public:
	/** Held while it lives */
	class Lock
	{
	public:
		Lock() = default;
		explicit Lock(const EditLock& lock, EditLocks& locks = EditLocks::instance());
		~Lock();
		Lock(Lock&& other) noexcept;
		Lock& operator=(Lock&& other) noexcept;
		Lock(const Lock&) = delete;
		Lock& operator=(const Lock&) = delete;
		void release();
		bool isHeld() const;

	private:
		EditLocks* m_locks = nullptr;
		int m_id = 0;
	};

	explicit EditLocks(QObject* parent = nullptr);

	/** The locks of the Editor */
	static EditLocks& instance();

	/** An id for release() */
	int acquire(const EditLock& lock);
	void release(int id);
	bool isEmpty() const;
	QList<EditLock> locks() const;

	/** The data of kind (ProjectWatcher::Database, ProjectWatcher::TreeMap
	 *  or a map id) can be read again now */
	bool canReload(int kind) const;

signals:
	void changed();

private:
	std::map<int, EditLock> m_locks;
	int m_next = 1;
};

/**
 * Holds an EditLock::Dialog while a modal window is shown (one blocking the
 * main window: a dialog opened by QmlHost::exec, a question of the
 * DialogService, a picker, the import progress...): it can keep pointers
 * into any data. A window with its own lock (the event and database
 * windows) has the property ownEditLock set; QmlHost::exec sets it too, as
 * it holds the lock itself until the caller has read the window's answer.
 */
class ModalWindowLocks : public QObject
{
	Q_OBJECT

public:
	static constexpr const char* OwnLockProperty = "ownEditLock";

	explicit ModalWindowLocks(EditLocks& locks = EditLocks::instance(), QObject* parent = nullptr);

	/** Watches the windows of the application, once */
	static void install();

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	EditLocks& m_editLocks;
	std::map<QObject*, EditLocks::Lock> m_locks;
};
