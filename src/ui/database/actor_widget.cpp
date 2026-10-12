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

#include "actor_widget.h"
#include "database_list_data.h"
#include "common/signal_blocker.h"
#include "ui_actor_widget.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGraphicsOpacityEffect>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "common/dbstring.h"
#include "common/lcf_widget_binding.h"
#include "model/actor.h"
#include "ui/picker/picker_dialog.h"
#include "ui/picker/picker_charset_widget.h"
#include "ui/picker/picker_faceset_widget.h"
#include "curve_dialog.h"
#include "experience_dialog.h"
#include "ui/common/text_limit.h"

ActorWidget::ActorWidget(ProjectData& project, QWidget *parent) :
	QWidget(parent),
	ui(new Ui::ActorWidget),
	m_project(project)
{
	auto& database = project.database();

	const auto kMaxLevel = database.system.ldb_id == 2003 ? 99 : 50;
	const auto kMaxHp = database.system.ldb_id == 2003 ? 9999 : 999;
	ui->setupUi(this);
	TextLimit::mark(ui->lineName, 12);
	TextLimit::mark(ui->lineTitle, 12);

	m_current = nullptr;

	ui->spinMinLv->setMaximum(kMaxLevel);
	ui->spinMaxLv->setMaximum(kMaxLevel);

	ui->graphicsBattleset->setScene(new QGraphicsScene(this));

	m_battlerItem = new BattleAnimationItem();

	for (int i = 0; i < kMaxLevel; i++)
		m_dummyCurve.push_back(0);
	m_hpItem = new CurveItem(CurveDialog::color(0), m_dummyCurve);
	m_hpItem->setMaxValue(kMaxHp);
	m_hpItem->setMaxLevel(kMaxLevel);
	m_mpItem = new CurveItem(CurveDialog::color(1), m_dummyCurve);
	m_mpItem->setMaxLevel(kMaxLevel);
	m_attItem = new CurveItem(CurveDialog::color(2), m_dummyCurve);
	m_attItem->setMaxLevel(kMaxLevel);
	m_defItem = new CurveItem(CurveDialog::color(3), m_dummyCurve);
	m_defItem->setMaxLevel(kMaxLevel);
	m_intItem = new CurveItem(CurveDialog::color(4), m_dummyCurve);
	m_intItem->setMaxLevel(kMaxLevel);
	m_agyItem = new CurveItem(CurveDialog::color(5), m_dummyCurve);
	m_agyItem->setMaxLevel(kMaxLevel);

	ui->graphicsBattleset->setScene(new QGraphicsScene(this));
	ui->graphicsBattleset->scene()->addItem(m_battlerItem);
	ui->graphicsBattleset->scene()->setSceneRect(0,0,48,48);

	int curve = 0;
	for (auto& uis : {
			ui->graphicsHp,
			ui->graphicsMp,
			ui->graphicsAtt,
			ui->graphicsDef,
			ui->graphicsInt,
			ui->graphicsAgy }) {
		uis->setScene(new QGraphicsScene(this));
		uis->scene()->setSceneRect(QRectF(QPointF(0, 0), QSize(uis->size().width() - 4, uis->size().height() - 4)));
		// like RPG Maker, a double-click on a graph edits its curve
		uis->viewport()->setToolTip(tr("Double-click to edit the curve"));
		ClassEditors::filterEvents(uis->viewport(), [this, curve](QEvent* event) {
			if (event->type() != QEvent::MouseButtonDblClick) {
				return false;
			}
			editCurves(curve);
			return true;
		});
		++curve;
	}
	ui->graphicsHp->scene()->addItem(m_hpItem);
	ui->graphicsMp->scene()->addItem(m_mpItem);
	ui->graphicsAtt->scene()->addItem(m_attItem);
	ui->graphicsDef->scene()->addItem(m_defItem);
	ui->graphicsInt->scene()->addItem(m_intItem);
	ui->graphicsAgy->scene()->addItem(m_agyItem);

	UpdateModels();

	// Classes, battle sets and battle commands only exist in RPG Maker 2003
	if (database.system.ldb_id != 2003) {
		for (QWidget* widget : {static_cast<QWidget*>(ui->label_48), static_cast<QWidget*>(ui->graphicsBattleset),
				static_cast<QWidget*>(ui->comboBattleset), static_cast<QWidget*>(ui->groupBoxClass),
				static_cast<QWidget*>(ui->groupBattleCommands)}) {
			widget->setEnabled(false);
		}
	}

	// The class is saved when chosen; Apply also copies its settings
	LcfWidgetBinding::connect<int32_t>(this, ui->comboProfession);
	connect(ui->comboProfession, &QComboBox::currentIndexChanged, this, [this] {
		ui->pushApplyProfession->setEnabled(ui->comboProfession->currentData().toInt() > 0);
	});

	for (auto* combo : commandCombos()) {
		combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		combo->setMinimumContentsLength(10);
		connect(combo, &QComboBox::currentIndexChanged, this, [this] {
			m_current->battle_commands = ClassEditors::battleCommands(commandCombos());
		});
	}

	ui->tableSkills->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	ClassEditors::connectSkillTable(ui->tableSkills, project, [this]() -> auto& { return m_current->skills; });

	connect(ui->pushExpCurveEdit, &QPushButton::clicked, this, [this] {
		if (ExperienceDialog::edit(this, m_project, m_current->exp_base, m_current->exp_inflation,
				m_current->exp_correction)) {
			ResetExpText(m_current);
		}
	});
	connect(ui->pushEditCustom, &QPushButton::clicked, this, &ActorWidget::editEasyRpgProperties);

	for (auto& uis : { ui->lineName, ui->lineTitle }) {
		LcfWidgetBinding::connect(this, uis);
	}

	for (auto& uis : {
			ui->checkAI,
			ui->checkDualWeapon,
			ui->checkFixedEquip,
			ui->checkStrongDefense,
			ui->checkTranslucent }) {
		LcfWidgetBinding::connect<bool>(this, uis);
	}

	for (auto& uis : {
			ui->spinMinLv,
			ui->spinMaxLv,
			ui->spinCritChance }) {
		LcfWidgetBinding::connect<int32_t>(this, uis);
	}

	for (auto& uis : {
			ui->comboInitialWeapon,
			ui->comboInitialShield,
			ui->comboInitialArmor,
			ui->comboInitialHelmet,
			ui->comboInitialMisc }) {
		LcfWidgetBinding::connect<int16_t>(this, uis);
		uis->makeModel(project);
	}

	LcfWidgetBinding::connect<bool>(this, ui->groupCritChance);

	LcfWidgetBinding::connect<int32_t>(this, ui->comboUnarmedAnimation);
	ui->comboUnarmedAnimation->makeModel(project);

	// GraphicViews
	m_charsetItem = new CharSetGraphicsItem(project);
	m_charsetItem->setSpin(true);
	m_charsetItem->setWalk(true);
	m_charsetItem->setGraphicsEffect(new QGraphicsOpacityEffect(this));
	ui->graphicsCharset->scale(2.0, 2.0);
	ui->graphicsCharset->setItem(m_charsetItem);
	ui->graphicsCharset->enableTimer();
	QObject::connect(ui->graphicsCharset, &RpgGraphicsViewBase::clicked, this, &ActorWidget::charSetClicked);

	m_faceItem = new FaceSetGraphicsItem(project);
	ui->graphicsFaceset->scale(2.0, 2.0);
	ui->graphicsFaceset->setItem(m_faceItem);
	QObject::connect(ui->graphicsFaceset, &RpgGraphicsViewBase::clicked, this, &ActorWidget::faceSetClicked);

	on_currentActorChanged(nullptr);

	// Array Size, a paste or a rename in another tab: list them again
	connect(&DatabaseListNotifier::instance(), &DatabaseListNotifier::listChanged, this, [this](const void* list) {
		const auto& database = m_project.database();
		if (list != &database.battleranimations && list != &database.classes
				&& list != &database.attributes && list != &database.states) {
			return;
		}
		{
			// the hero's battler animation must not follow the rebuilt combo box
			SignalBlocker blocker(ui->comboBattleset);
			UpdateModels();
		}
		setData(m_current); // the classes and the ranks again
	});
}

