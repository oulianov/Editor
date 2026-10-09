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

#include "jukebox_dialog.h"

#include <QCheckBox>
#include <QCollator>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QStyle>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <functional>
#include <map>

#include <lcf/rpg/database.h>

#include "common/audio_player.h"
#include "common/dbstring.h"
#include "common/filefinder.h"
#include "core.h"
#include "defines.h"
#include "model/project.h"

namespace {

const char GEOMETRY_KEY[] = "jukebox_geometry";
const char TAB_KEY[] = "jukebox_tab";

// the file of a line (empty: a group, or a file of the database not found)
constexpr int PathRole = Qt::UserRole;
// the database's fade-in, volume, tempo and balance of a "System" line
constexpr int SettingsRole = Qt::UserRole + 1;

enum Column { ColumnName, ColumnFormat, ColumnSource };

QString formatOf(const QString& path) {
	const QString suffix = QFileInfo(path).suffix().toUpper();
	return suffix == QLatin1String("MID") ? QStringLiteral("MIDI") : suffix;
}

/** The audio files of a folder, by lower case base name (what RPG Maker searches) */
std::map<QString, QString> audioFiles(const QString& dir, const QStringList& suffixes) {
	std::map<QString, QString> files;
	if (dir.isEmpty()) {
		return files;
	}
	for (const QFileInfo& info : QDir(dir).entryInfoList(QDir::Files | QDir::Readable)) {
		if (suffixes.contains('.' + info.suffix().toLower())) {
			// a.mid and a.wav: the first in RPG Maker's order wins, like Project::findFile
			const QString key = info.completeBaseName().toLower();
			auto found = files.find(key);
			if (found == files.end() || suffixes.indexOf('.' + info.suffix().toLower())
					< suffixes.indexOf('.' + QFileInfo(found->second).suffix().toLower())) {
				files[key] = info.absoluteFilePath();
			}
		}
	}
	return files;
}

} // namespace

