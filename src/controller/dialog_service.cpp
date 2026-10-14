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

#include "dialog_service.h"
#include <QDebug>

namespace {
DialogService* installed = nullptr;
}

DialogService::~DialogService()
{
	if (installed == this)
		installed = nullptr;
}

DialogService& DialogService::instance()
{
	static DialogService quiet;
	return installed ? *installed : quiet;
}

void DialogService::install(DialogService* service)
{
	installed = service;
}

StaleChoice DialogService::askStale(const QString&, bool, const QStringList&)
{
	// nobody to ask: what the other program wrote is not replaced
	return StaleChoice::Cancel;
}

DialogService::Answer DialogService::question(const QString&, const QString&, Buttons buttons)
{
	return buttons == Buttons::YesNoCancel ? Answer::Cancel : Answer::No;
}

bool DialogService::confirm(const QString&, const QString&, const QString&, const QString&)
{
	// nobody to ask: nothing is done
	return false;
}

void DialogService::information(const QString& title, const QString& text)
{
	qInfo().noquote() << title << ":" << text;
}

void DialogService::warning(const QString& title, const QString& text)
{
	qWarning().noquote() << title << ":" << text;
}

void DialogService::error(const QString& title, const QString& text)
{
	qCritical().noquote() << title << ":" << text;
}
