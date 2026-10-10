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
#include <QApplication>
#include <QBitmap>
#include <QBrush>
#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QQmlApplicationEngine>
#include "ui/map/map_scene.h"
#include "common/dbstring.h"
#include "common/image_loader.h"
#include <kddockwidgets/Config.h>
#include <kddockwidgets/qtquick/Platform.h>

//define static member
Core *Core::core_instance = nullptr;

Core& core() {
	return *Core::getCore();
}

Core::Core()
{
	m_tileSize = 16;
	m_tool = PENCIL;
	m_layer = LOWER;
	m_lowerSel.push_back(0);
	m_upperSel.push_back(10000);
	m_lowerSelW = 1;
	m_upperSelW = 1;
	m_lowerSelH = 1;
	m_upperSelH = 1;
	m_eventSel = 10000;
	m_currentMapEvents = nullptr;
	m_dictionary[UPLEFT]							= 1;
	m_dictionary[UPRIGHT]							= 2;
	m_dictionary[UPLEFT+UPRIGHT]					= 3;
	m_dictionary[DOWNRIGHT]							= 4;
	m_dictionary[UPLEFT+DOWNRIGHT]					= 5;
	m_dictionary[UPRIGHT+DOWNRIGHT]					= 6;
	m_dictionary[UPLEFT+UPRIGHT+DOWNRIGHT]			= 7;
	m_dictionary[DOWNLEFT]							= 8;
	m_dictionary[DOWNLEFT+UPLEFT]					= 9;
	m_dictionary[DOWNLEFT+UPRIGHT]					= 10;
	m_dictionary[DOWNLEFT+UPLEFT+UPRIGHT]			= 11;
	m_dictionary[DOWNLEFT+DOWNRIGHT]				= 12;
	m_dictionary[DOWNLEFT+DOWNRIGHT+UPLEFT]			= 13;
	m_dictionary[DOWNLEFT+DOWNRIGHT+UPRIGHT]		= 14;
	m_dictionary[DOWNLEFT+DOWNRIGHT+UPLEFT+UPRIGHT] = 15;
	m_dictionary[LEFT]								= 16;
	m_dictionary[LEFT+UPRIGHT]						= 17;
	m_dictionary[LEFT+DOWNRIGHT]					= 18;
	m_dictionary[LEFT+UPRIGHT+DOWNRIGHT]			= 19;
	m_dictionary[UP]								= 20;
	m_dictionary[UP+DOWNRIGHT]						= 21;
	m_dictionary[UP+DOWNLEFT]						= 22;
	m_dictionary[UP+DOWNRIGHT+DOWNLEFT]				= 23;
	m_dictionary[RIGHT]								= 24;
	m_dictionary[RIGHT+DOWNLEFT]					= 25;
	m_dictionary[RIGHT+UPLEFT]						= 26;
	m_dictionary[RIGHT+UPLEFT+DOWNLEFT]				= 27;
	m_dictionary[DOWN]								= 28;
	m_dictionary[DOWN+UPLEFT]						= 29;
	m_dictionary[DOWN+UPRIGHT]						= 30;
	m_dictionary[DOWN+UPLEFT+UPRIGHT]				= 31;
	m_dictionary[LEFT+RIGHT]						= 32;
	m_dictionary[UP+DOWN]							= 33;
	m_dictionary[UP+LEFT]							= 34;
	m_dictionary[UP+LEFT+DOWNRIGHT]					= 35;
	m_dictionary[UP+RIGHT]							= 36;
	m_dictionary[UP+RIGHT+DOWNLEFT]					= 37;
	m_dictionary[DOWN+RIGHT]						= 38;
	m_dictionary[DOWN+RIGHT+UPLEFT]					= 39;
	m_dictionary[DOWN+LEFT]							= 40;
	m_dictionary[DOWN+LEFT+UPRIGHT]					= 41;
	m_dictionary[UP+LEFT+RIGHT]						= 42;
	m_dictionary[UP+LEFT+DOWN]						= 43;
	m_dictionary[LEFT+RIGHT+DOWN]					= 44;
	m_dictionary[UP+RIGHT+DOWN]						= 45;
	m_dictionary[UP+DOWN+LEFT+RIGHT]				= 46;
	m_dictionary[SAMPLE]							= 47;

	m_runGameDialog = nullptr;
}

Core *Core::getCore()
{
	if (!core_instance) {
		core_instance = new Core();
	}
	return core_instance;
}

