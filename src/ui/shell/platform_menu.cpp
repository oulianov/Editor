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

#include "platform_menu.h"
#include <QIcon>
#include <QKeySequence>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>
#include <qpa/qplatformmenu.h>
#include <qpa/qplatformtheme.h>
#include <QtGui/private/qguiapplication_p.h>

namespace {

QPlatformTheme* theme()
{
	return QGuiApplicationPrivate::platformTheme();
}

/** The children declared in QML (Components...); menus and items are added */
template<typename Owner>
QQmlListProperty<QObject> dataList(Owner* owner, QList<QObject*>* data)
{
	return QQmlListProperty<QObject>(owner, data,
		[](QQmlListProperty<QObject>* list, QObject* object) {
			auto* owner = static_cast<Owner*>(list->object);
			static_cast<QList<QObject*>*>(list->data)->append(object);
			if (!object->parent())
				object->setParent(owner);
			owner->adopt(object);
		},
		[](QQmlListProperty<QObject>* list) { return static_cast<QList<QObject*>*>(list->data)->size(); },
		[](QQmlListProperty<QObject>* list, qsizetype index) { return static_cast<QList<QObject*>*>(list->data)->at(index); },
		[](QQmlListProperty<QObject>* list) { static_cast<QList<QObject*>*>(list->data)->clear(); });
}

}

/*
 * PlatformMenuItem
 */

PlatformMenuItem::PlatformMenuItem(QObject* parent) :
	QObject(parent)
{
	if (QPlatformTheme* platform = theme())
		m_handle.reset(platform->createPlatformMenuItem());
	if (m_handle) {
		m_handle->setTag(reinterpret_cast<quintptr>(this));
		connect(m_handle.get(), &QPlatformMenuItem::activated, this, &PlatformMenuItem::triggered);
	}
}

PlatformMenuItem::~PlatformMenuItem()
{
	if (m_handle && m_menu && m_menu->handle())
		m_menu->handle()->removeMenuItem(m_handle.get());
	if (m_handle)
		m_handle->setMenu(nullptr);
}

void PlatformMenuItem::setText(const QString& text)
{
	if (m_text == text)
		return;
	m_text = text;
	sync();
	emit changed();
}

void PlatformMenuItem::setIconSource(const QUrl& source)
{
	if (m_iconSource == source)
		return;
	m_iconSource = source;
	sync();
	emit changed();
}

void PlatformMenuItem::setEnabled(bool enabled)
{
	if (m_enabled == enabled)
		return;
	m_enabled = enabled;
	sync();
	emit changed();
}

void PlatformMenuItem::setVisible(bool visible)
{
	if (m_visible == visible)
		return;
	m_visible = visible;
	sync();
	emit changed();
}

void PlatformMenuItem::setCheckable(bool checkable)
{
	if (m_checkable == checkable)
		return;
	m_checkable = checkable;
	sync();
	emit changed();
}

void PlatformMenuItem::setChecked(bool checked)
{
	if (m_checked == checked)
		return;
	m_checked = checked;
	sync();
	emit changed();
}

void PlatformMenuItem::setSeparator(bool separator)
{
	if (m_separator == separator)
		return;
	m_separator = separator;
	sync();
	emit changed();
}

void PlatformMenuItem::setShortcut(const QVariant& shortcut)
{
	if (m_shortcut == shortcut)
		return;
	m_shortcut = shortcut;
	sync();
	emit changed();
}

void PlatformMenuItem::setRole(Role role)
{
	if (m_role == role)
		return;
	m_role = role;
	sync();
	emit changed();
}

void PlatformMenuItem::setMenu(PlatformMenu* menu)
{
	m_menu = menu;
}

void PlatformMenuItem::setSubMenu(PlatformMenu* menu)
{
	m_subMenu = menu;
	sync();
}

void PlatformMenuItem::sync()
{
	if (!m_handle)
		return;
	m_handle->setText(m_text);
	QIcon icon;
	if (m_iconSource.scheme() == QLatin1String("qrc"))
		icon = QIcon(QLatin1Char(':') + m_iconSource.path());
	else if (m_iconSource.isLocalFile())
		icon = QIcon(m_iconSource.toLocalFile());
	m_handle->setIcon(icon);
	m_handle->setVisible(m_visible);
	m_handle->setIsSeparator(m_separator);
	m_handle->setRole(static_cast<QPlatformMenuItem::MenuRole>(m_role));
	m_handle->setCheckable(m_checkable);
	m_handle->setChecked(m_checked);
	m_handle->setEnabled(m_enabled);
	QKeySequence keys;
	if (m_shortcut.typeId() == QMetaType::Int)
		keys = QKeySequence(static_cast<QKeySequence::StandardKey>(m_shortcut.toInt()));
	else
		keys = QKeySequence::fromString(m_shortcut.toString());
	m_handle->setShortcut(keys);
	m_handle->setMenu(m_subMenu ? m_subMenu->handle() : nullptr);
	if (m_menu && m_menu->handle())
		m_menu->handle()->syncMenuItem(m_handle.get());
}