void ActorWidget::setData(lcf::rpg::Actor* actor) {
	on_currentActorChanged(actor);
}

ActorWidget::~ActorWidget()
{
	delete ui;
}

void ActorWidget::UpdateModels()
{
	auto& database = m_project.database();

	/* Clear */
	ui->comboBattleset->clear();
	ui->listAttributeRanks->clear();
	ui->listStatusRanks->clear();
	/* Fill */
	ui->comboBattleset->addItem(tr("<none>"));
	for (unsigned int i = 0; i < database.battleranimations.size(); i++)
		ui->comboBattleset->addItem(database.battleranimations[i].name.c_str());
	for (unsigned int i = 0; i < database.attributes.size(); i++)
		ui->listAttributeRanks->addItem(database.attributes[i].name.c_str());
	for (unsigned int i = 0; i < database.states.size(); i++)
		ui->listStatusRanks->addItem(database.states[i].name.c_str());
}

void ActorWidget::on_spinMinLv_valueChanged(int value) {
	if (ui->spinMaxLv->value() < value) {
		ui->spinMaxLv->setValue(value);
	}
}

void ActorWidget::on_spinMaxLv_valueChanged(int value) {
	if (ui->spinMinLv->value() > value) {
		ui->spinMinLv->setValue(value);
	}
}