void Core::LoadChipset(int n_chipsetid)
{
	lcf::rpg::Chipset n_chipset;
	for (auto & chipset : project()->database().chipsets)
		if (chipset.ID == n_chipsetid)
		{
			n_chipset = chipset;
			break;
		}
	// Compare the whole chipset, not its id: a database reloaded from disk
	// can have changed its image or terrain
	if (n_chipset == m_chipset && m_chipsetProject == project().get())
		return;
	m_chipset = n_chipset;
	m_chipsetProject = project().get();
	m_charsetCache.clear();
	if (m_chipset.ID == 0)
	{
		// unknown chipset: do not keep the tiles of the previous one
		m_tileCache.clear();
		for (auto& frames : m_frameCache)
			frames.clear();
		emit chipsetChanged();
		return;
	}

	const QString chipset_name = ToQString(m_chipset.chipset_name);
	QPixmap o_chipset = ImageLoader::Load(project()->findFile(CHIPSET, chipset_name, FileFinder::FileType::Image));
	if (!o_chipset)
		o_chipset = ImageLoader::Load(rtpPath(CHIPSET, chipset_name));
	if (!o_chipset)
	{
		qWarning()<<"Chipset"<<chipset_name<<"not found.";
		o_chipset = createDummyPixmap(480,256);
	}
	else // Skip color mask when using the dummy pixmap
	{
		if (o_chipset.toImage().colorCount() > 0)
			m_keycolor = QColor(o_chipset.toImage().color(0));
		else
			// Qt might remove the color table. Use the color of the first upper tile instead
			m_keycolor = QColor(o_chipset.toImage().pixel(288,128));
		o_chipset.setMask(o_chipset.createMaskFromColor(m_keycolor));
	}

	int r_tileSize = o_chipset.width()/30;
	int r_tileHalf = r_tileSize/2;

	/* BindWaterTiles */
	m_tileCache.clear();
	for (auto& frames : m_frameCache)
		frames.clear();

	/*
	 * TileIDs:
	 * 0- WaterA
	 * 1- WaterB
	 * 2- DeepWater
	 * 3- Ground
	 */
	//Go throught all posible combinations
	for (int terrain_id = 0; terrain_id < 4; terrain_id++)
	for (int tile_u = 0; tile_u < 4; tile_u++)
	for (int tile_d = 0; tile_d < 4; tile_d++)
	for (int tile_l = 0; tile_l < 4; tile_l++)
	for (int tile_r = 0; tile_r < 4; tile_r++)
	for (int tile_ul = 0; tile_ul < 4; tile_ul++)
	for (int tile_ur = 0; tile_ur < 4; tile_ur++)
	for (int tile_dl = 0; tile_dl < 4; tile_dl++)
	for (int tile_dr = 0; tile_dr < 4; tile_dr++)
	{
		int _code, _scode;
		if (terrain_id == 3)
			// Once main foreach reaches ground, we have finished
			 break;
		int u=0,d=0,l=0,r=0,ul=0,ur=0,dl=0,dr=0,sul=0,sur=0,sdl=0,sdr=0;

		if (!isWater(tile_u))
			u = UP;
		if (!isWater(tile_d))
			d = DOWN;
		if (!isWater(tile_l))
			l = LEFT;
		if (!isWater(tile_r))
			r = RIGHT;
		_code = u+d+l+r;
		// DeepWater Special Corners
		if (isDWater(terrain_id))
		{
			if (isABWater(tile_u) && isABWater (tile_l) && isABWater (tile_ul))
				sul = UPLEFT;
			if (isABWater(tile_u) && isABWater (tile_r) && isABWater (tile_ur))
				sur = UPRIGHT;
			if (isABWater(tile_d) && isABWater (tile_l) && isABWater (tile_dl))
				sdl = DOWNRIGHT;
			if (isABWater(tile_d) && isABWater (tile_r) && isABWater (tile_dr))
				sdr = DOWNLEFT;
		}
		else
		{
			if (isDWater (tile_u) && isDWater (tile_l) && isWater(tile_ul))
				sul = UPLEFT;
			if (isDWater (tile_u) && isDWater (tile_r) && isWater(tile_ur))
				sur = UPRIGHT;
			if (isDWater (tile_d) && isDWater (tile_l) && isWater(tile_dl))
				sdl = DOWNRIGHT;
			if (isDWater (tile_d) && isDWater (tile_r) && isWater(tile_dr))
				sdr = DOWNLEFT;
		}
		_scode = sul+sur+sdl+sdr;
		if ((u+l) == 0 && tile_ul == 3)
			ul = UPLEFT;
		if ((u+r) == 0 && tile_ur == 3)
			ur = UPRIGHT;
		if ((d+l) == 0 && tile_dl == 3)
			dl = DOWNLEFT;
		if ((d+r) == 0 && tile_dr == 3)
			dr = DOWNRIGHT;
		_code += ul+ur+dl+dr;
		short id = translate(terrain_id,_code,_scode);
		if (!m_tileCache[id].isNull()) //item exist?
			continue;
		// Water B uses second block of 3x4 tiles for borders
		// Water A and Deep Water uses first block
		int border_xoffset = (terrain_id == 1) ? 3*r_tileSize : 0;
		// The frames of the water are the columns of its blocks (the Player's animID)
		auto water_tile = [&](int frame) {
			const int frame_x = frame*r_tileSize;
			/*
			 * Get base
			 */
			QPixmap p_tile(tileSize(), tileSize());
			p_tile.fill(Qt::transparent);
			QPainter p(&p_tile);
			if (isABWater (terrain_id))
				p.drawPixmap(0,0,tileSize(),tileSize(),o_chipset.copy(frame_x, 4*r_tileSize,r_tileSize,r_tileSize));
			else
				p.drawPixmap(0,0,tileSize(),tileSize(),o_chipset.copy(frame_x, 7*r_tileSize,r_tileSize,r_tileSize));
			// Draw UpperLeft corner
			int dest_x = 0, dest_y = 0;
#define blit(x,y) p.drawPixmap(dest_x,dest_y,tileSize()/2,tileSize()/2,o_chipset.copy(frame_x+(x), y,r_tileHalf,r_tileHalf))
			if (u+l == 5)
				blit(border_xoffset, 0);
			else if (u)
				blit(border_xoffset, 2*r_tileSize);
			else if (l)
				blit(border_xoffset, r_tileSize);
			else if (ul)
				blit(border_xoffset, 3*r_tileSize);
			else if (sul)
			{
				if (isABWater (terrain_id))
					blit(0, 5*r_tileSize);
				else
					blit(0, 6*r_tileSize);
			}
			//Draw UpperRight corner
			dest_x = tileSize()/2;
			if (u+r == 9)
				blit(border_xoffset+r_tileHalf, 0);
			else if (u)
				blit(border_xoffset+r_tileHalf, 2*r_tileSize);
			else if (r)
				blit(border_xoffset+r_tileHalf, r_tileSize);
			else if (ur)
				blit(border_xoffset+r_tileHalf, 3*r_tileSize);
			else if (sur)
			{
				if (isABWater (terrain_id))
					blit(r_tileHalf, 5*r_tileSize);
				else
					blit(r_tileHalf, 6*r_tileSize);
			}
			//Draw LowerRight corner
			dest_y = tileSize()/2;
			if (d+r == 10)
				blit(border_xoffset+r_tileHalf, r_tileSize/2);
			else if (d)
				blit(border_xoffset+r_tileHalf, r_tileSize*5/2);
			else if (r)
				blit(border_xoffset+r_tileHalf, r_tileSize*3/2);
			else if (dr)
				blit(border_xoffset+r_tileHalf, r_tileSize*7/2);
			else if (sdr)
			{
				if (isABWater (terrain_id))
					blit(r_tileHalf, r_tileSize*11/2);
				else
					blit(r_tileHalf, r_tileSize*13/2);
			}
			//Draw LowerLeft corner
			dest_x = 0;
			if (d+l == 6)
				blit(border_xoffset, r_tileSize/2);
			else if (d)
				blit(border_xoffset, r_tileSize*5/2);
			else if (l)
				blit(border_xoffset, r_tileSize*3/2);
			else if (dl)
				blit(border_xoffset, r_tileSize*7/2);
			else if (sdl)
			{
				if (isABWater (terrain_id))
					blit(0, r_tileSize*11/2);
				else
					blit(0, r_tileSize*13/2);
			}
#undef blit
			p.end();
			return p_tile;
		};
		m_tileCache[id] = water_tile(0);
		for (int frame = 1; frame < 3; frame++)
			m_frameCache[frame - 1][id] = water_tile(frame);
	}

	/* Register AnimationTiles */
	// 3 columns of 4 frames, under the borders of the water B
	for (int column = 0; column < 3; column++)
		for (int frame = 0; frame < 4; frame++)
		{
			QPixmap a_tile(tileSize(), tileSize());
			a_tile.fill(Qt::transparent);
			QPainter a(&a_tile);
			a.drawPixmap(0,0,tileSize(),tileSize(),o_chipset.copy((3+column)*r_tileSize,(4+frame)*r_tileSize,r_tileSize,r_tileSize));
			a.end();
			// animated tiles have no borders: RPG Maker writes 3000, 3050, 3100
			for (int i = 0; i < 50; i++) {
				const short id = static_cast<short>(translate(3 + column) + i);
				if (frame == 0)
					m_tileCache[id] = a_tile;
				else
					m_frameCache[frame - 1][id] = a_tile;
			}
		}

	/* BindGroundTiles */
	// Each tileset contains 5 columns with a size of 6x16 tiles
	int tileset_col = 0;

	// Each tileset column contains 4 blocks with a size of 3x4 tiles
	// Anyway we should skip water colums because they are already stored
	int block_col = 0;
	int block_row = 2;

	int terrain_id = 6;

	while (tileset_col < 2)
	{
		int orig_x = (2 * tileset_col + block_col) *3*r_tileSize;
		int orig_y = block_row *4*r_tileSize;
		QPixmap p_block = o_chipset.copy(orig_x, orig_y, 3*r_tileSize, 4*r_tileSize);

		/*
		 * Generate binded cache and store them on hash table
		 */

		//Simulate all posible combinations

		std::vector<bool> is_binded;
		is_binded.push_back(true);
		is_binded.push_back(false);
#define forx(_type,_iter,_coll) for (_type::iterator _iter = _coll.begin(); _iter != _coll.end(); ++_iter)

		forx(std::vector<bool>, bu, is_binded)
		forx(std::vector<bool>, bd, is_binded)
		forx(std::vector<bool>, bl, is_binded)
		forx(std::vector<bool>, br, is_binded)
		forx(std::vector<bool>, bul, is_binded)
		forx(std::vector<bool>, bur, is_binded)
		forx(std::vector<bool>, bdl, is_binded)
		forx(std::vector<bool>, bdr, is_binded)
#undef forx
		{
			int u = 0, d = 0, l = 0, r = 0;
			if (*bu) u = UP;
			if (*bd) d = DOWN;
			if (*bl) l = LEFT;
			if (*br) r = RIGHT;
			int ul = 0, ur = 0, dl = 0, dr = 0;
			if (u + l == 0 && *bul)
				ul = UPLEFT;
			if (u + r == 0 && *bur)
				ur = UPRIGHT;
			if (d + l == 0 && *bdl)
				dl = DOWNLEFT;
			if (d + r == 0 && *bdr)
				dr = DOWNRIGHT;

			short id = translate (terrain_id, u+d+l+r+ul+ur+dl+dr);
			if (!m_tileCache[id].isNull()) //item exist?
				continue;

			/*
			 * Get base
			 */
			QPixmap p_tile(tileSize(), tileSize());
			QPainter p(&p_tile);

			/*
			 * Draw upper_left corner
			 */
			int dest_x = 0;
			int dest_y = 0;
#define blit(x,y) p.drawPixmap(dest_x,dest_y,tileSize()/2,tileSize()/2, p_block.copy(x, y,r_tileHalf,r_tileHalf))
			if (u+l == 5)
				blit(0, r_tileSize);
			else if (u)
			{
				if (r)
					blit(r_tileSize*2, r_tileSize);
				else
					blit(r_tileSize, r_tileSize);
			}
			else if (l)
			{
				if (d)
					blit(0, r_tileSize*3);
				else
					blit(0, r_tileSize*2);
			}
			else if (ul)
				blit(r_tileSize*2, 0);
			else //0
			{
				if (d+r == 10)
					blit(r_tileSize*2, r_tileSize*3);
				else if (r)
					blit(r_tileSize*2, r_tileSize*2);
				else if (d)
					blit(r_tileSize*1, r_tileSize*3);
				else
					blit(r_tileSize*1, r_tileSize*2);
			}
			/*
			 * Draw upper_right corner
			 */
			dest_x = tileSize()/2;
			if (u+r == 9)
				blit(r_tileSize*5/2, r_tileSize);
			else if (u)
			{
				if(l)
					blit(r_tileSize/2, r_tileSize);
				else
					blit(r_tileSize*3/2, r_tileSize);
			}
			else if (r)
			{
				if (d)
					blit(r_tileSize*5/2, r_tileSize*3);
				else
					blit(r_tileSize*5/2, r_tileSize*2);
			}
			else if (ur)
				blit(r_tileSize*5/2, 0);
			else //0
			{
				if (d+l == 6)
					blit(r_tileSize/2, r_tileSize*3);
				else if (l)
					blit(r_tileSize/2, r_tileSize*2);
				else if (d)
					blit(r_tileSize*3/2, r_tileSize*3);
				else
					blit(r_tileSize*3/2, r_tileSize*2);
			}
			/*
			 * Draw down_left corner
			 */
			dest_x = 0;
			dest_y = tileSize()/2;
			if (d+l == 6)
				blit(0, r_tileSize*7/2);
			else if (d)
			{
				if (r)
					blit(r_tileSize*2, r_tileSize*7/2);
				else
					blit(r_tileSize, r_tileSize*7/2);
			}
			else if (l)
			{
				if (u)
					blit(0, r_tileSize*3/2);
				else
					blit(0, r_tileSize*5/2);
			}
			else if (dl)
				blit(r_tileSize*2, r_tileHalf);
			else
			{
				if (u+r == 9)
					blit(r_tileSize*2, r_tileSize*3/2);
				else if (r)
					blit(r_tileSize*2, r_tileSize*5/2);
				else if (u)
					blit(r_tileSize, r_tileSize*3/2);
				else
					blit(r_tileSize, r_tileSize*5/2);
			}
			/*
			 * Draw down_right corner
			 */
			dest_x = tileSize()/2;
			if (d+r == 10)
				blit(r_tileSize*5/2, r_tileSize*7/2);
			else if (d)
			{
				if (l)
					blit(r_tileSize/2, r_tileSize*7/2);
				else
					blit(r_tileSize*3/2, r_tileSize*7/2);
			}
			else if (r)
			{
				if (u)
					blit(r_tileSize*5/2, r_tileSize*3/2);
				else
					blit(r_tileSize*5/2, r_tileSize*5/2);
			}
			else if (dr)
				blit(r_tileSize*5/2, r_tileHalf);
			else //0
			{
				if (u+l == 5)
					blit(r_tileHalf,r_tileSize*3/2);
				else if (l)
					blit(r_tileHalf,r_tileSize*5/2);
				else if (u)
					blit(r_tileSize*3/2,r_tileSize*3/2);
				else
					blit(r_tileSize*3/2,r_tileSize*5/2);
			}
#undef blit
			/*
			 * Register tile
			 */
			m_tileCache[id] = p_tile;
		}

		terrain_id++;
		block_col++;

		// Go to the next block
		if (block_col > 1) {
				block_col = 0;
				block_row++;
		}

		// Go to the next column
		if (block_row > 3) {
				block_row = 0;
				tileset_col++;
		}
	}
	int tile_row = 0;
	/* Register E and F blocks */
	while (tileset_col < 5 )
	{
		for (int col = 0; col < 6; col++)
		{
			QPixmap ef_tile(tileSize(), tileSize());
			ef_tile.fill(QColor(0,0,0,0));
			QPainter ef(&ef_tile);
			int orig_x = tileset_col*6*r_tileSize+col*r_tileSize;
			int orig_y = tile_row*r_tileSize;
			ef.drawPixmap(0,0,tileSize(),tileSize(),o_chipset.copy(orig_x,orig_y,r_tileSize,r_tileSize));
			ef.end();
			ef_tile.setMask(ef_tile.createMaskFromColor(m_keycolor));
			m_tileCache[translate(terrain_id)] = ef_tile;
			terrain_id++;
		}

		tile_row++;
		if (tile_row == 16)
		{
			tile_row = 0;
			tileset_col++;
		}
	}

	emit chipsetChanged();
}

