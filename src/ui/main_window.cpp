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

#include "ui/other/new_project_dialog.h"
#include "ui/other/open_project_dialog.h"
#include "ui/other/import_project_dialog.h"
#include "ui/other/settings_dialog.h"
#include "ui/other/jukebox_dialog.h"
#include "common/editor_settings.h"
#include "common/translations.h"
#include "ui/event/event_dialog.h"
#include "ui/maptree/map_properties_dialog.h"
#include "ui/maptree/area_properties_dialog.h"
#include "ui/maptree/shift_map_dialog.h"
#include "ui/maptree/generate_dungeon_dialog.h"
#include "ui/other/run_game_dialog.h"
#include "model/map_tools.h"
#include "main_window.h"
#include "ui/common/theme_icon.h"
#include "ui/common/toolbar_overflow.h"
#include "ui_main_window.h"
#include "common/dbstring.h"
#include <QImage>
#include <QToolBar>
#include <QToolButton>
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QActionGroup>
#include <QApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressDialog>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStringList>
#include <QDir>
#include <QMenu>
#include <QTimer>
#include <QTreeWidgetItemIterator>
#include <algorithm>
#include <set>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <cassert>
#include <sstream>
#include <iomanip>
#include "core.h"
#include <lcf/lmu/reader.h>
#include <lcf/lmt/reader.h>
#include <lcf/ldb/reader.h>
#include <lcf/inireader.h>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include "model/project.h"
#include "common/logging.h"

Q_DECLARE_METATYPE(QList<int>)
Q_DECLARE_METATYPE(QList<float>)

// View > Animate Tiles

const std::vector<QString> resource_dirs =
	{BACKDROP, BATTLE, BATTLE2, BATTLECHARSET, BATTLEWEAPON, CHARSET, CHIPSET, FACESET,
	GAMEOVER, MONSTER, MOVIE, MUSIC, PANORAMA, PICTURE, SOUND, SYSTEM, SYSTEM2, TITLE};

static void recurseAddDir(QDir d, QStringList & list) {

	QStringList qsl = d.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);

	foreach (QString file, qsl) {

		QFileInfo finfo(QString("%1/%2").arg(d.path()).arg(file));

		if (finfo.isSymLink())
			return;

		if (finfo.isDir()) {

			QString dirname = finfo.fileName();
			QDir sd(finfo.filePath());

			recurseAddDir(sd, list);

		} else
			list << QDir::toNativeSeparators(finfo.filePath());
	}
}

/** The file of a map, as Project::loadMap names it */
/** The scale maps open at, the 1/2 ratio: tiles twice their size, readable on
 *  today's screens; 1/1 zooms in further */
static constexpr double default_map_scale = 2.0;

static QString mapFileName(int id) {
	const bool easyrpg = core().project()->projectType() == FileFinder::ProjectType::EasyRpg;
	return QString("Map%1.%2").arg(id, 4, 10, QChar('0')).arg(easyrpg ? "emu" : "lmu");
}

#ifdef Q_OS_WIN
static void associateFileTypes(const QStringList &fileTypes)
{
	QString displayName = QGuiApplication::applicationDisplayName();
	QString filePath = QCoreApplication::applicationFilePath();
	QString fileName = QFileInfo(filePath).fileName();

	QSettings settings("HKEY_CURRENT_USER\\Software\\Classes\\Applications\\" + fileName, QSettings::NativeFormat);
	settings.setValue("FriendlyAppName", displayName);

	settings.beginGroup("SupportedTypes");
	foreach (const QString& fileType, fileTypes)
		settings.setValue(fileType, QString());
	settings.endGroup();

	settings.beginGroup("shell");
	settings.beginGroup("open");
	settings.setValue("FriendlyAppName", displayName);
	settings.beginGroup("Command");
	settings.setValue(".", QChar('"') + QDir::toNativeSeparators(filePath) + QString("\" \"%1\""));
}
//! [0]
#endif




MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	// Panels have their View menu actions; the fixed toolbars are always shown.
	setContextMenuPolicy(Qt::PreventContextMenu);
#ifdef Q_OS_MACOS
	for (auto* button : ui->mainToolBar->findChildren<QToolButton*>())
		button->setAttribute(Qt::WA_MacSmallSize);
#endif
	refreshIcons();
	connect(ui->actionAbout, &QAction::triggered, this, [this] {
		QMessageBox::about(this, tr("About EasyRPG Editor"),
			tr("<h3>EasyRPG Editor</h3>"
				"<p>A game editor for EasyRPG and RPG Maker 2000/2003 projects.</p>"
				"<p>Free software, licensed under the GNU General Public License "
				"(version 3 or later).</p>"
				"<p><a href=\"https://easyrpg.org/\">EasyRPG website</a></p>"));
	});

	// Hide map ids
	ui->treeMap->hideColumn(1);
	// Created hardcoded toolbar for palette window.
	ui->widgetBar->layout()->addWidget(ui->toolBar);
	// "»": a menu of the buttons that do not fit
	for (QToolBar* toolbar : findChildren<QToolBar*>())
		ToolBarOverflowMenu::install(toolbar);
	//Create dialogs
	dlg_resource = new ResourceManagerDialog(this);
	dlg_resource->setModal(true);
	dlg_db = nullptr;
	m_paletteScene = new PaletteScene(ui->graphicsPalette);
	ui->graphicsPalette->setScene(m_paletteScene);
	ui->graphicsPalette->verticalScrollBar()->setSliderPosition(1);
	connect(&core(),
			SIGNAL(toolChanged()),
			this,
			SLOT(updateToolActions()));
	connect(&core(),
			SIGNAL(layerChanged()),
			this,
			SLOT(updateLayerActions()));
	connect(&core(),
			SIGNAL(layerChanged()),
			m_paletteScene,
			SLOT(onLayerChange()));
	connect(&core(),
			SIGNAL(chipsetChanged()),
			m_paletteScene,
			SLOT(onChipsetChange()));
	connect(&core(), &Core::toolChanged, this, &MainWindow::updateStatusBar);
	connect(&core(), &Core::layerChanged, this, &MainWindow::updateStatusBar);


	// Map Zoom: RPG Maker's ratios, maps open at 1/2; none checked at the
	// other scales
	m_zoomGroup = new QActionGroup(this);
	m_zoomGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
	const std::pair<QAction*, double> ratios[] = {{ui->actionZoom1_1, default_map_scale * 2},
		{ui->actionZoom1_2, default_map_scale}, {ui->actionZoom1_4, default_map_scale / 2},
		{ui->actionZoom1_8, default_map_scale / 4}};
	for (const auto& [action, ratio] : ratios) {
		action->setData(ratio);
		m_zoomGroup->addAction(action);
		connect(action, &QAction::triggered, this, [this, ratio] {
			if (MapScene* scene = currentScene())
				scene->setScale(static_cast<float>(ratio));
			// a click on the checked ratio unchecks it: check it again
			updateZoomActions();
		});
	}

	// Status bar: RPG Maker's edit mode and map information, after the hint
	// of the hovered action (the status tips, shown by Qt)
	m_editModeLabel = new QLabel(this);
	m_editModeLabel->setObjectName("labelEditMode");
	m_mapLabel = new QLabel(this);
	m_mapLabel->setObjectName("labelMapName");
	m_cellLabel = new QLabel(this);
	m_cellLabel->setObjectName("labelCell");
	statusBar()->addPermanentWidget(m_editModeLabel);
	statusBar()->addPermanentWidget(m_mapLabel);
	statusBar()->addPermanentWidget(m_cellLabel);
	// Show what other programs (like a script sync tool) write into the project
	auto& watcher = core().watcher();
	watcher.setCanDeliver([this](int kind) { return canReload(kind); });
	connect(&watcher, &ProjectWatcher::databaseChanged, this, &MainWindow::onDatabaseFileChanged);
	connect(&watcher, &ProjectWatcher::treeMapChanged, this, &MainWindow::onTreeMapFileChanged);
	connect(&watcher, &ProjectWatcher::mapChanged, this, &MainWindow::onMapFileChanged);

	// Map tree: menus like RPG Maker's, drag and drop to make a map the
	// child of another, and the tree shortcuts (Ctrl+C, Ctrl+V, Del, Ctrl+H)
	ui->treeMap->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->treeMap, &QWidget::customContextMenuRequested, this, &MainWindow::onMapTreeContextMenu);
	ui->treeMap->setDragDropMode(QAbstractItemView::InternalMove);
	ui->treeMap->setDefaultDropAction(Qt::MoveAction);
	// only the project item is at the top level
	ui->treeMap->invisibleRootItem()->setFlags(ui->treeMap->invisibleRootItem()->flags() & ~Qt::ItemIsDropEnabled);
	ui->treeMap->viewport()->installEventFilter(this);
	// a click on the selected map shows it again (its tab can be closed)
	connect(ui->treeMap, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) { openTreeItem(item); });
	ui->treeMap->addActions({ui->actionMapCopy, ui->actionMapPaste, ui->actionMapDelete,
		ui->actionMapShift, ui->actionAreaDelete});
	auto rememberExpanded = [this](QTreeWidgetItem* item) {
		if (auto* info = mapInfo(item->data(1, Qt::DisplayRole).toInt())) {
			info->expanded_node = item->isExpanded();
		}
	};
	connect(ui->treeMap, &QTreeWidget::itemExpanded, this, rememberExpanded);
	connect(ui->treeMap, &QTreeWidget::itemCollapsed, this, rememberExpanded);

	// Test play: F9 like RPG Maker, the options are remembered
	ui->actionFullScreenToggle->setChecked(m_settings.value(TESTPLAY_FULLSCREEN_KEY, false).toBool());
	ui->actionTitleBackgroundToggle->setChecked(m_settings.value(TESTPLAY_SHOW_TITLE_KEY, true).toBool());
	RunGameDialog::setBeforeLaunch([this] { return saveBeforeTestPlay(); });
	setPlatformShortcuts();

	update_actions();
	// the options of the Settings window, now and when they change
	applySettings();
	connect(&EditorSettings::instance(), &EditorSettings::changed, this, &MainWindow::applySettings);
	if (core().rtpPath("").isEmpty())
		on_actionSettings_triggered();
	core().setDefDir(m_settings.value(DEFAULT_DIR_KEY,
										qApp->applicationDirPath()).toString());
	updateLayerActions();
	updateToolActions();
	restoreGeometry(m_settings.value(QStringLiteral("main_window_geometry")).toByteArray());
	connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveWorkspace);
}

