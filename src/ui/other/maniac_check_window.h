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

#include <QString>
#include "model/maniac_compat.h"

class QWindow;

/**
 * The windows of the Maniac Patch check (ManiacCheckWindow.qml), over a
 * window: what a game uses that the Maniac Patch does not run, and what the
 * conversion did. The conversion itself is the caller's. The same window
 * lists what a game uses from the add-ons Project Settings turns off.
 */
namespace ManiacCheckWindow {
	/** The findings of a check, with Yes and No (OK alone when nothing
	 *  converts): true when Yes converts. backup: the folder the files go to */
	bool ask(QWindow* over, const ManiacCompat::Report& report, const QString& backup);
	/** What the conversion did: error empty when it worked; the uses
	 *  converted, those that stay; the folders of the files as they were */
	void result(QWindow* over, const QString& error, int converted, int remaining, const QString& backup,
		const QString& backup2000);
	/** The findings of a check of add-ons turned off (ManiacCompat::uses),
	 *  with Keep It Off and Go Back: true keeps them off */
	bool askTurnedOff(QWindow* over, const ManiacCompat::Report& report);
}