void ActorWidget::on_comboBattleset_currentIndexChanged(int index)
{
	if (!m_current)
		return;

	m_current->battler_animation = index;

	auto& database = m_project.database();

	if (index <= 0 || index > static_cast<int>(database.battleranimations.size()))
		m_battlerItem->setBasePix(BattleAnimationItem::Battler,"");
	else
		m_battlerItem->setDemoAnimation(database.battleranimations[static_cast<size_t>(index) - 1]);
}

void ActorWidget::ResetExpText(lcf::rpg::Actor* actor) {
	ui->labelExpCurve->setText(ExperienceDialog::summary(m_project, actor->exp_base, actor->exp_inflation,
		actor->exp_correction));
}

void ActorWidget::fillClasses() {
	// The class page can rename or add classes: list them again
	SignalBlocker blocker(ui->comboProfession);
	ui->comboProfession->clear();
	ui->comboProfession->addItem(tr("<none>"), 0);
	for (const auto& cls : m_project.database().classes) {
		ui->comboProfession->addItem(QString("%1: %2").arg(cls.ID, 4, 10, QChar('0')).arg(ToQString(cls.name)), cls.ID);
	}
	const int id = m_current->class_id;
	if (ui->comboProfession->findData(id) < 0) {
		ui->comboProfession->addItem(tr("<%1?>").arg(id), id);
	}
}

ClassEditors::CommandCombos ActorWidget::commandCombos() const {
	return {ui->comboBattleCommand1, ui->comboBattleCommand2, ui->comboBattleCommand3,
		ui->comboBattleCommand4, ui->comboBattleCommand5, ui->comboBattleCommand6};
}

void ActorWidget::editCurves(int tab) {
	const bool is2k3 = m_project.database().system.ldb_id == 2003;
	if (ClassEditors::editParameters(this, m_current->parameters, is2k3 ? 99 : 50, is2k3 ? 9999 : 999, tab)) {
		on_currentActorChanged(m_current);
	}
}