MainWindow::~MainWindow()
{
	delete ui;
	if (core().project())
		delete searchdialog;
	delete dlg_resource;
	delete dlg_db;
}

void MainWindow::LoadLastProject()
{
	QString l_project = m_settings.value(CURRENT_PROJECT_KEY, QString()).toString();
	if (!l_project.isEmpty())
		LoadProject(l_project);
}

void MainWindow::LoadProject(QString foldername)
{
	auto prj = Project::load(foldername);

	if (!prj) {
		QMessageBox::critical(this,
							  tr("Error loading project"),
							  QString(tr("Failed to load project %1")).arg(foldername));
		return;
	}

	if (!prj->loadDatabaseAndMapTree()) {
		QMessageBox::critical(this,
							  tr("Error loading project"),
							  QString(tr("Failed to load database or maptree of %1")).arg(foldername));
		return;
	}

	core().project() = prj;

	searchdialog = new SearchDialog(core().project()->projectData(), this);
	searchdialog->enableCache(EditorSettings::instance().searchCache());

	setWindowTitle(tr("EasyRPG Editor") + " - " +  core().project()->gameTitle());
	m_settings.setValue(CURRENT_PROJECT_KEY, core().project()->projectDir().absolutePath());
	restoreWorkspace();
	updateWatcher();
	qCInfo(lcProject).noquote() << "project loaded:" << foldername
		<< "maps:" << (core().project()->treeMap().maps.size() - 1)
		<< "reload changed files:" << EditorSettings::instance().reloadExternalChanges();

	update_actions();
}

void MainWindow::populateMapTree(bool open_active_map)
{
	ui->treeMap->clear();
	m_treeItems.clear();
	QTreeWidgetItem *root = new QTreeWidgetItem();
	root->setData(1, Qt::DisplayRole, 0);
	root->setData(0,Qt::DisplayRole,  core().project()->gameTitle());
	root->setIcon(0, themeIcon(this, "folder"));
	// the project item stays where it is
	root->setFlags(root->flags() & ~Qt::ItemIsDragEnabled);
	ui->treeMap->addTopLevelItem(root);
	m_treeItems[0] = root;
	const lcf::rpg::TreeMap& maps = core().project()->treeMap();
	//Add Items
	for (unsigned int i = 1; i < maps.maps.size(); i++)
		addTreeItem(maps.maps[i]);
	//Parent Items, in the tree order
	for (unsigned int i = 1; i < maps.tree_order.size(); i++)
	{
		const int id = maps.tree_order[i];
		QTreeWidgetItem* item = m_treeItems.value(id);
		const lcf::rpg::MapInfo* info = mapInfo(id);
		if (!item || !info || item->parent() || id == 0)
			continue;
		// a map whose parent is gone (older versions left some) goes under the project
		QTreeWidgetItem* parent = m_treeItems.value(info->parent_map);
		(parent && parent != item ? parent : root)->addChild(item);
	}
	// maps missing from the tree order
	for (auto it = m_treeItems.cbegin(); it != m_treeItems.cend(); ++it)
	{
		if (it.key() != 0 && !it.value()->parent())
			root->addChild(it.value());
	}
	//Expand Items
	for (unsigned int i = 0; i < maps.maps.size(); i++)
	{
		if (QTreeWidgetItem* item = m_treeItems.value(maps.maps[i].ID))
			item->setExpanded(maps.maps[i].expanded_node);
	}
	root->setExpanded(true);

	if (open_active_map && maps.active_node > 0 && m_treeItems.value(maps.active_node))
	{
		ui->treeMap->setCurrentItem(m_treeItems[maps.active_node]);
		this->on_treeMap_itemDoubleClicked(m_treeItems[maps.active_node], 0);
	}
}

void MainWindow::ImportProject(const QDir& src_dir, QDir& target_dir, bool convert_xyz)
{
	if (!FileFinder::IsRPG2kProject(src_dir)) {
		QMessageBox::critical(this,
			tr("Error loading project"),
			QString(tr("The selected project %1 is not a legacy RPG Maker project")).arg(src_dir.dirName()));
		return;
	}

	auto prj = Project::load(src_dir);

	if (!prj) {
		QMessageBox::critical(this,
							  tr("Error loading project"),
							  QString(tr("Failed to load project %1")).arg(src_dir.dirName()));
		return;
	}

	if (!prj->loadDatabaseAndMapTree()) {
		QMessageBox::critical(this,
							  tr("Error loading project"),
							  QString(tr("Failed to load database or maptree of %1")).arg(src_dir.dirName()));
		return;
	}

	// Load all maps
	lcf::rpg::TreeMap& maps = prj->treeMap();
	QProgressDialog progress(tr("Processing Maps..."), "", 0, 0, this);
	progress.setMaximum(static_cast<int>(maps.maps.size()));
	progress.setWindowModality(Qt::WindowModal);

	std::vector<std::tuple<int, std::unique_ptr<lcf::rpg::Map>>> loaded_maps;

	for (size_t i = 1; i < maps.maps.size(); i++) {
		progress.setValue(static_cast<int>(i));
		if (maps.maps[i].type == lcf::rpg::TreeMap::MapType_area)
			continue;
		loaded_maps.push_back(std::make_tuple(maps.maps[i].ID, prj->loadMap(maps.maps[i].ID)));
	}

	// Save new database and treemap
	prj->relocate(target_dir, FileFinder::ProjectType::EasyRpg);
	progress.setLabelText(tr("Importing database and map tree..."));
	progress.setValue(0);
	progress.setMaximum(0);
	prj->saveDatabase(false);
	prj->saveTreeMap();

	progress.setLabelText(tr("Importing maps..."));
	progress.setMaximum(static_cast<int>(loaded_maps.size()));
	int i = 1;
	for (auto& [id, map] : loaded_maps) {
		prj->saveMap(*map, id, false);
		progress.setValue(i++);
	}
	loaded_maps.clear();

	progress.setLabelText(tr("Importing resources..."));

	// Create all asset directories
	for (const QString& dir : resource_dirs)
		target_dir.mkpath(dir);

	QStringList entries;
	for (const QString& dir : resource_dirs) {
		// Enumerate source files
		QDir subdir = src_dir;
		if (subdir.cd(dir)) {
			recurseAddDir(subdir, entries);
		}
	}

	progress.setMaximum(entries.count());

	// Copy all assets from the asset subdirectories
	for (int i = 0; i < entries.count(); i++) {
		progress.setValue(i);
		QFileInfo info(entries[i]);
		QString dest_file = target_dir.path() + "/" + info.dir().dirName() + "/" + info.fileName();
		if (!QFile::copy(entries[i], dest_file))
		{
			QMessageBox box(this);
			QString name = tr("Error");
			QString text = tr("Could not copy file %1 to \n"
							  "%2").arg(entries[i]).arg(dest_file);

			box.setModal(true);
			box.setWindowTitle(name);
			box.setText(QString::fromLatin1("%1").arg(text));
			box.setIcon(QMessageBox::Critical);
			box.setStandardButtons(QMessageBox::Ok);

			box.exec();

			on_actionProjectClose_triggered();
			return;
		}
		if (convert_xyz && info.dir().dirName() != MUSIC && info.dir().dirName() != SOUND)
		{
			QFile file(dest_file);
			bool success = file.open(QIODevice::ReadOnly);
			if (success && file.read(4) == "XYZ1")
			{
				QString conv_path = target_dir.path() + "/" + info.dir().dirName() + "/" + info.completeBaseName() + ".png";
				if (convertXYZtoPNG(file, conv_path))
					file.remove();
				else
					qWarning() << QString(tr("Failed to convert %1 to PNG")).arg(dest_file);
			}
		}

	}
	progress.setValue(entries.count());

	core().project() = prj;
	populateMapTree(false);

	/*m_projSett = new QSettings(core().project()->findFile(ROOT, EASY_CFG),
							   QSettings::IniFormat,
							   this);
	m_projSett->setValue(GAMETITLE, title);
	m_projSett->setValue(LAYER, core().layer());
	m_projSett->setValue(MAPS, m_mapList);
	m_projSett->setValue(SCALES, m_scaleList);
	m_projSett->setValue(TILESIZE, 16);
	this->on_treeMap_itemDoubleClicked(m_treeItems[m_mapList[0].toInt()], 0);*/

	setWindowTitle(tr("EasyRPG Editor") + " - " +  core().project()->gameTitle());
	m_settings.setValue(CURRENT_PROJECT_KEY, core().project()->projectDir().absolutePath());
}

