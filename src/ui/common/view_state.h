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

#include <QVariantMap>

class QWidget;
class QDialog;

namespace ViewState {

/** Selection and scroll positions within one dialog page. */
QVariantMap capture(QWidget* page);
/** Restore after the page's layout has settled. Does not change any game data. */
void restore(QWidget* page, const QVariantMap& state);

/** Structural layout of a window: docks, splitters, tabs and table/tree headers.
 * Widgets are discovered automatically; edited values are never included. */
QVariantMap captureLayout(QWidget* window);
void restoreLayout(QWidget* window, const QVariantMap& state);

/** Remember Editor dialog geometry and layout after the first show. */
void rememberDialog(QDialog* dialog);
/** Reset tabs, selections and scrolling without changing the edited data. */
void reset(QWidget* page);

}
