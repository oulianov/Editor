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

#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QAbstractButton>
#include <QAbstractItemModel>
#include <type_traits>
#include <lcf/rpg/fwd.h>
#include "widget_as_dialog_wrapper.h"
#include "ui/database/database_split_widget.h"

/**
 * The list of a database type in a window (the "..." of the combo boxes):
 * edits a copy of the project, OK and Apply write it back and return the
 * ID of the entry selected with valueSelected.
 */
template<typename LCF>
class WidgetAsDialogModelWrapper : public WidgetAsDialogWrapperBase {

public:
	/** Opens on the entry with this ID (else the first one: "none" was selected) */
	explicit WidgetAsDialogModelWrapper(ProjectData& project, int id, QWidget* parent) :
			WidgetAsDialogWrapperBase(parent), dataOriginal(project), dataCopy(project) {
		wrappedWidget = new DatabaseSplitWidget<LCF>(dataCopy, this);
		if (!wrappedWidget->selectId(id)) {
			wrappedWidget->selectId(1);
		}

		if constexpr (std::is_same_v<LCF, lcf::rpg::Switch>) {
			setWindowTitle(tr("Switches"));
		} else if constexpr (std::is_same_v<LCF, lcf::rpg::Variable>) {
			setWindowTitle(tr("Variables"));
		} else {
			setWindowTitle(tr("Database"));
		}

		auto* verticalLayout = new QVBoxLayout(this);
		verticalLayout->addWidget(wrappedWidget);
		verticalLayout->addWidget(buttonBox);
		setLayout(verticalLayout);
	}
	~WidgetAsDialogModelWrapper() override {
		// before the copy it shows goes away
		delete wrappedWidget;
	}
	DatabaseSplitWidget<LCF>* widget() const {
		return wrappedWidget;
	}

private:
	void apply() override {
		// the other windows showing these lists refresh
		DatabaseListNotifier::assign(dataOriginal, dataCopy, this);
		emit valueSelected(wrappedWidget->currentId());
	}

private:
	DatabaseSplitWidget<LCF>* wrappedWidget = nullptr;

	ProjectData& dataOriginal;
	ProjectData dataCopy;
};