bool MainWindow::convertXYZtoPNG(QFile &xyz_file, QString out_path)
{
	QByteArray compressed_data(xyz_file.readAll());
	assert(!compressed_data.isEmpty());
	uint16_t width = static_cast<uint16_t>(static_cast<uint8_t>(compressed_data[0]) + (static_cast<uint8_t>(compressed_data[1]) << 8));
	uint16_t height = static_cast<uint16_t>(static_cast<uint8_t>(compressed_data[2]) + (static_cast<uint8_t>(compressed_data[3]) << 8));
	uint32_t size = width * height;
	// Put the total size in the first 4 bytes to make it valid for qUncompress
	for (int i = 0; i < 4; ++i)
		compressed_data[i] = static_cast<char>((reinterpret_cast<uint8_t*>(&size))[3-i]);

	QByteArray xyz_data(qUncompress(compressed_data));

	if (xyz_data.isEmpty())
		return false;

	QImage image(reinterpret_cast<uchar*>(xyz_data.data()) + 768, width, height, QImage::Format_Indexed8);
	// Add color table
	for (int i = 0; i < 256; ++i)
		image.setColor(i, qRgb(xyz_data[i * 3], xyz_data[i * 3 + 1], xyz_data[i * 3 + 2]));

	return image.save(out_path, "PNG");
}

void MainWindow::on_actionQuit_triggered()
{
	if (saveAll()) {
		//this->on_actionJukebox_triggered(true);
		qApp->quit();
	}
}

void MainWindow::on_actionPaletteToggle_triggered(bool checked)
{
	if (checked)
		ui->dockPalette->show();
	else
		ui->dockPalette->hide();
}

void MainWindow::on_actionMapTreeToggle_triggered(bool checked)
{
	if (checked)
		ui->dockMapTree->show();
	else
		ui->dockMapTree->hide();
}

void MainWindow::on_actionResourceManager_triggered()
{
	dlg_resource->show();
}

void MainWindow::on_actionDatabase_triggered()
{
	if (dlg_db)
		delete dlg_db;
	dlg_db = new DatabaseDialog(core().project()->projectData(), this);
	dlg_db->setModal(true);
	dlg_db->exec();
}

void MainWindow::update_actions()
{
	bool has_project = core().project() != nullptr;
	MapScene* scene = currentScene();
	// the selected node of the map tree (nothing selected: the project)
	const int type = has_project ? mapType(currentMapId()) : -1;
	const bool on_root = has_project && (type == lcf::rpg::TreeMap::MapType_root || type == -1);
	const bool on_map = type == lcf::rpg::TreeMap::MapType_map;
	const bool on_area = type == lcf::rpg::TreeMap::MapType_area;

	ui->actionDrawCircle->setEnabled(has_project);
	ui->actionDrawSelect->setEnabled(has_project);
	ui->actionGameDiskCreate->setEnabled(has_project);
	ui->actionDatabase->setEnabled(has_project);
	ui->actionDrawPen->setEnabled(has_project);
	ui->actionDrawFill->setEnabled(has_project);
	ui->actionProjectImport->setEnabled(!has_project);
	ui->actionDrawRectangle->setEnabled(has_project);
	ui->actionResourceManager->setEnabled(has_project);
	// zooming needs a map
	ui->actionZoom100->setEnabled(scene);
	ui->actionZoomIn->setEnabled(scene);
	ui->actionZoomOut->setEnabled(scene);
	m_zoomGroup->setEnabled(scene);
	updateZoomActions();
	updateStatusBar();
	ui->actionMapRevert->setEnabled(scene && scene->isModified());
	ui->actionSearch->setEnabled(has_project);
	ui->actionJukebox->setEnabled(has_project);
	ui->actionUndo->setEnabled(scene && scene->canUndo());
	ui->actionRedo->setEnabled(scene && scene->canRedo());
	ui->actionZoom->setEnabled(has_project);
	ui->actionProjectClose->setEnabled(has_project);
	ui->actionLayerEvents->setEnabled(has_project);
	ui->actionFullScreenToggle->setEnabled(has_project);
	ui->actionLayerLower->setEnabled(has_project);
	ui->actionLayerUpper->setEnabled(has_project);
	ui->actionProjectNew->setEnabled(!has_project);
	ui->actionProjectOpen->setEnabled(!has_project);
	ui->actionPlayTest->setEnabled(has_project);
	ui->actionPlayTestOptions->setEnabled(has_project);
	ui->actionMapSave->setEnabled(scene && scene->isModified());
	ui->actionScriptEditor->setEnabled(has_project);
	ui->actionTitleBackgroundToggle->setEnabled(has_project);

	// Map tree: what RPG Maker's menus offer on the selected node
	ui->actionMapNew->setEnabled(on_root || on_map);
	ui->actionMapProperties->setEnabled(on_map || on_area || scene);
	ui->actionMapCopy->setEnabled(on_map);
	ui->actionMapPaste->setEnabled(m_copiedMap && (on_root || on_map));
	ui->actionMapDelete->setEnabled(on_map);
	ui->actionMapShift->setEnabled(on_map);
	ui->actionAreaNew->setEnabled(on_map);
	ui->actionMapGenerateDungeon->setEnabled(on_map);
	ui->actionAreaProperties->setEnabled(on_area);
	ui->actionAreaDelete->setEnabled(on_area);
}

void MainWindow::on_actionProjectNew_triggered() {
	NewProjectDialog dlg(this);
	dlg.setDefDir(core().defDir());
	dlg.exec();
	if (dlg.result() == QDialog::Accepted)
	{
		if (dlg.getProjectFolder() == QString())
			return;
		QDir d_gamepath(core().defDir()+dlg.getProjectFolder());
		if (d_gamepath.exists())
		{
			int response = QMessageBox::warning(this,
							tr("Game folder exist"),
							QString(tr("The folder %1 where you want to place your new game already exist.\n"
									"Do you want to delete this folder and all it's content?")
									).arg(core().defDir()+dlg.getProjectFolder()),
							QMessageBox::Ok,
							QMessageBox::Cancel);
			if (response == QMessageBox::Cancel)
				return;
			removeDir(core().defDir()+dlg.getProjectFolder(),
					  core().defDir()+dlg.getProjectFolder());
		}
		else
			d_gamepath.mkpath(".");
		core().setTileSize(dlg.getTileSize());
		core().setDefDir(dlg.getDefDir());
		for (const QString& dir : resource_dirs)
			d_gamepath.mkpath(dir);
		m_settings.setValue(DEFAULT_DIR_KEY,dlg.getDefDir());

		const QString error = Project::create(d_gamepath, dlg.getGameTitle(), static_cast<Project::Format>(dlg.getFormat()));
		if (!error.isEmpty()) {
			QMessageBox::critical(this, tr("Creating new project failed"), error);
			return;
		}
		LoadProject(d_gamepath.absolutePath());
	}
}

bool MainWindow::removeDir(const QString & dirName, const QString &root)
{
	bool result = true;
	QDir dir(dirName);

	if (dir.exists(dirName)) {
		Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden	| QDir::AllDirs | QDir::Files, QDir::DirsFirst))
		{
			if (info.isDir())
				result = removeDir(info.absoluteFilePath(),root);
			else
				result = QFile::remove(info.absoluteFilePath());

			if (!result)
			{
				QMessageBox::warning(this,
									 tr("An error ocurred"),
									 QString(tr("Could't delete %1")).arg(info.absoluteFilePath()),
									 QMessageBox::Ok);
				return false;
			}
		}
		if (root != dirName)
			result = dir.rmdir(dirName);
	}
	return result;
}

QGraphicsView *MainWindow::getView(int id)
{
	QGraphicsView* view = m_views[id];
	if (!view)
	{
		view = new QGraphicsView(this);
		m_views[id] = view;
		view->setTransformationAnchor(QGraphicsView::NoAnchor);
		QString mapName;
		for (auto & map : core().project()->treeMap().maps) {
			if (map.ID == id) {
				mapName = ToQString(map.name);
				break;
			}
		}
		ui->tabMap->addTab(view, mapName);
		view->setScene(new MapScene(core().project()->projectData(), id, view, this));
		connect(getScene(id),
				SIGNAL(mapChanged()),
				this,
				SLOT(on_mapChanged()));
		connect(getScene(id),
				SIGNAL(mapReverted()),
				this,
				SLOT(on_mapUnchanged()));
		connect(getScene(id),
				SIGNAL(mapSaved()),
				this,
				SLOT(on_mapUnchanged()));
		connect(getScene(id), &MapScene::mapReloaded, this, &MainWindow::onMapReloaded);
		connect(getScene(id), &MapScene::scaleChanged, this, &MainWindow::updateZoomActions);
		connect(getScene(id), &MapScene::cellHovered, this, [this](const QString& text) {
			if (sender() == currentScene())
				m_cellLabel->setText(text);
		});
		core().setCurrentMapEvents(getScene(id)->mapEvents());
		getScene(id)->setScale(static_cast<float>(default_map_scale));
		getScene(id)->Init();
	}
	return view;
}

MapScene *MainWindow::getScene(int id)
{
	QGraphicsView* view = m_views.value(id);
	if (!view)
		return nullptr;
	return (static_cast<MapScene*>(view->scene()));
}

QGraphicsView *MainWindow::getTabView(int index)
{
	if (index == -1)
		return nullptr;
	return (static_cast<QGraphicsView*>(ui->tabMap->widget(index)));
}

MapScene *MainWindow::getTabScene(int index)
{
	if (!getTabView(index))
		return nullptr;
	QGraphicsView* view = getTabView(index);
	return (static_cast<MapScene*>(view->scene()));
}

MapScene *MainWindow::currentScene()
{
	QGraphicsView* view = static_cast<QGraphicsView*>(ui->tabMap->currentWidget());
	if (!view)
		return nullptr;
	return (static_cast<MapScene*>(view->scene()));
}

void MainWindow::removeView(int id)
{
	QGraphicsView* view = m_views.value(id);
	if (!view)
		return;
	MapScene* scene = static_cast<MapScene*>(view->scene());
	m_views.remove(id);
	delete scene;
	delete view;
}

