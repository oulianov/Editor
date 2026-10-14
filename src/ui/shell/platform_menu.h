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
#include <QPointer>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QUrl>
#include <QVariant>
#include <QtQmlIntegration>
#include <memory>

class PlatformMenu;
class QPlatformMenu;
class QPlatformMenuBar;
class QPlatformMenuItem;
class QWindow;

/*
 * The menu bar of the platform (macOS's), as Qt.labs.platform has it but
 * without Qt Widgets, which that module links: the menus and items of the
 * platform's theme (QPlatformTheme), shown by the system. Where the platform
 * has none (offscreen), the menus and items are kept all the same, with
 * their roles, and can be triggered (the scripts check them).
 */

/** An item of a PlatformMenu: an action, a separator or a submenu */
class PlatformMenuItem : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
	/** A picture file (qrc:/..., file:...), empty for none */
	Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY changed)
	Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY changed)
	Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY changed)
	Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable NOTIFY changed)
	Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY changed)
	Q_PROPERTY(bool separator READ isSeparator WRITE setSeparator NOTIFY changed)
	/** Its keys: a text (QKeySequence::PortableText) or a QKeySequence::StandardKey */
	Q_PROPERTY(QVariant shortcut READ shortcut WRITE setShortcut NOTIFY changed)
	/** Where macOS puts it: in its menu, or in the application menu */
	Q_PROPERTY(Role role READ role WRITE setRole NOTIFY changed)

public:
	/** As QPlatformMenuItem::MenuRole */
	enum Role {
		NoRole = 0,
		TextHeuristicRole,
		ApplicationSpecificRole,
		AboutQtRole,
		AboutRole,
		PreferencesRole,
		QuitRole,
	};
	Q_ENUM(Role)

	explicit PlatformMenuItem(QObject* parent = nullptr);
	~PlatformMenuItem() override;

	QString text() const { return m_text; }
	void setText(const QString& text);
	QUrl iconSource() const { return m_iconSource; }
	void setIconSource(const QUrl& source);
	bool isEnabled() const { return m_enabled; }
	void setEnabled(bool enabled);
	bool isVisible() const { return m_visible; }
	void setVisible(bool visible);
	bool isCheckable() const { return m_checkable; }
	void setCheckable(bool checkable);
	bool isChecked() const { return m_checked; }
	void setChecked(bool checked);
	bool isSeparator() const { return m_separator; }
	void setSeparator(bool separator);
	QVariant shortcut() const { return m_shortcut; }
	void setShortcut(const QVariant& shortcut);
	Role role() const { return m_role; }
	void setRole(Role role);

	/** The platform's item, nullptr without one */
	QPlatformMenuItem* handle() const { return m_handle.get(); }
	/** The menu it is in */
	void setMenu(PlatformMenu* menu);
	/** The submenu it opens */
	void setSubMenu(PlatformMenu* menu);
	/** Its state given to the platform's item */
	void sync();

signals:
	void changed();
	/** Chosen in the menu */
	void triggered();

private:
	std::unique_ptr<QPlatformMenuItem> m_handle;
	QPointer<PlatformMenu> m_menu;
	QPointer<PlatformMenu> m_subMenu;
	QString m_text;
	QUrl m_iconSource;
	bool m_enabled = true;
	bool m_visible = true;
	bool m_checkable = false;
	bool m_checked = false;
	bool m_separator = false;
	QVariant m_shortcut;
	Role m_role = NoRole;
};

/** A menu of a PlatformMenuBar, or a submenu of a menu */
class PlatformMenu : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
	/** The items it has, in their order */
	Q_PROPERTY(QList<PlatformMenuItem*> items READ items NOTIFY itemsChanged)
	Q_PROPERTY(QQmlListProperty<QObject> data READ data CONSTANT)
	Q_CLASSINFO("DefaultProperty", "data")

public:
	explicit PlatformMenu(QObject* parent = nullptr);
	~PlatformMenu() override;

	QString title() const { return m_title; }
	void setTitle(const QString& title);
	QList<PlatformMenuItem*> items() const;
	QQmlListProperty<QObject> data();
	/** A child declared in QML: an item or a submenu is added */
	void adopt(QObject* object);

	/** Puts an item at the end */
	Q_INVOKABLE void addItem(PlatformMenuItem* item);
	/** Puts a submenu at the end */
	Q_INVOKABLE void addMenu(PlatformMenu* menu);

	/** The platform's menu, nullptr without one */
	QPlatformMenu* handle() const { return m_handle.get(); }
	/** The item of the menu it is a submenu of (made when added to one) */
	PlatformMenuItem* menuItem() const { return m_menuItem; }
	/** Its title given to the platform's menu (and to the item showing it) */
	void sync();

signals:
	void titleChanged();
	void itemsChanged();

private:
	std::unique_ptr<QPlatformMenu> m_handle;
	QList<QPointer<PlatformMenuItem>> m_items;
	QList<QObject*> m_data;
	PlatformMenuItem* m_menuItem = nullptr;
	QString m_title;
};

/** The menu bar of the window it is made in */
class PlatformMenuBar : public QObject, public QQmlParserStatus
{
	Q_OBJECT
	QML_ELEMENT
	Q_INTERFACES(QQmlParserStatus)
	Q_PROPERTY(QList<PlatformMenu*> menus READ menus NOTIFY menusChanged)
	/** The platform shows it (macOS); false: its menus are only kept (offscreen) */
	Q_PROPERTY(bool shown READ isShown CONSTANT)
	Q_PROPERTY(QQmlListProperty<QObject> data READ data CONSTANT)
	Q_CLASSINFO("DefaultProperty", "data")

public:
	explicit PlatformMenuBar(QObject* parent = nullptr);
	~PlatformMenuBar() override;

	bool isShown() const { return m_handle != nullptr; }

	QList<PlatformMenu*> menus() const;
	QQmlListProperty<QObject> data();
	/** A child declared in QML: a menu is added */
	void adopt(QObject* object);

	/** Puts a menu at the end */
	Q_INVOKABLE void addMenu(PlatformMenu* menu);

	void classBegin() override;
	void componentComplete() override;

signals:
	void menusChanged();

private:
	/** The window of the item or window it is made in */
	QWindow* findWindow() const;

	std::unique_ptr<QPlatformMenuBar> m_handle;
	QList<QPointer<PlatformMenu>> m_menus;
	QList<QObject*> m_data;
	QPointer<QWindow> m_window;
};
