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

#include "ui/event/event_command_base_widget.h"
#include <QList>
#include <QPair>
#include <vector>

class QBoxLayout;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFormLayout;
class RpgSpinBox;

/** Editors of the scene and battle commands. Each field writes its parameter
 * only when edited: unknown values and missing optional parameters stay. */
class SceneCommandWidget : public EventCommandBaseWidget
{
	Q_OBJECT
public:
	SceneCommandWidget(ProjectData& project, QWidget* parent);

protected:
	int parameter(int index, int fallback = 0) const;
	void setParameter(int index, int value);
	QFormLayout* section(QBoxLayout* layout, const QString& title);
	QComboBox* enumField(const QString& name, int index, const QStringList& choices, int first = 0);
	/** Selects the parameter's value (an unknown one is added) and writes the choices */
	void bindCombo(QComboBox* field, int index, int fallback = 0);
	/** Radio buttons as row labels, each enabling the field of its row. With
	 * value_index, a choice also writes its field's value there. */
	QButtonGroup* radioField(QFormLayout* form, const QString& name, int index,
		const QList<QPair<QString, QWidget*>>& choices, int value_index = -1);
	RpgSpinBox* spinField(const QString& name, int index, int minimum, int maximum);
	QCheckBox* checkField(const QString& text, const QString& name, int index, bool inverted = false);
	template <typename T> T* databaseField(const QString& name, int index);
	QComboBox* monsterField(const QString& name, int index);
	QWidget* backdropField(const QString& name);
	/** Monster, operation and amount of Change Monster HP and MP */
	void monsterAmountFields(bool hp);
};

/** Commands followed by handler branches: the editor works on a copy and
 * adds or removes the branches on OK. */
class SceneBlockWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	using SceneCommandWidget::setData;
	void setData(EventCommandList* commands) override;
	bool apply();

protected:
	SceneBlockWidget(ProjectData& project, lcf::rpg::EventCommand::Code closing, QWidget* parent);
	/** The branch lines a command with these parameters has */
	virtual std::vector<lcf::rpg::EventCommand::Code> handlers(const lcf::rpg::EventCommand& cmd) const = 0;

private:
	lcf::rpg::EventCommand::Code m_closing;
	lcf::rpg::EventCommand m_edited;
};

class EnemyEncounterWidget : public SceneBlockWidget
{
	Q_OBJECT
public:
	EnemyEncounterWidget(ProjectData& project, QWidget* parent);
	using SceneBlockWidget::setData;
	void setData(lcf::rpg::EventCommand* cmd) override;

protected:
	std::vector<lcf::rpg::EventCommand::Code> handlers(const lcf::rpg::EventCommand& cmd) const override;
};

class OpenShopWidget : public SceneBlockWidget
{
	Q_OBJECT
public:
	OpenShopWidget(ProjectData& project, QWidget* parent);
	using SceneBlockWidget::setData;
	void setData(lcf::rpg::EventCommand* cmd) override;

protected:
	std::vector<lcf::rpg::EventCommand::Code> handlers(const lcf::rpg::EventCommand& cmd) const override;
};

class ShowInnWidget : public SceneBlockWidget
{
	Q_OBJECT
public:
	ShowInnWidget(ProjectData& project, QWidget* parent);
	using SceneBlockWidget::setData;
	void setData(lcf::rpg::EventCommand* cmd) override;

protected:
	std::vector<lcf::rpg::EventCommand::Code> handlers(const lcf::rpg::EventCommand& cmd) const override;
};

class ChangeMonsterHPWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	ChangeMonsterHPWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class ChangeMonsterMPWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	ChangeMonsterMPWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class ChangeMonsterConditionWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	ChangeMonsterConditionWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class ShowHiddenMonsterWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	ShowHiddenMonsterWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class ChangeBattleBGWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	ChangeBattleBGWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class ShowBattleAnimation_BWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	ShowBattleAnimation_BWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class CallCommonEventWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	CallCommonEventWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class ForceFleeWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	ForceFleeWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};

class EnableComboWidget : public SceneCommandWidget
{
	Q_OBJECT
public:
	EnableComboWidget(ProjectData& project, QWidget* parent);
	void setData(lcf::rpg::EventCommand* cmd) override;
};
