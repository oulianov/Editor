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

#include <QObject>
#include <QPoint>
#include <lcf/rpg/battleranimation.h>

class QTimer;

/**
 * The editing of a battler animation (RPG Maker 2003 only) apart from its
 * page: the pose and the weapon shown, the rules of their fields, and the
 * playback of the previews, the three frames of a row at the display speed.
 * The page loads the graphics and draws the frame of step().
 */
class BattlerAnimationEditor : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int currentPose READ currentPose WRITE setCurrentPose NOTIFY currentPoseChanged)
	Q_PROPERTY(int currentWeapon READ currentWeapon WRITE setCurrentWeapon NOTIFY currentWeaponChanged)
	Q_PROPERTY(int step READ step NOTIFY stepped)

public:
	/** What the weapon preview shows it in: RPG Maker shows it in the hands of the hero */
	enum Hand { RightHand, LeftHand, BothHands, WeaponOnly };
	Q_ENUM(Hand)

	// frames of a BattleCharSet and of a BattleWeapon
	static constexpr int kBattlerSize = 48;
	static constexpr int kWeaponSize = 64;
	// RPG Maker 2003 has 32 poses and 32 weapons in each battler animation
	static constexpr int kSlotCount = 32;
	// the poses the Player uses (idle to item), named by RPG Maker
	static constexpr int kFixedPoses = 12;
	// the rows of a BattleCharSet or BattleWeapon graphic, shown 4 by 2
	static constexpr int kSheetRows = 8;
	static constexpr int kRowColumns = 4;

	explicit BattlerAnimationEditor(QObject* parent = nullptr);

	lcf::rpg::BattlerAnimation* animation() const;
	/**
	 * Shows the first pose and weapon of anim. Battler animations are only
	 * used by RPG Maker 2003: otherwise nothing is changed nor played.
	 */
	void setAnimation(lcf::rpg::BattlerAnimation* anim, bool rpg2k3);
	/** Entries made outside of RPG Maker can have shorter lists: kSlotCount
	 *  poses and weapons */
	static void fillSlots(lcf::rpg::BattlerAnimation& anim);

	/** The display speed: the frames (at 60 per second) a pattern is shown */
	void setSpeed(int speed);
	/** The time a pattern is shown, in ms */
	static int interval(int speed);

	/** The pose selected, -1 when none */
	int currentPose() const;
	void setCurrentPose(int index);
	lcf::rpg::BattlerAnimationPose* pose() const;
	int currentWeapon() const;
	void setCurrentWeapon(int index);
	lcf::rpg::BattlerAnimationWeapon* weapon() const;

	/** The poses the Player uses keep the names RPG Maker gives them */
	static bool isPoseNameFixed(int index);
	/** The graphic is used by character poses, the animation by the others */
	static bool usesAnimation(const lcf::rpg::BattlerAnimationPose& pose);
	/** The attack pose of a hand (RightHand or LeftHand) drawn under the
	 *  weapon: a character pose, else none */
	const lcf::rpg::BattlerAnimationPose* handPose(int hand) const;

	/** The frames played since the preview was shown again */
	int step() const;
	/** The preview starts from its first frame */
	void restart();
	/** The next frame of the previews; the timer calls it at the display speed */
	void nextStep();
	/** The column of the row shown: the three frames played 0 1 2 1 */
	int frameColumn() const;
	/** The hand the weapon preview shows: both hands take turns, a cycle each */
	int shownHand(int choice) const;
	/** The weapon of the left hand is behind the hero, the other in front */
	static bool isWeaponBehind(int hand);
	/** The row under a point of the rows of a graphic (frames of size), -1 when none */
	static int rowAt(const QPoint& pos, int size);

signals:
	void currentPoseChanged();
	void currentWeaponChanged();
	/** The previews show their next frame */
	void stepped();

private:
	lcf::rpg::BattlerAnimation* m_anim = nullptr;
	int m_pose = -1;
	int m_weapon = -1;
	int m_step = 0;
	QTimer* m_timer = nullptr;
};
