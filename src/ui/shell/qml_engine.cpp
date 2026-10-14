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

#include "qml_engine.h"
#include "core.h"
#include "common/filefinder.h"
#include "model/project.h"
#include "qmlbinding/database_session.h"
#include "qmlbinding/editor_bridge.h"
#include "qmlbinding/project_data_gadget.h"
#include "qmlbinding/project_image_provider.h"
#include "qmlbinding/qml_dialog_service.h"
#include <Kirigami/Platform/Units>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QCoreApplication>
#include <QEvent>
#include <QDebug>
#include <QDir>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <kddockwidgets/Config.h>
#include <kddockwidgets/qtquick/Platform.h>
#include <kddockwidgets/qtquick/ViewFactory.h>

namespace {

// the docks of the main window with title bars in the colors of the
// Editor (DockTitleBar.qml), light or dark
class DockViewFactory : public KDDockWidgets::QtQuick::ViewFactory
{
public:
	QUrl titleBarFilename() const override
	{
		return QUrl(QStringLiteral("qrc:/qt/qml/org/easyrpg/editor/kddockwidgets/DockTitleBar.qml"));
	}
};

}

QQmlApplicationEngine* QmlEngine::instance() {
	static QQmlApplicationEngine* engine = nullptr;
	if (!engine) {
		// The density of RPG Maker's forms (Ez.Metrics): Breeze sizes its
		// controls from the application font and pads them with the spacing
		// of Kirigami. The windows get the font of the forms (the small one
		// of macOS)
		if (EditorBridge::formFont() != QGuiApplication::font()) {
			QGuiApplication::setFont(EditorBridge::formFont());
		}
		engine = new QQmlApplicationEngine(QCoreApplication::instance());
#if defined(EDITOR_KDE_QML_DIR)
		// Kirigami and Breeze outside Qt's folders: a build of the Editor that
		// runs from its build folder (a packaged one has them with Qt's)
		const QString qt_qml = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
		if (!QDir(qt_qml + QStringLiteral("/org/kde/kirigami")).exists()
				&& QDir(QStringLiteral(EDITOR_KDE_QML_DIR)).exists()) {
			engine->addImportPath(QStringLiteral(EDITOR_KDE_QML_DIR));
		}
#endif
		// image://project/<Folder>/<File>: the pictures of the open project
		engine->addImageProvider(QStringLiteral("project"), new ProjectImageProvider([](const QString& folder, const QString& name) {
			const auto project = core().project();
			return project ? project->findFile(folder, name, FileFinder::FileType::Image) : QString();
		}));
		// a new language (Settings): the texts of the windows at once
		class Retranslate : public QObject {
		public:
			using QObject::QObject;
			bool eventFilter(QObject* watched, QEvent* event) override {
				if (event->type() == QEvent::LanguageChange && watched == QCoreApplication::instance()) {
					static_cast<QQmlEngine*>(parent())->retranslate();
				}
				return false;
			}
		};
		QCoreApplication::instance()->installEventFilter(new Retranslate(engine));
		// compact paddings (Breeze: the frame of a field is the height of its
		// text and twice the medium spacing). The large spacing, around the
		// text of fields and buttons, cannot be changed (Kirigami ignores
		// setLargeSpacing): the Ez controls pad it with Ez.Metrics.fieldPadding
		QQmlComponent platform(engine);
		platform.setData("import QtQml\nimport org.kde.kirigami.platform\nQtObject {}", QUrl());
		delete platform.create();
		if (auto* units = engine->singletonInstance<Kirigami::Platform::Units*>(
				QStringLiteral("org.kde.kirigami.platform"), QStringLiteral("Units"))) {
			units->setSmallSpacing(3);
			units->setMediumSpacing(4);
		}
		// the tool tip the items share (Controls.ToolTip.text...): Ez.ToolTip,
		// Breeze's without its binding loop. Qt Quick Controls makes it on
		// first use and keeps it in this property of the engine; made first,
		// it is the one used
		QQmlComponent tool_tip(engine);
		tool_tip.setData("import org.easyrpg.editor as Ez\nEz.ToolTip {}", QUrl());
		if (QObject* tip = tool_tip.create()) {
			tip->setParent(engine);
			engine->setProperty("_q_QQuickToolTip", QVariant::fromValue(tip));
		} else {
			qWarning().noquote() << tool_tip.errorString();
		}
		// the database windows ask their questions in QML windows over them
		DatabaseSession::setWindowDialogs([](QWindow* over) {
			return std::make_unique<QmlDialogService>(over);
		});
		KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtQuick);
		KDDockWidgets::Config::self().setViewFactory(new DockViewFactory());
		KDDockWidgets::QtQuick::Platform::instance()->setQmlEngine(engine);
	}
	return engine;
}

void QmlEngine::setProject(ProjectData* data) {
	if (auto* gadget = instance()->singletonInstance<ProjectDataGadget*>(
			QStringLiteral("org.easyrpg.editor"), QStringLiteral("ProjectData"))) {
		gadget->setProjectData(data);
	}
}

void QmlEngine::projectReloaded() {
	if (auto* gadget = instance()->singletonInstance<ProjectDataGadget*>(
			QStringLiteral("org.easyrpg.editor"), QStringLiteral("ProjectData"))) {
		gadget->databaseJson().notifyChanged(QString());
		gadget->treeMapJson().notifyChanged(QString());
	}
}
