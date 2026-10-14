// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "DatabaseDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The database edited: a copy of the project's (session), put into the
 * project by apply(), on the 17 pages of RPG Maker (tabOld_Pages). It
 * remembers the page and the entries shown in each project
 * (saveViewState()).
 *
 * list: one list of the database instead of the pages, as the windows of
 * the "..." buttons of the switches and variables ("switches",
 * "variables").
 */
Item {
    id: root

    /** One list instead of the pages ("": the pages) */
    property string list: ""
    readonly property alias session: session
    readonly property var database: session.database
    property alias currentIndex: tabs.currentIndex
    /** The topic of the guide for the page shown */
    readonly property string topic: list !== "" ? (singleTopics[list] ?? "database")
                                                : (pages[tabs.currentIndex] ? pages[tabs.currentIndex].topic : "database")

    /**
     * The pages: title, the list of the entries (list), the page of an
     * entry or of the database (page), the topic of the guide. Their order
     * gives the pages' numbers (active_page, the pages Search opens). Their
     * titles name their context: qsTr() in this list is not given the pragma's
     */
    readonly property var pages: [
        { title: qsTranslate("DatabaseDialog", "Characters"), list: "actors", page: actorPage, topic: "database-heroes" },
        { title: qsTranslate("DatabaseDialog", "Professions"), list: "classes", page: classPage, topic: "database-classes" },
        { title: qsTranslate("DatabaseDialog", "Skills"), list: "skills", page: skillPage, topic: "database-skills" },
        { title: qsTranslate("DatabaseDialog", "Items"), list: "items", page: itemPage, topic: "database-items" },
        { title: qsTranslate("DatabaseDialog", "Monsters"), list: "enemies", page: enemyPage, topic: "database-monsters" },
        { title: qsTranslate("DatabaseDialog", "Monster Groups"), list: "troops", page: troopPage, topic: "database-monster-groups" },
        { title: qsTranslate("DatabaseDialog", "Attributes"), list: "attributes", page: attributePage, topic: "database-attributes" },
        { title: qsTranslate("DatabaseDialog", "Hero Status"), list: "states", page: statePage, topic: "database-conditions" },
        { title: qsTranslate("DatabaseDialog", "Battle Animation"), list: "animations", page: battleAnimationPage, topic: "database-battle-animations" },
        { title: qsTranslate("DatabaseDialog", "Battle Animation 2"), list: "battleranimations", page: battleAnimation2Page, topic: "database-battler-animations" },
        { title: qsTranslate("DatabaseDialog", "Battle screen"), list: "", page: battleScreenPage, topic: "database-battle-screen" },
        { title: qsTranslate("DatabaseDialog", "Terrain"), list: "terrains", page: terrainPage, topic: "database-terrain" },
        { title: qsTranslate("DatabaseDialog", "Chipset"), list: "chipsets", page: chipsetPage, topic: "database-chipset" },
        { title: qsTranslate("DatabaseDialog", "Vocabulary"), list: "", page: vocabularyPage, topic: "database-vocabulary" },
        { title: qsTranslate("DatabaseDialog", "System"), list: "", page: systemPage, topic: "database-system" },
        { title: qsTranslate("DatabaseDialog", "System 2"), list: "", page: system2Page, topic: "database-system2" },
        { title: qsTranslate("DatabaseDialog", "Common events"), list: "commonevents", page: commonEventPage, topic: "database-common-events" },
    ]
    /** The pages of the lists shown alone */
    readonly property var singlePages: ({ switches: switchPage, variables: variablePage, attributes: attributePage })
    readonly property var singleTopics: ({ switches: "database-switches", variables: "database-variables" })

    /**
     * The page, the entry and its line shown when the window opens (the
     * Search's results, see showEntry); null: as the window was closed
     */
    property var start: null
    /**
     * Shows a page (start.page, its index), an entry of its list
     * (start.id), and what the entry's page shows of it (its showLine(start):
     * a line of a common event, a battle page of a monster group)
     */
    function showEntry(start) {
        if (!start || list !== "" || !(start.page >= 0 && start.page < pages.length)) {
            return
        }
        tabs.currentIndex = start.page
        const loader = pageRepeater.itemAt(start.page) as Loader
        const split = loader ? loader.item as DatabaseSplit : null
        if (!split) {
            return
        }
        split.pane.selectId(start.id)
        // once the entry's page shows it
        Qt.callLater(() => {
            const page = split.entryPage
            if (page && page["showLine"]) {
                page["showLine"](start)
            }
        })
    }

    /** OK, Apply: the copy into the project (see DatabaseSession.apply) */
    function apply() {
        return session.apply()
    }

    // the state of the pages: read once, written when the window closes
    property var viewState: ({})
    function saveViewState() {
        const state = {}
        if (list === "") {
            state.active_page = tabs.currentIndex
            for (let i = 0; i < pageRepeater.count; ++i) {
                const loader = pageRepeater.itemAt(i) as Loader
                const page = loader ? pageState(loader.item) : undefined
                if (page !== undefined) {
                    state["qml_" + i] = page
                }
            }
        } else if (single.item) {
            state["qml_" + list] = pageState(single.item)
        }
        session.setViewState(state)
    }
    /** What a page remembers (its viewState()), undefined for nothing */
    function pageState(page) {
        return page && page["viewState"] ? page["viewState"]() : undefined
    }
    function restorePage(page, state) {
        if (page && page["restoreViewState"]) {
            page["restoreViewState"](state)
        }
    }

    implicitWidth: 480
    implicitHeight: 320

    Ez.DatabaseSession {
        id: session
        projectData: Ez.ProjectData
        // Settings > Reset local state: the first page
        onViewStateReset: {
            root.viewState = {}
            tabs.currentIndex = 0
        }
    }

    // the pages are made once the state they show is read
    property bool ready: false
    Component.onCompleted: {
        viewState = session.viewState()
        const page = Number(viewState.active_page ?? 0)
        tabs.currentIndex = page >= 0 && page < pages.length ? page : 0
        ready = true
        showEntry(start)
    }

    Component {
        id: attributePage
        AttributePage {}
    }
    Component {
        id: actorPage
        ActorPage {}
    }
    Component {
        id: classPage
        ClassPage {}
    }
    Component {
        id: itemPage
        ItemPage {}
    }
    Component {
        id: skillPage
        SkillPage {}
    }
    Component {
        id: statePage
        StatePage {}
    }
    Component {
        id: system2Page
        System2Page {}
    }
    Component {
        id: systemPage
        SystemPage {}
    }
    Component {
        id: terrainPage
        TerrainPage {}
    }
    Component {
        id: switchPage
        SwitchPage {}
    }
    Component {
        id: variablePage
        VariablePage {}
    }
    Component {
        id: battleScreenPage
        BattleScreenPage {}
    }
    Component {
        id: enemyPage
        EnemyPage {}
    }
    Component {
        id: troopPage
        TroopPage {}
    }
    Component {
        id: chipsetPage
        ChipsetPage {}
    }
    Component {
        id: battleAnimationPage
        BattleAnimationPage {}
    }
    Component {
        id: battleAnimation2Page
        BattleAnimation2Page {}
    }
    Component {
        id: commonEventPage
        CommonEventPage {}
    }
    Component {
        id: vocabularyPage
        VocabularyPage {
            function viewState() {
                return { tab: currentIndex }
            }
            function restoreViewState(state) {
                if (state && state.tab >= 0) {
                    currentIndex = state.tab
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: root.list === ""

        Controls.TabBar {
            id: tabs
            objectName: "tabOld_Pages"
            Layout.fillWidth: true
            font: Ez.Metrics.font

            Repeater {
                model: root.pages
                Controls.TabButton {
                    required property var modelData
                    text: modelData.title
                    width: implicitWidth
                    font: Ez.Metrics.font
                }
            }
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            Repeater {
                id: pageRepeater
                model: root.pages

                // a page is made when first shown, then kept
                Loader {
                    id: loader

                    required property var modelData
                    required property int index

                    active: root.ready && root.list === "" && (tabs.currentIndex === index || loaded)
                    property bool loaded: false
                    onLoaded: {
                        loaded = true
                        root.restorePage(item, root.viewState["qml_" + index])
                    }
                    sourceComponent: modelData.list !== "" ? splitPage : modelData.page

                    Component {
                        id: splitPage
                        DatabaseSplit {
                            database: root.database
                            rpg2k3: session.rpg2k3
                            listKey: loader.modelData.list
                            page: loader.modelData.page
                        }
                    }
                    Binding {
                        target: loader.item
                        property: "database"
                        value: root.database
                        when: loader.modelData.list === "" && !!loader.modelData.page && !!loader.item
                    }
                    Binding {
                        target: loader.item
                        property: "rpg2k3"
                        value: session.rpg2k3
                        when: loader.modelData.list === "" && !!loader.modelData.page && !!loader.item
                    }
                }
            }
        }
    }

    // one list alone
    Loader {
        id: single
        anchors.fill: parent
        active: root.ready && root.list !== ""
        sourceComponent: DatabaseSplit {
            database: root.database
            rpg2k3: session.rpg2k3
            listKey: root.list
            page: root.singlePages[root.list] ?? null
        }
        onLoaded: root.restorePage(item, root.viewState["qml_" + root.list])
    }
}
