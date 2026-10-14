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
#include "common/editor_settings.h"
#include "ui/shell/qml_shell.h"
#include "ui/tools/automation.h"
#include "ui/tools/screenshot.h"
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLibraryInfo>
#include <QLoggingCategory>
#include <QPalette>
#include <QSettings>
#include <QDebug>
#include <QIcon>
#include <QQuickStyle>
#include <QStyleHints>

int main(int argc, char *argv[]) {
	QGuiApplication app(argc, argv);
	// The QML interface: KDE's Breeze style, as upstream (Qt's Fusion when it
	// is missing)
	if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
#if defined(KIRIGAMI_STATIC)
		// a static Kirigami loads a style only when told to
		if (qEnvironmentVariableIsEmpty("KIRIGAMI_FORCE_STYLE")) {
			qputenv("KIRIGAMI_FORCE_STYLE", "1");
		}
#endif
		QQuickStyle::setStyle(QStringLiteral("org.kde.breeze"));
	}
	QQuickStyle::setFallbackStyle(QStringLiteral("Fusion"));
#if defined(EDITOR_KDE_PLUGIN_DIR)
	// the Kirigami platform plugin of Breeze, outside Qt's folders: a build
	// that runs from its build folder (a packaged one has it with Qt's plugins)
	if (!QDir(QLibraryInfo::path(QLibraryInfo::PluginsPath) + QStringLiteral("/kf6/kirigami")).exists()) {
		QCoreApplication::addLibraryPath(QStringLiteral(EDITOR_KDE_PLUGIN_DIR));
	}
#endif
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
	QCommandLineOption script_option("script",
		QCoreApplication::translate("main", "Run the Editor commands of <file> and quit, for tests and screenshots (see docs/Automation.adoc)."),
		"file");
	parser.addOption(script_option);
	parser.process(app);

	// The details only on request
	QLoggingCategory::setFilterRules(parser.isSet(verbose_option)
		? QStringLiteral("editor.*.debug=true")
		: QStringLiteral("editor.*.debug=false"));

	const bool automation = parser.isSet(script_option);
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
		// $SCRIPT_DIR in the commands of the script (Automation)
		qputenv("EASYRPG_EDITOR_SCRIPT_DIR", settings_dir.toLocal8Bit());
		// the Players a script downloads are its own too (Launchers::folder)
		if (!qEnvironmentVariableIsSet("EASYRPG_EDITOR_LAUNCHER_DIR")) {
			qputenv("EASYRPG_EDITOR_LAUNCHER_DIR", QDir(settings_dir).filePath(QStringLiteral("launchers")).toLocal8Bit());
		}
	}

	// the icons drawn for the light or the dark windows
	auto hints = app.styleHints();
	auto scheme_changed = [=]() {
		// a platform that does not know its scheme (offscreen): the palette tells
		const bool dark = hints->colorScheme() == Qt::ColorScheme::Unknown
			? QGuiApplication::palette().color(QPalette::Window).lightness() < 128
			: hints->colorScheme() == Qt::ColorScheme::Dark;
		QIcon::setThemeName(dark ? "ezdark" : "ezbright");
	};
	scheme_changed();
	QObject::connect(hints, &QStyleHints::colorSchemeChanged, scheme_changed);

	// load translations: the language chosen in the Editor, else the system's
	QString language = parser.value(language_option);
	if (language.isEmpty()) {
		// scripts find controls by their text: the same language everywhere
		language = automation ? QStringLiteral("en") : QSettings().value(LANGUAGE_KEY).toString();
	}
	Translations::apply(language);

	// main window and project
	QmlShell window;
	// a script runs without an RTP (as in CI): no window may wait for one
	if (!automation) {
		window.askForRtp();
	}
	if (automation) {
		// Clean up after the window has saved its workspace on aboutToQuit.
		QObject::connect(&app, &QCoreApplication::aboutToQuit, [] {
			QDir(QDir::temp().filePath(QStringLiteral("easyrpg-editor-script-%1")
				.arg(QCoreApplication::applicationPid()))).removeRecursively();
		});
	}

	const QStringList args = parser.positionalArguments();
	if (!args.isEmpty()) {
		window.LoadProject(QDir(args.first()).absolutePath());
	} else if (!automation) {
		window.loadLastProject();
	}
	if (!window.show()) {
		qCritical().noquote() << "Cannot make the main window of the QML interface";
		return 1;
	}

	// Ctrl+Alt+S: where the screenshot went, in the status bar (a script's
	// in its folder)
	Screenshot::installShortcut([&window](const QString& message) { window.showStatusMessage(message, 5000); });
	if (automation) {
		Automation script(window, parser.value(script_option));
		if (!script.isValid()) {
			qCritical().noquote() << "Cannot read the script" << parser.value(script_option);
			return 1;
		}
		script.start();
		const int failures = app.exec();
		// what the script wrote in its folder (its settings, copies, exports, downloads) goes
		const QString script_dir = qEnvironmentVariable("EASYRPG_EDITOR_SCRIPT_DIR");
		if (!script_dir.isEmpty()) {
			QDir(script_dir).removeRecursively();
		}
		return failures;
	}

	// into event loop
	return app.exec();
}
