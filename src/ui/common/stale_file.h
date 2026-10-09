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

#include "common/logging.h"
#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>

/** What to do with data whose file another program changed since the Editor
 *  read it (rpgsync, a second editor...): saving would replace those changes */
enum class StaleChoice { Overwrite, Reload, Cancel };

/** conflicts: what both sides changed differently (what could not be merged) */
inline StaleChoice askStale(QWidget* parent, const QString& what, bool can_reload = true, const QStringList& conflicts = {}) {
	auto tr = [](const char* text) { return QCoreApplication::translate("StaleFile", text); };
	QString text = tr("%1 was changed by another program since the Editor read it.\n"
		"Saving now replaces those changes.").arg(what);
	if (!conflicts.isEmpty()) {
		constexpr int shown = 5;
		QStringList names = conflicts.mid(0, shown);
		if (conflicts.size() > shown) {
			names << tr("and %1 more").arg(conflicts.size() - shown);
		}
		text += QLatin1Char('\n') + tr("Changed on both sides: %1.").arg(names.join(QStringLiteral(", ")));
	}
	qCWarning(lcReload).noquote() << "asking about a change on disk:" << what << conflicts.join(QStringLiteral(", "));
	QMessageBox box(QMessageBox::Warning, tr("Changed on Disk"), text, QMessageBox::NoButton, parent);
	QPushButton* reload = can_reload ? box.addButton(tr("Reload (Discard Mine)"), QMessageBox::DestructiveRole) : nullptr;
	QPushButton* overwrite = box.addButton(tr("Save Mine"), QMessageBox::AcceptRole);
	QPushButton* cancel = box.addButton(QMessageBox::Cancel);
	box.setDefaultButton(cancel);
	box.exec();
	if (box.clickedButton() == overwrite) {
		return StaleChoice::Overwrite;
	}
	return reload && box.clickedButton() == reload ? StaleChoice::Reload : StaleChoice::Cancel;
}
