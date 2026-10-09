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
#include "ui/main_window.h"
#include "ui/common/editor_style.h"
#include "ui/tools/automation.h"
#include "ui/tools/screenshot.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLoggingCategory>
#include <QSettings>
#include <QSplashScreen>
#include <QTranslator>
#include <QTimer>
#include <QDebug>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	app.setStyle(new EditorStyle());

	app.setApplicationName("EasyRPG Editor");
	app.setOrganizationName("EasyRPG");
	app.setOrganizationDomain("easyrpg.org");

	// a project folder on the command line, else the last opened project
	QCommandLineParser parser;
	parser.setApplicationDescription("EasyRPG Editor");
	parser.addHelpOption();
	parser.addPositionalArgument("project", QCoreApplication::translate("main", "Project folder to open (EasyRPG or RPG Maker 2000/2003)."), "[project]");
	QCommandLineOption script_option("script",
		QCoreApplication::translate("main", "Run the Editor commands of <file> and quit, for tests and screenshots (see docs/Automation.adoc)."),
		"file");
	QCommandLineOption verbose_option("verbose",
		QCoreApplication::translate("main", "Log the details of what the Editor does."));
	parser.addOption(script_option);
	parser.addOption(verbose_option);
	parser.process(app);

	// The details only on request
	QLoggingCategory::setFilterRules(parser.isSet(verbose_option)
		? QStringLiteral("editor.*.debug=true")
		: QStringLiteral("editor.*.debug=false"));

	const bool automation = parser.isSet(script_option);
	if (automation) {
		// Keep the settings of scripts apart from the user's (last project,
		// window sizes...), but use the same RTP
		QVariant rtp = QSettings().value(RTP_KEY);
		app.setApplicationName("EasyRPG Editor Automation");
		QSettings().setValue(RTP_KEY, rtp);
	}

	// show splash
	QPixmap logo(":/app/splash.png");
	QSplashScreen s(logo, Qt::WindowStaysOnTopHint);

	// Style setup
	bool force_dark = false;

	// Kirigami only loads a custom style when using a static build
	// Lets wait for upstream to improve this
#if defined(KIRIGAMI_STATIC)
	// Default to org.kde.breeze style (from qqc2-breeze style)
	if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
		const char* fstyle = "KIRIGAMI_FORCE_STYLE";
		if (qEnvironmentVariableIsEmpty(fstyle) || qEnvironmentVariableIntValue(fstyle) > 0) {
			// Built in EasyRPG Style (Theme.qml)
			qputenv("KIRIGAMI_FORCE_STYLE", "1");
			force_dark = true;
		}
		QQuickStyle::setStyle(QStringLiteral("org.kde.breeze"));
	}
#endif

	auto hints = app.styleHints();
	auto scheme_changed = [=]() {
		if (!force_dark && hints->colorScheme() == Qt::ColorScheme::Light) {
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

	// setup qml engine
	//auto engine = core().qmlEngine();

	/*engine->loadFromModule("org.easyrpg.editor", "MainWindow");
	if (engine->rootObjects().isEmpty()) {
		return -1;
	}*/

	// load translations
	s.showMessage("Loading translations...");
	app.processEvents();
	QTranslator t;
	bool found = false;
#ifndef NDEBUG
	// allow overwriting in debug builds
	QString e = getenv("EASYRPG_EDITOR_LANG_FILE");
	if (!e.isEmpty())
		found = t.load(e);
#endif
	if (!found)
		found = t.load(QLocale(), QLatin1String("easyrpg-editor"), QLatin1String("_"), QLatin1String(":/i18n"));
	if (found)
		app.installTranslator(&t);
	else
		qDebug() << "No translation(s) available.";

	// main window and project
	s.showMessage("Loading main window...");
	app.processEvents();
	MainWindow w;

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

	if (automation) {
		Automation script(w, parser.value(script_option));
		if (!script.isValid()) {
			qCritical().noquote() << "Cannot read the script" << parser.value(script_option);
			return 1;
		}
		script.start();
		return app.exec();
	}
	Screenshot::installShortcut();
#ifndef NDEBUG
	// close splash immediately in debug
	s.finish(&w);
#endif

	// into event loop
	return app.exec();
}