int Core::chipsetId() const
{
	return m_chipset.ID;
}

void Core::LoadBackground(QString name)
{
	if (name.isEmpty()) {
		m_background = QPixmap(640,480);
		m_background.fill(Qt::magenta);
	} else
		m_background = ImageLoader::Load(project()->findFile(PANORAMA, name, FileFinder::FileType::Image));
}

const QPixmap& Core::background() const
{
	return m_background;
}

void Core::setBackground(const QPixmap& background, const QTransform& transform)
{
	m_background = background;
	m_backgroundTransform = transform;
}

int Core::tileSize()
{
	return m_tileSize;
}

void Core::setTileSize(int tile_size)
{
	m_tileSize = tile_size;
}

QString Core::rtpPath(const QString &folder, const QString &filename) const
{
	return m_rtpDir+folder+filename;
}

Core::Layer Core::layer()
{
	return m_layer;
}

void Core::setLayer(const Layer &current_layer)
{
	m_layer = current_layer;
	emit layerChanged();
}
Core::Tool Core::tool()
{
	return m_tool;
}

void Core::setTool(const Tool &current_tool)
{
	m_tool = current_tool;
	emit toolChanged();
}

bool Core::isWater(int terrain_id)
{
	return terrain_id >= 0 && terrain_id <= 2;
}

