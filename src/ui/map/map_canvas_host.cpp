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

#include "map_canvas_host.h"
#include <QQuickWindow>
#include "core.h"
#include "controller/project_controller.h"
#include "model/map_document.h"
#include "qmlbinding/qml_host.h"
#include "map_editor.h"

QQuickWindow* MapCanvasHost::open(ProjectController& controller, int id, QWindow* over)
{
	if (!core().project())
		return nullptr;
	MapDocument* doc = controller.openDocument(id);
	if (!doc)
		return nullptr;
	// a map no tab shows is read here, as its view would
	if (!doc->hasMap())
		doc->load();
	// gone with the window, or with the document when it closes first
	auto* editor = new MapEditor(doc, core().editorState(), doc);
	QQuickWindow* window = QmlHost::open(QStringLiteral("MapCanvasWindow"), over,
		{{QStringLiteral("editor"), QVariant::fromValue(editor)}});
	if (!window) {
		delete editor;
		return nullptr;
	}
	QObject::connect(window, &QObject::destroyed, editor, &QObject::deleteLater);
	return window;
}
