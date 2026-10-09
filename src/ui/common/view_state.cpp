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

#include "view_state.h"
#include "common/editor_settings.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QHash>
#include <QHeaderView>
#include <QLayout>
#include <QMainWindow>
#include <QScrollBar>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>
#include <algorithm>

namespace {

// Object paths distinguish repeated controls (for example each database tab's
// "splitter") without depending on translated labels or a list of controls.
QString layoutKey(QWidget* root, QWidget* widget) {
	QStringList path;
	for (QWidget* current = widget; current && current != root; current = current->parentWidget()) {
		// Floating docks still belong to the main layout; child dialogs and
		// combo-box popups have their own state and must not be captured here.
		if (qobject_cast<QDialog*>(current) || current->windowType() == Qt::Popup) return {};
		int occurrence = 0;
		if (auto* stack = qobject_cast<QStackedWidget*>(current->parentWidget())) occurrence = stack->indexOf(current);
		else if (auto* splitter = qobject_cast<QSplitter*>(current->parentWidget())) occurrence = splitter->indexOf(current);
		else if (auto* header = qobject_cast<QHeaderView*>(current)) occurrence = header->orientation() == Qt::Horizontal ? 0 : 1;
		else if (current->parentWidget()) {
			for (QObject* sibling : current->parentWidget()->children()) {
				if (sibling == current) break;
				if (sibling->metaObject() == current->metaObject() && sibling->objectName() == current->objectName()) ++occurrence;
			}
		}
		path.prepend(QStringLiteral("%1:%2[%3]").arg(QString::fromLatin1(current->metaObject()->className()), current->objectName()).arg(occurrence));
	}
	return path.join(QLatin1Char('/'));
}

class SplitterState final : public QObject {
public:
	explicit SplitterState(QSplitter* splitter) : QObject(splitter) { splitter->installEventFilter(this); }

protected:
	bool eventFilter(QObject* watched, QEvent* event) override {
		auto* splitter = static_cast<QSplitter*>(watched);
		if (event->type() == QEvent::Show && splitter->property("layout_restore").isValid()) {
			const int revision = qApp->property("local_state_revision").toInt();
			QTimer::singleShot(0, this, [splitter, revision] {
				if (revision != qApp->property("local_state_revision").toInt() || !splitter->isVisible()) return;
				if (!splitter->property("layout_default").isValid()) splitter->setProperty("layout_default", splitter->saveState());
				const QByteArray state = splitter->property(splitter->property("layout_restore_default").toBool()
					? "layout_default" : "layout_restore").toByteArray();
				splitter->restoreState(state);
				splitter->setProperty("layout_restore", QVariant());
			});
		}
		return false;
	}
};

class DialogState final : public QObject {
public:
	explicit DialogState(QDialog* dialog, const QString& key) : QObject(dialog), m_dialog(dialog), m_key(key) {
		dialog->installEventFilter(this);
		connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { save(); });
		connect(&EditorSettings::instance(), &EditorSettings::localStateReset, this, [this, dialog] {
			m_restoring = false;
			ViewState::reset(dialog);
			if (m_defaultSize.isValid()) {
				if (dialog->isVisible() && (dialog->isMaximized() || dialog->isMinimized())) dialog->showNormal();
				dialog->resize(m_defaultSize);
				const QRect parent = dialog->parentWidget() ? dialog->parentWidget()->window()->frameGeometry()
					: dialog->screen()->availableGeometry();
				dialog->move(parent.center() - QPoint(dialog->frameGeometry().width() / 2, dialog->frameGeometry().height() / 2));
				ViewState::restoreLayout(dialog, m_defaultLayout);
			}
		});
	}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override {
		auto* dialog = qobject_cast<QDialog*>(watched);
		if (!dialog || qApp->property("local_state_resetting").toBool()) return false;
		if (event->type() == QEvent::Show && !m_restored) {
			m_restored = true;
			m_restoring = true;
			m_defaultSize = dialog->size();
			const QVariantMap state = QSettings().value(m_key).toMap();
			QSize size;
			if (dialog->restoreGeometry(state.value(QStringLiteral("geometry")).toByteArray())) size = dialog->size();
			else size = QSettings().value(QString(m_key).replace(QStringLiteral("window_states/"), QStringLiteral("dialog_sizes/"))).toSize();
			if (!size.isValid()) {
				const QString legacyKey = dialog->property("dialog_geometry_key").toString();
				if (!legacyKey.isEmpty() && dialog->restoreGeometry(QSettings().value(legacyKey).toByteArray())) size = dialog->size();
			}
			if (size.isValid()) {
				if (const QScreen* screen = dialog->screen()) {
					const QSize frame = dialog->frameGeometry().size() - dialog->size();
					size = size.boundedTo(screen->availableGeometry().size() - frame);
				}
				dialog->resize(size);
			}
			const int revision = qApp->property("local_state_revision").toInt();
			// Child show handlers establish their default split sizes. Restore
			// only after them and the outer window geometry have settled.
			QTimer::singleShot(0, this, [this, dialog, state, revision] {
				if (revision != qApp->property("local_state_revision").toInt()) return;
				m_defaultLayout = ViewState::captureLayout(dialog);
				ViewState::restoreLayout(dialog, state.value(QStringLiteral("layout")).toMap());
				m_restoring = false;
			});
		} else if (m_restored && (event->type() == QEvent::Hide ||
			((event->type() == QEvent::Resize || event->type() == QEvent::Move) && dialog->isVisible()))) {
			save();
		}
		return false;
	}

private:
	void save() {
		if (!m_restored || m_restoring || qApp->property("local_state_resetting").toBool()) return;
		QSettings().setValue(m_key, QVariantMap{{QStringLiteral("geometry"), m_dialog->saveGeometry()},
			{QStringLiteral("layout"), ViewState::captureLayout(m_dialog)}});
	}
	QDialog* m_dialog;
	QString m_key;
	QSize m_defaultSize;
	QVariantMap m_defaultLayout;
	bool m_restored = false;
	bool m_restoring = false;
};