JukeboxDialog::JukeboxDialog(QWidget* parent) : QDialog(parent) {
	setObjectName(QStringLiteral("JukeboxDialog"));
	setWindowTitle(tr("Jukebox"));
	setModal(false);

	m_player = new AudioPlayer(this);

	// Left: the files
	m_filter = new QLineEdit(this);
	m_filter->setObjectName(QStringLiteral("editFilter"));
	m_filter->setPlaceholderText(tr("Filter"));
	m_filter->setClearButtonEnabled(true);
	m_filter->installEventFilter(this);

	m_tabs = new QTabWidget(this);
	m_tabs->setObjectName(QStringLiteral("tabsJukebox"));
	m_listMusic = createList(QStringLiteral("listMusic"));
	m_listSound = createList(QStringLiteral("listSound"));
	m_tabs->addTab(m_listMusic, tr("Music"));
	m_tabs->addTab(m_listSound, tr("Sound Effects"));

	auto* left = new QVBoxLayout();
	left->addWidget(m_filter);
	left->addWidget(m_tabs, 1);

	// Right: playing and RPG Maker's settings
	auto* transport = new QHBoxLayout();
	auto makeButton = [this, transport](const QString& name, const QString& text, QStyle::StandardPixmap icon) {
		auto* button = new QPushButton(style()->standardIcon(icon), text, this);
		button->setObjectName(name);
		// Return plays the line selected: no button may take it
		button->setAutoDefault(false);
		transport->addWidget(button);
		return button;
	};
	m_play = makeButton(QStringLiteral("buttonPlay"), tr("Play"), QStyle::SP_MediaPlay);
	m_play->setToolTip(tr("Play the file selected (Return, or double-click it)"));
	m_pause = makeButton(QStringLiteral("buttonPause"), tr("Pause"), QStyle::SP_MediaPause);
	m_pause->setCheckable(true);
	m_stop = makeButton(QStringLiteral("buttonStop"), tr("Stop"), QStyle::SP_MediaStop);
	transport->addStretch(1);

	m_loop = new QCheckBox(tr("Loop"), this);
	m_loop->setObjectName(QStringLiteral("checkLoop"));
	m_loop->setToolTip(tr("Repeat at the end. MIDI music uses its loop point when set."));
	m_loop->setChecked(m_loopMusic);

	m_position = new QSlider(Qt::Horizontal, this);
	m_position->setObjectName(QStringLiteral("sliderPosition"));
	m_position->setEnabled(false);
	m_time = new QLabel(timeText(0), this);
	m_time->setObjectName(QStringLiteral("labelTime"));
	m_time->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	// does not jump while playing
	m_time->setMinimumWidth(m_time->fontMetrics().horizontalAdvance(QStringLiteral("00:00 / 00:00")));
	auto* seek = new QHBoxLayout();
	seek->addWidget(m_position, 1);
	seek->addWidget(m_time);

	m_status = new QLabel(this);
	m_status->setObjectName(QStringLiteral("labelStatus"));
	m_status->setWordWrap(true);
	m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

	// The four settings of RPG Maker's windows, with their scales
	auto* settings = new QGridLayout();
	auto addSetting = [this, settings](int row, int column, const QString& title, const QString& name,
			int minimum, int maximum, int value, const QStringList& scale, std::function<QString(int)> text) {
		auto* group = new QGroupBox(title, this);
		group->setObjectName("group" + name);
		auto* slider = new QSlider(Qt::Horizontal, group);
		slider->setObjectName("slider" + name);
		slider->setRange(minimum, maximum);
		slider->setValue(value);
		slider->setTickPosition(QSlider::TicksBelow);
		slider->setTickInterval((maximum - minimum) / 2);
		slider->setPageStep(std::max(1, (maximum - minimum) / 10));
		auto* label = new QLabel(text(value), group);
		label->setObjectName("label" + name);
		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		// the widest value, so that the slider does not move
		label->setMinimumWidth(std::max(label->fontMetrics().horizontalAdvance(text(minimum)),
			label->fontMetrics().horizontalAdvance(text(maximum))));
		connect(slider, &QSlider::valueChanged, label, [label, text](int v) { label->setText(text(v)); });

		auto* grid = new QGridLayout(group);
		grid->addWidget(slider, 0, 0, 1, 3);
		grid->addWidget(label, 0, 3);
		const Qt::Alignment alignments[] = { Qt::AlignLeft, Qt::AlignHCenter, Qt::AlignRight };
		for (int i = 0; i < scale.size() && i < 3; ++i) {
			auto* mark = new QLabel(scale[i], group);
			mark->setAlignment(alignments[i]);
			grid->addWidget(mark, 1, i);
		}
		grid->setColumnStretch(0, 1);
		grid->setColumnStretch(1, 1);
		grid->setColumnStretch(2, 1);
		settings->addWidget(group, row, column);
		return std::pair{group, slider};
	};
	auto percent = [](int v) { return tr("%1%").arg(v); };
	std::tie(m_groupFadeIn, m_fadeIn) = addSetting(0, 0, tr("Fade-in Time"), QStringLiteral("FadeIn"), 0, 10, 0,
		{ tr("None"), tr("5 s"), tr("10 s") }, [](int v) { return tr("%1 s").arg(v); });
	m_volume = addSetting(0, 1, tr("Volume"), QStringLiteral("Volume"), 0, 100, 100,
		{ tr("0%"), tr("50%"), tr("100%") }, percent).second;
	m_tempo = addSetting(1, 0, tr("Tempo"), QStringLiteral("Tempo"), 50, 150, 100,
		{ tr("50%"), tr("100%"), tr("150%") }, percent).second;
	m_balance = addSetting(1, 1, tr("Balance"), QStringLiteral("Balance"), 0, 100, 50,
		{ tr("Left"), tr("Center"), tr("Right") }, [](int v) { return QStringLiteral("%1:%2").arg(100 - v).arg(v); }).second;
	m_balance->setToolTip(tr("Left and right volume, like RPG Maker (50:50 is the center)"));

	auto* right = new QVBoxLayout();
	right->addLayout(transport);
	right->addWidget(m_loop);
	right->addLayout(seek);
	right->addWidget(m_status);
	right->addLayout(settings);
	right->addStretch(1);

	auto* columns = new QHBoxLayout();
	columns->addLayout(left, 1);
	columns->addLayout(right);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	auto* close = buttons->button(QDialogButtonBox::Close);
	close->setObjectName(QStringLiteral("buttonClose"));
	close->setAutoDefault(false);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto* layout = new QVBoxLayout(this);
	layout->addLayout(columns, 1);
	layout->addWidget(buttons);

	// Controls
	connect(m_play, &QPushButton::clicked, this, &JukeboxDialog::playSelected);
	connect(m_pause, &QPushButton::clicked, this, &JukeboxDialog::pauseOrResume);
	connect(m_stop, &QPushButton::clicked, this, &JukeboxDialog::stop);
	connect(m_loop, &QCheckBox::toggled, this, [this](bool on) {
		(currentKind() == Kind::Music ? m_loopMusic : m_loopSound) = on;
		// a tab shown does not change the file of the other tab playing
		if (m_playingKind == currentKind()) {
			m_player->setLooping(on);
		}
	});
	connect(m_fadeIn, &QSlider::valueChanged, this, [this](int v) {
		// for the next start: a fade-in is not heard in the middle of a file
		m_player->setFadeIn(currentKind() == Kind::Music ? v * 1000 : 0);
	});
	connect(m_volume, &QSlider::valueChanged, m_player, &AudioPlayer::setVolume);
	connect(m_tempo, &QSlider::valueChanged, m_player, &AudioPlayer::setTempo);
	connect(m_balance, &QSlider::valueChanged, m_player, &AudioPlayer::setBalance);
	connect(m_position, &QSlider::valueChanged, this, [this](int v) {
		if (!m_updatingPosition && !m_position->isSliderDown()) {
			m_player->setPosition(v);
		}
	});
	connect(m_position, &QSlider::sliderMoved, this, [this](int v) {
		m_time->setText(timeText(v));
	});
	connect(m_position, &QSlider::sliderReleased, this, [this] {
		m_player->setPosition(m_position->value());
	});
	connect(m_filter, &QLineEdit::textChanged, this, &JukeboxDialog::applyFilter);
	connect(m_tabs, &QTabWidget::currentChanged, this, &JukeboxDialog::tabChanged);
	auto* find = new QShortcut(QKeySequence::Find, this);
	connect(find, &QShortcut::activated, this, [this] {
		m_filter->setFocus(Qt::ShortcutFocusReason);
		m_filter->selectAll();
	});

	connect(m_player, &AudioPlayer::stateChanged, this, [this](AudioPlayer::State state) {
		switch (state) {
			case AudioPlayer::State::Playing:
				showStatus(tr("Playing %1").arg(m_playingName));
				break;
			case AudioPlayer::State::Paused:
				showStatus(tr("Paused %1").arg(m_playingName));
				break;
			case AudioPlayer::State::Stopped:
				// also at the end of a file played once; an error follows when it failed
				showStatus(tr("Stopped"));
				break;
		}
		updateButtons();
	});
	connect(m_player, &AudioPlayer::positionChanged, this, &JukeboxDialog::updatePosition);
	connect(m_player, &AudioPlayer::durationChanged, this, &JukeboxDialog::updateDuration);
	connect(m_player, &AudioPlayer::errorOccurred, this, [this](const QString& message) {
		showStatus(message, true);
	});
	m_player->setVolume(m_volume->value());
	m_player->setTempo(m_tempo->value());
	m_player->setBalance(m_balance->value());
	m_player->setLooping(m_loop->isChecked());

	// Open as last time
	QSettings stored;
	if (!restoreGeometry(stored.value(GEOMETRY_KEY).toByteArray())) {
		resize(820, 540);
	}
	if (const QScreen* screen = parent ? parent->screen() : this->screen()) {
		resize(size().boundedTo(screen->availableGeometry().size() * 0.95));
	}
	m_tabs->setCurrentIndex(std::clamp(stored.value(TAB_KEY, 0).toInt(), 0, m_tabs->count() - 1));
	tabChanged();
	updateButtons();
}

