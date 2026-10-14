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

#include "qml_dialog_service.h"
#include "common/logging.h"
#include "qml_host.h"

#include <QCoreApplication>
#include <QQuickWindow>

namespace {

// QQuickDialogButtonBox::StandardButton and ButtonRole (the values of QPlatformDialogHelper)
constexpr int Ok = 0x00000400;
constexpr int Cancel = 0x00400000;
constexpr int Yes = 0x00004000;
constexpr int No = 0x00010000;
constexpr int AcceptRole = 0;
constexpr int RejectRole = 1;
constexpr int DestructiveRole = 2;

QString staleTr(const char* text) {
	return QCoreApplication::translate("StaleFile", text);
}

} // namespace

QmlDialogService::QmlDialogService(QWindow* over) : m_over(over) {}

QString QmlDialogService::ask(const QString& kind, const QString& title, const QString& text, int standard, const QVariantList& extra) {
	QString answer;
	QmlHost::exec(QStringLiteral("MessageWindow"), m_over, {
		{QStringLiteral("kind"), kind},
		{QStringLiteral("title"), title},
		{QStringLiteral("text"), text},
		{QStringLiteral("standardButtons"), standard},
		{QStringLiteral("extraButtons"), extra},
	}, [&answer](QQuickWindow* window) {
		answer = window->property("answer").toString();
	});
	return answer;
}

QString QmlDialogService::staleText(const QString& what, const QStringList& conflicts) {
	QString text = staleTr(QT_TRANSLATE_NOOP("StaleFile", "%1 was changed by another program since the Editor read it.\n"
		"Saving now replaces those changes.")).arg(what);
	if (!conflicts.isEmpty()) {
		constexpr int shown = 5;
		QStringList names = conflicts.mid(0, shown);
		if (conflicts.size() > shown) {
			names << staleTr(QT_TRANSLATE_NOOP("StaleFile", "and %1 more")).arg(conflicts.size() - shown);
		}
		text += QLatin1Char('\n') + staleTr(QT_TRANSLATE_NOOP("StaleFile", "Changed on both sides: %1.")).arg(names.join(QStringLiteral(", ")));
	}
	return text;
}

StaleChoice QmlDialogService::askStale(const QString& what, bool can_reload, const QStringList& conflicts) {
	qCWarning(lcReload).noquote() << "asking about a change on disk:" << what << conflicts.join(QStringLiteral(", "));
	QVariantList buttons;
	if (can_reload) {
		buttons.append(QVariantMap{{QStringLiteral("answer"), QStringLiteral("reload")},
			{QStringLiteral("text"), staleTr(QT_TRANSLATE_NOOP("StaleFile", "Reload (Discard Mine)"))}, {QStringLiteral("role"), DestructiveRole}});
	}
	buttons.append(QVariantMap{{QStringLiteral("answer"), QStringLiteral("overwrite")},
		{QStringLiteral("text"), staleTr(QT_TRANSLATE_NOOP("StaleFile", "Save Mine"))}, {QStringLiteral("role"), AcceptRole}});
	const QString answer = ask(QStringLiteral("warning"), staleTr(QT_TRANSLATE_NOOP("StaleFile", "Changed on Disk")), staleText(what, conflicts), Cancel, buttons);
	if (answer == QLatin1String("overwrite")) {
		return StaleChoice::Overwrite;
	}
	return answer == QLatin1String("reload") ? StaleChoice::Reload : StaleChoice::Cancel;
}

DialogService::Answer QmlDialogService::question(const QString& title, const QString& text, Buttons buttons) {
	const bool cancel = buttons == Buttons::YesNoCancel;
	const QString answer = ask(QStringLiteral("question"), title, text, Yes | No | (cancel ? Cancel : 0));
	if (answer == QLatin1String("yes")) {
		return Answer::Yes;
	}
	if (answer == QLatin1String("no")) {
		return Answer::No;
	}
	return cancel ? Answer::Cancel : Answer::No;
}

bool QmlDialogService::confirm(const QString& title, const QString& text, const QString& accept, const QString& reject) {
	const QVariantList buttons = {
		QVariantMap{{QStringLiteral("answer"), QStringLiteral("accept")}, {QStringLiteral("text"), accept}, {QStringLiteral("role"), AcceptRole}},
		QVariantMap{{QStringLiteral("answer"), QStringLiteral("reject")}, {QStringLiteral("text"), reject}, {QStringLiteral("role"), RejectRole}},
	};
	return ask(QStringLiteral("question"), title, text, 0, buttons) == QLatin1String("accept");
}

void QmlDialogService::information(const QString& title, const QString& text) {
	ask(QStringLiteral("information"), title, text, Ok);
}

void QmlDialogService::warning(const QString& title, const QString& text) {
	ask(QStringLiteral("warning"), title, text, Ok);
}

void QmlDialogService::error(const QString& title, const QString& text) {
	ask(QStringLiteral("error"), title, text, Ok);
}
