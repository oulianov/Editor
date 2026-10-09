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

#include "translations.h"
#include <QCoreApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

namespace {
	QTranslator* editor_translator = nullptr;
	QTranslator* qt_translator = nullptr;
	QString applied;
}

void Translations::apply(const QString& code) {
	applied = code;
	const QLocale locale = code.isEmpty() ? QLocale::system() : QLocale(code);
	QLocale::setDefault(locale);
	auto remove = [](QTranslator*& translator) {
		if (translator) {
			QCoreApplication::removeTranslator(translator);
			delete translator;
			translator = nullptr;
		}
	};
	remove(editor_translator);
	remove(qt_translator);

	auto* editor = new QTranslator(qApp);
	bool found = false;
	QString chosen;
#ifndef NDEBUG
	// allow overwriting in debug builds
	const QString file = qEnvironmentVariable("EASYRPG_EDITOR_LANG_FILE");
	found = !file.isEmpty() && editor->load(file);
#endif
	// The first of the user's languages that the Editor has. English, the
	// language it is written in, needs no file: it ends the search (an
	// English user whose second language is French gets English)
	for (QString ui_language : locale.uiLanguages()) {
		if (found) {
			break;
		}
		ui_language.replace(QLatin1Char('-'), QLatin1Char('_'));
		if (ui_language == QLatin1String("en") || ui_language.startsWith(QLatin1String("en_"))) {
			break;
		}
		// "fr_FR" falls back to "fr"
		if (editor->load(QLatin1String("easyrpg-editor_") + ui_language, QLatin1String(":/i18n"))) {
			found = true;
			chosen = ui_language;
		}
	}
	if (!found) {
		delete editor;
		qDebug() << "No translation used: English.";
		return;
	}
	editor_translator = editor;
	QCoreApplication::installTranslator(editor);
	// the standard buttons and dialogs of Qt, in the same language
	auto* qt = new QTranslator(qApp);
	if (!chosen.isEmpty() && qt->load(QLatin1String("qtbase_") + chosen,
			QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
		qt_translator = qt;
		QCoreApplication::installTranslator(qt);
	} else {
		delete qt;
	}
}

QString Translations::current() {
	return applied;
}