JukeboxDialog::~JukeboxDialog() = default;

QTreeWidget* JukeboxDialog::createList(const QString& name) {
	auto* list = new QTreeWidget(this);
	list->setObjectName(name);
	list->setColumnCount(3);
	list->setHeaderLabels({ tr("Name"), tr("Format"), tr("Source") });
	list->header()->setStretchLastSection(false);
	list->header()->setSectionResizeMode(ColumnName, QHeaderView::Stretch);
	list->header()->setSectionResizeMode(ColumnFormat, QHeaderView::ResizeToContents);
	list->header()->setSectionResizeMode(ColumnSource, QHeaderView::ResizeToContents);
	list->setUniformRowHeights(true);
	list->setAllColumnsShowFocus(true);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	list->installEventFilter(this);
	connect(list, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
		itemSelected(item);
	});
	connect(list, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
		playItem(item);
	});
	return list;
}

void JukeboxDialog::refresh() {
	fillList(Kind::Music);
	fillList(Kind::Sound);
	applyFilter();
	updateButtons();
}

void JukeboxDialog::fillList(Kind kind) {
	QTreeWidget* list = kind == Kind::Music ? m_listMusic : m_listSound;
	const QString selected = list->currentItem() ? list->currentItem()->data(ColumnName, PathRole).toString() : QString();
	list->clear();

	const auto& project = core().project();
	if (!project) {
		return;
	}
	const QString folder = kind == Kind::Music ? QStringLiteral(MUSIC) : QStringLiteral(SOUND);
	const auto type = kind == Kind::Music ? FileFinder::FileType::Music : FileFinder::FileType::Sound;
	const QStringList suffixes = FileFinder::GetFiltersForType(type);

	// The game's files, then the RTP's not replaced by one of the game (the game uses its own)
	const QString gameDir = project->findDirectory(folder);
	const QString rtp = core().rtpPath(QString());
	const QString rtpDir = rtp.isEmpty() ? QString()
		: FileFinder::Find(QDir(rtp), folder, FileFinder::FileType::Default, QDir::Dirs);
	auto files = audioFiles(gameDir, suffixes);
	const size_t gameCount = files.size();
	std::map<QString, bool> fromRtp;
	for (const auto& [key, path] : audioFiles(rtpDir, suffixes)) {
		if (files.emplace(key, path).second) {
			fromRtp[key] = true;
		}
	}

	const QColor dimmed = palette().color(QPalette::PlaceholderText);
	auto addLine = [&](QTreeWidgetItem* group, const QString& name, const QString& path, bool rtpFile) {
		auto* item = new QTreeWidgetItem(group);
		item->setText(ColumnName, name);
		item->setData(ColumnName, PathRole, path);
		item->setText(ColumnFormat, path.isEmpty() ? QString() : formatOf(path));
		item->setText(ColumnSource, path.isEmpty() ? tr("Not found") : rtpFile ? tr("RTP") : tr("Game"));
		if (rtpFile || path.isEmpty()) {
			item->setForeground(ColumnSource, dimmed);
		}
		item->setToolTip(ColumnName, path.isEmpty() ? tr("No file of this name in the game or in the RTP") : QDir::toNativeSeparators(path));
		return item;
	};
	auto addGroup = [&](const QString& title) {
		auto* group = new QTreeWidgetItem(list);
		group->setText(ColumnName, title);
		group->setFlags(Qt::ItemIsEnabled);
		group->setFirstColumnSpanned(true);
		QFont font = group->font(ColumnName);
		font.setBold(true);
		group->setFont(ColumnName, font);
		return group;
	};

	auto* filesGroup = addGroup(tr("Files (%1 in the game, %2 in the RTP)").arg(gameCount).arg(files.size() - gameCount));
	std::vector<std::pair<QString, QString>> sorted;
	for (const auto& [key, path] : files) {
		sorted.emplace_back(QFileInfo(path).completeBaseName(), path);
	}
	QCollator collator;
	collator.setNumericMode(true);
	collator.setCaseSensitivity(Qt::CaseInsensitive);
	std::sort(sorted.begin(), sorted.end(), [&](const auto& a, const auto& b) {
		return collator.compare(a.first, b.first) < 0;
	});
	for (const auto& [name, path] : sorted) {
		addLine(filesGroup, name, path, fromRtp.count(name.toLower()) > 0);
	}

	// The music and sound effects of the System tab of the database, with their settings
	auto* systemGroup = addGroup(tr("System (database)"));
	const auto& sys = project->database().system;
	auto addSystem = [&](const QString& label, const std::string& file, QList<int> values) {
		const QString name = ToQString(file);
		if (name.isEmpty() || name == QLatin1String("(OFF)")) {
			return;
		}
		const QString path = project->findFile(folder, name, type);
		const bool rtpFile = !path.isEmpty() && QFileInfo(path).absolutePath() != QFileInfo(gameDir).absoluteFilePath();
		auto* item = addLine(systemGroup, QStringLiteral("%1: %2").arg(label, name), path, rtpFile);
		item->setData(ColumnName, SettingsRole, QVariant::fromValue(values));
	};
	if (kind == Kind::Music) {
		for (auto [label, music] : std::initializer_list<std::pair<QString, const lcf::rpg::Music*>>{
				{ tr("Title Screen"), &sys.title_music },
				{ tr("Battle"), &sys.battle_music },
				{ tr("Victory"), &sys.battle_end_music },
				{ tr("Inn"), &sys.inn_music },
				{ tr("Skiff"), &sys.boat_music },
				{ tr("Ship"), &sys.ship_music },
				{ tr("Airship"), &sys.airship_music },
				{ tr("Game Over Screen"), &sys.gameover_music } }) {
			addSystem(label, music->name, { music->fadein, music->volume, music->tempo, music->balance });
		}
	} else {
		for (auto [label, sound] : std::initializer_list<std::pair<QString, const lcf::rpg::Sound*>>{
				{ tr("Cursor"), &sys.cursor_se },
				{ tr("Select Choice"), &sys.decision_se },
				{ tr("Cancel"), &sys.cancel_se },
				{ tr("Buzzer"), &sys.buzzer_se },
				{ tr("Battle Transition"), &sys.battle_se },
				{ tr("Flee Battle"), &sys.escape_se },
				{ tr("Enemy Attacks"), &sys.enemy_attack_se },
				{ tr("Enemy Damaged"), &sys.enemy_damaged_se },
				{ tr("Ally Damaged"), &sys.actor_damaged_se },
				{ tr("Evade Attack"), &sys.dodge_se },
				{ tr("Enemy Defeated"), &sys.enemy_death_se },
				{ tr("Item Used"), &sys.item_se } }) {
			addSystem(label, sound->name, { 0, sound->volume, sound->tempo, sound->balance });
		}
	}
	if (systemGroup->childCount() == 0) {
		delete systemGroup;
	}

	list->expandAll();
	// the same line as before, else the first file
	QTreeWidgetItem* current = nullptr;
	for (QTreeWidgetItemIterator it(list); *it && !selected.isEmpty(); ++it) {
		if ((*it)->data(ColumnName, PathRole).toString() == selected) {
			current = *it;
			break;
		}
	}
	if (!current && filesGroup->childCount() > 0) {
		current = filesGroup->child(0);
	}
	if (current) {
		list->setCurrentItem(current);
	}
}