void MainWindow::updateLayerActions()
{
	ui->actionLayerLower->setChecked(core().layer() == Core::LOWER);
	ui->actionLayerUpper->setChecked(core().layer() == Core::UPPER);
	ui->actionLayerEvents->setChecked(core().layer() == Core::EVENT);
}

void MainWindow::updateToolActions()
{
	ui->actionZoom->setChecked(core().tool() == Core::ZOOM);
	ui->actionDrawPen->setChecked(core().tool() == Core::PENCIL);
	ui->actionDrawRectangle->setChecked(core().tool() == Core::RECTANGLE);
	ui->actionDrawCircle->setChecked(core().tool() == Core::CIRCLE);
	ui->actionDrawSelect->setChecked(core().tool() == Core::SELECT);
	ui->actionDrawFill->setChecked(core().tool() == Core::FILL);
}

void MainWindow::updateZoomActions()
{
	MapScene* scene = currentScene();
	for (QAction* action : m_zoomGroup->actions())
		action->setChecked(scene && qFuzzyCompare(scene->scale(), scene->snapScale(action->data().toFloat())));
}

void MainWindow::updateStatusBar()
{
	MapScene* scene = currentScene();
	if (!scene) {
		m_editModeLabel->clear();
		m_mapLabel->clear();
		m_cellLabel->clear();
		return;
	}
	QString mode;
	switch (core().layer()) {
	case Core::LOWER:
		mode = tr("Lower Layer Editing Mode");
		break;
	case Core::UPPER:
		mode = tr("Upper Layer Editing Mode");
		break;
	case Core::EVENT:
		mode = tr("Event Editing Mode");
		break;
	}
	// the tools draw on the tile layers only
	if (core().layer() != Core::EVENT) {
		QString tool;
		switch (core().tool()) {
		case Core::ZOOM:
			tool = tr("Zoom Tool");
			break;
		case Core::PENCIL:
			tool = tr("Pen Tool");
			break;
		case Core::RECTANGLE:
			tool = tr("Rectangle Tool");
			break;
		case Core::CIRCLE:
			tool = tr("Ellipse Tool");
			break;
		case Core::FILL:
			tool = tr("Flood Fill");
			break;
		case Core::SELECT:
			tool = tr("Selection Tool");
			break;
		}
		//: Status bar: the edit mode and the tool
		mode = tr("%1 - %2").arg(mode, tool);
	}
	m_editModeLabel->setText(mode);
	//: Status bar: the id and the name of the map shown
	m_mapLabel->setText(tr("%1: %2").arg(scene->id(), 4, 10, QChar('0')).arg(scene->mapName()));
}

void MainWindow::on_actionProjectClose_triggered()
{
	int result = saveAll();

	if (result == true) {
		saveWorkspace();
		m_settings.setValue(CURRENT_PROJECT_KEY, QString());
		ui->treeMap->clear();
		m_treeItems.clear();
		while (ui->tabMap->currentIndex() != -1)
			removeView(currentScene()->id());

		// its files are the project's
		if (auto* jukebox = findChild<JukeboxDialog*>(QString(), Qt::FindDirectChildrenOnly))
			jukebox->close();
		core().project().reset();
		updateWatcher();

		m_copiedMap.reset();

		update_actions();
		setWindowTitle(tr("EasyRPG Editor"));
	}
}

void MainWindow::on_actionProjectOpen_triggered()
{
	OpenProjectDialog dlg(this);
	dlg.setDefaultDir(core().defDir());
	if (dlg.exec() == QDialog::Accepted)
		LoadProject(dlg.getProject()->projectDir().absolutePath());
	core().setDefDir(dlg.getDefaultDir());
	m_settings.setValue(DEFAULT_DIR_KEY,dlg.getDefaultDir());
}

void MainWindow::on_actionJukebox_triggered(bool)
{
	// one window, kept between openings (size, tab, selection); it lists the files again when shown
	auto* jukebox = findChild<JukeboxDialog*>(QString(), Qt::FindDirectChildrenOnly);
	if (!jukebox) {
		jukebox = new JukeboxDialog(this);
	} else if (jukebox->isVisible()) {
		jukebox->refresh();
	}
	jukebox->show();
	jukebox->raise();
	jukebox->activateWindow();
}

void MainWindow::on_actionLayerLower_triggered()
{
	core().setLayer(Core::LOWER);
	updateLayerActions();
}

void MainWindow::on_actionLayerUpper_triggered()
{
	core().setLayer(Core::UPPER);
	updateLayerActions();
}

void MainWindow::on_actionLayerEvents_triggered()
{
	core().setLayer(Core::EVENT);
	updateLayerActions();
}

void MainWindow::on_actionZoomIn_triggered()
{
	MapScene* scene = currentScene();
	if (scene && static_cast<double>(scene->scale()) < 8.0)
		scene->setScale(scene->scale()*2);
}

void MainWindow::on_actionZoomOut_triggered()
{
	MapScene* scene = currentScene();
	if (scene && static_cast<double>(scene->scale()) > 0.125)
		scene->setScale(scene->scale()/2);
}

void MainWindow::on_actionZoom100_triggered()
{
	// the normal view, as maps open (1/2)
	if (currentScene())
		currentScene()->setScale(static_cast<float>(default_map_scale));
}

void MainWindow::on_treeMap_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
	if (!item)
		return;
	const int id = item->data(1,Qt::DisplayRole).toInt();
	if (id == 0)
		return;
	if (mapType(id) == lcf::rpg::TreeMap::MapType_area)
	{
		// an area has no map file: it is shown on its map
		QTreeWidgetItem* parent = item->parent();
		if (parent && parent->data(1, Qt::DisplayRole).toInt() != 0)
		{
			const int parent_id = parent->data(1, Qt::DisplayRole).toInt();
			on_treeMap_itemDoubleClicked(parent, column);
			ui->treeMap->setCurrentItem(item);
			const auto* info = mapInfo(id);
			if (MapScene* scene = m_views.value(parent_id) ? getScene(parent_id) : nullptr; scene && info) {
				const auto& r = info->area_rect;
				scene->showArea(QRect(r.l, r.t, r.r - r.l, r.b - r.t));
			}
		}
		return;
	}
	if (!m_views.value(id) && !core().project()->loadMap(id))
	{
		QMessageBox::warning(this, tr("Open Map"),
			tr("The file of the map %1 (%2) is missing or cannot be read.").arg(item->text(0), mapFileName(id)));
		return;
	}
	QGraphicsView *view = getView(id);
	ui->tabMap->setCurrentWidget(view);
	if (ui->tabMap->count() == 1)
		m_paletteScene->onChipsetChange();
}

void MainWindow::on_tabMap_tabCloseRequested(int index)
{
	if (!getTabScene(index))
		return;
	if (getTabScene(index)->isModified())
	{
		int result = QMessageBox::question(this,
										   tr("Save map changes"),
										   QString(tr("%1 has unsaved changes.\n"
										   "Do you want to save them before closing"
										   " it?")).arg(getTabScene(index)->mapName()),
										   QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		switch (result)
		{
		case (QMessageBox::Yes):
			getTabScene(index)->Save();
			break;
		case (QMessageBox::No):
			break;
		case (QMessageBox::Cancel):
			return;
		}
	}
	removeView(getTabScene(index)->id());
}

void MainWindow::on_tabMap_currentChanged(int index)
{
	if (index == -1)
	{
		for (int i = 0; i < m_paletteScene->items().size(); i++)
			m_paletteScene->items()[i]->setVisible(false);
		update_actions();
		return;
	}
	m_cellLabel->clear();
	if (currentScene())
	{
		core().LoadChipset(currentScene()->chipsetId());
		core().setCurrentMapEvents(currentScene()->mapEvents());
		// the tree shows the map of the tab, Map Properties edits it
		if (QTreeWidgetItem* item = m_treeItems.value(currentScene()->id()))
			ui->treeMap->setCurrentItem(item);
	}
	if (QGraphicsView* view = getTabView(index); view && view->property("workspace_scroll").isValid()) {
		// Hidden tabs receive their final viewport size only when selected.
		QTimer::singleShot(0, view, [view] {
			if (!view->isVisible() || !view->property("workspace_scroll").isValid())
				return;
			const QVariantMap scroll = view->property("workspace_scroll").toMap();
			view->horizontalScrollBar()->setValue(scroll.value("x").toInt());
			view->verticalScrollBar()->setValue(scroll.value("y").toInt());
			view->setProperty("workspace_scroll", QVariant());
		});
	}
	update_actions();
}

void MainWindow::on_actionProjectImport_triggered()
{
	ImportProjectDialog dlg(this);
	dlg.setDefDir(core().defDir());
	dlg.exec();
	if (dlg.result() == QDialog::Accepted) {
		core().setDefDir(dlg.getDefDir());
		if (dlg.getProjectFolder() == QString())
			return;
		QDir d_gamepath(dlg.getDefDir()+dlg.getProjectFolder());
			if (d_gamepath.exists())
			{
				int response = QMessageBox::warning(this,
								tr("Game folder exist"),
								QString(tr("The folder %1 where you want to place your new game already exist.\n"
										"Do you want to delete this folder and all it's content?")
										).arg(dlg.getDefDir()+dlg.getProjectFolder()),
								QMessageBox::Ok,
								QMessageBox::Cancel);
				if (response == QMessageBox::Cancel)
					return;
				removeDir(dlg.getDefDir()+dlg.getProjectFolder(),
						  dlg.getDefDir()+dlg.getProjectFolder());
			}
			else
				d_gamepath.mkpath(".");
		core().setTileSize(16);
		ImportProject(dlg.getSourceFolder(), d_gamepath, dlg.getConvertXYZ());
	}
	m_settings.setValue(DEFAULT_DIR_KEY, dlg.getDefDir());
	update_actions();
}

void MainWindow::on_actionSettings_triggered()
{
	SettingsDialog dialog(this);
	dialog.exec();
}

void MainWindow::setPlatformShortcuts()
{
	// the keys of the platform, which the .ui cannot say (set again after
	// retranslateUi, which resets the shortcuts of the .ui)
	ui->actionQuit->setShortcuts(QKeySequence::Quit);
	ui->actionSettings->setShortcut(QKeySequence::Preferences);
#ifdef Q_OS_MACOS
	// the Delete key of Mac keyboards is Backspace
	for (QAction* action : {ui->actionMapDelete, ui->actionAreaDelete})
		action->setShortcuts({QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace)});
#endif
}

