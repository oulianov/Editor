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

#include "search_combobox.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

namespace {

/** "0012: Potion": the number, then the name */
const QRegularExpression& entryPattern() {
	static const QRegularExpression pattern(QStringLiteral("^\\s*(\\d+)\\s*:\\s*(.*)$"),
		QRegularExpression::DotMatchesEverythingOption);
	return pattern;
}

/** The digits of a word typed without its leading zeros ("0012": "12"), empty if it is not a number */
QString searchNumber(const QString& word) {
	if (word.isEmpty()) {
		return {};
	}
	for (const QChar c : word) {
		if (c < u'0' || c > u'9') {
			return {};
		}
	}
	QString number = word;
	while (number.size() > 1 && number.startsWith(u'0')) {
		number.remove(0, 1);
	}
	return number;
}

/** The number of an entry: its data (the database lists), else read from its text */
int entryId(const QModelIndex& index) {
	bool ok = false;
	const int id = index.data(Qt::UserRole).toInt(&ok);
	if (ok) {
		return id;
	}
	const auto match = entryPattern().match(index.data().toString());
	return match.hasMatch() ? match.captured(1).toInt() : -1;
}

class SearchFilter : public QSortFilterProxyModel {
public:
	using QSortFilterProxyModel::QSortFilterProxyModel;

	void setSearch(const QString& search) {
		beginFilterChange();
		m_search = search;
		endFilterChange(Direction::Rows);
	}

protected:
	bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
		const QModelIndex index = sourceModel()->index(row, filterKeyColumn(), parent);
		return SearchComboBox::matches(index.data().toString(), entryId(index), m_search);
	}

private:
	QString m_search;
};

} // namespace

/** The pop-up: a search field over the entries that match it */
class SearchComboPopup : public QFrame {
public:
	explicit SearchComboPopup(SearchComboBox* combo) :
		QFrame(nullptr, Qt::Popup), m_combo(combo)
	{
		setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
		// a click on the list closes it without opening it again
		setAttribute(Qt::WA_NoMouseReplay);
		auto* layout = new QVBoxLayout(this);
		layout->setContentsMargins(4, 4, 4, 4);
		layout->setSpacing(4);

		m_field = new QLineEdit(this);
		m_field->setObjectName(QStringLiteral("searchField"));
		m_field->setPlaceholderText(SearchComboBox::tr("Search by number or name"));
		m_field->setClearButtonEnabled(true);
		m_field->installEventFilter(this);

		m_filter = new SearchFilter(this);
		m_list = new QListView(this);
		m_list->setObjectName(QStringLiteral("searchList"));
		m_list->setModel(m_filter);
		// the field keeps the focus: the arrows are sent to the list
		m_list->setFocusPolicy(Qt::NoFocus);
		m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
		m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		// thousands of variables: the size of one row is enough
		m_list->setUniformItemSizes(true);
		m_list->setMouseTracking(true);

		layout->addWidget(m_field);
		layout->addWidget(m_list);

		connect(m_field, &QLineEdit::textChanged, this, [this](const QString& text) {
			search(text);
		});
		connect(m_list, &QListView::clicked, this, [this](const QModelIndex& index) {
			choose(index);
		});
		// the row under the pointer is the one Return chooses, as in a menu
		connect(m_list, &QListView::entered, this, [this](const QModelIndex& index) {
			m_list->setCurrentIndex(index);
		});
	}

	void open(const QString& text, QWidget* anchor) {
		m_anchor = anchor;
		m_filter->setSourceModel(m_combo->model());
		m_filter->setFilterKeyColumn(m_combo->modelColumn());
		m_list->setModelColumn(m_combo->modelColumn());
		m_list->setIconSize(m_combo->iconSize());
		{
			const QSignalBlocker blocker(m_field);
			m_field->setText(QString());
		}
		// the size of the whole list: it stays while searching
		m_filter->setSearch(QString());
		place();
		show();
		m_field->setFocus();
		// the first key typed on the closed list
		if (text.isEmpty()) {
			search(text);
		} else {
			m_field->setText(text);
		}
	}

	/** Keys typed on the list while this opens: they follow the first one */
	void type(const QString& text) {
		m_field->insert(text);
	}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override {
		if (watched == m_field && event->type() == QEvent::KeyPress) {
			switch (static_cast<QKeyEvent*>(event)->key()) {
			case Qt::Key_Up:
			case Qt::Key_Down:
			case Qt::Key_PageUp:
			case Qt::Key_PageDown:
				QCoreApplication::sendEvent(m_list, event);
				return true;
			case Qt::Key_Return:
			case Qt::Key_Enter:
				choose(m_list->currentIndex());
				return true;
			case Qt::Key_Escape:
				hide();
				return true;
			default:
				break;
			}
		}
		return QFrame::eventFilter(watched, event);
	}

private:
	void search(const QString& text) {
		m_filter->setSearch(text);
		QModelIndex best;
		if (text.trimmed().isEmpty()) {
			best = m_filter->mapFromSource(m_combo->model()->index(
				m_combo->currentIndex(), m_combo->modelColumn(), m_combo->rootModelIndex()));
		} else {
			// the entry of the number typed, else the first one
			const QString number = searchNumber(text.trimmed());
			const int id = number.isEmpty() ? -1 : number.toInt();
			for (int row = 0; row < m_filter->rowCount(); ++row) {
				const QModelIndex index = m_filter->index(row, m_combo->modelColumn());
				if (!(index.flags() & Qt::ItemIsEnabled)) {
					continue;
				}
				if (!best.isValid()) {
					best = index;
				}
				if (id >= 0 && entryId(index) == id) {
					best = index;
					break;
				}
			}
		}
		m_list->setCurrentIndex(best);
		if (best.isValid()) {
			m_list->scrollTo(best, text.isEmpty() ? QAbstractItemView::PositionAtCenter : QAbstractItemView::EnsureVisible);
		}
	}

