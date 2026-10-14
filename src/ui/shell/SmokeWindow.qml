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

// The QML stack of the Editor in one window, for the automation's qml-smoke
// command: Kirigami and its Breeze style, a dock of KDDockWidgets and the
// project's data through the binding of the module

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
// KDDockWidgets registers its types from C++, with no qmldir: qmllint cannot
// know them
// qmllint disable import
import com.kdab.dockwidgets as KDDW
// qmllint enable import
import org.easyrpg.editor as Ez

Kirigami.ApplicationWindow {
	id: root
	objectName: "qmlSmoke"
	title: "QML smoke test"
	width: 720
	height: 420

	KDDW.DockWidget { // qmllint disable import
		id: dock
		uniqueName: "smokeDock"
		title: "Dock"
		Kirigami.FormLayout {
			anchors.fill: parent
			Controls.TextField {
				objectName: "smokeHero"
				Kirigami.FormData.label: "First hero:"
				text: heroName.value ?? ""
				onTextEdited: heroName.value = text

				// the name, both ways
				Ez.JsonField {
					id: heroName
					objectName: "smokeHeroField"
					source: Ez.ProjectData.database()
					path: "actors/0/name"
				}
			}
			Controls.CheckBox {
				objectName: "smokeCheck"
				text: "A Breeze check box"
				checked: true
			}
			Controls.SpinBox {
				objectName: "smokeSpin"
				Kirigami.FormData.label: "Number:"
				value: 42
			}
			Controls.ComboBox {
				objectName: "smokeCombo"
				Kirigami.FormData.label: "Choice:"
				model: ["One", "Two", "Three"]
			}
		}
	}

	// qmllint disable import unqualified unresolved-type
	KDDW.DockingArea {
		anchors.fill: parent
		uniqueName: "smokeLayout"
		Component.onCompleted: addDockWidget(dock, KDDW.KDDockWidgets.Location_OnLeft)
	}
	// qmllint enable import unqualified unresolved-type
}