void MainWindow::changeEvent(QEvent* event)
{
	QMainWindow::changeEvent(event);
	if (event->type() == QEvent::PaletteChange)
		refreshIcons();
	if (event->type() != QEvent::LanguageChange)
		return;
	// the texts again, in the language just chosen
	ui->retranslateUi(this);
	setPlatformShortcuts();
	setWindowTitle(core().project() ? tr("EasyRPG Editor") + " - " + core().project()->gameTitle() : tr("EasyRPG Editor"));
	updateStatusBar();
	for (int id : m_views.keys())
		if (MapScene* scene = getScene(id))
			scene->retranslate();
}

void MainWindow::applySettings()
{
	const EditorSettings& settings = EditorSettings::instance();
	core().setRtpDir(settings.rtpPath());
	core().setAnimationEnabled(settings.animateTiles());
	if (searchdialog)
		searchdialog->enableCache(settings.searchCache());
	updateWatcher();
	// a new language at once: every window shows its texts again (the one
	// the Editor started in can come from --language, not from the settings)
	if (m_languageSetting && *m_languageSetting != settings.language())
		Translations::apply(settings.language());
	m_languageSetting = settings.language();
}

void MainWindow::on_actionZoom_triggered()
{
	core().setTool(Core::ZOOM);
}

void MainWindow::on_actionDrawPen_triggered()
{
	core().setTool(Core::PENCIL);
}

void MainWindow::on_actionDrawRectangle_triggered()
{
	core().setTool(Core::RECTANGLE);
}

void MainWindow::on_actionDrawCircle_triggered()
{
	core().setTool(Core::CIRCLE);
}

void MainWindow::on_actionDrawSelect_triggered()
{
	core().setTool(Core::SELECT);
}

void MainWindow::on_actionDrawFill_triggered()
{
	core().setTool(Core::FILL);
}

void MainWindow::on_actionPlayTest_triggered()
{
	RunGameDialog::launch(this, {});
}

void MainWindow::on_actionPlayTestOptions_triggered()
{
	RunGameDialog dlg(core().project()->projectData(), this);
	dlg.exec();
}

void MainWindow::on_actionFullScreenToggle_toggled(bool checked)
{
	m_settings.setValue(TESTPLAY_FULLSCREEN_KEY, checked);
}

void MainWindow::on_actionTitleBackgroundToggle_toggled(bool checked)
{
	m_settings.setValue(TESTPLAY_SHOW_TITLE_KEY, checked);
}