	void choose(const QModelIndex& index) {
		if (!index.isValid() || !(index.flags() & Qt::ItemIsEnabled)) {
			return;
		}
		const int row = m_filter->mapToSource(index).row();
		hide();
		m_combo->setCurrentIndex(row);
		emit m_combo->activated(row);
		emit m_combo->textActivated(m_combo->itemText(row));
	}

	/** Under the list (above it near the bottom of the screen), as wide as its longest entry */
	void place() {
		const QRect screen = m_anchor->screen()->availableGeometry();
		const int rows = qBound(1, m_combo->count(), qMax(m_combo->maxVisibleItems(), 14));
		const int rowHeight = m_filter->rowCount() > 0
			? m_list->sizeHintForRow(0) : m_list->fontMetrics().height() + 4;

		int textWidth = 0;
		const QFontMetrics metrics = m_list->fontMetrics();
		for (int row = 0; row < m_combo->count(); ++row) {
			textWidth = qMax(textWidth, metrics.horizontalAdvance(m_combo->itemText(row)));
		}
		const auto margins = contentsMargins() + layout()->contentsMargins();
		const int extra = margins.left() + margins.right() + 2 * m_list->frameWidth()
			+ m_list->verticalScrollBar()->sizeHint().width() + m_combo->iconSize().width() + 16;
		const int least = qMax(m_anchor->width(), 240);
		const int width = qBound(least, textWidth + extra, qMax(least, screen.width() / 3));
		int height = margins.top() + margins.bottom() + m_field->sizeHint().height()
			+ layout()->spacing() + rows * rowHeight + 2 * m_list->frameWidth();

		const QPoint below = m_anchor->mapToGlobal(QPoint(0, m_anchor->height()));
		const QPoint top = m_anchor->mapToGlobal(QPoint(0, 0));
		int y = below.y();
		if (y + height > screen.bottom() && top.y() - screen.top() > screen.bottom() - below.y()) {
			height = qMin(height, top.y() - screen.top());
			y = top.y() - height;
		} else {
			height = qMin(height, screen.bottom() - y);
		}
		const int x = qBound(screen.left(), top.x(), qMax(screen.left(), screen.right() - width));
		setGeometry(x, y, width, height);
	}

	SearchComboBox* m_combo;
	// the widget it opens under: the list, or a button using it
	QWidget* m_anchor = nullptr;
	QLineEdit* m_field;
	QListView* m_list;
	SearchFilter* m_filter;
};

SearchComboBox::SearchComboBox(QWidget* parent) : QComboBox(parent) {
}

SearchComboBox::~SearchComboBox() {
	delete m_popup;
}

bool SearchComboBox::matches(const QString& text, int id, const QString& search) {
	QString name = text;
	const auto match = entryPattern().match(text);
	if (match.hasMatch()) {
		if (id < 0) {
			id = match.captured(1).toInt();
		}
		name = match.captured(2);
	}
	// only an entry written with its number has one: "Hero" (10001) does not
	const QString number = match.hasMatch() && id >= 0 ? QString::number(id) : QString();
	for (const QString& word : search.split(u' ', Qt::SkipEmptyParts)) {
		const QString digits = searchNumber(word);
		if (!digits.isEmpty() && !number.isEmpty() && number.startsWith(digits)) {
			continue;
		}
		if (!name.contains(word, Qt::CaseInsensitive)) {
			return false;
		}
	}
	return true;
}

void SearchComboBox::showPopup() {
	if (isEditable()) {
		QComboBox::showPopup();
		return;
	}
	openSearch(QString());
}

void SearchComboBox::hidePopup() {
	if (m_popup) {
		m_popup->hide();
	}
	QComboBox::hidePopup();
}

void SearchComboBox::keyPressEvent(QKeyEvent* event) {
	// a letter or a digit typed on the closed list starts a search
	const QString text = event->text();
	const bool shortcut = event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier);
	const bool printable = !isEditable() && !shortcut && !text.isEmpty() && text.at(0).isPrint();
	// (a space opens the list, or separates words once it is open)
	const bool open = m_popup && m_popup->isVisible();
	if (printable && (open || !text.at(0).isSpace())) {
		openSearch(text);
		return;
	}
	QComboBox::keyPressEvent(event);
}

void SearchComboBox::showPopupUnder(QWidget* anchor) {
	if (!m_popup) {
		m_popup = new SearchComboPopup(this);
	}
	m_popup->open(QString(), anchor);
}

void SearchComboBox::openSearch(const QString& search) {
	if (!m_popup) {
		m_popup = new SearchComboPopup(this);
	}
	if (m_popup->isVisible()) {
		m_popup->type(search);
		return;
	}
	m_popup->open(search, this);
}