/*
 * PlatformMenu
 */

PlatformMenu::PlatformMenu(QObject* parent) :
	QObject(parent)
{
	if (QPlatformTheme* platform = theme())
		m_handle.reset(platform->createPlatformMenu());
	if (m_handle)
		m_handle->setTag(reinterpret_cast<quintptr>(this));
}

PlatformMenu::~PlatformMenu()
{
	// out of the menu it is a submenu of, then its items out of it
	delete m_menuItem;
	m_menuItem = nullptr;
	for (const QPointer<PlatformMenuItem>& item : std::as_const(m_items)) {
		if (!item)
			continue;
		if (m_handle && item->handle())
			m_handle->removeMenuItem(item->handle());
		item->setMenu(nullptr);
	}
}

void PlatformMenu::setTitle(const QString& title)
{
	if (m_title == title)
		return;
	m_title = title;
	sync();
	emit titleChanged();
}

QList<PlatformMenuItem*> PlatformMenu::items() const
{
	QList<PlatformMenuItem*> items;
	for (const QPointer<PlatformMenuItem>& item : m_items) {
		if (item)
			items.append(item);
	}
	return items;
}

QQmlListProperty<QObject> PlatformMenu::data()
{
	return dataList(this, &m_data);
}

void PlatformMenu::adopt(QObject* object)
{
	if (auto* item = qobject_cast<PlatformMenuItem*>(object))
		addItem(item);
	else if (auto* menu = qobject_cast<PlatformMenu*>(object))
		addMenu(menu);
}

void PlatformMenu::addItem(PlatformMenuItem* item)
{
	if (!item || m_items.contains(item))
		return;
	m_items.append(item);
	item->setMenu(this);
	if (m_handle && item->handle())
		m_handle->insertMenuItem(item->handle(), nullptr);
	item->sync();
	emit itemsChanged();
}

void PlatformMenu::addMenu(PlatformMenu* menu)
{
	if (!menu || menu == this)
		return;
	if (!menu->m_menuItem) {
		menu->m_menuItem = new PlatformMenuItem(menu);
		menu->m_menuItem->setText(menu->title());
		menu->m_menuItem->setSubMenu(menu);
	}
	addItem(menu->m_menuItem);
}

void PlatformMenu::sync()
{
	if (m_handle) {
		m_handle->setText(m_title);
		m_handle->setEnabled(true);
		m_handle->setVisible(true);
	}
	if (m_menuItem)
		m_menuItem->setText(m_title);
}

/*
 * PlatformMenuBar
 */

PlatformMenuBar::PlatformMenuBar(QObject* parent) :
	QObject(parent)
{
	if (QPlatformTheme* platform = theme())
		m_handle.reset(platform->createPlatformMenuBar());
}

PlatformMenuBar::~PlatformMenuBar()
{
	for (const QPointer<PlatformMenu>& menu : std::as_const(m_menus)) {
		if (menu && m_handle && menu->handle())
			m_handle->removeMenu(menu->handle());
	}
}

QList<PlatformMenu*> PlatformMenuBar::menus() const
{
	QList<PlatformMenu*> menus;
	for (const QPointer<PlatformMenu>& menu : m_menus) {
		if (menu)
			menus.append(menu);
	}
	return menus;
}

QQmlListProperty<QObject> PlatformMenuBar::data()
{
	return dataList(this, &m_data);
}

void PlatformMenuBar::adopt(QObject* object)
{
	if (auto* menu = qobject_cast<PlatformMenu*>(object))
		addMenu(menu);
}

void PlatformMenuBar::addMenu(PlatformMenu* menu)
{
	if (!menu || m_menus.contains(menu))
		return;
	m_menus.append(menu);
	menu->sync();
	if (m_handle && menu->handle()) {
		m_handle->insertMenu(menu->handle(), nullptr);
		// a new title (another language) in the bar too
		connect(menu, &PlatformMenu::titleChanged, this, [this, menu] {
			if (m_handle && menu->handle())
				m_handle->syncMenu(menu->handle());
		});
	}
	emit menusChanged();
}

void PlatformMenuBar::classBegin()
{
}

void PlatformMenuBar::componentComplete()
{
	m_window = findWindow();
	if (m_handle)
		m_handle->handleReparent(m_window);
}

QWindow* PlatformMenuBar::findWindow() const
{
	for (QObject* object = parent(); object; object = object->parent()) {
		if (auto* window = qobject_cast<QWindow*>(object))
			return window;
		if (auto* item = qobject_cast<QQuickItem*>(object); item && item->window())
			return item->window();
	}
	return nullptr;
}