bool MainWindow::saveBeforeTestPlay()
{
	bool modified = false;
	for (int i = 0; i < ui->tabMap->count(); i++)
		modified = modified || getTabScene(i)->isModified();
	if (!modified)
		return true;
	// The Player reads the files: what is not saved is not tested
	int result = QMessageBox::question(this, tr("Test Play"),
		tr("Some maps have unsaved changes.\nSave them before the test play?"),
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	if (result == QMessageBox::Cancel)
		return false;
	if (result == QMessageBox::Yes)
		for (int i = 0; i < ui->tabMap->count(); i++)
			getTabScene(i)->Save();
	return true;
}

MapScene* MainWindow::senderScene()
{
	return qobject_cast<MapScene*>(sender());
}

void MainWindow::setTabText(MapScene* scene)
{
	// the scene that changed, which is not always the current tab
	int index = ui->tabMap->indexOf(m_views.value(scene->id()));
	if (index >= 0)
		ui->tabMap->setTabText(index, scene->mapName() + (scene->isModified() ? " *" : ""));
}

void MainWindow::on_mapChanged()
{
	update_actions();
	if (MapScene* scene = senderScene())
		setTabText(scene);
}

void MainWindow::on_mapUnchanged()
{
	update_actions();
	if (MapScene* scene = senderScene())
		setTabText(scene);
	// A saved map writes its copy of its tree entry: keep the tree's
	// parents and order, which can have changed since it was opened
	if (core().project() && syncTreeOrder())
		core().project()->saveTreeMap();
}

void MainWindow::on_actionUndo_triggered()
{
	if (currentScene())
		currentScene()->undo();
}

void MainWindow::on_actionRedo_triggered()
{
	if (currentScene())
		currentScene()->redo();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	if (!saveAll())
		event->ignore();
	else {
		saveWorkspace();
		QMainWindow::closeEvent(event);
	}
}

void MainWindow::saveWorkspace()
{
	m_settings.setValue(QStringLiteral("main_window_geometry"), saveGeometry());
	if (!core().project()) {
		m_settings.sync();
		return;
	}
	QVariantList maps;
	for (int i = 0; i < ui->tabMap->count(); ++i) {
		QGraphicsView* view = getTabView(i);
		MapScene* scene = getTabScene(i);
		QVariantMap scroll = view->property("workspace_scroll").toMap();
		if (scroll.isEmpty())
			scroll = {{"x", view->horizontalScrollBar()->value()}, {"y", view->verticalScrollBar()->value()}};
		maps.append(QVariantMap{{"id", scene->id()}, {"zoom", scene->scale()}, {"scroll", scroll}});
	}
	QVariantList expanded;
	for (auto it = m_treeItems.cbegin(); it != m_treeItems.cend(); ++it)
		if (it.value()->isExpanded())
			expanded.append(it.key());
	QVariantList tiles;
	for (int y = 0; y < core().selHeight(); ++y)
		for (int x = 0; x < core().selWidth(); ++x)
			tiles.append(core().selection(x, y));
	const QVariantMap workspace = {
		{"maps", maps}, {"active_map", currentScene() ? currentScene()->id() : -1},
		{"selected_map", currentMapId()}, {"expanded", expanded},
		{"tree_scroll", ui->treeMap->verticalScrollBar()->value()},
		{"layer", core().layer()}, {"tool", core().tool()},
		{"tiles", tiles}, {"tile_width", core().selWidth()}, {"tile_height", core().selHeight()},
		{"palette_x", ui->graphicsPalette->horizontalScrollBar()->value()},
		{"palette_y", ui->graphicsPalette->verticalScrollBar()->value()},
		{"panels", saveState()}
	};
	const QString key = "project_workspace/" + QString::fromLatin1(QCryptographicHash::hash(
		core().project()->projectDir().absolutePath().toUtf8(), QCryptographicHash::Sha256).toHex());
	m_settings.setValue(key, workspace);
	m_settings.sync();
}

void MainWindow::restoreWorkspace()
{
	const QString key = "project_workspace/" + QString::fromLatin1(QCryptographicHash::hash(
		core().project()->projectDir().absolutePath().toUtf8(), QCryptographicHash::Sha256).toHex());
	const QVariantMap workspace = m_settings.value(key).toMap();
	populateMapTree(!workspace.contains("maps"));
	core().setLayer(static_cast<Core::Layer>(std::clamp(workspace.value("layer", Core::LOWER).toInt(),
		static_cast<int>(Core::LOWER), static_cast<int>(Core::EVENT))));
	core().setTool(static_cast<Core::Tool>(std::clamp(workspace.value("tool", Core::PENCIL).toInt(),
		static_cast<int>(Core::ZOOM), static_cast<int>(Core::SELECT))));
	if (workspace.isEmpty())
		return;
	restoreState(workspace.value("panels").toByteArray());
	// Older workspaces may have hidden it through Qt's toolbar context menu.
	ui->mainToolBar->show();
	ui->actionPaletteToggle->setChecked(!ui->dockPalette->isHidden());
	ui->actionMapTreeToggle->setChecked(!ui->dockMapTree->isHidden());
	for (const QVariant& entry : workspace.value("maps").toList()) {
		const QVariantMap map = entry.toMap();
		const int id = map.value("id", -1).toInt();
		// Removed maps, areas and unreadable files cannot be restored as tabs.
		if (mapType(id) != lcf::rpg::TreeMap::MapType_map || getScene(id) || !core().project()->loadMap(id))
			continue;
		QGraphicsView* view = getView(id);
		getScene(id)->setScale(map.value("zoom", default_map_scale).toFloat());
		view->setProperty("workspace_scroll", map.value("scroll"));
	}
	if (QGraphicsView* view = m_views.value(workspace.value("active_map", -1).toInt()))
		ui->tabMap->setCurrentWidget(view);
	on_tabMap_currentChanged(ui->tabMap->currentIndex());
	{
		QSignalBlocker blocker(ui->treeMap);
		const QVariantList expanded = workspace.value("expanded").toList();
		for (auto it = m_treeItems.cbegin(); it != m_treeItems.cend(); ++it)
			it.value()->setExpanded(expanded.contains(QVariant(it.key())));
		if (QTreeWidgetItem* item = m_treeItems.value(workspace.value("selected_map", -1).toInt()))
			ui->treeMap->setCurrentItem(item);
	}
	const int width = workspace.value("tile_width").toInt();
	const int height = workspace.value("tile_height").toInt();
	const QVariantList tiles = workspace.value("tiles").toList();
	if (width > 0 && width <= 6 && height > 0 && height <= 30 && tiles.size() == width * height) {
		std::vector<short> selection;
		for (const QVariant& tile : tiles)
			selection.push_back(static_cast<short>(tile.toInt()));
		core().setSelection(std::move(selection), width, height);
	}
	QTimer::singleShot(0, this, [this, workspace, project = core().project()] {
		if (core().project() != project)
			return;
		ui->treeMap->verticalScrollBar()->setValue(workspace.value("tree_scroll").toInt());
		ui->graphicsPalette->horizontalScrollBar()->setValue(workspace.value("palette_x").toInt());
		ui->graphicsPalette->verticalScrollBar()->setValue(workspace.value("palette_y").toInt());
	});
}

bool MainWindow::saveAll()
{
	bool need_save = false;
	for (int i = 0; i < ui->tabMap->count(); i++)
		if (getTabScene(i)->isModified())
		{
			need_save = true;
			break;
		}
	if (need_save)
	{
		int result = QMessageBox::question(this,
										   tr("Save map changes"),
										   tr("Some maps have unsaved changes.\n"
										   "Do you want to save them before closing them?"),
										   QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		switch (result)
		{
		case (QMessageBox::Yes):
			for (int i = 0; i < ui->tabMap->count(); i++)
				getTabScene(i)->Save();
			break;
 //		case (No): do nothing;
		case (QMessageBox::Cancel):
			return false;
		}
	}

	// The map tree is written whenever the Editor changes it (maps saved,
	// added, removed...): writing it again here could only overwrite the
	// changes of another program
	return true;
}

void MainWindow::on_actionMapSave_triggered()
{
	if(currentScene())
		currentScene()->Save();
}

void MainWindow::on_actionMapRevert_triggered()
{
	if (!currentScene())
		return;
	if (QMessageBox::question(this, tr("Revert Map"),
			tr("Map data will revert to the last time it was saved. Proceed?")) != QMessageBox::Yes)
		return;
	currentScene()->Load(true);
}


void MainWindow::on_treeMap_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
	Q_UNUSED(previous)
	// what can be done depends on the node: project, map or area
	update_actions();
	// As RPG Maker: the map selected in the tree (a click, the arrow keys) is
	// the map edited. The Editor's own selections (a tab shown) open nothing
	if (current && ui->treeMap->hasFocus())
		openTreeItem(current);
}

void MainWindow::openTreeItem(QTreeWidgetItem* item)
{
	// opening an area selects its map then the area again: once
	if (m_openingTreeItem)
		return;
	m_openingTreeItem = true;
	on_treeMap_itemDoubleClicked(item, 0);
	m_openingTreeItem = false;
}

int MainWindow::currentMapId() const
{
	QTreeWidgetItem* item = ui->treeMap->currentItem();
	return item ? item->data(1, Qt::DisplayRole).toInt() : -1;
}

int MainWindow::mapType(int id) const
{
	if (!core().project() || id < 0)
		return -1;
	if (id == 0)
		return lcf::rpg::TreeMap::MapType_root;
	for (const auto& info : core().project()->treeMap().maps)
		if (info.ID == id)
			return info.type;
	return -1;
}

lcf::rpg::MapInfo* MainWindow::mapInfo(int id)
{
	if (!core().project())
		return nullptr;
	for (auto& info : core().project()->treeMap().maps)
		if (info.ID == id)
			return &info;
	return nullptr;
}

int MainWindow::freeMapId() const
{
	// RPG Maker takes the first free id, maps and areas share them
	std::set<int> used;
	for (const auto& info : core().project()->treeMap().maps)
		used.insert(info.ID);
	int id = 1;
	while (used.count(id))
		id++;
	return id;
}

QTreeWidgetItem* MainWindow::addTreeItem(const lcf::rpg::MapInfo& info)
{
	QTreeWidgetItem *item = new QTreeWidgetItem();
	item->setData(1, Qt::DisplayRole, info.ID);
	item->setData(0, Qt::DisplayRole, ToQString(info.name));
	if (info.type == lcf::rpg::TreeMap::MapType_area)
	{
		item->setIcon(0, themeIcon(this, "select"));
		// an area belongs to its map: it is not moved and holds nothing
		item->setFlags(item->flags() & ~(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
	}
	m_treeItems[info.ID] = item;
	return item;
}

QTreeWidgetItem* MainWindow::insertMapInfo(const lcf::rpg::MapInfo& info)
{
	// sorted by id: the Player looks the maps up by id
	auto& maps = core().project()->treeMap().maps;
	auto pos = std::lower_bound(maps.begin(), maps.end(), info.ID,
		[](const lcf::rpg::MapInfo& other, int id) { return other.ID < id; });
	maps.insert(pos, info);

	QTreeWidgetItem* parent = m_treeItems.value(info.parent_map, m_treeItems.value(0));
	QTreeWidgetItem* item = addTreeItem(info);
	parent->addChild(item);
	parent->setExpanded(true);
	syncTreeOrder();
	core().project()->saveTreeMap();
	ui->treeMap->setCurrentItem(item);
	return item;
}

bool MainWindow::syncTreeOrder()
{
	auto& tree = core().project()->treeMap();
	const auto old_maps = tree.maps;
	const auto old_order = tree.tree_order;

	// tree_order lists the nodes depth first, parents before children
	std::vector<int32_t> order;
	for (QTreeWidgetItemIterator it(ui->treeMap); *it; ++it)
	{
		const int id = (*it)->data(1, Qt::DisplayRole).toInt();
		order.push_back(id);
		lcf::rpg::MapInfo* info = mapInfo(id);
		if (!info)
			continue;
		info->expanded_node = (*it)->isExpanded();
		if (id == 0)
			continue;
		QTreeWidgetItem* parent = (*it)->parent();
		info->parent_map = parent ? parent->data(1, Qt::DisplayRole).toInt() : 0;
		int depth = 0;
		for (; parent; parent = parent->parent())
			depth++;
		info->indentation = depth;
	}
	tree.tree_order = order;
	return tree.tree_order != old_order || tree.maps != old_maps;
}

QSize MainWindow::mapSize(int id)
{
	auto map = core().project()->loadMap(id);
	return map ? QSize(map->width, map->height) : QSize();
}

void MainWindow::onMapTreeContextMenu(const QPoint& pos)
{
	if (!core().project())
		return;
	// a click beside the maps is a click on the project
	QTreeWidgetItem* item = ui->treeMap->itemAt(pos);
	ui->treeMap->setCurrentItem(item ? item : m_treeItems.value(0));
	update_actions();

	QMenu menu(this);
	switch (mapType(currentMapId()))
	{
	case lcf::rpg::TreeMap::MapType_map:
		menu.addAction(ui->actionMapProperties);
		menu.addSeparator();
		menu.addAction(ui->actionMapNew);
		menu.addAction(ui->actionAreaNew);
		menu.addAction(ui->actionMapGenerateDungeon);
		menu.addSeparator();
		menu.addAction(ui->actionMapCopy);
		menu.addAction(ui->actionMapPaste);
		menu.addAction(ui->actionMapDelete);
		menu.addSeparator();
		menu.addAction(ui->actionMapShift);
		break;
	case lcf::rpg::TreeMap::MapType_area:
		menu.addAction(ui->actionAreaProperties);
		menu.addSeparator();
		menu.addAction(ui->actionAreaDelete);
		break;
	default:
		menu.addAction(ui->actionMapNew);
		menu.addSeparator();
		menu.addAction(ui->actionMapPaste);
		break;
	}
	menu.exec(ui->treeMap->viewport()->mapToGlobal(pos));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
	// the type first: the tree can be gone while the window is destroyed
	if (event->type() == QEvent::Drop && watched == ui->treeMap->viewport())
	{
		// the tree moves the item itself, the map tree data follows after
		QTimer::singleShot(0, this, &MainWindow::onMapTreeDropped);
	}
	return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onMapTreeDropped()
{
	if (!core().project())
		return;
	if (syncTreeOrder())
	{
		core().project()->saveTreeMap();
		// the open maps keep a copy of their tree entry
		for (int i = 0; i < ui->tabMap->count(); i++)
			getTabScene(i)->refreshMapInfo();
	}
	update_actions();
}

void MainWindow::on_actionMapCopy_triggered()
{
	const int id = currentMapId();
	if (mapType(id) != lcf::rpg::TreeMap::MapType_map)
		return;
	MapScene* scene = getScene(id);
	if (scene && scene->isModified())
	{
		int result = QMessageBox::question(this, tr("Copy Map"),
			tr("%1 has unsaved changes.\nSave them, to copy them too?").arg(scene->mapName()),
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (result == QMessageBox::Cancel)
			return;
		if (result == QMessageBox::Yes)
			scene->Save();
	}
	auto map = core().project()->loadMap(id);
	const lcf::rpg::MapInfo* info = mapInfo(id);
	if (!map || !info)
	{
		QMessageBox::critical(this, tr("Copy Map"), tr("The file %1 cannot be read.").arg(mapFileName(id)));
		return;
	}
	m_copiedMap = std::move(map);
	m_copiedMapInfo = *info;
	update_actions();
}

void MainWindow::on_actionMapNew_triggered()
{
	const int current = currentMapId();
	lcf::rpg::MapInfo info;
	info.ID = freeMapId();
	info.name = ToDBString(tr("MAP%1").arg(info.ID, 4, 10, QChar('0')));
	info.type = lcf::rpg::TreeMap::MapType_map;
	info.parent_map = mapType(current) == lcf::rpg::TreeMap::MapType_map ? current : 0;
	if (info.parent_map == 0)
	{
		// nothing to inherit from
		info.music_type = lcf::rpg::MapInfo::MusicType_event;
		info.background_type = lcf::rpg::MapInfo::BGMType_terrain;
		info.teleport = lcf::rpg::MapInfo::TriState_allow;
		info.escape = lcf::rpg::MapInfo::TriState_allow;
		info.save = lcf::rpg::MapInfo::TriState_allow;
	}

	// RPG Maker asks for the properties first: Cancel creates nothing
	lcf::rpg::Map map = MapTools::newMap();
	MapPropertiesDialog dlg(core().project()->projectData(), info, map, this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	if (!core().project()->saveMap(map, info.ID))
	{
		QMessageBox::critical(this, tr("New Map"), tr("The file %1 cannot be written.").arg(mapFileName(info.ID)));
		return;
	}
	QTreeWidgetItem* item = insertMapInfo(info);
	on_treeMap_itemDoubleClicked(item, 0);
}

void MainWindow::on_actionMapPaste_triggered()
{
	const int current = currentMapId();
	const int type = mapType(current);
	if (!m_copiedMap || type == lcf::rpg::TreeMap::MapType_area)
		return;

	// The copy becomes a child of the selected map
	lcf::rpg::MapInfo info = m_copiedMapInfo;
	info.ID = freeMapId();
	info.type = lcf::rpg::TreeMap::MapType_map;
	info.parent_map = type == lcf::rpg::TreeMap::MapType_map ? current : 0;
	info.scrollbar_x = 0;
	info.scrollbar_y = 0;
	info.expanded_node = false;
	if (info.parent_map == 0)
	{
		if (info.music_type == lcf::rpg::MapInfo::MusicType_parent)
			info.music_type = lcf::rpg::MapInfo::MusicType_event;
		if (info.background_type == lcf::rpg::MapInfo::BGMType_parent)
			info.background_type = lcf::rpg::MapInfo::BGMType_terrain;
		if (info.teleport == lcf::rpg::MapInfo::TriState_parent)
			info.teleport = lcf::rpg::MapInfo::TriState_allow;
		if (info.escape == lcf::rpg::MapInfo::TriState_parent)
			info.escape = lcf::rpg::MapInfo::TriState_allow;
		if (info.save == lcf::rpg::MapInfo::TriState_parent)
			info.save = lcf::rpg::MapInfo::TriState_allow;
	}

	// saving counts the saves: the clipboard keeps its own copy
	lcf::rpg::Map map = *m_copiedMap;
	if (!core().project()->saveMap(map, info.ID))
	{
		QMessageBox::critical(this, tr("Paste Map"), tr("The file %1 cannot be written.").arg(mapFileName(info.ID)));
		return;
	}
	QTreeWidgetItem* item = insertMapInfo(info);
	on_treeMap_itemDoubleClicked(item, 0);
}

void MainWindow::on_actionMapDelete_triggered()
{
	const int id = currentMapId();
	if (mapType(id) != lcf::rpg::TreeMap::MapType_map)
		return;
	int result = QMessageBox::question(this,
		tr("Delete Map"),
		tr("%1 will be deleted, with its child maps and areas.\nThis cannot be undone. Proceed?")
			.arg(m_treeItems.value(id)->text(0)),
		QMessageBox::Yes | QMessageBox::No);
	if (result != QMessageBox::Yes)
		return;

	removeMap(id);
	syncTreeOrder();
	core().project()->saveTreeMap();
	update_actions();
}

void MainWindow::removeMap(const int id)
{
	QTreeWidgetItem* item = m_treeItems.value(id);
	if (!item || id == 0)
		return;

	// Children first: each call takes its child out of the item
	while (item->childCount() > 0)
	{
		QTreeWidgetItem* child = item->child(0);
		const int child_id = child->data(1, Qt::DisplayRole).toInt();
		if (m_treeItems.value(child_id) == child)
			removeMap(child_id);
		else
			delete item->takeChild(0);
	}

	// areas have no file
	if (mapType(id) == lcf::rpg::TreeMap::MapType_map)
	{
		removeView(id);
		const QString path = core().project()->findFile(mapFileName(id));
		if (!path.isEmpty())
			QFile::remove(path);
		else
			qWarning() << QString("file not found: %1").arg(mapFileName(id));
	}

	auto& tree = core().project()->treeMap();
	tree.maps.erase(std::remove_if(tree.maps.begin(), tree.maps.end(),
		[id](const lcf::rpg::MapInfo& info) { return info.ID == id; }), tree.maps.end());
	tree.tree_order.erase(std::remove(tree.tree_order.begin(), tree.tree_order.end(), id), tree.tree_order.end());

	m_treeItems.remove(id);
	delete item;
}

void MainWindow::on_actionMapProperties_triggered()
{
	// the map selected in the tree, else the map shown
	int id = currentMapId();
	const int type = mapType(id);
	if (type == lcf::rpg::TreeMap::MapType_area)
	{
		on_actionAreaProperties_triggered();
		return;
	}
	if (type != lcf::rpg::TreeMap::MapType_map)
	{
		if (!currentScene())
			return;
		id = currentScene()->id();
	}

	if (MapScene* scene = getScene(id))
	{
		scene->editMapProperties(m_treeItems.value(id));
		return;
	}

	// a map that is not open is edited on disk
	auto map = core().project()->loadMap(id);
	if (!map || !mapInfo(id))
	{
		QMessageBox::critical(this, tr("Map Properties"), tr("The file %1 cannot be read.").arg(mapFileName(id)));
		return;
	}
	lcf::rpg::MapInfo info = *mapInfo(id);
	MapPropertiesDialog dlg(core().project()->projectData(), info, *map, this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	*mapInfo(id) = info;
	if (!core().project()->saveMap(*map, id))
		QMessageBox::critical(this, tr("Map Properties"), tr("The file %1 cannot be written.").arg(mapFileName(id)));
	core().project()->saveTreeMap();
	if (QTreeWidgetItem* item = m_treeItems.value(id))
		item->setText(0, ToQString(info.name));
}

void MainWindow::on_actionMapGenerateDungeon_triggered()
{
	const int id = currentMapId();
	if (mapType(id) != lcf::rpg::TreeMap::MapType_map)
		return;
	const MapScene* scene = getScene(id);
	auto source = scene ? std::make_unique<lcf::rpg::Map>(scene->mapData()) : core().project()->loadMap(id);
	const lcf::rpg::MapInfo* source_info = scene ? &scene->mapInfo() : mapInfo(id);
	if (!source || !source_info)
	{
		QMessageBox::critical(this, tr("Generate Dungeon"), tr("The file %1 cannot be read.").arg(mapFileName(id)));
		return;
	}
	if (!source->generator_flag)
	{
		QMessageBox::information(this, tr("Generate Dungeon"),
			tr("%1 does not use the Random Dungeon Generator.\n"
				"Check \"Use Random Dungeon Generator\" in its Map Properties first.").arg(ToQString(source_info->name)));
		return;
	}

	// help: "a new map, retaining the same settings"; a child of the source
	lcf::rpg::MapInfo info = *source_info;
	info.ID = freeMapId();
	info.parent_map = id;
	info.scrollbar_x = 0;
	info.scrollbar_y = 0;
	info.expanded_node = false;
	GenerateDungeonDialog dlg(core().project()->projectData(), *source,
		tr("MAP%1").arg(info.ID, 4, 10, QChar('0')), this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	info.name = ToDBString(dlg.name());
	lcf::rpg::Map map = dlg.map();
	if (!core().project()->saveMap(map, info.ID))
	{
		QMessageBox::critical(this, tr("Generate Dungeon"), tr("The file %1 cannot be written.").arg(mapFileName(info.ID)));
		return;
	}
	QTreeWidgetItem* item = insertMapInfo(info);
	on_treeMap_itemDoubleClicked(item, 0);
}

void MainWindow::on_actionMapShift_triggered()
{
	const int id = currentMapId();
	if (mapType(id) != lcf::rpg::TreeMap::MapType_map)
		return;
	ShiftMapDialog dlg(this);
	if (dlg.exec() != QDialog::Accepted)
		return;

	// The map is shifted on disk, then the open tab reads it again
	MapScene* scene = getScene(id);
	if (scene && scene->isModified())
	{
		if (QMessageBox::question(this, tr("Shift Map"),
				tr("%1 has unsaved changes. They are saved before the map is shifted. Proceed?").arg(scene->mapName()))
				!= QMessageBox::Yes)
			return;
		scene->Save();
	}
	auto map = core().project()->loadMap(id);
	if (!map)
	{
		QMessageBox::critical(this, tr("Shift Map"), tr("The file %1 cannot be read.").arg(mapFileName(id)));
		return;
	}
	const int dx = dlg.dx();
	const int dy = dlg.dy();
	MapTools::shiftMap(*map, dx, dy);
	if (!core().project()->saveMap(*map, id))
	{
		QMessageBox::critical(this, tr("Shift Map"), tr("The file %1 cannot be written.").arg(mapFileName(id)));
		return;
	}

	// the start positions on this map move with it
	auto& start = core().project()->treeMap().start;
	bool start_moved = false;
	auto shiftStart = [&](int map_id, int32_t& x, int32_t& y) {
		if (map_id == id)
		{
			x = MapTools::wrap(x + dx, map->width);
			y = MapTools::wrap(y + dy, map->height);
			start_moved = true;
		}
	};
	shiftStart(start.party_map_id, start.party_x, start.party_y);
	shiftStart(start.boat_map_id, start.boat_x, start.boat_y);
	shiftStart(start.ship_map_id, start.ship_x, start.ship_y);
	shiftStart(start.airship_map_id, start.airship_x, start.airship_y);
	if (start_moved)
		core().project()->saveTreeMap();

	if (scene)
		scene->reloadFromDisk();
}

void MainWindow::on_actionAreaNew_triggered()
{
	const int parent_id = currentMapId();
	if (mapType(parent_id) != lcf::rpg::TreeMap::MapType_map)
		return;
	const QSize size = mapSize(parent_id);
	if (!size.isValid())
	{
		QMessageBox::critical(this, tr("Create Area"), tr("The file %1 cannot be read.").arg(mapFileName(parent_id)));
		return;
	}

	lcf::rpg::MapInfo info;
	info.ID = freeMapId();
	info.name = ToDBString(tr("AREA%1").arg(info.ID, 4, 10, QChar('0')));
	info.type = lcf::rpg::TreeMap::MapType_area;
	info.parent_map = parent_id;
	info.area_rect.r = static_cast<uint32_t>(std::min(5, size.width()));
	info.area_rect.b = static_cast<uint32_t>(std::min(5, size.height()));
	AreaPropertiesDialog dlg(core().project()->projectData(), info, size.width(), size.height(), this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	insertMapInfo(info);
}

void MainWindow::on_actionAreaProperties_triggered()
{
	const int id = currentMapId();
	lcf::rpg::MapInfo* stored = mapInfo(id);
	if (!stored || stored->type != lcf::rpg::TreeMap::MapType_area)
		return;
	const QSize size = mapSize(stored->parent_map);
	if (!size.isValid())
	{
		QMessageBox::critical(this, tr("Area Properties"), tr("The file %1 cannot be read.").arg(mapFileName(stored->parent_map)));
		return;
	}
	lcf::rpg::MapInfo info = *stored;
	AreaPropertiesDialog dlg(core().project()->projectData(), info, size.width(), size.height(), this);
	if (dlg.exec() != QDialog::Accepted)
		return;
	*mapInfo(id) = info;
	core().project()->saveTreeMap();
	if (QTreeWidgetItem* item = m_treeItems.value(id))
		item->setText(0, ToQString(info.name));
}

void MainWindow::on_actionAreaDelete_triggered()
{
	const int id = currentMapId();
	if (mapType(id) != lcf::rpg::TreeMap::MapType_area)
		return;
	if (QMessageBox::question(this, tr("Delete Area"),
			tr("%1 will be deleted. Proceed?").arg(m_treeItems.value(id)->text(0))) != QMessageBox::Yes)
		return;
	removeMap(id);
	syncTreeOrder();
	core().project()->saveTreeMap();
	update_actions();
}

void MainWindow::on_actionSearch_triggered()
{
	searchdialog->updateUI();
	searchdialog->setVisible(true);
}

void MainWindow::updateSearchUI()
{
	searchdialog->updateUI();
}

void MainWindow::openScene(int mapID)
{
	this->on_treeMap_itemDoubleClicked(m_treeItems.value(mapID), 0);
}

bool MainWindow::hasMap(int mapID) const
{
	return mapID > 0 && m_treeItems.value(mapID) != nullptr;
}

void MainWindow::selectTile(int x, int y)
{
	if (currentScene())
	{
		core().setLayer(Core::EVENT);
		updateLayerActions();

		currentScene()->selectTile(x, y);
	}
}

void MainWindow::refreshIcons() {
	auto set = [this](QAction* action, const QString& icon) {
		action->setIcon(themeIcon(this, icon));
	};

	// File
	set(ui->actionProjectNew, "project-new");
	set(ui->actionProjectOpen, "project-open");
	set(ui->actionProjectClose, "project-close");
	set(ui->actionQuit, "exit");

	// Map
	set(ui->actionMapProperties, "properties");
	set(ui->actionMapSave, "save");
	set(ui->actionMapRevert, "revert");
	set(ui->actionLayerEvents, "layer-event");
	set(ui->actionLayerUpper, "layer-high");
	set(ui->actionLayerLower, "layer-low");
	set(ui->actionZoom100, "zoom-original");
	set(ui->actionZoomIn, "zoom-in");
	set(ui->actionZoomOut, "zoom-out");
	set(ui->actionPaletteToggle, "palette");
	set(ui->actionMapTreeToggle, "tree");

	// Tilemap
	set(ui->actionDrawPen, "draw-pen");
	set(ui->actionDrawCircle, "draw-circle");
	set(ui->actionDrawSelect, "select");
	set(ui->actionDrawRectangle, "draw-rectangle");
	set(ui->actionDrawFill, "draw-fill");
	set(ui->actionUndo, "edit-undo");
	set(ui->actionRedo, "edit-redo");
	set(ui->actionZoom, "zoom");

	// Map Tree
	set(ui->actionMapNew, "new");
	set(ui->actionMapCopy, "edit-copy");
	set(ui->actionMapPaste, "edit-paste");
	set(ui->actionMapDelete, "edit-delete");

	// Tools
	set(ui->actionDatabase, "database");
	set(ui->actionJukebox, "audio");
	set(ui->actionResourceManager, "resources");
	set(ui->actionSearch, "edit-find");

	// Other
	set(ui->actionGameDiskCreate, "gamedisk-create");
	set(ui->actionScriptEditor, "code");

	// Game
	set(ui->actionPlayTest, "play");
	set(ui->actionFullScreenToggle, "fullscreen");
	set(ui->actionTitleBackgroundToggle, "show-title");

	// Map Tree
	set(ui->actionAreaProperties, "properties");
	set(ui->actionAreaDelete, "edit-delete");

	// Help
	set(ui->actionContents, "help-contents");
	set(ui->actionZoom1_1, "zoom-ratio-1");
	set(ui->actionZoom1_2, "zoom-ratio-2");
	set(ui->actionZoom1_4, "zoom-ratio-4");
	set(ui->actionZoom1_8, "zoom-ratio-8");
	set(ui->actionAreaNew, "area-new");
	set(ui->actionMapShift, "map-shift");
	set(ui->actionAbout, "help-about");
}

void MainWindow::updateWatcher()
{
	const auto& project = core().project();
	if (project && EditorSettings::instance().reloadExternalChanges()) {
		core().watcher().setProject(project->projectDir().absolutePath(),
			project->projectType() == FileFinder::ProjectType::EasyRpg);
	} else {
		core().watcher().setProject(QString(), false);
	}
}

bool MainWindow::canReload(int kind) const
{
	QWidget* modal = QApplication::activeModalWidget();
	if (!modal) {
		return true;
	}
	// An event dialog follows the changes of its map. Other dialogs keep
	// pointers into the data: wait until they are closed.
	return kind > 0 && qobject_cast<EventDialog*>(modal) != nullptr;
}

void MainWindow::onDatabaseFileChanged()
{
	if (!core().project()) {
		return;
	}
	if (!core().project()->reloadDatabase()) {
		statusBar()->showMessage(tr("The database changed on disk but could not be read"), 5000);
		return;
	}
	// Chipsets can have changed
	if (currentScene()) {
		core().LoadChipset(currentScene()->chipsetId());
		m_paletteScene->onChipsetChange();
	}
	for (int i = 0; i < ui->tabMap->count(); i++) {
		getTabScene(i)->redrawMap();
	}
	statusBar()->showMessage(tr("Database reloaded"), 3000);
	qCInfo(lcReload) << "database reloaded";
}

void MainWindow::onTreeMapFileChanged()
{
	if (!core().project()) {
		return;
	}
	QTreeWidgetItem* current = ui->treeMap->currentItem();
	int current_id = current ? current->data(1, Qt::DisplayRole).toInt() : -1;

	if (!core().project()->reloadTreeMap()) {
		statusBar()->showMessage(tr("The map tree changed on disk but could not be read"), 5000);
		return;
	}
	populateMapTree(false);
	if (m_treeItems.contains(current_id) && m_treeItems[current_id]) {
		ui->treeMap->setCurrentItem(m_treeItems[current_id]);
	}

	// Close the maps that were removed, update the names of the others
	for (int i = ui->tabMap->count() - 1; i >= 0; i--) {
		MapScene* scene = getTabScene(i);
		if (!m_treeItems.contains(scene->id())) {
			if (!scene->isModified()) {
				removeView(scene->id());
			}
			continue;
		}
		scene->refreshMapInfo();
		ui->tabMap->setTabText(i, scene->mapName() + (scene->isModified() ? " *" : ""));
	}
	statusBar()->showMessage(tr("Map tree reloaded"), 3000);
	qCInfo(lcReload) << "map tree reloaded";
}

void MainWindow::onMapFileChanged(int id)
{
	if (MapScene* scene = getScene(id)) {
		scene->reloadFromDisk();
		statusBar()->showMessage(tr("%1 reloaded").arg(scene->mapName()), 3000);
		qCInfo(lcReload) << "map" << id << "reloaded";
	} else {
		qCDebug(lcReload) << "map" << id << "changed on disk, not open";
	}
}

void MainWindow::onMapReloaded()
{
	auto* scene = qobject_cast<MapScene*>(sender());
	if (scene && scene == currentScene()) {
		// The event list points into the map, which was replaced
		core().setCurrentMapEvents(scene->mapEvents());
		core().LoadChipset(scene->chipsetId());
		m_paletteScene->onChipsetChange();
	}
	if (!scene) {
		return;
	}
	// undoing Map Properties can rename the map
	if (QTreeWidgetItem* item = m_treeItems.value(scene->id())) {
		item->setText(0, scene->mapName());
	}
	int index = ui->tabMap->indexOf(m_views.value(scene->id()));
	if (index >= 0) {
		ui->tabMap->setTabText(index, scene->mapName() + (scene->isModified() ? " *" : ""));
	}
	updateStatusBar();
}
