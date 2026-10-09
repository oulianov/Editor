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
#include <QComboBox>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QCompleter>
#include <QToolButton>
#include <functional>
#include <vector>
#include <QHBoxLayout>
#include <QMessageBox>
#include "rpg_model.h"
#include "model/rpg_reflect.h"
#include "edit_dialog.h"
#include "ui/common/widget_as_dialog_wrapper.h"
#include "ui/common/widget_as_dialog_model_wrapper.h"
#include "ui/database/database_split_widget.h"

class RpgComboBoxBase : public QWidget {
	Q_OBJECT
public:
	explicit RpgComboBoxBase(QWidget *parent = nullptr) : QWidget(parent) {}

	QComboBox* comboBox() {
		return m_comboBox;
	}

protected:
	QComboBox* m_comboBox;
};

template <typename LCF>
class RpgComboBox : public RpgComboBoxBase
{
public:
	RpgComboBox(QWidget *parent, QAbstractItemModel *model = nullptr);

	// The member "setEditable" is missing on purpose. Don't set it in the UI!

	void setCurrentIndex(int index) {
		m_comboBox->setCurrentIndex(index);
	}

	int currentIndex() const {
		return m_comboBox->currentIndex();
	}

	void addItem(QString s) {
		m_comboBox->addItem(s);
	}

	void setItemText(int index, const QString& text) {
		m_comboBox->setItemText(index, text);
	}

	void makeModel(ProjectData& project) {
		m_project = &project;
		m_model = new RpgModel<LCF>(project, this);

		m_comboBox->setModel(m_model);
		watchModel();
		m_comboBox->setEditable(true);

		QCompleter *comp = new QCompleter(m_model, this);
		comp->setCaseSensitivity(Qt::CaseInsensitive);
		comp->setFilterMode(Qt::MatchContains);
		comp->setCompletionMode(QCompleter::PopupCompletion);
		m_comboBox->setCompleter(comp);
	}

	void setFilter(QSortFilterProxyModel* filter) {
		filter->setParent(this);
		filter->setSourceModel(m_model);
		m_comboBox->setModel(filter);
		watchModel();
		filter->invalidate();
	}

private:
	// Another tab resizes the list (Array Size, a picker's OK): Qt moves the
	// current row and the binding would write it into the data. The entry
	// shown stays (none when it was removed) and nothing is written.
	void watchModel() {
		for (const auto& connection : m_watch) {
			QObject::disconnect(connection);
		}
		auto hold = [this] {
			if (m_held++ == 0) {
				m_heldId = m_comboBox->currentData();
				m_heldBlocked = m_comboBox->blockSignals(true);
			}
		};
		auto release = [this] {
			if (--m_held == 0) {
				m_comboBox->setCurrentIndex(m_heldId.isValid() ? m_comboBox->findData(m_heldId) : -1);
				m_comboBox->blockSignals(m_heldBlocked);
			}
		};
		auto* model = m_comboBox->model();
		m_watch = {
			QObject::connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, hold),
			QObject::connect(model, &QAbstractItemModel::rowsInserted, this, release),
			QObject::connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, hold),
			QObject::connect(model, &QAbstractItemModel::rowsRemoved, this, release),
			QObject::connect(model, &QAbstractItemModel::modelAboutToBeReset, this, hold),
			QObject::connect(model, &QAbstractItemModel::modelReset, this, release),
		};
	}

	QToolButton* m_editButton;
	ProjectData* m_project = nullptr;
	RpgModel<LCF>* m_model = nullptr;
	std::vector<QMetaObject::Connection> m_watch;
	int m_held = 0;
	bool m_heldBlocked = false;
	QVariant m_heldId;
};

template <class LCF>
RpgComboBox<LCF>::RpgComboBox(QWidget *parent, QAbstractItemModel *model) :
	RpgComboBoxBase(parent)
{
	m_comboBox = new QComboBox(this);
	// a tool button: as tall as the combo box in every style (a push button
	// has a large bezel on macOS)
	m_editButton = new QToolButton(this);
	m_editButton->setObjectName(QStringLiteral("editButton"));
	m_editButton->setText(QStringLiteral("..."));
	m_editButton->setToolTip(QObject::tr("Edit in the database"));

	// No margins: it sits in other layouts like a plain combo box
	auto* layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	layout->addWidget(m_comboBox, 1);
	layout->addWidget(m_editButton);

	// Long database names are elided instead of widening the whole dialog
	m_comboBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	m_comboBox->setMinimumContentsLength(14);
	m_comboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_editButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	if (model) {
		m_comboBox->setModel(model);
		watchModel();
	}

	QObject::connect(m_editButton, &QToolButton::clicked, this, [this] {
		if (!m_project) {
			return;
		}
		// "none" (0) opens the list on its first entry.
		// FIXME: This copies the entire project, kinda slow
		WidgetAsDialogModelWrapper<LCF> dialog(*m_project, m_comboBox->currentData().toInt(), this);
		QObject::connect(&dialog, &WidgetAsDialogWrapperBase::valueSelected, this, [this](int id) {
			// a filtered combo box (equipment) keeps its entry when the one chosen does not fit
			const int index = m_comboBox->findData(id);
			if (index >= 0) {
				m_comboBox->setCurrentIndex(index);
			}
		});
		dialog.exec();
	});
}

using ActorRpgComboBox = RpgComboBox<lcf::rpg::Actor>;
using SkillRpgComboBox = RpgComboBox<lcf::rpg::Skill>;
using ItemRpgComboBox = RpgComboBox<lcf::rpg::Item>;
using EnemyRpgComboBox = RpgComboBox<lcf::rpg::Enemy>;
using TroopRpgComboBox = RpgComboBox<lcf::rpg::Troop>;
using TerrainRpgComboBox = RpgComboBox<lcf::rpg::Terrain>;
using AttributeRpgComboBox = RpgComboBox<lcf::rpg::Attribute>;
using StateRpgComboBox = RpgComboBox<lcf::rpg::State>;
using AnimationRpgComboBox = RpgComboBox<lcf::rpg::Animation>;
using ChipsetRpgComboBox = RpgComboBox<lcf::rpg::Chipset>;
using CommonEventRpgComboBox = RpgComboBox<lcf::rpg::CommonEvent>;
using ClassRpgComboBox = RpgComboBox<lcf::rpg::Class>;
using BattlerAnimationRpgComboBox = RpgComboBox<lcf::rpg::BattlerAnimation>;
using SwitchRpgComboBox = RpgComboBox<lcf::rpg::Switch>;
using VariableRpgComboBox = RpgComboBox<lcf::rpg::Variable>;