bool Core::isABWater(int terrain_id)
{
	return terrain_id == 0 || terrain_id == 1;
}

bool Core::isDWater(int terrain_id)
{
	return terrain_id == 2;
}

bool Core::isAnimation(int terrain_id)
{
	return terrain_id >= 3 && terrain_id <= 5;
}

bool Core::isDblock(int terrain_id)
{
	return terrain_id >= 6 && terrain_id <= 17;
}

bool Core::isEblock(int terrain_id)
{
	return terrain_id >= 18 && terrain_id <= 161;
}

bool Core::isFblock(int terrain_id)
{
	return terrain_id >= 162 && terrain_id <= 305;
}
QString Core::defDir() const
{
	return m_defDir;
}

void Core::setDefDir(const QString &defDir)
{
	m_defDir = defDir + (defDir.endsWith('/') ? "" : "/");
}

void Core::runGame()
{
	if (!m_runGameDialog)
		m_runGameDialog = new RunGameDialog(project()->projectData());
	m_runGameDialog->exec();
}

void Core::runGameHere(int map_id, int x, int y)
{
	if (!m_runGameDialog)
		m_runGameDialog = new RunGameDialog(project()->projectData());
	m_runGameDialog->runHere(map_id, x, y);
}

void Core::runBattleTest(int troop_id)
{
	if (!m_runGameDialog)
		m_runGameDialog = new RunGameDialog(project()->projectData());
	//Set parametters
	m_runGameDialog->runBattle(troop_id);
}

