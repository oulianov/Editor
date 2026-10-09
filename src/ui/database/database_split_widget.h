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

#include <QWidget>
#include <QAction>
#include <QSortFilterProxyModel>
#include <lcf/rpg/database.h>
#include "ui/common/rpg_model.h"
#include "ui_database_split_widget.h"
#include "database_list_data.h"
#include "model/rpg_base.h"
#include "model/rpg_reflect.h"
#include <QAbstractButton>
#include <QLineEdit>
#include <algorithm>
#include <type_traits>
#include "actor_widget.h"
#include "attribute_widget.h"
#include "battle_animation2_widget.h"
#include "battle_animation_widget.h"
#include "chipset_widget.h"
#include "class_widget.h"
#include "common_event_widget.h"
#include "enemy_group_widget.h"
#include "enemy_widget.h"
#include "item_widget.h"
#include "skill_widget.h"
#include "state_widget.h"
#include "terrain_widget.h"
#include "variable_widget.h"
#include "switch_widget.h"

namespace Ui {
class DatabaseSplitWidget;
}

/**
 * The list of a database tab (and of the "..." pickers): filter, Array Size,
 * the Copy/Paste/Clear/Cut/Multiple Copy menu and F4/F5 like RPG Maker.
 * The typed part below does the work on the entries.
 */
class DatabaseSplitWidgetBase : public QWidget
{
	Q_OBJECT

public:
	explicit DatabaseSplitWidgetBase(QWidget *parent = nullptr);
	~DatabaseSplitWidgetBase() override;

	QListView* listWidget() {
		return ui->list;
	}
	/** ID of the entry shown (0: none) */
	int currentId() const {
		return currentRow() + 1;
	}
	/** Shows the entry with this ID; false when there is none or the filter hides it */
	bool selectId(int id) {
		return selectRow(id - 1);
	}

protected:
	/** Shows the model in the list, through the filter; called once by the typed class */
	void setupList(QAbstractItemModel* model);
	/** Puts the page of the entry next to the list, scrolled when the window is lower */
	void addPage(QWidget* page);
	/** Row of the entry shown in the data (-1: none) */
	int currentRow() const;
	bool selectRow(int row);

	virtual int entryCount() const = 0;
	virtual bool entryIsEmpty(int row) const = 0;
	virtual void copyEntries(int row, int count) = 0;
	virtual int clipboardSize() const = 0;
	virtual void pasteEntries(int row) = 0;
	virtual void clearEntry(int row) = 0;
	virtual bool canClear(int /* row */) const {
		return true;
	}
	virtual void resizeEntries(int count) = 0;

	bool eventFilter(QObject* watched, QEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

	Ui::DatabaseSplitWidget *ui;
	QSortFilterProxyModel* m_filter = nullptr;

private:
	void showMenu(const QPoint& pos);
	void step(int delta);
	void changeArraySize();
	void multipleCopy();
	int askNumber(const QString& title, const QString& label, int value, int maximum, const QString& name);

	QAction* m_copy = nullptr;
	QAction* m_paste = nullptr;
	QAction* m_clear = nullptr;
	QAction* m_cut = nullptr;
	QAction* m_multipleCopy = nullptr;
	QAction* m_previous = nullptr;
	QAction* m_next = nullptr;
};

// Qt MOC limitation: Can't mix Q_OBJECT and template
template<class LCF>
class DatabaseSplitWidget : public DatabaseSplitWidgetBase {
public:
	using widget_type = typename RpgReflect<LCF>::widget_type;

	explicit DatabaseSplitWidget(ProjectData& project, QWidget* parent = nullptr);
	DatabaseSplitWidget(ProjectData& project, std::vector<LCF>& data, QWidget* parent = nullptr);