QList<std::pair<QString, QAbstractScrollArea*>> scrollAreas(QWidget* page) {
	QList<std::pair<QString, QAbstractScrollArea*>> areas;
	QHash<QString, int> occurrences;
	for (auto* area : page->findChildren<QAbstractScrollArea*>()) {
		if (!area->isVisibleTo(page)) continue;
		bool popup = false;
		for (QWidget* parent = area->parentWidget(); parent && parent != page; parent = parent->parentWidget()) {
			if (qobject_cast<QComboBox*>(parent)) popup = true;
		}
		if (popup) continue;
		const QString name = area->objectName().isEmpty() ? QString::fromLatin1(area->metaObject()->className()) : area->objectName();
		areas.append({QStringLiteral("%1/%2").arg(name).arg(occurrences[name]++), area});
	}
	return areas;
}

void restoreSelection(QAbstractItemView* view, const QVariantMap& area) {
	if (!view || !view->model() || !area.contains(QStringLiteral("rows"))) return;
	QModelIndex index;
	for (const auto& row : area.value(QStringLiteral("rows")).toList()) {
		const int count = view->model()->rowCount(index);
		if (count == 0) { index = {}; break; }
		index = view->model()->index(std::clamp(row.toInt(), 0, count - 1), 0, index);
	}
	if (index.isValid()) {
		const int columns = view->model()->columnCount(index.parent());
		index = index.siblingAtColumn(std::clamp(area.value(QStringLiteral("column")).toInt(), 0, std::max(0, columns - 1)));
		view->setCurrentIndex(index);
	}
}

}

QVariantMap ViewState::captureLayout(QWidget* window) {
	QVariantMap splitters, headers, tabs;
	for (auto* splitter : window->findChildren<QSplitter*>()) {
		const QString key = layoutKey(window, splitter);
		if (!key.isEmpty()) splitters.insert(key,
			splitter->property("layout_restore").isValid() ? splitter->property("layout_restore") : QVariant(splitter->saveState()));
	}
	for (auto* header : window->findChildren<QHeaderView*>()) {
		const QString key = layoutKey(window, header);
		if (!key.isEmpty()) headers.insert(key, header->saveState());
	}
	if (!window->property("view_state_contextual").toBool()) {
		for (auto* tab : window->findChildren<QTabWidget*>()) {
			const QString key = layoutKey(window, tab);
			if (!key.isEmpty()) tabs.insert(key, tab->currentIndex());
		}
	}
	QVariantMap state{{QStringLiteral("splitters"), splitters}, {QStringLiteral("headers"), headers}, {QStringLiteral("tabs"), tabs}};
	if (auto* main = qobject_cast<QMainWindow*>(window)) state.insert(QStringLiteral("docks"), main->saveState());
	return state;
}