std::shared_ptr<Project>& Core::project() {
	return m_project;
}

const std::shared_ptr<Project>& Core::project() const {
	return m_project;
}

QQmlApplicationEngine* Core::qmlEngine() {
	if (!m_qml_engine) {
		m_qml_engine = new QQmlApplicationEngine();
		KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtQuick);
		KDDockWidgets::QtQuick::Platform::instance()->setQmlEngine(m_qml_engine);
	}

	return m_qml_engine;
}

ProjectWatcher& Core::watcher() {
	return m_watcher;
}

QPixmap Core::paletteTiles(Layer layer, int size)
{
	const bool lower = layer == LOWER;
	const int count = lower ? 162 : 144;
	QPixmap tiles(6 * size, (count / 6) * size);
	tiles.fill(Qt::transparent);
	beginPainting(tiles);
	for (int i = 0; i < count; i++) {
		const QRect rect((i % 6) * size, (i / 6) * size, size, size);
		renderTile(lower ? translate(i, 15) : translate(i + 162), rect);
	}
	endPainting();
	return tiles;
}

void Core::beginPainting(QPixmap &dest)
{
	m_painter.begin(&dest);
	if (m_painter.isActive()) {
		QBrush panorama(m_background);
		panorama.setTransform(m_backgroundTransform);
		m_painter.setBackground(panorama);
	}
	m_painter.setPen(Qt::yellow);
}

void Core::renderTile(const short &tile_id, const QRect &dest_rect)
{
	if (tile_id < 10000) {
		// the panorama, at the place and zoom of the map (setBackground)
		QBrush panorama(m_background);
		panorama.setTransform(m_backgroundTransform);
		m_painter.fillRect(dest_rect, panorama);
	}
	// value(): operator[] would add an empty tile for every unknown id
	const int frame = !isAnimatedTile(tile_id) ? 0 : tile_id < 3000 ? m_waterFrame : m_animatedTileFrame;
	if (frame > 0)
		m_painter.drawPixmap(dest_rect, m_frameCache[frame - 1].value(tile_id));
	else
		m_painter.drawPixmap(dest_rect, m_tileCache.value(tile_id));
}

bool Core::isAnimatedTile(short tile_id)
{
	return tile_id >= 0 && tile_id < 3150;
}

void Core::setAnimationEnabled(bool enabled)
{
	if (enabled == animationEnabled())
		return;
	if (!enabled) {
		m_animationTimer->stop();
		// the first frame, as the tiles were drawn before
		m_waterFrame = 0;
		m_animatedTileFrame = 0;
		emit animationFrameChanged();
		return;
	}
	if (!m_animationTimer) {
		m_animationTimer = new QTimer(this);
		connect(m_animationTimer, &QTimer::timeout, this, &Core::nextAnimationFrame);
	}
	// the Player changes the animated tiles every 6 frames (60 per second)
	m_animationTimer->start(100);
}

bool Core::animationEnabled() const
{
	return m_animationTimer && m_animationTimer->isActive();
}

int Core::waterFrame() const
{
	return m_waterFrame;
}

int Core::animatedTileFrame() const
{
	return m_animatedTileFrame;
}

void Core::nextAnimationFrame()
{
	if (chipsetIsNull())
		return;
	m_animationTick++;
	// As the Player (TilemapLayer::Draw): the animated tiles have 4 frames;
	// the water changes every 12 (fast) or 24 (slow) frames, 1-2-3-2 or 1-2-3
	const int tile_frame = static_cast<int>(m_animationTick % 4);
	int water_frame = static_cast<int>(m_animationTick / (m_chipset.animation_speed ? 2 : 4));
	if (m_chipset.animation_type == lcf::rpg::Chipset::AnimType_cyclic) {
		water_frame %= 3;
	} else {
		water_frame %= 4;
		if (water_frame == 3)
			water_frame = 1;
	}
	if (tile_frame == m_animatedTileFrame && water_frame == m_waterFrame)
		return;
	m_animatedTileFrame = tile_frame;
	m_waterFrame = water_frame;
	emit animationFrameChanged();
}

QRect Core::renderEventFrame(const QRect &dest_rect)
{
	// the middle of the cell, in a thin frame of the pen's color: filled
	// lines of a pixel, the same on pixmaps of any resolution
	const int fact = dest_rect.width();
	QRect final_rect = dest_rect.adjusted(fact/6,fact/6,-fact/6,-fact/6);
	const QRect frame = final_rect.adjusted(-1,-1,1,1);
	const QColor color = m_painter.pen().color();
	m_painter.fillRect(QRect(frame.left(), frame.top(), frame.width(), 1), color);
	m_painter.fillRect(QRect(frame.left(), frame.bottom(), frame.width(), 1), color);
	m_painter.fillRect(QRect(frame.left(), frame.top(), 1, frame.height()), color);
	m_painter.fillRect(QRect(frame.right(), frame.top(), 1, frame.height()), color);
	return final_rect;
}

void Core::renderEvent(const lcf::rpg::Event& event, const QRect &dest_rect)
{
	if (event.pages.empty())
		return;

	const QRect final_rect = renderEventFrame(dest_rect);
	if (event.pages[0].character_name.empty())
		renderTile(static_cast<short>(event.pages[0].character_index+10000), final_rect);
	else {
		if (!m_eventCache.contains(event.ID))
			return;
		m_painter.drawPixmap(final_rect, m_eventCache.value(event.ID), QRect(0,6,24,24));
	}
}