void ActorWidget::on_currentActorChanged(lcf::rpg::Actor *actor)
{
	if (!actor) {
		actor = &dummy;
	}
	m_current = actor;

	auto& database = m_project.database();

	ResetExpText(actor);

	LcfWidgetBinding::bind(ui->lineName, actor->name);
	LcfWidgetBinding::bind(ui->lineTitle, actor->title);
	LcfWidgetBinding::bind(ui->checkAI, actor->auto_battle);
	LcfWidgetBinding::bind(ui->checkDualWeapon, actor->two_weapon);
	LcfWidgetBinding::bind(ui->checkFixedEquip, actor->lock_equipment);
	LcfWidgetBinding::bind(ui->checkStrongDefense, actor->super_guard);
	LcfWidgetBinding::bind(ui->checkTranslucent, actor->transparent);
	LcfWidgetBinding::bind(ui->spinMinLv, actor->initial_level);
	LcfWidgetBinding::bind(ui->spinMaxLv, actor->final_level);
	LcfWidgetBinding::bind(ui->spinCritChance, actor->critical_hit_chance);
	LcfWidgetBinding::bind(ui->groupCritChance, actor->critical_hit);
	LcfWidgetBinding::bind(ui->comboUnarmedAnimation, actor->unarmed_animation);

	auto equipFilter = [&](auto& cbox, auto type) {
		SignalBlocker s(cbox->comboBox());
		cbox->setFilter(static_cast<QSortFilterProxyModel*>(ActorModel(m_project, *m_current).CreateEquipmentFilter(type, this)));
	};

	equipFilter(ui->comboInitialWeapon, lcf::rpg::Item::Type_weapon);
	equipFilter(ui->comboInitialShield, lcf::rpg::Item::Type_shield);
	equipFilter(ui->comboInitialArmor, lcf::rpg::Item::Type_armor);
	equipFilter(ui->comboInitialHelmet, lcf::rpg::Item::Type_helmet);
	equipFilter(ui->comboInitialMisc, lcf::rpg::Item::Type_accessory);

	LcfWidgetBinding::bind(ui->comboInitialWeapon, actor->initial_equipment.weapon_id);
	LcfWidgetBinding::bind(ui->comboInitialShield, actor->initial_equipment.shield_id);
	LcfWidgetBinding::bind(ui->comboInitialHelmet, actor->initial_equipment.helmet_id);
	LcfWidgetBinding::bind(ui->comboInitialArmor, actor->initial_equipment.armor_id);
	LcfWidgetBinding::bind(ui->comboInitialMisc, actor->initial_equipment.accessory_id);

	ui->comboBattleset->setCurrentIndex(actor->battler_animation);

	fillClasses();
	LcfWidgetBinding::bind(ui->comboProfession, actor->class_id);
	ui->pushApplyProfession->setEnabled(actor->class_id > 0);
	ClassEditors::fillBattleCommands(commandCombos(), actor->battle_commands, m_project);
	ClassEditors::fillSkills(ui->tableSkills, actor->skills, m_project);
	for (int i = 0; i < ui->listAttributeRanks->count(); i++)
	{
		if (i >= static_cast<int>(actor->attribute_ranks.size()))
			ui->listAttributeRanks->item(static_cast<int>(i))->setIcon(QIcon(":/ranks/rank2"));
		else
			ui->listAttributeRanks->item(static_cast<int>(i))->setIcon(QIcon(QString(":/ranks/rank%1").arg(static_cast<int>(actor->attribute_ranks[static_cast<size_t>(i)]))));
	}
	for (int i = 0; i < ui->listStatusRanks->count(); i++)
	{
		if (i >= static_cast<int>(actor->state_ranks.size()))
			ui->listStatusRanks->item(i)->setIcon(QIcon(":/ranks/rank2"));
		else
			ui->listStatusRanks->item(i)->setIcon(QIcon(QString(":/ranks/rank%1").arg(static_cast<int>(actor->state_ranks[static_cast<size_t>(i)]))));
	}

	if (actor->battler_animation <= 0 ||
			actor->battler_animation > static_cast<int>(database.battleranimations.size()))
		m_battlerItem->setBasePix(BattleAnimationItem::Battler,"");
	else
		m_battlerItem->setDemoAnimation(database.battleranimations[static_cast<size_t>(actor->battler_animation)-1]);

	m_hpItem->setData(actor->parameters.maxhp);
	m_mpItem->setData(actor->parameters.maxsp);
	m_attItem->setData(actor->parameters.attack);
	m_defItem->setData(actor->parameters.defense);
	m_intItem->setData(actor->parameters.spirit);
	m_agyItem->setData(actor->parameters.agility);

	// GraphicViews
	m_faceItem->refresh(*actor);
	m_charsetItem->refresh(*actor);
	m_charsetItem->graphicsEffect()->setEnabled(actor->transparent);

	this->setEnabled(actor != &dummy);
}