void JukeboxDialog::applyFilter() {
	const QString text = m_filter->text().trimmed();
	for (QTreeWidget* list : { m_listMusic, m_listSound }) {
		for (int g = 0; g < list->topLevelItemCount(); ++g) {
			QTreeWidgetItem* group = list->topLevelItem(g);
			int shown = 0;
			for (int i = 0; i < group->childCount(); ++i) {
				QTreeWidgetItem* item = group->child(i);
				const bool match = text.isEmpty() || item->text(ColumnName).contains(text, Qt::CaseInsensitive);
				item->setHidden(!match);
				shown += match ? 1 : 0;
			}
			group->setHidden(shown == 0 && !text.isEmpty());
		}
		// the selection follows the filter: the first line shown
		QTreeWidgetItem* current = list->currentItem();
		if (!current || current->isHidden() || !current->parent() || current->parent()->isHidden()) {
			QTreeWidgetItem* first = nullptr;
			for (QTreeWidgetItemIterator it(list, QTreeWidgetItemIterator::NotHidden); *it && !first; ++it) {
				if ((*it)->parent()) {
					first = *it;
				}
			}
			if (first) {
				list->setCurrentItem(first);
			} else {
				list->setCurrentItem(nullptr);
			}
		}
	}
	updateButtons();
}

