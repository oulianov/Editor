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

#include "battle_page_widget.h"
#include "ui_battle_page_widget.h"
#include "battle_page_condition_dialog.h"

BattlePageWidget::BattlePageWidget(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::BattlePageWidget)
{
	ui->setupUi(this);
	connect(ui->pushTrigger, &QToolButton::clicked, this, &BattlePageWidget::editTrigger);
}

BattlePageWidget::~BattlePageWidget()
{
	delete ui;
}

lcf::rpg::TroopPage* BattlePageWidget::eventPage() const {
	return m_eventPage;
}

void BattlePageWidget::setEventPage(ProjectData& project, const lcf::rpg::Troop& troop, lcf::rpg::TroopPage* eventPage) {
	m_project = &project;
	m_troop = &troop;
	m_eventPage = eventPage;

	ui->commands->setData(project, eventPage);
	updateTrigger();
}

void BattlePageWidget::editTrigger() {
	if (!m_eventPage) {
		return;
	}
	BattlePageConditionDialog dialog(*m_project, *m_troop, m_eventPage->condition, this);
	if (dialog.exec() == QDialog::Accepted) {
		updateTrigger();
	}
}

void BattlePageWidget::updateTrigger() {
	QString text = m_eventPage ? BattlePageConditionDialog::conditionText(*m_project, *m_troop, m_eventPage->condition) : QString();
	ui->lineTrigger->setText(text);
	// The whole text when the line is too short for it
	ui->lineTrigger->setToolTip(text);
	ui->lineTrigger->setCursorPosition(0);
}