void ActorWidget::on_pushApplyProfession_clicked()
{
	const auto& classes = m_project.database().classes;
	const int id = ui->comboProfession->currentData().toInt();
	if (m_current == &dummy || id < 1 || id > static_cast<int>(classes.size())) {
		return;
	}

	// What RPG Maker's help says Apply copies: the basic statistics, the
	// experience curve, the skills, the resistances and the battle commands
	const lcf::rpg::Class& cls = classes[static_cast<size_t>(id) - 1];
	m_current->class_id = id;
	m_current->parameters = cls.parameters;
	m_current->exp_base = cls.exp_base;
	m_current->exp_inflation = cls.exp_inflation;
	m_current->exp_correction = cls.exp_correction;
	m_current->skills = cls.skills;
	m_current->state_ranks = cls.state_ranks;
	m_current->attribute_ranks = cls.attribute_ranks;
	m_current->battle_commands = cls.battle_commands;

	on_currentActorChanged(m_current);
}

void ActorWidget::editEasyRpgProperties()
{
	// Settings of the actor that only EasyRPG Player reads (liblcf's
	// easyrpg_* fields of the hero), shown as checkboxes and numbers
	auto& actor = *m_current;
	QDialog dialog(this);
	dialog.setWindowTitle(tr("EasyRPG Properties"));
	auto* layout = new QVBoxLayout(&dialog);
	auto* info = new QLabel(tr("EasyRPG Player reads these settings; RPG Maker ignores them."), &dialog);
	info->setWordWrap(true);
	layout->addWidget(info);

	struct Flag {
		QString text;
		const char* name;
		bool& value;
		QCheckBox* check = nullptr;
	};
	std::array<Flag, 6> flags = {{
		{tr("Prevent critical hits"), "checkPreventCritical", actor.easyrpg_prevent_critical},
		{tr("Raise evasion"), "checkRaiseEvasion", actor.easyrpg_raise_evasion},
		{tr("Immune to attribute reductions"), "checkImmuneAttributeDownshifts", actor.easyrpg_immune_to_attribute_downshifts},
		{tr("Ignore evasion"), "checkIgnoreEvasion", actor.easyrpg_ignore_evasion},
		{tr("Attack twice"), "checkDualAttack", actor.easyrpg_dual_attack},
		{tr("Attack all enemies"), "checkAttackAll", actor.easyrpg_attack_all},
	}};
	auto* defense = new QGroupBox(tr("Defense"), &dialog);
	auto* defense_layout = new QVBoxLayout(defense);
	auto* unarmed = new QGroupBox(tr("Unarmed Attacks"), &dialog);
	auto* unarmed_layout = new QFormLayout(unarmed);
	for (size_t i = 0; i < flags.size(); ++i) {
		auto& flag = flags[i];
		flag.check = new QCheckBox(flag.text, &dialog);
		flag.check->setObjectName(QString::fromLatin1(flag.name));
		flag.check->setChecked(flag.value);
		if (i < 3) {
			defense_layout->addWidget(flag.check);
		} else {
			unarmed_layout->addRow(flag.check);
		}
	}
	auto* hit = new QSpinBox(unarmed);
	hit->setObjectName(QStringLiteral("spinUnarmedHit"));
	// -1: the hit rate of RPG Maker, 90%
	hit->setRange(-1, 100);
	hit->setSuffix(tr("%"));
	hit->setSpecialValueText(tr("Default (90%)"));
	hit->setValue(actor.easyrpg_unarmed_hit);
	unarmed_layout->addRow(tr("Hit rate"), hit);
	layout->addWidget(defense);
	layout->addWidget(unarmed);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}
	for (auto& flag : flags) {
		flag.value = flag.check->isChecked();
	}
	actor.easyrpg_unarmed_hit = hit->value();
}

