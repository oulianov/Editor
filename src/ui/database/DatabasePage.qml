// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "DatabaseDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Displays categories (e.g. Actors and Skills): the pages of the database
 * window.
 *
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
    /** Database of the current project: the copy edited (session.database) */
    readonly property Ez.JsonView jsonData: session.database
    readonly property alias database: root.jsonData
    property alias currentIndex: tabs.currentIndex
    /** The topic of the guide for the page shown */
    readonly property string topic: list !== "" ? (singleTopics[list] ?? "database")
                                                : (pages[tabs.currentIndex] ? pages[tabs.currentIndex].topic : "database")

    /**
     * The pages (the categories):
     * name: Text shown to the user
     * key: JSON pointer to access the list items of this category
     * targetPage: Page to load for the items (or for the database)
     * single: The page shows the database itself, not a list
     * topic: The topic of the guide
     * Their order gives the pages' numbers (active_page, the pages Search
     * opens). Their names name their context: qsTr() in this list is not
     * given the pragma's
     */
    readonly property var pages: [
        { name: qsTranslate("DatabaseDialog", "Characters"), key: "actors", targetPage: actorPage, topic: "database-heroes" },
        { name: qsTranslate("DatabaseDialog", "Professions"), key: "classes", targetPage: classPage, topic: "database-classes" },
        { name: qsTranslate("DatabaseDialog", "Skills"), key: "skills", targetPage: skillPage, topic: "database-skills" },
        { name: qsTranslate("DatabaseDialog", "Items"), key: "items", targetPage: itemPage, topic: "database-items" },
        { name: qsTranslate("DatabaseDialog", "Monsters"), key: "enemies", targetPage: enemyPage, topic: "database-monsters" },
        { name: qsTranslate("DatabaseDialog", "Monster Groups"), key: "troops", targetPage: troopPage, topic: "database-monster-groups" },
        { name: qsTranslate("DatabaseDialog", "Attributes"), key: "attributes", targetPage: attributePage, topic: "database-attributes" },
        { name: qsTranslate("DatabaseDialog", "Hero Status"), key: "states", targetPage: statePage, topic: "database-conditions" },
        { name: qsTranslate("DatabaseDialog", "Battle Animation"), key: "animations", targetPage: battleAnimationPage, topic: "database-battle-animations" },
        { name: qsTranslate("DatabaseDialog", "Battle Animation 2"), key: "battleranimations", targetPage: battleAnimation2Page, topic: "database-battler-animations" },
        { name: qsTranslate("DatabaseDialog", "Battle screen"), key: "", targetPage: battleScreenPage, single: true, topic: "database-battle-screen" },
        { name: qsTranslate("DatabaseDialog", "Terrain"), key: "terrains", targetPage: terrainPage, topic: "database-terrain" },
        { name: qsTranslate("DatabaseDialog", "Chipset"), key: "chipsets", targetPage: chipsetPage, topic: "database-chipset" },
        { name: qsTranslate("DatabaseDialog", "Vocabulary"), key: "", targetPage: vocabularyPage, single: true, topic: "database-vocabulary" },
        { name: qsTranslate("DatabaseDialog", "System"), key: "", targetPage: systemPage, single: true, topic: "database-system" },
        { name: qsTranslate("DatabaseDialog", "System 2"), key: "", targetPage: system2Page, single: true, topic: "database-system2" },
        { name: qsTranslate("DatabaseDialog", "Common events"), key: "commonevents", targetPage: commonEventPage, topic: "database-common-events" },
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
        selectCategory(start.page)
        const loader = pageRepeater.itemAt(start.page) as Loader
        const listPage = loader ? loader.item as DatabaseEntryListPage : null
        if (!listPage) {
            return
        }
        listPage.selectId(start.id)
        // once the entry's page shows it
        Qt.callLater(() => {
            const page = listPage.entryPage
            if (page && page["showLine"]) {
                page["showLine"](start)
            }
        })
    }

    /** Shows the page of this index (its tab) */
    function selectCategory(index) {
        if (index < 0 || index >= pages.length) {
            return
        }
        tabs.currentIndex = index
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
        selectCategory(page >= 0 && page < pages.length ? page : 0)
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
                    text: modelData.name
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
                    sourceComponent: loader.modelData.single ? loader.modelData.targetPage : listPage

                    Component {
                        id: listPage
                        DatabaseEntryListPage {
                            database: root.jsonData
                            rpg2k3: session.rpg2k3
                            key: loader.modelData.key
                            targetPage: loader.modelData.targetPage
                        }
                    }
                    Binding {
                        target: loader.item
                        property: "database"
                        value: root.jsonData
                        when: !!loader.modelData.single && !!loader.item
                    }
                    Binding {
                        target: loader.item
                        property: "rpg2k3"
                        value: session.rpg2k3
                        when: !!loader.modelData.single && !!loader.item
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
        sourceComponent: DatabaseEntryListPage {
            database: root.jsonData
            rpg2k3: session.rpg2k3
            key: root.list
            targetPage: root.singlePages[root.list] ?? null
        }
        onLoaded: root.restorePage(item, root.viewState["qml_" + root.list])
    }
}