void ViewState::restoreLayout(QWidget* window, const QVariantMap& state) {
	if (auto* main = qobject_cast<QMainWindow*>(window); main && state.contains(QStringLiteral("docks")))
		main->restoreState(state.value(QStringLiteral("docks")).toByteArray());
	if (!window->property("view_state_contextual").toBool()) {
		const QVariantMap tabs = state.value(QStringLiteral("tabs")).toMap();
		for (auto* tab : window->findChildren<QTabWidget*>()) {
			const QString key = layoutKey(window, tab);
			if (!key.isEmpty() && tabs.contains(key) && tab->count() > 0)
				tab->setCurrentIndex(std::clamp(tabs.value(key).toInt(), 0, tab->count() - 1));
		}
	}
	const QVariantMap splitters = state.value(QStringLiteral("splitters")).toMap();
	for (auto* splitter : window->findChildren<QSplitter*>()) {
		const QString key = layoutKey(window, splitter);
		if (key.isEmpty() || !splitters.contains(key)) continue;
		if (splitter->isVisible()) {
			if (!splitter->property("layout_default").isValid()) splitter->setProperty("layout_default", splitter->saveState());
			splitter->restoreState(qApp->property("local_state_resetting").toBool() ? splitter->property("layout_default").toByteArray() : splitters.value(key).toByteArray());
			splitter->setProperty("layout_restore", QVariant());
		} else {
			// Hidden tabs have not received their final width. Restoring now
			// would clamp the saved panes to their minimums and lose the ratio.
			splitter->setProperty("layout_restore", splitters.value(key));
			splitter->setProperty("layout_restore_default", qApp->property("local_state_resetting"));
			if (!splitter->property("layout_restore_installed").toBool()) {
				splitter->setProperty("layout_restore_installed", true);
				new SplitterState(splitter);
			}
		}
	}
	const QVariantMap headers = state.value(QStringLiteral("headers")).toMap();
	for (auto* header : window->findChildren<QHeaderView*>()) {
		const QString key = layoutKey(window, header);
		if (!key.isEmpty() && headers.contains(key)) {
			if (!header->property("layout_default").isValid()) header->setProperty("layout_default", header->saveState());
			header->restoreState(qApp->property("local_state_resetting").toBool() ? header->property("layout_default").toByteArray() : headers.value(key).toByteArray());
		}
	}
	if (window->layout()) window->layout()->activate();
}

void ViewState::rememberDialog(QDialog* dialog) {
	if (!dialog || dialog->property("dialog_size_state_installed").toBool() ||
		dialog->minimumSize() == dialog->maximumSize()) return;
	QString name = QString::fromLatin1(dialog->metaObject()->className());
	// Native file choosers and Qt's message/input boxes manage their own sizes.
	if (name.startsWith(QLatin1Char('Q')) && (name != QStringLiteral("QDialog") || dialog->objectName().isEmpty())) return;
	if (!dialog->objectName().isEmpty()) name += QLatin1Char('/') + dialog->objectName();
	if (name == QStringLiteral("WidgetAsDialogWrapperBase") && dialog->layout() && dialog->layout()->count() > 0) {
		if (QWidget* content = dialog->layout()->itemAt(0)->widget())
			name += QLatin1Char('/') + QString::fromLatin1(content->metaObject()->className());
	}
	dialog->setProperty("dialog_size_state_installed", true);
	new DialogState(dialog, QStringLiteral("window_states/") + name);
}

