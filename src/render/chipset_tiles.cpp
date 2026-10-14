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

#include "chipset_tiles.h"
#include <QBitmap>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <vector>
#include "common/dbstring.h"
#include "common/image_loader.h"
#include "defines.h"
#include "model/autotile.h"
#include "model/project.h"
#include "render/tile_painter.h"

ChipsetTiles::ChipsetTiles(const lcf::rpg::Chipset& chipset) :
	m_chipset(chipset)
{
}

ChipsetTiles::ChipsetTiles(const lcf::rpg::Chipset& chipset, QPixmap o_chipset, int tile_size) :
	m_chipset(chipset), m_tileSize(tile_size)
{
	using namespace Autotile;

	if (!o_chipset)
		o_chipset = TilePainter::createDummyPixmap(480,256);
	else // Skip color mask when using the dummy pixmap
	{
		if (o_chipset.toImage().colorCount() > 0)
			m_keyColor = QColor(o_chipset.toImage().color(0));
		else
			// Qt might remove the color table. Use the color of the first upper tile instead
			m_keyColor = QColor(o_chipset.toImage().pixel(288,128));
		o_chipset.setMask(o_chipset.createMaskFromColor(m_keyColor));
	}

	int r_tileSize = o_chipset.width()/30;
	int r_tileHalf = r_tileSize/2;

	/* BindWaterTiles */
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
		if (!m_tiles[id].isNull()) //item exist?
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
			QPixmap p_tile(m_tileSize, m_tileSize);
			p_tile.fill(Qt::transparent);
			QPainter p(&p_tile);
			if (isABWater (terrain_id))
				p.drawPixmap(0,0,m_tileSize,m_tileSize,o_chipset.copy(frame_x, 4*r_tileSize,r_tileSize,r_tileSize));
			else
				p.drawPixmap(0,0,m_tileSize,m_tileSize,o_chipset.copy(frame_x, 7*r_tileSize,r_tileSize,r_tileSize));
			// Draw UpperLeft corner
			int dest_x = 0, dest_y = 0;
#define blit(x,y) p.drawPixmap(dest_x,dest_y,m_tileSize/2,m_tileSize/2,o_chipset.copy(frame_x+(x), y,r_tileHalf,r_tileHalf))
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
			dest_x = m_tileSize/2;
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
			dest_y = m_tileSize/2;
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
		m_tiles[id] = water_tile(0);
		for (int frame = 1; frame < 3; frame++)
			m_frames[frame - 1][id] = water_tile(frame);
	}

	/* Register AnimationTiles */
	// 3 columns of 4 frames, under the borders of the water B
	for (int column = 0; column < 3; column++)
		for (int frame = 0; frame < 4; frame++)
		{
			QPixmap a_tile(m_tileSize, m_tileSize);
			a_tile.fill(Qt::transparent);
			QPainter a(&a_tile);
			a.drawPixmap(0,0,m_tileSize,m_tileSize,o_chipset.copy((3+column)*r_tileSize,(4+frame)*r_tileSize,r_tileSize,r_tileSize));
			a.end();
			// animated tiles have no borders: RPG Maker writes 3000, 3050, 3100
			for (int i = 0; i < 50; i++) {
				const short id = static_cast<short>(translate(3 + column) + i);
				if (frame == 0)
					m_tiles[id] = a_tile;
				else
					m_frames[frame - 1][id] = a_tile;
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
			if (!m_tiles[id].isNull()) //item exist?
				continue;

			/*
			 * Get base
			 */
			QPixmap p_tile(m_tileSize, m_tileSize);
			QPainter p(&p_tile);

			/*
			 * Draw upper_left corner
			 */
			int dest_x = 0;
			int dest_y = 0;
#define blit(x,y) p.drawPixmap(dest_x,dest_y,m_tileSize/2,m_tileSize/2, p_block.copy(x, y,r_tileHalf,r_tileHalf))
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
			dest_x = m_tileSize/2;
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
			dest_y = m_tileSize/2;
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
			dest_x = m_tileSize/2;
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
			m_tiles[id] = p_tile;
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
			QPixmap ef_tile(m_tileSize, m_tileSize);
			ef_tile.fill(QColor(0,0,0,0));
			QPainter ef(&ef_tile);
			int orig_x = tileset_col*6*r_tileSize+col*r_tileSize;
			int orig_y = tile_row*r_tileSize;
			ef.drawPixmap(0,0,m_tileSize,m_tileSize,o_chipset.copy(orig_x,orig_y,r_tileSize,r_tileSize));
			ef.end();
			ef_tile.setMask(ef_tile.createMaskFromColor(m_keyColor));
			m_tiles[translate(terrain_id)] = ef_tile;
			terrain_id++;
		}

		tile_row++;
		if (tile_row == 16)
		{
			tile_row = 0;
			tileset_col++;
		}
	}
}

std::shared_ptr<const ChipsetTiles> ChipsetTiles::load(const Project& project, const lcf::rpg::Chipset& chipset,
	int tile_size, const QString& rtp_dir)
{
	if (chipset.ID == 0)
		return std::make_shared<const ChipsetTiles>(chipset);
	const QString chipset_name = ToQString(chipset.chipset_name);
	QPixmap image = ImageLoader::Load(project.findFile(CHIPSET, chipset_name, FileFinder::FileType::Image));
	if (!image)
		image = ImageLoader::Load(rtp_dir + CHIPSET + chipset_name);
	if (!image)
		qWarning()<<"Chipset"<<chipset_name<<"not found.";
	return std::make_shared<const ChipsetTiles>(chipset, image, tile_size);
}

QPixmap ChipsetTiles::tile(short tile_id, int frame) const
{
	// value(): operator[] would add an empty tile for every unknown id
	if (frame > 0)
		return m_frames[frame - 1].value(tile_id);
	return m_tiles.value(tile_id);
}