void Core::renderMarker(StartMarker marker, const QString& letter, const QRect &dest_rect)
{
	const QColor black(0, 0, 0);
	const QColor white(255, 255, 255);
	const QColor cyan(128, 255, 255);
	const QColor ink(40, 0, 80);
	const QColor badge(64, 0, 128);
	const QColor yellow(255, 255, 0);
	// the two ends of each gradient, bottom left to top right: light enough
	// for the dark drawing
	static const QColor ends[4][2] = {
		{QColor(72, 144, 248), QColor(240, 80, 72)},
		{QColor(248, 144, 40), QColor(72, 144, 248)},
		{QColor(240, 80, 72), QColor(80, 200, 96)},
		{QColor(72, 144, 248), QColor(248, 216, 48)},
	};

	// framed like an event, the marker taking the place of its graphic
	const QRect area = renderEventFrame(dest_rect);
	if (area.isEmpty())
		return;
	const qreal dpr = m_painter.device()->devicePixelRatioF();
	// the drawing is pixel art on a grid of 22 units (a pixel each in cells
	// of 32 pixels): a border of black, light and black lines around a field
	// of 16, from
	// cells of 16 pixels; smaller cells have the gradient alone, a unit per
	// pixel
	const bool detailed = area.width() >= 11;
	const int units = detailed ? 22 : area.width();
	const int border = detailed ? 3 : 0;

	// a diagonal gradient as pixel art: each unit takes one of the two
	// colors, after a 4x4 ordered (Bayer) dither
	static const int bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
	QImage image(units, units, QImage::Format_ARGB32_Premultiplied);
	image.fill(black);
	if (detailed) {
		QPainter painter(&image);
		painter.fillRect(QRect(1, 1, units - 2, units - 2), cyan);
		painter.fillRect(QRect(2, 2, units - 4, units - 4), black);
	}
	const int field = units - 2 * border;
	const int steps = std::max(1, 2 * field - 2);
	for (int y = 0; y < field; y++)
		for (int x = 0; x < field; x++) {
			const int level = 16 * (x + field - 1 - y) / steps;
			image.setPixelColor(border + x, border + y, ends[marker][level > bayer[y & 3][x & 3] ? 1 : 0]);
		}

	if (detailed) {
		// in units of the field, in its middle
		QList<QRect> shape;
		switch (marker) {
		case PARTY:
			// a hero holding a staff
			shape = {{4, 1, 1, 14}, {3, 1, 3, 2}, {8, 1, 3, 3}, {9, 4, 1, 1}, {7, 5, 5, 4},
				{5, 6, 2, 1}, {12, 6, 1, 3}, {7, 9, 2, 5}, {10, 9, 2, 5}, {6, 14, 3, 1}, {10, 14, 3, 1}};
			break;
		case BOAT:
			// two crossed oars over a low hull
			shape = {{1, 2, 2, 2}, {3, 4, 1, 1}, {4, 5, 1, 1}, {5, 6, 1, 1}, {6, 7, 1, 1}, {7, 8, 1, 1}, {8, 9, 1, 1},
				{13, 2, 2, 2}, {12, 4, 1, 1}, {11, 5, 1, 1}, {10, 6, 1, 1}, {9, 7, 1, 1}, {8, 8, 1, 1}, {7, 9, 1, 1},
				{0, 9, 1, 1}, {15, 9, 1, 1}, {0, 10, 16, 1}, {1, 11, 14, 1}, {3, 12, 10, 1}};
			break;
		case SHIP:
			// a mast with two sails, on a tall hull
			shape = {{7, 2, 1, 7}, {8, 2, 2, 1},
				{9, 3, 2, 1}, {9, 4, 3, 1}, {9, 5, 4, 1}, {9, 6, 5, 1}, {9, 7, 6, 1},
				{5, 4, 1, 1}, {4, 5, 2, 1}, {3, 6, 3, 1}, {2, 7, 4, 1},
				{0, 9, 16, 1}, {1, 10, 14, 1}, {2, 11, 12, 1}, {3, 12, 10, 1}};
			break;
		case AIRSHIP:
			// a balloon with a tail, its gondola hanging below
			shape = {{4, 2, 7, 1}, {2, 3, 11, 1}, {1, 4, 13, 3}, {2, 7, 11, 1}, {4, 8, 7, 1},
				{14, 4, 1, 3}, {15, 3, 1, 5}, {4, 9, 1, 2}, {10, 9, 1, 2}, {3, 11, 9, 1}, {4, 12, 7, 1}};
			break;
		}
		QPainter painter(&image);
		painter.translate(border, border);
		for (const QRect& part : shape)
			painter.fillRect(part, ink);
	}

	m_painter.save();
	// the badges may pass the event's frame, not the cell
	m_painter.setClipRect(dest_rect);
	// square pixels, unless the cell has fewer pixels than the drawing
	m_painter.setRenderHint(QPainter::SmoothPixmapTransform, area.width() * dpr < units);
	m_painter.drawImage(area, image);
	if (!detailed) {
		// the lines of the border, of at least a pixel of the screen each;
		// the light one alone when there is no room for three
		const qreal width = std::max(1 / dpr, area.width() / 22.0);
		const bool three = area.width() * dpr >= 12;
		auto ring = [&](qreal inset, const QColor& color) {
			QPainterPath path;
			path.addRect(QRectF(area).adjusted(inset, inset, -inset, -inset));
			path.addRect(QRectF(area).adjusted(inset + width, inset + width, -inset - width, -inset - width));
			m_painter.fillPath(path, color);
		};
		if (three) {
			ring(0, black);
			ring(2 * width, black);
		}
		ring(three ? width : 0, cyan);
	}

	// a text as large as its box allows (a box in units of the drawing),
	// with a dark outline of a unit of the 22 unit grid, at least a pixel of
	// the screen
	const qreal unit = static_cast<qreal>(area.width()) / units;
	auto drawText = [&](const QString& text, const QRectF& units_box, const QColor& color, const QColor& outline) {
		QFont font = m_painter.font();
		font.setPixelSize(64);
		font.setBold(true);
		QPainterPath path;
		path.addText(0, 0, font, text);
		// the outlines of a glyph can overlap
		path.setFillRule(Qt::WindingFill);
		const QRectF bounds = path.boundingRect();
		if (bounds.isEmpty())
			return;
		const QRectF box(area.x() + units_box.x() * unit, area.y() + units_box.y() * unit,
			units_box.width() * unit, units_box.height() * unit);
		const qreal scale = std::min(box.width() / bounds.width(), box.height() / bounds.height());
		m_painter.save();
		m_painter.setRenderHint(QPainter::Antialiasing, true);
		m_painter.translate(box.center());
		m_painter.scale(scale, scale);
		m_painter.translate(-bounds.center());
		if (outline.isValid()) {
			const qreal width = std::max(1 / dpr, area.width() / 22.0);
			m_painter.strokePath(path, QPen(outline, 2 * width / scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		}
		m_painter.fillPath(path, color);
		m_painter.restore();
	};
	if (detailed) {
		// on the border, astride the middle of its bottom corners: the
		// vehicles' number at the left, the badge of the letter at the right
		if (marker != PARTY)
			drawText(QString::number(static_cast<int>(marker)), QRectF(-1.5, 17, 6, 7), yellow, black);
		m_painter.save();
		m_painter.translate(area.topLeft());
		m_painter.scale(unit, unit);
		m_painter.fillRect(QRect(17, 16, 8, 9), white);
		m_painter.fillRect(QRect(18, 17, 6, 7), badge);
		m_painter.restore();
		drawText(letter, QRectF(19, 18, 4, 5), cyan, QColor());
	} else if (area.width() * dpr >= 12) {
		// the letter alone, when the screen has the pixels for it
		drawText(letter, QRectF(units / 5.0, units / 5.0, units * 3 / 5.0, units * 3 / 5.0), white, black);
	}
	m_painter.restore();
}

void Core::endPainting()
{
	m_painter.end();
}

QColor Core::keycolor()
{
	return m_keycolor;
}

short Core::translate(int terrain_id, int _code, int _scode)
{
	if (terrain_id < 0)
		return 0x7FFF;
	if (isWater(terrain_id))
		return (static_cast<short>(terrain_id)*1000+m_dictionary[_code]+m_dictionary[_scode]*50);
	// animated tiles have no borders: RPG Maker writes 3000, 3050, 3100
	if (isAnimation(terrain_id))
		return (3000+(static_cast<short>(terrain_id)-3)*50);
	if (isDblock(terrain_id))
		return (4000+(static_cast<short>(terrain_id)-6)*50+m_dictionary[_code]);
	if (isEblock(terrain_id))
		return (5000+static_cast<short>(terrain_id)-18);
	if (isFblock(terrain_id))
		return (10000+static_cast<short>(terrain_id)-162);
	return 0x7FFF;
}

int Core::translate(short tile_id)
{
	if (tile_id < 0)
		return -1;
	else if (tile_id < 3000)
		return tile_id/1000;
	else if (tile_id >= 3000 && tile_id < 3150)
		return (tile_id-3000)/50+3;
	else if (tile_id >= 4000 && tile_id < 4600)
		return (tile_id-4000)/50+6;
	else if (tile_id >= 5000 && tile_id < 5144)
		return tile_id-5000+18;
	else if (tile_id >= 10000 && tile_id < 10144)
		// the inverse of translate(int): F block terrains start at 162
		return tile_id-10000+162;
	else
		return -1;
}


void Core::setRtpDir(const QString &n_path)
{
	m_rtpDir = n_path;
}

short Core::selection(int off_x, int off_y, std::optional<Layer> layer)
{
	short result = 0;
	switch(layer.value_or(m_layer))
	{
	case LOWER:
		off_x %= m_lowerSelW;
		off_y %= m_lowerSelH;
		if (off_x < 0)
			off_x += m_lowerSelW;
		if (off_y < 0)
			off_y += m_lowerSelH;
		result = m_lowerSel[static_cast<size_t>(off_x+off_y*m_lowerSelW)];
		break;
	case UPPER:
		off_x %= m_upperSelW;
		off_y %= m_upperSelH;
		if (off_x < 0)
			off_x += m_upperSelW;
		if (off_y < 0)
			off_y += m_upperSelH;
		result = m_upperSel[static_cast<size_t>(off_x+off_y*m_upperSelW)];
		break;
	case EVENT:
		result = m_eventSel;
		break;
	}
	return result;
}

int Core::selWidth()
{
	int w = 1;
	if (m_layer == LOWER)
		w = m_lowerSelW;
	if (m_layer == UPPER)
		w = m_upperSelW;
	return w;
}

int Core::selHeight()
{
	int h = 1;
	if (m_layer == LOWER)
		h = m_lowerSelH;
	if (m_layer == UPPER)
		h = m_upperSelH;
	return h;
}

void Core::setSelection(std::vector<short> n_sel, int n_w, int n_h)
{
	if (static_cast<int>(n_sel.size()) != n_w * n_h)
		return;
	switch(m_layer)
	{
	case LOWER:
		m_lowerSel = n_sel;
		m_lowerSelW = n_w;
		m_lowerSelH = n_h;
		break;
	case UPPER:
		m_upperSel = n_sel;
		m_upperSelW = n_w;
		m_upperSelH = n_h;
		break;
	case EVENT:
		m_eventSel = n_sel[0];
		break;
	}
	emit selectionChanged();
}

QVariantMap Core::editingState() const
{
	QVariantMap state{{"layer", m_layer}, {"tool", m_tool}, {"event_tile", m_eventSel}};
	for (const auto layer : {LOWER, UPPER}) {
		const auto& selection = layer == LOWER ? m_lowerSel : m_upperSel;
		QVariantList tiles;
		for (short tile : selection) tiles.append(tile);
		state.insert(layer == LOWER ? "lower" : "upper", QVariantMap{{"tiles", tiles},
			{"width", layer == LOWER ? m_lowerSelW : m_upperSelW}, {"height", layer == LOWER ? m_lowerSelH : m_upperSelH}});
	}
	return state;
}

void Core::restoreEditingState(const QVariantMap& state)
{
	m_lowerSel = {0};
	m_upperSel = {10000};
	m_lowerSelW = m_lowerSelH = m_upperSelW = m_upperSelH = 1;
	m_eventSel = static_cast<short>(state.value("event_tile", 10000).toInt());
	m_layer = static_cast<Layer>(std::clamp(state.value("layer", LOWER).toInt(), static_cast<int>(LOWER), static_cast<int>(EVENT)));
	m_tool = static_cast<Tool>(std::clamp(state.value("tool", PENCIL).toInt(), static_cast<int>(ZOOM), static_cast<int>(SELECT)));
	for (const auto layer : {LOWER, UPPER}) {
		QVariantMap brush = state.value(layer == LOWER ? "lower" : "upper").toMap();
		// Migrate workspaces that remembered only the active layer's brush.
		if (brush.isEmpty() && m_layer == layer && state.contains("tiles"))
			brush = {{"tiles", state.value("tiles")}, {"width", state.value("tile_width")}, {"height", state.value("tile_height")}};
		const int width = brush.value("width").toInt(), height = brush.value("height").toInt();
		const QVariantList tiles = brush.value("tiles").toList();
		if (width <= 0 || width > 6 || height <= 0 || height > 30 || tiles.size() != width * height) continue;
		auto& selection = layer == LOWER ? m_lowerSel : m_upperSel;
		selection.clear();
		for (const auto& tile : tiles) selection.push_back(static_cast<short>(tile.toInt()));
		(layer == LOWER ? m_lowerSelW : m_upperSelW) = width;
		(layer == LOWER ? m_lowerSelH : m_upperSelH) = height;
	}
	emit layerChanged();
	emit toolChanged();
	emit selectionChanged();
}

const Core::TileClipboard& Core::tileClipboard() const
{
	return m_tileClipboard;
}

void Core::setTileClipboard(const TileClipboard& clipboard)
{
	m_tileClipboard = clipboard;
}

const lcf::rpg::Event* Core::eventClipboard() const
{
	return m_eventClipboardSet ? &m_eventClipboard : nullptr;
}

void Core::setEventClipboard(const lcf::rpg::Event& event)
{
	m_eventClipboard = event;
	m_eventClipboardSet = true;
}

const QMap<int, lcf::rpg::Event *>* Core::currentMapEvents() const
{
	return m_currentMapEvents;
}

lcf::rpg::Event *Core::currentMapEvent(int eventID)
{
	lcf::rpg::Event *event = nullptr;
	if (m_currentMapEvents)
		event = m_currentMapEvents->value(eventID);
	if (!event)
	{
		// one shared placeholder instead of a new (leaked) event per call
		static lcf::rpg::Event unknown;
		unknown = lcf::rpg::Event();
		unknown.name = "<?>";
		event = &unknown;
	}
	return event;
}

void Core::setCurrentMapEvents(QMap<int, lcf::rpg::Event *> *events)
{
	m_currentMapEvents = events;

	m_eventCache.clear();
	if (!events)
		return;
	for (QMap<int, lcf::rpg::Event*>::iterator it = events->begin(); it != events->end(); ++it)
	{
		lcf::rpg::Event* ev = it.value();
		if (ev->pages.empty())
			continue;

		lcf::rpg::EventPage& evp = ev->pages[0];
		if (evp.character_name.empty())
			continue;

		const QPixmap charset = charsetImage(ToQString(evp.character_name));

		int char_index = evp.character_index;
		int src_x = (char_index%4)*72 + evp.character_pattern * 24;
		int src_y = (char_index/4)*128 + evp.character_direction * 32;
		m_eventCache[it.key()] = charset.copy(src_x, src_y, 24, 32);
	}
}

QPixmap Core::charsetImage(const QString& name)
{
	if (m_cacheProject != project().get()) {
		m_charsetCache.clear();
		m_cacheProject = project().get();
	}
	auto it = m_charsetCache.constFind(name);
	if (it != m_charsetCache.constEnd())
		return it.value();

	QPixmap charset(ImageLoader::Load(project()->findFile(CHARSET, name, FileFinder::FileType::Image)));
	if (!charset)
		charset = ImageLoader::Load(rtpPath(CHARSET, name));
	if (!charset)
	{
		qWarning()<<"CharSet"<<name<<"not found.";
		charset = createDummyPixmap(288,256);
	}
	m_charsetCache.insert(name, charset);
	return charset;
}

QBrush Core::transparencyBrush()
{
	static const QBrush brush = [] {
		QPixmap squares(16, 16);
		squares.fill(QColor(0x80, 0x80, 0x80));
		QPainter p(&squares);
		p.fillRect(0, 0, 8, 8, QColor(0x66, 0x66, 0x66));
		p.fillRect(8, 8, 8, 8, QColor(0x66, 0x66, 0x66));
		return QBrush(squares);
	}();
	return brush;
}

QPixmap Core::eraserRow()
{
	// transparent cells crossed out in red: the tile is removed
	QPixmap row(192, 32);
	QPainter p(&row);
	p.fillRect(row.rect(), transparencyBrush());
	p.setRenderHint(QPainter::Antialiasing);
	p.setPen(QPen(QColor(0xd0, 0x30, 0x30), 2));
	for (int x = 0; x < 192; x += 32) {
		p.drawLine(x + 6, 26, x + 26, 6);
		p.drawRect(QRectF(x + 0.5, 0.5, 31, 31));
	}
	return row;
}

QPixmap Core::createDummyPixmap(int width, int height)
{
	QPixmap dummy(width, height);
	QPainter p(&dummy);
	p.fillRect(0, 0, width, height, transparencyBrush());
	p.end();
	return dummy;
}
