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

#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QtQmlIntegration>
#include <array>

#include "model/battler_animation_editor.h"
#include "qmlbinding/json_view.h"

class ProjectData;

/**
 * The previews of the Battle Animation 2 page (BattlerAnimationEditor) in a
 * database window's copy: the pose or weapon chosen played at the display
 * speed (its three frames, a weapon in the hands of the hero as RPG Maker
 * shows it), and the rows of its graphic, 4 by 2, the one used framed. The
 * page shows the pictures (Ez.ZoomImageView) and edits the fields itself.
 */
class BattlerAnimationPreview : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	/** The battler animation shown (its view, "/battleranimations/0") */
	Q_PROPERTY(JsonView* entry READ entry WRITE setEntry NOTIFY entryChanged)
	/** Battler animations are only used by RPG Maker 2003: else nothing plays */
	Q_PROPERTY(bool rpg2k3 READ rpg2k3 WRITE setRpg2k3 NOTIFY entryChanged)
	Q_PROPERTY(int currentPose READ currentPose WRITE setCurrentPose NOTIFY currentPoseChanged)
	Q_PROPERTY(int currentWeapon READ currentWeapon WRITE setCurrentWeapon NOTIFY currentWeaponChanged)
	/** The weapons are shown, else the poses */
	Q_PROPERTY(bool weapons READ weapons WRITE setWeapons NOTIFY weaponsChanged)
	/** BattlerAnimationEditor::Hand: what the weapon preview shows it in */
	Q_PROPERTY(int hand READ hand WRITE setHand NOTIFY handChanged)
	/** The frame of the preview, at its size (null: nothing to show) */
	Q_PROPERTY(QImage preview READ preview NOTIFY previewChanged)
	/** The middle frame of each row of the graphic, the row used framed */
	Q_PROPERTY(QImage rows READ rows NOTIFY rowsChanged)
	/** The size of a frame: 48 for a pose, 64 for a weapon */
	Q_PROPERTY(int frameSize READ frameSize NOTIFY rowsChanged)
	/** The frame of the row used */
	Q_PROPERTY(QColor highlight READ highlight WRITE setHighlight NOTIFY rowsChanged)

public:
	explicit BattlerAnimationPreview(QObject* parent = nullptr);

	JsonView* entry() const;
	void setEntry(JsonView* entry);
	bool rpg2k3() const;
	void setRpg2k3(bool rpg2k3);
	int currentPose() const;
	void setCurrentPose(int index);
	int currentWeapon() const;
	void setCurrentWeapon(int index);
	bool weapons() const;
	void setWeapons(bool weapons);
	int hand() const;
	void setHand(int hand);
	QImage preview() const;
	QImage rows() const;
	int frameSize() const;
	QColor highlight() const;
	void setHighlight(const QColor& color);

	/** The row of the graphic at a point of the rows picture, -1 for none */
	Q_INVOKABLE int rowAt(qreal x, qreal y) const;
	/** The poses the Player uses keep the names RPG Maker gives them */
	Q_INVOKABLE bool isPoseNameFixed(int index) const;
	/** The frames shown since the preview started again (the scripts) */
	Q_INVOKABLE int step() const;

signals:
	void entryChanged();
	void currentPoseChanged();
	void currentWeaponChanged();
	void weaponsChanged();
	void handChanged();
	void previewChanged();
	void rowsChanged();

private:
	ProjectData* project() const;
	/** The animation of the entry, again (the list may have moved) */
	void findAnimation();
	void onDataChanged(const QString& jsonPtr);
	/** The graphics of the part shown, read again */
	void loadPreview();
	void updatePreview();
	QImage graphic(const QString& dir, const QString& name);

	QPointer<JsonView> m_entry;
	bool m_rpg2k3 = true;
	BattlerAnimationEditor m_editor;
	bool m_weapons = false;
	int m_hand = 0;
	QColor m_highlight = QColor(48, 140, 198);
	/** The graphic of the part shown, its frames' size and the row used */
	QImage m_sheet;
	int m_sheetSize = BattlerAnimationEditor::kBattlerSize;
	int m_sheetRow = 0;
	/** The attack poses of the right and left hands, under a weapon */
	std::array<QImage, 2> m_handSheets;
	std::array<int, 2> m_handRows {0, 0};
	QImage m_preview;
	QImage m_rows;
	/** The graphics read, by folder and name: a field typed does not read them again */
	QHash<QString, QImage> m_graphics;
};
