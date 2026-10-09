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

#include "picker_audio_widget.h"
#include "ui_picker_audio_widget.h"

#include <QFileInfo>
#include <QStyle>
#include <lcf/rpg/music.h>
#include <lcf/rpg/sound.h>

#include "common/audio_player.h"

PickerAudioWidget::PickerAudioWidget(const lcf::rpg::Music& music, QWidget *parent) :
	PickerChildWidget(parent),
	ui(new Ui::PickerAudioWidget),
	m_type(Type::Music) {
	ui->setupUi(this);
	init();

	ui->sliderFadeIn->setValue(music.fadein / 1000);
	ui->sliderVolume->setValue(music.volume);
	ui->sliderTempo->setValue(music.tempo);
	ui->sliderBalance->setValue(music.balance);
}

PickerAudioWidget::PickerAudioWidget(const lcf::rpg::Sound& sound, QWidget *parent) :
	PickerChildWidget(parent),
	ui(new Ui::PickerAudioWidget),
	m_type(Type::Sound) {
	ui->setupUi(this);
	init();

	ui->sliderVolume->setValue(sound.volume);
	ui->sliderTempo->setValue(sound.tempo);
	ui->sliderBalance->setValue(sound.balance);

	ui->groupFadeIn->hide();
}

PickerAudioWidget::~PickerAudioWidget() {
	delete ui;
}

void PickerAudioWidget::init() {
	m_player = new AudioPlayer(this);
	ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	ui->buttonStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
	ui->buttonStop->setEnabled(false);
	// the settings are heard at once
	connect(ui->sliderVolume, &QSlider::valueChanged, m_player, &AudioPlayer::setVolume);
	connect(ui->sliderTempo, &QSlider::valueChanged, m_player, &AudioPlayer::setTempo);
	connect(ui->sliderBalance, &QSlider::valueChanged, m_player, &AudioPlayer::setBalance);
	connect(m_player, &AudioPlayer::stateChanged, this, [this](AudioPlayer::State state) {
		ui->buttonStop->setEnabled(state != AudioPlayer::State::Stopped);
		if (state == AudioPlayer::State::Stopped && m_player->errorString().isEmpty()) {
			ui->labelStatus->clear();
		}
	});
	connect(m_player, &AudioPlayer::errorOccurred, ui->labelStatus, &QLabel::setText);
}

void PickerAudioWidget::fileChanged(const QString& filename) {
	m_filename = filename;
	// like RPG Maker: while listening, another file chosen is played
	if (m_player->state() == AudioPlayer::State::Playing) {
		on_buttonPlay_clicked();
	}
}

void PickerAudioWidget::on_buttonPlay_clicked() {
	if (m_filename.isEmpty() || !QFileInfo(m_filename).isFile()) {
		ui->labelStatus->setText(tr("Choose a file to play."));
		return;
	}
	m_player->setVolume(volume());
	m_player->setTempo(tempo());
	m_player->setBalance(balance());
	m_player->setFadeIn(m_type == Type::Music ? fadeInTime() : 0);
	m_player->setLooping(m_type == Type::Music);
	if (m_player->play(m_filename)) {
		ui->labelStatus->setText(tr("Playing %1").arg(QFileInfo(m_filename).fileName()));
	}
}

void PickerAudioWidget::on_buttonStop_clicked() {
	m_player->stop();
}

int PickerAudioWidget::fadeInTime() const {
	return ui->sliderFadeIn->value() * 1000;
}

int PickerAudioWidget::volume() const {
	return ui->sliderVolume->value();
}

int PickerAudioWidget::tempo() const {
	return ui->sliderTempo->value();
}

int PickerAudioWidget::balance() const {
	return ui->sliderBalance->value();
}

void PickerAudioWidget::on_sliderFadeIn_valueChanged(int value) {
	ui->labelFadeIn->setText(QString("%1s").arg(value));
}

void PickerAudioWidget::on_sliderVolume_valueChanged(int value) {
	ui->labelVolume->setText(QString("%1%").arg(value));
}

void PickerAudioWidget::on_sliderTempo_valueChanged(int value) {
	ui->labelTempo->setText(QString("%1%").arg(value));
}

void PickerAudioWidget::on_sliderBalance_valueChanged(int value) {
	ui->labelBalance->setText(QString("%1:%2").arg(100 - value).arg(value));
}
