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

#include "edit_locks.h"
#include <QCoreApplication>
#include <QEvent>
#include <QList>
#include <QVariant>
#include <QWindow>
#include <utility>

EditLocks::Lock::Lock(const EditLock& lock, EditLocks& locks) :
	m_locks(&locks), m_id(locks.acquire(lock))
{
}

EditLocks::Lock::~Lock()
{
	release();
}

EditLocks::Lock::Lock(Lock&& other) noexcept :
	m_locks(std::exchange(other.m_locks, nullptr)), m_id(std::exchange(other.m_id, 0))
{
}

EditLocks::Lock& EditLocks::Lock::operator=(Lock&& other) noexcept
{
	if (this != &other) {
		release();
		m_locks = std::exchange(other.m_locks, nullptr);
		m_id = std::exchange(other.m_id, 0);
	}
	return *this;
}

void EditLocks::Lock::release()
{
	if (m_locks && m_id)
		m_locks->release(m_id);
	m_locks = nullptr;
	m_id = 0;
}

bool EditLocks::Lock::isHeld() const
{
	return m_locks && m_id;
}

EditLocks::EditLocks(QObject* parent) :
	QObject(parent)
{
}

EditLocks& EditLocks::instance()
{
	static EditLocks locks;
	return locks;
}

int EditLocks::acquire(const EditLock& lock)
{
	const int id = m_next++;
	m_locks[id] = lock;
	emit changed();
	return id;
}

void EditLocks::release(int id)
{
	if (m_locks.erase(id))
		emit changed();
}

bool EditLocks::isEmpty() const
{
	return m_locks.empty();
}

QList<EditLock> EditLocks::locks() const
{
	QList<EditLock> locks;
	for (const auto& [id, lock] : m_locks)
		locks.append(lock);
	return locks;
}

bool EditLocks::canReload(int kind) const
{
	for (const auto& [id, lock] : m_locks) {
		// An event dialog follows the changes of the maps (positive kinds).
		// Other editors keep pointers into the data: wait until they close.
		if (lock.kind != EditLock::Event || kind <= 0)
			return false;
	}
	return true;
}

ModalWindowLocks::ModalWindowLocks(EditLocks& locks, QObject* parent) :
	QObject(parent), m_editLocks(locks)
{
}

void ModalWindowLocks::install()
{
	static ModalWindowLocks* locks = nullptr;
	if (!locks) {
		locks = new ModalWindowLocks(EditLocks::instance(), QCoreApplication::instance());
		QCoreApplication::instance()->installEventFilter(locks);
	}
}

bool ModalWindowLocks::eventFilter(QObject* watched, QEvent* event)
{
	// what QGuiApplication::modalWindow() tells: the modal windows shown (a
	// minimized one stays modal)
	const bool show = event->type() == QEvent::Show;
	if ((!show && event->type() != QEvent::Hide) || event->spontaneous() || !watched->isWindowType())
		return false;
	auto* window = static_cast<QWindow*>(watched);
	if (!show) {
		if (m_locks.erase(window))
			disconnect(window, &QObject::destroyed, this, nullptr);
	} else if (window->modality() != Qt::NonModal && !window->property(OwnLockProperty).toBool()
			&& !m_locks.contains(window)) {
		m_locks.emplace(window, EditLocks::Lock({EditLock::Dialog}, m_editLocks));
		// deleted without being hidden first
		connect(window, &QObject::destroyed, this, [this, window] { m_locks.erase(window); });
	}
	return false;
}
