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

#include <QDir>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <functional>

/**
 * Notices when the database, the map tree or a map of the project changes
 * on disk because another program wrote it (a text editor sync tool, a second
 * editor...). Files written by the Editor itself are acknowledged and ignored.
 */
class ProjectWatcher : public QObject
{
	Q_OBJECT

public:
	explicit ProjectWatcher(QObject* parent = nullptr);

	/** A version of a file: two writes of the same size in the same second
	 *  differ by their file (a write replacing the file) or their time */
	struct Stamp {
		bool exists = false;
		qint64 mtime = 0; // nanoseconds where the system has them
		qint64 size = -1;
		quint64 file_id = 0; // inode
		bool operator==(const Stamp& o) const {
			return exists == o.exists && mtime == o.mtime && size == o.size && file_id == o.file_id;
		}
		bool operator!=(const Stamp& o) const { return !(*this == o); }
	};
	static Stamp stampOf(const QString& path);

	/** Watch the project in dir, or nothing when dir is empty. Watching the
	 *  same project again keeps the changes not reported yet. */
	void setProject(const QString& dir, bool easyrpg);

	/** The Editor read (loaded) or wrote this version of path: it is the
	 *  version the Editor has, and it is not reported as an outside change. */
	void acknowledge(const QString& path, const Stamp& stamp);

	/** path changed on disk since the Editor read or wrote it: writing it now
	 *  would replace the changes of another program. Known even when the
	 *  changes are not watched (reloading off). */
	bool isStale(const QString& path) const;

	/** Reload this data from disk (Database, TreeMap or a map id), as when
	 *  it changes: when the dialogs editing it allow */
	void requestReload(int kind);

	/** Decides whether a change (see kindOf) can be reported now; while it
	 *  cannot (a dialog editing that data is open), it is kept for later. */
	void setCanDeliver(std::function<bool(int kind)> can_deliver);

	static constexpr int Database = -2;
	static constexpr int TreeMap = -1;

signals:
	void databaseChanged();
	void treeMapChanged();
	void mapChanged(int id);

private:
	/** Database, TreeMap or a map id; 0 when not a project file. */
	int kindOf(const QString& file_name) const;
	QHash<QString, Stamp> scan() const;
	void rescan();
	void deliver();

	QFileSystemWatcher m_watcher;
	QTimer m_debounce;
	QTimer m_retry;
	QString m_dir;
	bool m_easyrpg = false;
	/** The versions last seen on disk, by file name (to notice changes) */
	QHash<QString, Stamp> m_stamps;
	/** The versions the Editor has, by absolute path */
	QHash<QString, Stamp> m_known;
	QSet<QString> m_watched;
	QSet<int> m_pending;
	std::function<bool(int)> m_canDeliver;
};
