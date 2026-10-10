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

#include "database_split_widget.h"
#include "ui_database_split_widget.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include "ui/common/edit_actions.h"
#include <QMessageBox>
#include "ui/common/page_scroll_area.h"
#include <QScrollBar>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {
/** The pictures of the lines (faces, monsters...) take at most a tenth of
 *  the width of the list: a narrow list keeps the room for the names */
class ListPictureDelegate : public QStyledItemDelegate {
public:
	explicit ListPictureDelegate(QListView* list) : QStyledItemDelegate(list), m_list(list) {
		list->installEventFilter(this);
	}

protected:
	void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
		QStyledItemDelegate::initStyleOption(option, index);
		const int width = std::max(1, m_list->width() / 10);
		if (option->decorationSize.width() > width) {
			option->decorationSize = option->decorationSize.scaled(width, option->decorationSize.height(), Qt::KeepAspectRatio);
		}
	}
	bool eventFilter(QObject* watched, QEvent* event) override {
		// the lines take their new height (the same for all of them)
		if (watched == m_list && event->type() == QEvent::Resize) {
			emit sizeHintChanged(QModelIndex());
		}
		return QStyledItemDelegate::eventFilter(watched, event);
	}

private:
	QListView* m_list;
};

}

DatabaseSplitWidgetBase::DatabaseSplitWidgetBase(QWidget* parent) :
	QWidget(parent),
	ui(new Ui::DatabaseSplitWidget)
{
	ui->setupUi(this);
}

DatabaseSplitWidgetBase::~DatabaseSplitWidgetBase()
{
	delete ui;
}

void DatabaseSplitWidgetBase::addPage(QWidget* page) {
	ui->splitter->addWidget(new PageScrollArea(page));
}

void DatabaseSplitWidgetBase::setupList(QAbstractItemModel* model) {
	QListView& list = *ui->list;
	m_filter = new QSortFilterProxyModel(this);
	m_filter->setSourceModel(model);
	m_filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
	// renaming the entry shown must not hide it while typing
	m_filter->setDynamicSortFilter(false);
	list.setModel(m_filter);
	// all the lines have the same height: the list does not ask every line
	// for its size when it scrolls
	list.setUniformItemSizes(true);
	list.setItemDelegate(new ListPictureDelegate(&list));
	// a narrow list cuts the end of "0001: Name"
	list.setTextElideMode(Qt::ElideRight);
	list.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	// wide enough for the ID and the start of a name (and its small picture)
	list.setMinimumWidth(list.fontMetrics().horizontalAdvance(QStringLiteral("0001: Abc\u2026")) * 10 / 9
		+ list.verticalScrollBar()->sizeHint().width() + 2 * list.frameWidth() + 8);

	// the lines show "0001: Name": the filter finds names and IDs
	ui->listFilter->setClearButtonEnabled(true);
	connect(ui->listFilter, &QLineEdit::textChanged, this, [this](const QString& text) {
		const int row = currentRow();
		m_filter->setFilterFixedString(text.trimmed());
		// keep the entry shown when it matches, else show the first match
		if (!selectRow(row) && m_filter->rowCount() > 0) {
			ui->list->setCurrentIndex(m_filter->index(0, 0));
		}
	});
	connect(ui->pushMax, &QPushButton::clicked, this, &DatabaseSplitWidgetBase::changeArraySize);

	// RPG Maker's list menu; the shortcuts work in the list only: the pages
	// have their own copy and paste (event commands, text fields)
	auto addListAction = [this, &list](const char* name, auto kind, auto slot) {
		QAction* action;
		if constexpr (std::is_same_v<decltype(kind), EditActions::Kind>) {
			action = EditActions::create(kind, &list);
		} else {
			action = EditActions::create(&list, kind);
		}
		action->setObjectName(QLatin1String(name));
		connect(action, &QAction::triggered, this, slot);
		return action;
	};
	using EditActions::Kind;
	m_copy = addListAction("actionDataCopy", Kind::Copy, [this] {
		if (currentRow() >= 0) {
			copyEntries(currentRow(), 1);
		}
	});
	m_paste = addListAction("actionDataPaste", Kind::Paste, [this] {
		if (currentRow() >= 0 && clipboardSize() > 0) {
			pasteEntries(currentRow());
		}
	});
	m_clear = addListAction("actionDataClear", Kind::Clear, [this] {
		if (currentRow() >= 0 && canClear(currentRow())) {
			clearEntry(currentRow());
		}
	});
	m_cut = addListAction("actionDataCut", Kind::Cut, [this] {
		if (currentRow() >= 0 && canClear(currentRow())) {
			copyEntries(currentRow(), 1);
			clearEntry(currentRow());
		}
	});
	m_multipleCopy = addListAction("actionDataMultiCopy", tr("Multiple Copy..."), [this] {
		multipleCopy();
	});
	list.setContextMenuPolicy(Qt::CustomContextMenu);
	connect(&list, &QListView::customContextMenuRequested, this, &DatabaseSplitWidgetBase::showMenu);

	// F4/F5 (and Ctrl+F4/Ctrl+F5, the same in RPG Maker): previous and next
	// entry, from anywhere in the page
	auto addStep = [this](QKeySequence key, QKeySequence ctrlKey, int delta) {
		auto* action = new QAction(this);
		action->setShortcuts({key, ctrlKey});
		action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		addAction(action);
		connect(action, &QAction::triggered, this, [this, delta] {
			step(delta);
		});
		return action;
	};
	m_previous = addStep(QKeySequence(Qt::Key_F4), QKeySequence(Qt::CTRL | Qt::Key_F4), -1);
	m_next = addStep(QKeySequence(Qt::Key_F5), QKeySequence(Qt::CTRL | Qt::Key_F5), 1);
	list.installEventFilter(this);
}

