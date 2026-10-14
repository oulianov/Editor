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

#include <QAbstractListModel>
#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>
#include <vector>
#include "shortcut_catalog.h"

/**
 * Every action of the main window, by its object name (actionMapSave...,
 * which scripts and the QML shell use): its texts (in the context
 * "MainWindow", as the window always had them), its icon (a name of the
 * ezbright and ezdark icon themes), its keys (ShortcutCatalog), whether it
 * can be checked and its group (the layers, the tools and the zoom ratios
 * each have one checked at most), and whether it is enabled and checked now,
 * which the controllers set. The menus and the toolbars are listed too.
 *
 * It is the one place the main window (Actions.qml, MainMenuBar.qml,
 * MainToolBar.qml) reads them from. As a
 * list model, one row per action, its roles named as the QML properties.
 */
class ActionCatalog : public QAbstractListModel
{
	Q_OBJECT

public:
	/** The actions of a group exclude each other: checking one unchecks the
	 *  others. A layer and a tool are always checked; no zoom ratio is
	 *  checked at the scales between them */
	enum class Group { None, Layer, Tool, Zoom };
	Q_ENUM(Group)
	/** Where macOS puts it (the application menu); None: in its menu */
	enum class MenuRole { None, Quit, About, Preferences };
	Q_ENUM(MenuRole)

	enum Roles {
		NameRole = Qt::UserRole + 1,
		TextRole,
		ToolTipRole,
		StatusTipRole,
		IconTextRole,
		IconNameRole,
		/** The keys, as text QKeySequence can read on any platform ("Ctrl+S") */
		ShortcutsRole,
		/** The platform's standard key (QKeySequence::StandardKey), 0 for none */
		StandardKeyRole,
		/** ShortcutCatalog::Context: 0 the window, 1 the map tree */
		ShortcutContextRole,
		CheckableRole,
		CheckedRole,
		EnabledRole,
		VisibleRole,
		GroupRole,
		MenuRoleRole,
	};

	/** A menu: its items are action names, "" a separator, "menu:" and a
	 *  menu's name a submenu */
	struct Menu {
		QString name;
		QString title;
		QStringList items;
	};

	explicit ActionCatalog(QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	/** The object names, in the order of the window's menus */
	Q_INVOKABLE QStringList names() const;
	Q_INVOKABLE bool contains(const QString& name) const;
	Q_INVOKABLE int indexOf(const QString& name) const;

	/** In the language of the Editor */
	Q_INVOKABLE QString text(const QString& name) const;
	/** Empty when it has none of its own (the text is shown) */
	Q_INVOKABLE QString toolTip(const QString& name) const;
	Q_INVOKABLE QString statusTip(const QString& name) const;
	/** The text of a toolbar button, empty for the text without its "&" */
	Q_INVOKABLE QString iconText(const QString& name) const;
	/** In the icon themes ezbright and ezdark (and :/bright, :/dark), empty
	 *  for none */
	Q_INVOKABLE QString iconName(const QString& name) const;
	QList<QKeySequence> shortcuts(const QString& name) const;
	/** As QKeySequence::PortableText: what QML's Shortcut reads */
	Q_INVOKABLE QStringList shortcutTexts(const QString& name) const;
	ShortcutCatalog::Context shortcutContext(const QString& name) const;
	/** Its keys work in the map tree only (ShortcutCatalog::Context::MapTree) */
	Q_INVOKABLE bool isTreeShortcut(const QString& name) const;
	Q_INVOKABLE bool isCheckable(const QString& name) const;
	Q_INVOKABLE bool isVisible(const QString& name) const;
	Group group(const QString& name) const;
	Q_INVOKABLE ActionCatalog::MenuRole menuRole(const QString& name) const;
	/** The actions of a group, in their order */
	QStringList groupActions(Group group) const;

	Q_INVOKABLE bool isEnabled(const QString& name) const;
	Q_INVOKABLE bool isChecked(const QString& name) const;
	void setEnabled(const QString& name, bool enabled);
	/** Checking an action of a group unchecks the others */
	void setChecked(const QString& name, bool checked);

	/** Every menu: those of the menu bar, then the submenus and the
	 *  right-click menus of the map tree (menuTreeMap, menuTreeArea,
	 *  menuTreeProject, on a map, an area, the project) */
	QList<Menu> menus() const;
	/** The menus of the menu bar, in their order */
	Q_INVOKABLE QStringList menuBar() const;
	/** The menus for QML: {name, title, items, menuBar} */
	Q_INVOKABLE QVariantList menuList() const;
	Q_INVOKABLE QString menuTitle(const QString& menu) const;
	/** "mainToolBar" (the window's) and "toolBar" (the palette's): action
	 *  names, "" a separator */
	Q_INVOKABLE QStringList toolBar(const QString& toolbar) const;
	QStringList toolBarNames() const;
	/** The title of a toolbar (the palette's has one), empty for none */
	QString toolBarTitle(const QString& toolbar) const;

	/** The action is chosen (a menu, a button, its keys): triggered */
	Q_INVOKABLE void trigger(const QString& name);
	/** The texts in the language chosen now */
	Q_INVOKABLE void retranslate();

signals:
	void triggered(const QString& name);
	/** Enabled or checked changed */
	void stateChanged(const QString& name);
	/** The texts changed language */
	void retranslated();

private:
	struct State {
		bool enabled = true;
		bool checked = false;
	};
	std::vector<State> m_states;
};
