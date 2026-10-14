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

// The actions of the main window (ActionCatalog, ShortcutCatalog): every
// action, their keys, groups, menus and icons, and the
// translations they keep.

#include <QAbstractItemModelTester>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QTest>
#include <QXmlStreamReader>
#include "controller/action_catalog.h"
#include "controller/shortcut_catalog.h"

namespace {

QString source(const QString& path)
{
	QFile file(QStringLiteral(EDITOR_SOURCE_DIR "/") + path);
	return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

/** The sources of a context of a .ts file */
QSet<QString> translated(const QString& path, const QString& wanted)
{
	QSet<QString> sources;
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return sources;
	QXmlStreamReader xml(&file);
	QString context;
	while (!xml.atEnd()) {
		if (xml.readNext() != QXmlStreamReader::StartElement)
			continue;
		if (xml.name() == QLatin1String("name"))
			context = xml.readElementText();
		else if (xml.name() == QLatin1String("source") && context == wanted)
			sources.insert(xml.readElementText());
	}
	return sources;
}

}

class ActionCatalogTest : public QObject
{
	Q_OBJECT

private slots:
	void everyActionOfTheWindow();
	void texts();
	void keepTheirTranslations();
	void shortcutsUnique();
	void shortcutsOfRpgMaker();
	void groupsExclusive();
	void menusAndToolbars();
	void icons();
	void stateAndTrigger();
	void model();
};

void ActionCatalogTest::everyActionOfTheWindow()
{
	ActionCatalog catalog;
	const QStringList names = catalog.names();
	// the object names the scripts use, each once
	QCOMPARE(names.size(), 53);
	QCOMPARE(QSet<QString>(names.cbegin(), names.cend()).size(), names.size());
	const QString ui = source(QStringLiteral("src/ui/main_window.ui"));
	if (ui.isEmpty())
		QSKIP("no src/ui/main_window.ui");
	QSet<QString> window;
	const QRegularExpression action(QStringLiteral("<action name=\"(\\w+)\""));
	for (auto match = action.globalMatch(ui); match.hasNext();)
		window.insert(match.next().captured(1));
	QCOMPARE(window, QSet<QString>(names.cbegin(), names.cend()));
	for (const QString& name : names) {
		QVERIFY(catalog.contains(name));
		QCOMPARE(catalog.names().at(catalog.indexOf(name)), name);
	}
	QVERIFY(!catalog.contains(QStringLiteral("actionNone")));
	QCOMPARE(catalog.indexOf(QStringLiteral("actionNone")), -1);
}

void ActionCatalogTest::texts()
{
	ActionCatalog catalog;
	for (const QString& name : catalog.names())
		QVERIFY2(!catalog.text(name).isEmpty(), qPrintable(name));
	QCOMPARE(catalog.text(QStringLiteral("actionMapSave")), QStringLiteral("&Save Map"));
	QCOMPARE(catalog.toolTip(QStringLiteral("actionMapSave")), QStringLiteral("Save current Map"));
	QCOMPARE(catalog.statusTip(QStringLiteral("actionMapSave")), QStringLiteral("Save all changes done in current Map"));
	// none of its own: the text is shown
	QVERIFY(catalog.toolTip(QStringLiteral("actionQuit")).isEmpty());
	QCOMPARE(catalog.iconText(QStringLiteral("actionZoom1_4")), QStringLiteral("1/4"));
	QVERIFY(!catalog.isVisible(QStringLiteral("actionScriptEditor")));
	QCOMPARE(catalog.menuRole(QStringLiteral("actionQuit")), ActionCatalog::MenuRole::Quit);
	QCOMPARE(catalog.menuRole(QStringLiteral("actionAbout")), ActionCatalog::MenuRole::About);
	QCOMPARE(catalog.menuRole(QStringLiteral("actionSettings")), ActionCatalog::MenuRole::Preferences);
	QCOMPARE(catalog.menuRole(QStringLiteral("actionPlayTestOptions")), ActionCatalog::MenuRole::None);
}

void ActionCatalogTest::keepTheirTranslations()
{
	// the texts of the window, in its context: what was translated still is
	const QSet<QString> french = translated(QStringLiteral(EDITOR_SOURCE_DIR "/translations/editor_fr.ts"),
		QStringLiteral("MainWindow"));
	if (french.isEmpty())
		QSKIP("no translations/editor_fr.ts");
	// added after the translation was made (lupdate adds them)
	const QSet<QString> untranslated = {
		QStringLiteral("Export &Game..."),
		QStringLiteral("Export the game for Windows, macOS, Linux and the web (RPG Maker's Create Gamedisk)"),
		QStringLiteral("&Normal Size (1/2)"), QStringLiteral("Normal size (1/2)"),
		QStringLiteral("Back to the normal size of the map, as maps open (1/2)"),
		QStringLiteral("Save All Maps"), QStringLiteral("Save the changes of every open map"),
		QStringLiteral("Project Settings..."),
		QStringLiteral("The folder, title, engine, text encoding and patches of the game"),
		QStringLiteral("Convert to RPG Maker 2003..."),
		QStringLiteral("Convert this RPG Maker 2000 project to the format of RPG Maker 2003"),
	};
	ActionCatalog catalog;
	QStringList missing;
	auto check = [&](const QString& text) {
		if (!text.isEmpty() && !french.contains(text) && !untranslated.contains(text))
			missing << text;
	};
	for (const QString& name : catalog.names()) {
		check(catalog.text(name));
		check(catalog.toolTip(name));
		check(catalog.statusTip(name));
		check(catalog.iconText(name));
	}
	for (const ActionCatalog::Menu& menu : catalog.menus())
		check(menu.title);
	for (const QString& toolbar : catalog.toolBarNames())
		check(catalog.toolBarTitle(toolbar));
	QVERIFY2(missing.isEmpty(), qPrintable(missing.join(QStringLiteral(" | "))));
}

void ActionCatalogTest::shortcutsUnique()
{
	ActionCatalog catalog;
	// a key does one thing: in the window, and in the map tree over it
	QHash<QKeySequence, QString> owner;
	for (const QString& name : catalog.names()) {
		for (const QKeySequence& key : catalog.shortcuts(name)) {
			QVERIFY2(!key.isEmpty(), qPrintable(name));
			const QString other = owner.value(key);
			QVERIFY2(other.isEmpty() || ShortcutCatalog::sharesKeys(name, other),
				qPrintable(key.toString() + ": " + other + ", " + name));
			owner.insert(key, name);
		}
	}
	// those sharing one are never enabled together, and only in the map tree
	QVERIFY(ShortcutCatalog::sharesKeys(QStringLiteral("actionMapDelete"), QStringLiteral("actionAreaDelete")));
	QVERIFY(!ShortcutCatalog::sharesKeys(QStringLiteral("actionMapDelete"), QStringLiteral("actionMapDelete")));
	QVERIFY(!ShortcutCatalog::sharesKeys(QStringLiteral("actionMapCopy"), QStringLiteral("actionMapPaste")));
	for (const QString& name : {QStringLiteral("actionMapDelete"), QStringLiteral("actionAreaDelete")})
		QCOMPARE(catalog.shortcutContext(name), ShortcutCatalog::Context::MapTree);
	// every action with keys is in the catalog
	for (const QString& name : ShortcutCatalog::actions())
		QVERIFY2(catalog.contains(name), qPrintable(name));
}

void ActionCatalogTest::shortcutsOfRpgMaker()
{
	ActionCatalog catalog;
	QCOMPARE(catalog.shortcutTexts(QStringLiteral("actionLayerLower")), QStringList{QStringLiteral("F5")});
	QCOMPARE(catalog.shortcutTexts(QStringLiteral("actionDatabase")), QStringList{QStringLiteral("F8")});
	QCOMPARE(catalog.shortcutTexts(QStringLiteral("actionPlayTest")), QStringList{QStringLiteral("F9")});
	QCOMPARE(catalog.shortcutTexts(QStringLiteral("actionZoom")), QStringList{QStringLiteral("1")});
	QCOMPARE(catalog.shortcutTexts(QStringLiteral("actionDrawSelect")), QStringList{QStringLiteral("6")});
	QCOMPARE(catalog.shortcutTexts(QStringLiteral("actionMapSave")), QStringList{QStringLiteral("Ctrl+S")});
	QCOMPARE(catalog.shortcutContext(QStringLiteral("actionMapCopy")), ShortcutCatalog::Context::MapTree);
	QCOMPARE(catalog.shortcutContext(QStringLiteral("actionMapSave")), ShortcutCatalog::Context::Window);
	QVERIFY(catalog.shortcuts(QStringLiteral("actionJukebox")).isEmpty());
	// the platform's
	QCOMPARE(catalog.shortcuts(QStringLiteral("actionUndo")), QKeySequence::keyBindings(QKeySequence::Undo));
	QCOMPARE(ShortcutCatalog::standardKey(QStringLiteral("actionQuit")), QKeySequence::Quit);
	QCOMPARE(ShortcutCatalog::standardKey(QStringLiteral("actionMapSave")), QKeySequence::UnknownKey);
}

void ActionCatalogTest::groupsExclusive()
{
	ActionCatalog catalog;
	QCOMPARE(catalog.groupActions(ActionCatalog::Group::Layer).size(), 3);
	QCOMPARE(catalog.groupActions(ActionCatalog::Group::Tool).size(), 6);
	QCOMPARE(catalog.groupActions(ActionCatalog::Group::Zoom), (QStringList{QStringLiteral("actionZoom1_1"),
		QStringLiteral("actionZoom1_2"), QStringLiteral("actionZoom1_4"), QStringLiteral("actionZoom1_8")}));
	QVERIFY(catalog.groupActions(ActionCatalog::Group::None).isEmpty());
	for (auto group : {ActionCatalog::Group::Layer, ActionCatalog::Group::Tool, ActionCatalog::Group::Zoom}) {
		const QStringList members = catalog.groupActions(group);
		for (const QString& member : members) {
			QVERIFY(catalog.isCheckable(member));
			QCOMPARE(catalog.group(member), group);
			catalog.setChecked(member, true);
			for (const QString& other : members)
				QCOMPARE(catalog.isChecked(other), other == member);
		}
	}
	// no zoom ratio at the scales between them
	catalog.setChecked(QStringLiteral("actionZoom1_8"), false);
	for (const QString& ratio : catalog.groupActions(ActionCatalog::Group::Zoom))
		QVERIFY(!catalog.isChecked(ratio));
	// the others are on their own
	catalog.setChecked(QStringLiteral("actionFullScreenToggle"), true);
	catalog.setChecked(QStringLiteral("actionTitleBackgroundToggle"), true);
	QVERIFY(catalog.isChecked(QStringLiteral("actionFullScreenToggle")));
	QVERIFY(catalog.isChecked(QStringLiteral("actionTitleBackgroundToggle")));
	// shown at first
	QVERIFY(catalog.isChecked(QStringLiteral("actionPaletteToggle")));
	QVERIFY(catalog.isChecked(QStringLiteral("actionMapTreeToggle")));
	// an action that cannot be checked is never checked
	catalog.setChecked(QStringLiteral("actionMapSave"), true);
	QVERIFY(!catalog.isChecked(QStringLiteral("actionMapSave")));
}

void ActionCatalogTest::menusAndToolbars()
{
	ActionCatalog catalog;
	QCOMPARE(catalog.menuBar(), (QStringList{QStringLiteral("menuGame"), QStringLiteral("menuMaps"),
		QStringLiteral("menuView"), QStringLiteral("menuTools"), QStringLiteral("menuGame2"), QStringLiteral("menuHelp")}));
	QSet<QString> menu_names;
	for (const ActionCatalog::Menu& menu : catalog.menus())
		menu_names.insert(menu.name);
	QSet<QString> placed;
	auto check = [&](const QStringList& items, const QString& where) {
		QVERIFY2(!items.isEmpty(), qPrintable(where));
		for (const QString& item : items) {
			if (item.startsWith(QLatin1String("menu:")))
				QVERIFY2(menu_names.contains(item.mid(5)), qPrintable(where + ": " + item));
			else if (!item.isEmpty())
				QVERIFY2(catalog.contains(item), qPrintable(where + ": " + item));
			placed.insert(item);
		}
	};
	for (const ActionCatalog::Menu& menu : catalog.menus()) {
		check(menu.items, menu.name);
		// the menu bar's have a title
		QCOMPARE(!menu.title.isEmpty(), !menu.name.startsWith(QLatin1String("menuTree")));
	}
	for (const QString& toolbar : catalog.toolBarNames())
		check(catalog.toolBar(toolbar), toolbar);
	QCOMPARE(catalog.toolBarNames(), (QStringList{QStringLiteral("mainToolBar"), QStringLiteral("toolBar")}));
	// every action can be chosen somewhere
	for (const QString& name : catalog.names())
		QVERIFY2(placed.contains(name), qPrintable(name));
	QCOMPARE(catalog.menuTitle(QStringLiteral("menuMapZoom")), QStringLiteral("Map &Zoom"));
	QCOMPARE(catalog.menuList().size(), catalog.menus().size());
}

void ActionCatalogTest::icons()
{
	const QString qrc = source(QStringLiteral("resources/Resources.qrc"));
	if (qrc.isEmpty())
		QSKIP("no resources/Resources.qrc");
	ActionCatalog catalog;
	int with_icon = 0;
	for (const QString& name : catalog.names()) {
		const QString icon = catalog.iconName(name);
		if (icon.isEmpty())
			continue;
		with_icon++;
		// for light and dark backgrounds, by name in both icon themes
		for (const QString& folder : {QStringLiteral("bright"), QStringLiteral("dark")}) {
			QVERIFY2(QFile::exists(QStringLiteral(EDITOR_SOURCE_DIR "/resources/icons/%1/%2.svg").arg(folder, icon)),
				qPrintable(folder + "/" + icon));
			QVERIFY2(qrc.contains(QStringLiteral("alias=\"icons/ez%1/icons/%2.svg\">icons/%1/%2.svg").arg(folder, icon)),
				qPrintable("ez" + folder + ": " + icon));
		}
	}
	QCOMPARE(with_icon, 47);
}

void ActionCatalogTest::stateAndTrigger()
{
	ActionCatalog catalog;
	QSignalSpy triggered(&catalog, &ActionCatalog::triggered);
	QSignalSpy changed(&catalog, &ActionCatalog::stateChanged);
	QVERIFY(catalog.isEnabled(QStringLiteral("actionMapSave")));
	catalog.setEnabled(QStringLiteral("actionMapSave"), false);
	QCOMPARE(changed.size(), 1);
	// the same again: nothing changes
	catalog.setEnabled(QStringLiteral("actionMapSave"), false);
	QCOMPARE(changed.size(), 1);
	catalog.trigger(QStringLiteral("actionMapSave"));
	QCOMPARE(triggered.size(), 0);
	catalog.setEnabled(QStringLiteral("actionMapSave"), true);
	catalog.trigger(QStringLiteral("actionMapSave"));
	QCOMPARE(triggered.size(), 1);
	QCOMPARE(triggered.first().first().toString(), QStringLiteral("actionMapSave"));
	catalog.trigger(QStringLiteral("actionNone"));
	QCOMPARE(triggered.size(), 1);
	// checking a layer unchecks the one checked
	catalog.setChecked(QStringLiteral("actionLayerLower"), true);
	changed.clear();
	catalog.setChecked(QStringLiteral("actionLayerUpper"), true);
	QCOMPARE(changed.size(), 2);
}

void ActionCatalogTest::model()
{
	ActionCatalog catalog;
	QAbstractItemModelTester tester(&catalog, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QCOMPARE(catalog.rowCount(), 53);
	const QHash<int, QByteArray> roles = catalog.roleNames();
	for (const char* role : {"name", "text", "toolTip", "statusTip", "iconText", "iconName", "shortcuts",
			"standardKey", "shortcutContext", "checkable", "checked", "enabled", "visible", "group", "menuRole"})
		QVERIFY2(roles.values().contains(role), role);
	const QModelIndex save = catalog.index(catalog.indexOf(QStringLiteral("actionMapSave")));
	QCOMPARE(save.data(ActionCatalog::NameRole).toString(), QStringLiteral("actionMapSave"));
	QCOMPARE(save.data(ActionCatalog::TextRole).toString(), QStringLiteral("&Save Map"));
	QCOMPARE(save.data(ActionCatalog::IconNameRole).toString(), QStringLiteral("save"));
	QCOMPARE(save.data(ActionCatalog::ShortcutsRole).toStringList(), QStringList{QStringLiteral("Ctrl+S")});
	QSignalSpy data(&catalog, &QAbstractItemModel::dataChanged);
	catalog.setEnabled(QStringLiteral("actionMapSave"), false);
	QCOMPARE(data.size(), 1);
	QCOMPARE(data.first().first().toModelIndex(), save);
	QVERIFY(!save.data(ActionCatalog::EnabledRole).toBool());
	// a new language: every text
	QSignalSpy retranslated(&catalog, &ActionCatalog::retranslated);
	catalog.retranslate();
	QCOMPARE(retranslated.size(), 1);
	QCOMPARE(data.size(), 2);
}

QTEST_MAIN(ActionCatalogTest)
#include "action_catalog_test.moc"