// Shortcuts only work in the active window: keys sent while the Editor is in
// the background (scripts) reach the widgets, and these handle them the same.
static bool triggerFor(QKeyEvent* event, std::initializer_list<QAction*> actions) {
	const QKeySequence key(event->keyCombination());
	for (auto* action : actions) {
		if (action->isEnabled() && action->shortcuts().contains(key)) {
			action->trigger();
			return true;
		}
	}
	return false;
}

bool DatabaseSplitWidgetBase::eventFilter(QObject* watched, QEvent* event) {
	if (watched == ui->list && event->type() == QEvent::KeyPress
			&& triggerFor(static_cast<QKeyEvent*>(event), {m_copy, m_paste, m_clear, m_cut})) {
		return true;
	}
	return QWidget::eventFilter(watched, event);
}

void DatabaseSplitWidgetBase::keyPressEvent(QKeyEvent* event) {
	// F4/F5 the widgets of the page leave to it
	if (!triggerFor(event, {m_previous, m_next})) {
		QWidget::keyPressEvent(event);
	}
}

int DatabaseSplitWidgetBase::currentRow() const {
	const QModelIndex index = m_filter->mapToSource(ui->list->currentIndex());
	return index.isValid() ? index.row() : -1;
}

bool DatabaseSplitWidgetBase::selectRow(int row) {
	if (row < 0 || row >= m_filter->sourceModel()->rowCount()) {
		return false;
	}
	const QModelIndex index = m_filter->mapFromSource(m_filter->sourceModel()->index(row, 0));
	if (!index.isValid()) {
		return false;
	}
	ui->list->setCurrentIndex(index);
	ui->list->scrollTo(index);
	return true;
}

void DatabaseSplitWidgetBase::showMenu(const QPoint& pos) {
	const QModelIndex index = ui->list->indexAt(pos);
	if (!index.isValid()) {
		return;
	}
	const int row = m_filter->mapToSource(index).row();
	m_paste->setEnabled(clipboardSize() > 0);
	m_clear->setEnabled(canClear(row));
	m_cut->setEnabled(canClear(row));

	// RPG Maker's order
	QMenu menu(this);
	menu.addActions({m_copy, m_paste, m_clear, m_cut});
	menu.addSeparator();
	menu.addAction(m_multipleCopy);
	EditActions::exec(menu, ui->list, pos);
}

void DatabaseSplitWidgetBase::step(int delta) {
	const int rows = m_filter->rowCount();
	if (rows == 0) {
		return;
	}
	const int row = std::clamp(ui->list->currentIndex().row() + delta, 0, rows - 1);
	ui->list->setCurrentIndex(m_filter->index(row, 0));
	ui->list->scrollTo(ui->list->currentIndex());
}

void DatabaseSplitWidgetBase::changeArraySize() {
	const int rows = entryCount();
	// RPG Maker allows 5000 entries (larger lists made by patches stay possible)
	const int count = askNumber(tr("Array Size"), tr("Array Size"), rows, std::max(5000, rows), QStringLiteral("spinArraySize"));
	if (count < 1 || count == rows) {
		return;
	}
	if (count < rows) {
		// only the entries holding data are worth a warning
		QStringList names;
		for (int row = count; row < rows; ++row) {
			if (!entryIsEmpty(row)) {
				names << m_filter->sourceModel()->index(row, 0).data().toString();
			}
		}
		if (!names.isEmpty()) {
			const int shown = 15;
			QString text = tr("Reducing the array size from %1 to %2 deletes %3 entries, %n of them with data:", "", names.size())
				.arg(rows).arg(count).arg(rows - count);
			text += QStringLiteral("\n\n") + names.mid(0, shown).join('\n');
			if (names.size() > shown) {
				text += '\n' + tr("and %n more", "", names.size() - shown);
			}
			if (QMessageBox::warning(this, tr("Array Size"), text, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Ok) {
				return;
			}
		}
	}
	resizeEntries(count);
}

void DatabaseSplitWidgetBase::multipleCopy() {
	const int row = currentRow();
	if (row < 0) {
		return;
	}
	// RPG Maker copies this many entries from the selected one on; Paste puts them back from the entry selected then
	const int count = askNumber(tr("Multiple Copy"), tr("No. of Items"), 1, entryCount() - row, QStringLiteral("spinMultipleCopy"));
	if (count > 0) {
		copyEntries(row, count);
	}
}

int DatabaseSplitWidgetBase::askNumber(const QString& title, const QString& label, int value, int maximum, const QString& name) {
	// RPG Maker's Array Size and Multiple Copy windows: one number in a group
	QDialog dialog(this);
	dialog.setWindowTitle(title);
	auto* layout = new QVBoxLayout(&dialog);
	auto* group = new QGroupBox(label, &dialog);
	auto* groupLayout = new QHBoxLayout(group);
	auto* spin = new QSpinBox(group);
	spin->setObjectName(name);
	spin->setRange(1, maximum);
	spin->setValue(value);
	groupLayout->addWidget(spin);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(group);
	layout->addWidget(buttons);
	spin->selectAll();
	spin->setFocus();
	return dialog.exec() == QDialog::Accepted ? spin->value() : -1;
}
