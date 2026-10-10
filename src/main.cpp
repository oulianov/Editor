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

#include "core.h"

#ifndef EDITOR_VERSION
#define EDITOR_VERSION "dev"
#endif
#include "common/translations.h"
#include "ui/main_window.h"
#include "ui/common/editor_style.h"
#ifdef EDITOR_SCRIPTS
#include "ui/tools/automation.h"
#endif
#include "ui/tools/screenshot.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLibraryInfo>
#include <QLoggingCategory>
#include <QSettings>
#include <QSplashScreen>
#include <QTranslator>
#include <QTimer>
#include <QDebug>
#include <QStyleHints>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	app.setStyle(new EditorStyle());
	// the shortcuts beside the actions of right-click menus too, as on
	// Windows and Linux (macOS hides them by default)
	QCoreApplication::setAttribute(Qt::AA_DontShowShortcutsInContextMenus, false);

	app.setApplicationName("EasyRPG Editor");
	app.setOrganizationName("EasyRPG");
	app.setOrganizationDomain("easyrpg.org");
	app.setApplicationVersion(QStringLiteral(EDITOR_VERSION));

	// a project folder on the command line, else the last opened project
	QCommandLineParser parser;
	parser.setApplicationDescription("EasyRPG Editor");
	parser.addHelpOption();
	parser.addPositionalArgument("project", QCoreApplication::translate("main", "Project folder to open (EasyRPG or RPG Maker 2000/2003)."), "[project]");
	QCommandLineOption verbose_option("verbose",
		QCoreApplication::translate("main", "Log the details of what the Editor does."));
	QCommandLineOption language_option("language",
		QCoreApplication::translate("main", "Language of the Editor (en, fr, de...), instead of the one chosen in the Editor."),
		"code");
	parser.addOption(language_option);
	parser.addOption(verbose_option);
#ifdef EDITOR_SCRIPTS
	// test builds only (EDITOR_SCRIPTS in CMake)
	QCommandLineOption script_option("script",
		QCoreApplication::translate("main", "Run the Editor commands of <file> and quit, for tests and screenshots (see docs/Automation.adoc)."),
		"file");
	parser.addOption(script_option);
#endif
	parser.process(app);

	// The details only on request
	QLoggingCategory::setFilterRules(parser.isSet(verbose_option)
		? QStringLiteral("editor.*.debug=true")
		: QStringLiteral("editor.*.debug=false"));

#ifdef EDITOR_SCRIPTS
	const bool automation = parser.isSet(script_option);
#else
	const bool automation = false;
#endif
	if (automation) {
		// Each script starts from the default settings, in its own file: runs
		// in parallel do not see each other's, nor the user's (last project,
		// window sizes...); only the RTP is the user's
		QVariant rtp = QSettings().value(RTP_KEY);
		const QString settings_dir = QDir::temp().filePath(
			QStringLiteral("easyrpg-editor-script-%1").arg(QCoreApplication::applicationPid()));
		QDir(settings_dir).removeRecursively();
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir);
		QSettings().setValue(RTP_KEY, rtp);
	}

	// show splash
	QPixmap logo(":/app/splash.png");
	QSplashScreen s(logo, Qt::WindowStaysOnTopHint);

	auto hints = app.styleHints();
	auto scheme_changed = [=]() {
		if (hints->colorScheme() == Qt::ColorScheme::Light) {
			QIcon::setThemeName("ezbright");
		} else {
			QIcon::setThemeName("ezdark");
		}
	};
	scheme_changed();
	QObject::connect(hints, &QStyleHints::colorSchemeChanged, scheme_changed);

	s.showMessage("EasyRPG Editor");
	//s.show();

#ifdef NDEBUG
	// close splash after 3 seconds for release
	QTimer::singleShot(3000, &s, &QWidget::close);
#endif

	// load translations: the language chosen in the Editor, else the system's
	s.showMessage("Loading translations...");
	app.processEvents();
	QString language = parser.value(language_option);
	if (language.isEmpty()) {
		// scripts find widgets by their text: the same language everywhere
		language = automation ? QStringLiteral("en") : QSettings().value(LANGUAGE_KEY).toString();
	}
	Translations::apply(language);

	// main window and project
	s.showMessage("Loading main window...");
	app.processEvents();
	MainWindow w;
	if (automation) {
		// Clean up after the window has saved its workspace on aboutToQuit.
		QObject::connect(&app, &QCoreApplication::aboutToQuit, [] {
			QDir(QDir::temp().filePath(QStringLiteral("easyrpg-editor-script-%1")
				.arg(QCoreApplication::applicationPid()))).removeRecursively();
		});
	}

	const QStringList args = parser.positionalArguments();
	if (!args.isEmpty()) {
		s.showMessage("Loading project...");
		app.processEvents();
		w.LoadProject(QDir(args.first()).absolutePath());
	} else if (!automation) {
		s.showMessage("Loading last project...");
		app.processEvents();
		w.LoadLastProject();
	}
	s.clearMessage();
	w.show();

#ifdef EDITOR_SCRIPTS
	if (automation) {
		Automation script(w, parser.value(script_option));
		if (!script.isValid()) {
			qCritical().noquote() << "Cannot read the script" << parser.value(script_option);
			return 1;
		}
		script.start();
		return app.exec();
	}
#endif
	Screenshot::installShortcut();
#ifndef NDEBUG
	// close splash immediately in debug
	s.finish(&w);
#endif

	// into event loop
	return app.exec();
}