QTreeWidget* JukeboxDialog::currentList() const {
	return currentKind() == Kind::Music ? m_listMusic : m_listSound;
}

JukeboxDialog::Kind JukeboxDialog::currentKind() const {
	return m_tabs->currentIndex() == 1 ? Kind::Sound : Kind::Music;
}

void JukeboxDialog::tabChanged() {
	const bool music = currentKind() == Kind::Music;
	// RPG Maker's sound effects have no fade-in
	m_groupFadeIn->setEnabled(music);
	m_loop->setChecked(music ? m_loopMusic : m_loopSound);
	m_player->setFadeIn(music ? m_fadeIn->value() * 1000 : 0);
	QSettings().setValue(TAB_KEY, m_tabs->currentIndex());
	updateButtons();
}

void JukeboxDialog::itemSelected(QTreeWidgetItem* item) {
	if (item) {
		// a file of the database: its settings, like the database plays it
		const QList<int> values = item->data(ColumnName, SettingsRole).value<QList<int>>();
		if (values.size() == 4) {
			m_fadeIn->setValue(values[0] / 1000);
			m_volume->setValue(values[1]);
			m_tempo->setValue(values[2]);
			m_balance->setValue(values[3]);
		}
	}
	updateButtons();
}

void JukeboxDialog::playItem(QTreeWidgetItem* item) {
	if (!item || !item->parent()) {
		return;
	}
	const QString path = item->data(ColumnName, PathRole).toString();
	if (path.isEmpty()) {
		showStatus(tr("%1: no file of this name in the game or in the RTP.").arg(item->text(ColumnName)), true);
		return;
	}
	const bool music = item->treeWidget() == m_listMusic;
	m_playingKind = music ? Kind::Music : Kind::Sound;
	m_player->setFadeIn(music ? m_fadeIn->value() * 1000 : 0);
	m_player->setLooping(m_loop->isChecked());
	m_playingName = QStringLiteral("%1 (%2)").arg(QFileInfo(path).fileName(), item->text(ColumnSource));
	// a file again from the start, also when it is the one playing
	m_player->play(path);
}