void ViewState::reset(QWidget* page) {
	page->setProperty("view_state_pending", false);
	page->setProperty("view_state", QVariant());
	for (auto* tab : page->findChildren<QTabWidget*>()) {
		if (tab->count() > 0) tab->setCurrentIndex(0);
		for (int i = 0; i < tab->count(); ++i) {
			tab->widget(i)->setProperty("view_state_pending", false);
			tab->widget(i)->setProperty("view_state", QVariant());
		}
	}
	// Hidden pages must also start at their first entry when opened later.
	for (auto* area : page->findChildren<QAbstractScrollArea*>()) {
		bool popup = false;
		for (QWidget* parent = area->parentWidget(); parent && parent != page; parent = parent->parentWidget())
			if (qobject_cast<QComboBox*>(parent)) popup = true;
		if (popup) continue;
		if (auto* view = qobject_cast<QAbstractItemView*>(area); view && view->model())
			view->setCurrentIndex(view->model()->index(0, 0));
		area->horizontalScrollBar()->setValue(0);
		area->verticalScrollBar()->setValue(0);
	}
}

QVariantMap ViewState::capture(QWidget* page) {
	if (!page) return {};
	// A page switched again before its first paint still has its saved position.
	if (page->property("view_state_pending").toBool()) return page->property("view_state").toMap();
	QVariantMap state;
	const auto areas = scrollAreas(page);
	for (auto it = areas.begin(); it != areas.end(); ++it) {
		QVariantMap area{{QStringLiteral("x"), it->second->horizontalScrollBar()->value()},
			{QStringLiteral("y"), it->second->verticalScrollBar()->value()}};
		if (auto* view = qobject_cast<QAbstractItemView*>(it->second)) {
			QVariantList rows;
			for (QModelIndex index = view->currentIndex(); index.isValid(); index = index.parent()) rows.prepend(index.row());
			area.insert(QStringLiteral("rows"), rows);
			area.insert(QStringLiteral("column"), view->currentIndex().column());
		}
		state.insert(it->first, area);
	}
	QVariantMap tabs;
	for (auto* tab : page->findChildren<QTabWidget*>()) {
		if (tab->isVisibleTo(page)) tabs.insert(tab->objectName(), tab->currentIndex());
	}
	state.insert(QStringLiteral("tabs"), tabs);
	return state;
}

void ViewState::restore(QWidget* page, const QVariantMap& state) {
	if (!page || state.isEmpty()) return;
	page->setProperty("view_state", state);
	page->setProperty("view_state_pending", true);
	const int revision = qApp->property("local_state_revision").toInt();
	QTimer::singleShot(0, page, [page, state, revision] {
		if (revision != qApp->property("local_state_revision").toInt()) return;
		if (page->property("view_state").toMap() != state) return;
		// Restore entry selections first: they populate the corresponding inspector.
		for (const auto& [key, area] : scrollAreas(page)) {
			bool nestedPage = false;
			for (QWidget* parent = area->parentWidget(); parent && parent != page; parent = parent->parentWidget())
				if (qobject_cast<QTabWidget*>(parent)) nestedPage = true;
			if (!nestedPage) restoreSelection(qobject_cast<QAbstractItemView*>(area), state.value(key).toMap());
		}
		const auto tabs = state.value(QStringLiteral("tabs")).toMap();
		for (auto* tab : page->findChildren<QTabWidget*>()) {
			if (tabs.contains(tab->objectName()) && tab->count() > 0)
				tab->setCurrentIndex(std::clamp(tabs.value(tab->objectName()).toInt(), 0, tab->count() - 1));
		}
		// Choosing an entry can rebuild its battle event pages, so inspect the new widgets.
		const auto areas = scrollAreas(page);
		for (const auto& [key, area] : areas)
			restoreSelection(qobject_cast<QAbstractItemView*>(area), state.value(key).toMap());
		if (page->layout()) page->layout()->activate();
		for (auto it = areas.begin(); it != areas.end(); ++it) {
			if (!state.contains(it->first)) continue;
			if (auto* view = qobject_cast<QAbstractItemView*>(it->second)) view->doItemsLayout();
			const auto area = state.value(it->first).toMap();
			it->second->horizontalScrollBar()->setValue(area.value(QStringLiteral("x")).toInt());
			it->second->verticalScrollBar()->setValue(area.value(QStringLiteral("y")).toInt());
		}
		page->setProperty("view_state_pending", false);
	});
}
