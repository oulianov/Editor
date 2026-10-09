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

#include "project_watcher.h"
#include "defines.h"
#include "common/logging.h"
#include <QDateTime>
#include <QFileInfo>
#include <QRegularExpression>

ProjectWatcher::ProjectWatcher(QObject* parent) : QObject(parent) {
	// Writers often replace a file in several steps: wait until it settles
	m_debounce.setSingleShot(true);
	m_debounce.setInterval(250);
	m_retry.setInterval(500);

	connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
	connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
	connect(&m_debounce, &QTimer::timeout, this, &ProjectWatcher::rescan);
	connect(&m_retry, &QTimer::timeout, this, &ProjectWatcher::deliver);
}

void ProjectWatcher::setProject(const QString& dir, bool easyrpg) {
	if (!m_watcher.files().isEmpty()) {
		m_watcher.removePaths(m_watcher.files());
	}
	if (!m_watcher.directories().isEmpty()) {
		m_watcher.removePaths(m_watcher.directories());
	}
	m_debounce.stop();
	m_retry.stop();
	m_pending.clear();
	m_stamps.clear();

	m_dir = dir;
	m_easyrpg = easyrpg;
	if (m_dir.isEmpty()) {
		return;
	}

	m_watcher.addPath(m_dir);
	m_stamps = scan();
	qCDebug(lcReload).noquote() << "watching" << m_dir << m_stamps.size() << "files";
	for (auto it = m_stamps.cbegin(); it != m_stamps.cend(); ++it) {
		m_watcher.addPath(QDir(m_dir).filePath(it.key()));
	}
}

void ProjectWatcher::acknowledge(const QString& path) {
	QFileInfo info(path);
	if (m_dir.isEmpty() || kindOf(info.fileName()) == 0) {
		return;
	}
	info.refresh();
	qCDebug(lcReload).noquote() << "written by the Editor:" << info.fileName();
	m_stamps[info.fileName()] = { info.lastModified().toMSecsSinceEpoch(), info.size() };
	if (!m_watcher.files().contains(info.absoluteFilePath())) {
		m_watcher.addPath(info.absoluteFilePath());
	}
}

void ProjectWatcher::setCanDeliver(std::function<bool(int)> can_deliver) {
	m_canDeliver = std::move(can_deliver);
}

int ProjectWatcher::kindOf(const QString& file_name) const {
	if (file_name.compare(m_easyrpg ? EASY_DB : RM_DB, Qt::CaseInsensitive) == 0) {
		return Database;
	}
	if (file_name.compare(m_easyrpg ? EASY_MT : RM_MT, Qt::CaseInsensitive) == 0) {
		return TreeMap;
	}
	static const QRegularExpression lmu(QStringLiteral("^map(\\d{4})\\.lmu$"), QRegularExpression::CaseInsensitiveOption);
	static const QRegularExpression emu(QStringLiteral("^map(\\d{4})\\.emu$"), QRegularExpression::CaseInsensitiveOption);
	auto match = (m_easyrpg ? emu : lmu).match(file_name);
	if (match.hasMatch()) {
		return match.captured(1).toInt();
	}
	return 0;
}

QHash<QString, ProjectWatcher::Stamp> ProjectWatcher::scan() const {
	QHash<QString, Stamp> stamps;
	const auto entries = QDir(m_dir).entryInfoList(QDir::Files);
	for (const QFileInfo& info : entries) {
		if (kindOf(info.fileName()) != 0) {
			stamps.insert(info.fileName(), { info.lastModified().toMSecsSinceEpoch(), info.size() });
		}
	}
	return stamps;
}

void ProjectWatcher::rescan() {
	if (m_dir.isEmpty()) {
		return;
	}
	auto stamps = scan();
	for (auto it = stamps.cbegin(); it != stamps.cend(); ++it) {
		auto old = m_stamps.constFind(it.key());
		if (old == m_stamps.cend() || !(*old == it.value())) {
			qCDebug(lcReload).noquote() << it.key() << "changed on disk";
			m_pending.insert(kindOf(it.key()));
		}
		// a file replaced by a rename is no longer watched
		QString path = QDir(m_dir).filePath(it.key());
		if (!m_watcher.files().contains(path)) {
			m_watcher.addPath(path);
		}
	}
	m_stamps = stamps;
	deliver();
}

void ProjectWatcher::deliver() {
	// The map tree first: maps can be added or removed with it
	QList<int> kinds = m_pending.values();
	std::sort(kinds.begin(), kinds.end(), [](int a, int b) {
		return a == TreeMap ? b != TreeMap : (b == TreeMap ? false : a < b);
	});
	for (int kind : kinds) {
		if (m_canDeliver && !m_canDeliver(kind)) {
			qCDebug(lcReload) << "reload of" << kind << "waits for the open dialog";
			continue;
		}
		m_pending.remove(kind);
		if (kind == Database) {
			emit databaseChanged();
		} else if (kind == TreeMap) {
			emit treeMapChanged();
		} else {
			emit mapChanged(kind);
		}
	}
	if (m_pending.isEmpty()) {
		m_retry.stop();
	} else if (!m_retry.isActive()) {
		m_retry.start();
	}
}