void ActorWidget::on_listStatusRanks_clicked() {
	if (!m_current) {
		return;
	}

	int index = ui->listStatusRanks->currentRow();
	if (static_cast<int>(m_current->state_ranks.size()) <= index) {
		for (int i = m_current->state_ranks.size(); i <= index; i++) {
			m_current->state_ranks.push_back(2);
		}
	}
	int rank = m_current->state_ranks[index];
	rank = (rank + 1) % 5;
	m_current->state_ranks[index] = rank;
	ui->listStatusRanks->item(index)->setIcon(QIcon(QString(":/ranks/rank%1").arg(rank)));

	if (index == static_cast<int>(m_current->state_ranks.size()) - 1 && rank == 2) {
		m_current->state_ranks.pop_back();
		for (int i = m_current->state_ranks.size() - 1; i >= 0; i--) {
			if (m_current->state_ranks[i] == 2) {
				m_current->state_ranks.pop_back();
			} else {
				break;
			}
		}
	}
}

void ActorWidget::on_listAttributeRanks_clicked() {
	if (!m_current) {
		return;
	}

	int index = ui->listAttributeRanks->currentRow();
	if (static_cast<int>(m_current->attribute_ranks.size()) <= index) {
		for (int i = m_current->attribute_ranks.size(); i <= index; i++) {
			m_current->attribute_ranks.push_back(2);
		}
	}
	int rank = m_current->attribute_ranks[index];
	rank = (rank + 1) % 5;
	m_current->attribute_ranks[index] = rank;
	ui->listAttributeRanks->item(index)->setIcon(QIcon(QString(":/ranks/rank%1").arg(rank)));

	if (index == static_cast<int>(m_current->attribute_ranks.size()) - 1 && rank == 2) {
		m_current->attribute_ranks.pop_back();
		for (int i = m_current->attribute_ranks.size() - 1; i >= 0; i--) {
			if (m_current->attribute_ranks[i] == 2) {
				m_current->attribute_ranks.pop_back();
			} else {
				break;
			}
		}
	}
}

void ActorWidget::resizeEvent(QResizeEvent *event)
{
	Q_UNUSED(event)
	for (auto& uis : {
			ui->graphicsHp,
			ui->graphicsMp,
			ui->graphicsAtt,
			ui->graphicsDef,
			ui->graphicsInt,
			ui->graphicsAgy }) {
		uis->scene()->setSceneRect(QRectF(QPointF(0, 0), QSize(uis->size().width() - 4, uis->size().height() - 4)));
	}
}

void ActorWidget::faceSetClicked() {
	if (!m_current) {
		return;
	}

	auto* widget = new PickerFacesetWidget(m_current->face_index, this);
	PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
	QObject::connect(&dialog, &PickerDialog::fileSelected, [&](const QString& baseName) {
		m_current->face_name = ToDBString(baseName);
		m_current->face_index = widget->index();
		m_faceItem->refresh(*m_current);
	});
	dialog.setDirectoryAndFile(FACESET, ToQString(m_current->face_name));
	dialog.exec();
}

void ActorWidget::charSetClicked() {
	if (!m_current) {
		return;
	}

	auto* widget = new PickerCharsetWidget(m_current->character_index, this);
	PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
	QObject::connect(&dialog, &PickerDialog::fileSelected, [&](const QString& baseName) {
		m_current->character_name = ToDBString(baseName);
		m_current->character_index = widget->index();
		m_charsetItem->refresh(*m_current);
	});
	dialog.setDirectoryAndFile(CHARSET, ToQString(m_current->character_name));
	dialog.exec();
}