	widget_type* contentWidget() {
		return m_contentWidget;
	}

protected:
	int entryCount() const override {
		return static_cast<int>(m_data.size());
	}
	bool entryIsEmpty(int row) const override {
		return m_data[row] == DatabaseList::defaultEntry<LCF>(m_project, m_data[row].ID);
	}
	void copyEntries(int row, int count) override {
		const int end = std::min(row + count, entryCount());
		DatabaseList::clipboard<LCF>().assign(m_data.begin() + row, m_data.begin() + end);
	}
	int clipboardSize() const override {
		return static_cast<int>(DatabaseList::clipboard<LCF>().size());
	}
	void pasteEntries(int row) override {
		// like RPG Maker: over the entries from the selected one on, keeping their IDs
		const auto& clipboard = DatabaseList::clipboard<LCF>();
		for (int i = 0; i < static_cast<int>(clipboard.size()) && row + i < entryCount(); ++i) {
			const int id = m_data[row + i].ID;
			m_data[row + i] = clipboard[i];
			m_data[row + i].ID = id;
		}
		changed();
	}
	void clearEntry(int row) override {
		m_data[row] = DatabaseList::defaultEntry<LCF>(m_project, m_data[row].ID);
		changed();
	}
	bool canClear(int row) const override {
		// RPG Maker keeps the first condition: the engine gives it at 0 HP
		return !std::is_same_v<LCF, lcf::rpg::State> || row > 0;
	}
	void resizeEntries(int count) override {
		const int id = currentId();
		// resizing can move every entry: release the page's binding first
		m_contentWidget->setData(nullptr);
		m_model->resize(count);
		DatabaseListNotifier::notify(&m_data, this);
		if (!selectId(std::min(id, count)) && m_filter->rowCount() > 0) {
			ui->list->setCurrentIndex(m_filter->index(0, 0));
		}
		showCurrent();
	}

private:
	void showCurrent() {
		const int row = currentRow();
		m_contentWidget->setData(row >= 0 && row < entryCount() ? &m_data[row] : nullptr);
	}
	/** Entries replaced: the views of the list (here, other tabs, combo boxes) and the page show them */
	void changed() {
		DatabaseListNotifier::notify(&m_data, this);
		showCurrent();
	}

	ProjectData& m_project;
	std::vector<LCF>& m_data;
	RpgModel<LCF>* m_model;
	widget_type* m_contentWidget;
};

template<class LCF>
inline DatabaseSplitWidget<LCF>::DatabaseSplitWidget(ProjectData& project, std::vector<LCF>& data, QWidget* parent) :
		DatabaseSplitWidgetBase(parent), m_project(project), m_data(data)
{
	m_contentWidget = new widget_type(m_project, this);
	m_model = new RpgModel<LCF>(project, data, this);
	setupList(m_model);
	addPage(m_contentWidget);
	ui->splitter->setStretchFactor(0, 1);
	ui->splitter->setStretchFactor(1, 4);

	QListView& list = *ui->list;
	connect(list.selectionModel(), &QItemSelectionModel::currentChanged, this, [this] {
		showCurrent();
	});
	// a picker's OK or another window changed the list: the entry may have moved
	connect(&DatabaseListNotifier::instance(), &DatabaseListNotifier::listChanged, this, [this](const void* changedList, QObject* source) {
		if (changedList == &m_data && source != this) {
			showCurrent();
		}
	});

	// The list reads the names and pictures from the data: repaint it after
	// edits (a new name, a graphic chosen with a button...)
	for (auto* edit : m_contentWidget->template findChildren<QLineEdit*>()) {
		connect(edit, &QLineEdit::textEdited, list.viewport(), qOverload<>(&QWidget::update));
		// the other tabs and the combo boxes show the new name once it is typed
		connect(edit, &QLineEdit::editingFinished, this, [this] {
			DatabaseListNotifier::notify(&m_data, this);
		});
	}
	for (auto* button : m_contentWidget->template findChildren<QAbstractButton*>()) {
		connect(button, &QAbstractButton::clicked, list.viewport(), qOverload<>(&QWidget::update));
	}

	// Show the first entry, like RPG Maker, instead of an empty page
	if (list.model()->rowCount() > 0) {
		list.setCurrentIndex(list.model()->index(0, 0));
	}
}

template<class LCF>
inline DatabaseSplitWidget<LCF>::DatabaseSplitWidget(ProjectData& project, QWidget* parent) :
	DatabaseSplitWidget(project, RpgReflect<LCF>::items(project.database()), parent)
{
}