void JukeboxDialog::playSelected() {
	QTreeWidgetItem* item = currentList()->currentItem();
	if (item && item->parent()) {
		playItem(item);
	} else if (m_player->state() == AudioPlayer::State::Paused) {
		m_player->play();
	}
}

void JukeboxDialog::pauseOrResume() {
	if (m_player->state() == AudioPlayer::State::Playing) {
		m_player->pause();
	} else if (m_player->state() == AudioPlayer::State::Paused) {
		m_player->play();
	}
	// a click while stopped must not leave the button checked
	updateButtons();
}

void JukeboxDialog::stop() {
	m_player->stop();
}

void JukeboxDialog::updateButtons() {
	const QTreeWidgetItem* item = currentList()->currentItem();
	const auto state = m_player->state();
	m_play->setEnabled((item && item->parent()) || state == AudioPlayer::State::Paused);
	m_pause->setEnabled(state != AudioPlayer::State::Stopped);
	m_pause->setChecked(state == AudioPlayer::State::Paused);
	m_stop->setEnabled(state != AudioPlayer::State::Stopped);
	m_position->setEnabled(state != AudioPlayer::State::Stopped && m_player->duration() > 0);
}

void JukeboxDialog::updatePosition(qint64 ms) {
	if (m_position->isSliderDown()) {
		return;
	}
	m_updatingPosition = true;
	m_position->setValue(static_cast<int>(ms));
	m_updatingPosition = false;
	const qint64 length = m_player->duration();
	m_time->setText(length > 0 ? QStringLiteral("%1 / %2").arg(timeText(ms), timeText(length)) : timeText(ms));
}

void JukeboxDialog::updateDuration(qint64 ms) {
	m_updatingPosition = true;
	m_position->setRange(0, static_cast<int>(ms));
	m_position->setPageStep(10000);
	m_position->setSingleStep(1000);
	m_updatingPosition = false;
	updateButtons();
	updatePosition(m_player->position());
}

void JukeboxDialog::showStatus(const QString& text, bool error) {
	m_status->setText(text);
	QFont font = m_status->font();
	font.setBold(error);
	m_status->setFont(font);
}

QString JukeboxDialog::timeText(qint64 ms) const {
	const qint64 seconds = ms / 1000;
	return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void JukeboxDialog::showEvent(QShowEvent* event) {
	// files added since: listed again each time it opens (not when restored from the Dock)
	if (!event->spontaneous()) {
		refresh();
	}
	QDialog::showEvent(event);
}

void JukeboxDialog::hideEvent(QHideEvent* event) {
	// closed, not minimized: silence
	if (!event->spontaneous()) {
		m_player->stop();
		QSettings().setValue(GEOMETRY_KEY, saveGeometry());
	}
	QDialog::hideEvent(event);
}

bool JukeboxDialog::eventFilter(QObject* watched, QEvent* event) {
	if (event->type() != QEvent::KeyPress) {
		return QDialog::eventFilter(watched, event);
	}
	auto* key = static_cast<QKeyEvent*>(event);
	const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
	if (watched == m_listMusic || watched == m_listSound) {
		if (enter) {
			// not the dialog's default button, and macOS lists do not activate on Return
			playSelected();
			return true;
		}
	} else if (watched == m_filter && (enter || key->key() == Qt::Key_Down)) {
		// from the filter to the lines it shows
		QTreeWidget* list = currentList();
		list->setFocus(Qt::OtherFocusReason);
		if (!list->currentItem() || !list->currentItem()->parent()) {
			for (QTreeWidgetItemIterator it(list, QTreeWidgetItemIterator::NotHidden); *it; ++it) {
				if ((*it)->parent()) {
					list->setCurrentItem(*it);
					break;
				}
			}
		}
		if (enter) {
			playSelected();
		}
		return true;
	}
	return QDialog::eventFilter(watched, event);
}
