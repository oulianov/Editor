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

#include "settings_dialog.h"
#include "common/editor_settings.h"
#include "ui/other/run_game_dialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include "ui/common/path_field.h"
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
	// A path field with a button to choose it
	QWidget* pathRow(QWidget* parent, QLineEdit* field, const std::function<void()>& browse) {
		auto* row = new QWidget(parent);
		auto* button = new QToolButton(row);
		button->setObjectName(field->objectName() + QStringLiteral("Browse"));
		QObject::connect(button, &QToolButton::clicked, row, browse);
		row->setLayout(pathFieldLayout(field, button));
		return row;
	}
}

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
	setObjectName(QStringLiteral("SettingsDialog"));
	setWindowTitle(tr("Settings"));
	const EditorSettings& settings = EditorSettings::instance();
	auto* layout = new QVBoxLayout(this);

	auto* general = new QGroupBox(tr("General"), this);
	auto* general_form = new QFormLayout(general);
	m_language = new QComboBox(general);
	m_language->setObjectName(QStringLiteral("comboLanguage"));
	for (const auto& [code, name] : std::vector<std::pair<QString, QString>>{
			{QString(), tr("System Language")}, {"en", QStringLiteral("English")},
			{"fr", QStringLiteral("Français (Rabbi-Bodom)")}, {"de", QStringLiteral("Deutsch")}}) {
		m_language->addItem(name, code);
	}
	m_language->setCurrentIndex(std::max(0, m_language->findData(settings.language())));
	general_form->addRow(tr("Language"), m_language);
	layout->addWidget(general);

	auto* project = new QGroupBox(tr("Project"), this);
	auto* project_layout = new QVBoxLayout(project);
	project_layout->setSpacing(12);
	project_layout->setSizeConstraint(QLayout::SetMinimumSize);
	m_reloadExternal = new QCheckBox(tr("Reload files changed by other programs (rpgsync...)"), project);
	m_reloadExternal->setObjectName(QStringLiteral("checkReloadExternal"));
	m_reloadExternal->setChecked(settings.reloadExternalChanges());
	project_layout->addWidget(m_reloadExternal);
	m_searchCache = new QCheckBox(tr("Keep search results between searches"), project);
	m_searchCache->setObjectName(QStringLiteral("checkSearchCache"));
	m_searchCache->setChecked(settings.searchCache());
	project_layout->addWidget(m_searchCache);
	layout->addWidget(project);

	auto* paths = new QGroupBox(tr("Paths"), this);
	auto* paths_form = new QFormLayout(paths);
	m_rtpPath = new QLineEdit(settings.rtpPath(), paths);
	m_rtpPath->setObjectName(QStringLiteral("lineRtpPath"));
	paths_form->addRow(tr("RTP folder"), pathRow(paths, m_rtpPath, [this] {
		const QString path = QFileDialog::getExistingDirectory(this, tr("Select the RTP folder"), m_rtpPath->text());
		if (!path.isEmpty()) {
			m_rtpPath->setText(path + QLatin1Char('/'));
		}
	}));
	m_playerPath = new QLineEdit(settings.playerPath(), paths);
	m_playerPath->setObjectName(QStringLiteral("linePlayerPath"));
	m_playerPath->setPlaceholderText(tr("Searched next to the Editor and in the PATH"));
	paths_form->addRow(tr("EasyRPG Player"), pathRow(paths, m_playerPath, [this] {
		const QString program = RunGameDialog::choosePlayer(this);
		if (!program.isEmpty()) {
			m_playerPath->setText(program);
		}
	}));
	layout->addWidget(paths);

	auto* appearance = new QGroupBox(tr("Appearance"), this);
	auto* appearance_layout = new QVBoxLayout(appearance);
	appearance_layout->setSpacing(12);
	appearance_layout->setSizeConstraint(QLayout::SetMinimumSize);
	m_animateTiles = new QCheckBox(tr("Animate water and animated tiles in the map"), appearance);
	m_animateTiles->setObjectName(QStringLiteral("checkAnimateTiles"));
	m_animateTiles->setChecked(settings.animateTiles());
	appearance_layout->addWidget(m_animateTiles);
	m_commandColors = new QCheckBox(tr("Color the event commands by kind, as RPG Maker 2003"), appearance);
	m_commandColors->setObjectName(QStringLiteral("checkCommandColors"));
	m_commandColors->setChecked(settings.commandColors());
	appearance_layout->addWidget(m_commandColors);
	m_commandStripes = new QCheckBox(tr("Stripe the lines of the event commands"), appearance);
	m_commandStripes->setObjectName(QStringLiteral("checkCommandStripes"));
	m_commandStripes->setChecked(settings.commandStripes());
	appearance_layout->addWidget(m_commandStripes);
	layout->addWidget(appearance);
	// Native checkbox layout margins can put their painted indicators into
	// the next row. Reserve the complete widget bounds for these toggles.
	for (auto* toggle : findChildren<QCheckBox*>()) toggle->setAttribute(Qt::WA_LayoutUsesWidgetRect);

	auto* reset = new QPushButton(tr("Reset local state"), this);
	reset->setObjectName(QStringLiteral("buttonResetLocalState"));
	reset->setAutoDefault(false);
	auto* resetNote = new QLabel(tr("Reset window sizes, tabs, selections and scroll positions to their defaults. Game data and preferences are kept."), this);
	resetNote->setWordWrap(true);
	layout->addWidget(reset, 0, Qt::AlignLeft);
	layout->addWidget(resetNote);
	connect(reset, &QPushButton::clicked, this, [resetNote] {
		EditorSettings::instance().resetLocalState();
		resetNote->setText(tr("Local state reset to defaults."));
	});

	layout->addStretch();
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
}

void SettingsDialog::resizeEvent(QResizeEvent* event)
{
	QDialog::resizeEvent(event);
	const int height = layout()->totalHeightForWidth(width());
	if (height > 0) setMinimumHeight(height);
}

void SettingsDialog::accept()
{
	EditorSettings& settings = EditorSettings::instance();
	settings.setLanguage(m_language->currentData().toString());
	settings.setReloadExternalChanges(m_reloadExternal->isChecked());
	settings.setSearchCache(m_searchCache->isChecked());
	QString rtp = m_rtpPath->text().trimmed();
	if (!rtp.isEmpty() && !rtp.endsWith(QLatin1Char('/'))) {
		rtp += QLatin1Char('/');
	}
	settings.setRtpPath(rtp);
	settings.setPlayerPath(m_playerPath->text().trimmed());
	settings.setAnimateTiles(m_animateTiles->isChecked());
	settings.setCommandColors(m_commandColors->isChecked());
	settings.setCommandStripes(m_commandStripes->isChecked());
	QDialog::accept();
}
