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

#include "project_settings_dialog.h"
#include "common/exfont.h"
#include "common/game_font.h"
#include "common/ini_file.h"
#include "common/rpg_rt_exe.h"
#include "common/script_link.h"
#include "model/game_patches.h"
#include "model/project.h"
#include "model/project_convert.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

/** The code pages of RPG Maker games, as EasyRPG Player reads them */
const std::vector<std::pair<int, const char*>>& codePages() {
	static const std::vector<std::pair<int, const char*>> pages = {
		{1252, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Western European (English, French, German...)")},
		{1250, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Central European (Polish, Czech...)")},
		{1251, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Cyrillic (Russian...)")},
		{1253, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Greek")},
		{1254, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Turkish")},
		{1255, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Hebrew")},
		{1256, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Arabic")},
		{1257, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Baltic")},
		{1258, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Vietnamese")},
		{874, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Thai")},
		{932, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Japanese (Shift-JIS)")},
		{936, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Simplified Chinese (GBK)")},
		{949, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Korean")},
		{950, QT_TRANSLATE_NOOP("ProjectSettingsDialog", "Traditional Chinese (Big5)")},
	};
	return pages;
}

/** The code page of an encoding ("1252", "windows-1252", "ibm-943_P15A-2003"), else 0 */
int codePageOf(const QString& encoding) {
	bool ok = false;
	const int number = encoding.trimmed().toInt(&ok);
	if (ok) {
		return number;
	}
	// liblcf's names (ReaderUtil::CodepageToEncoding, DetectEncodings)
	if (encoding.startsWith(QLatin1String("ibm-943"))) {
		return 932;
	}
	if (encoding.startsWith(QLatin1String("ibm-5348"))) {
		return 1252;
	}
	static const QRegularExpression windows(QStringLiteral("^windows-(\\d+)"));
	const auto match = windows.match(encoding);
	return match.hasMatch() ? match.captured(1).toInt() : 0;
}

QString describe(const QString& encoding) {
	const int page = codePageOf(encoding);
	for (const auto& [number, name] : codePages()) {
		if (number == page) {
			return QStringLiteral("%1 - %2").arg(number).arg(ProjectSettingsDialog::tr(name));
		}
	}
	return encoding;
}

/** The extensions of the fonts EasyRPG Player reads (FileFinder FONTS_TYPES) */
const QStringList& fontTypes() {
	static const QStringList types = {QStringLiteral("fon"), QStringLiteral("fnt"), QStringLiteral("bdf"),
		QStringLiteral("ttf"), QStringLiteral("ttc"), QStringLiteral("otf"), QStringLiteral("woff2"), QStringLiteral("woff")};
	return types;
}

/** Font/Font.ttf (font 1) and Font/Font2.ttf (font 2) of the game, as the Player finds them */
QStringList fontFiles(const QDir& game, int font) {
	QDir dir = game;
	QStringList found;
	const QString base = font == 0 ? QStringLiteral("font") : QStringLiteral("font2");
	for (const QString& sub : game.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (sub.compare(QLatin1String("Font"), Qt::CaseInsensitive) != 0) {
			continue;
		}
		dir.cd(sub);
		for (const QFileInfo& file : dir.entryInfoList(QDir::Files)) {
			if (file.completeBaseName().compare(base, Qt::CaseInsensitive) == 0
					&& fontTypes().contains(file.suffix().toLower())) {
				found << file.absoluteFilePath();
			}
		}
		break;
	}
	return found;
}

/** Wrapped text as tall as it is at its width: layouts, those of tabs too, ask
 *  for the height of the width they gave before */
class WrapLabel : public QLabel {
public:
	using QLabel::QLabel;
	QSize minimumSizeHint() const override {
		QSize size = QLabel::minimumSizeHint();
		if (m_placed) {
			size.setHeight(heightForWidth(width()));
		}
		return size;
	}
	QSize sizeHint() const override {
		QSize size = QLabel::sizeHint();
		if (m_placed) {
			size.setHeight(heightForWidth(width()));
		}
		return size;
	}

protected:
	void showEvent(QShowEvent* event) override {
		QLabel::showEvent(event);
		// on a page of a tab shown at last: its width is the one laid out
		m_placed = true;
		updateGeometry();
	}
	void resizeEvent(QResizeEvent* event) override {
		QLabel::resizeEvent(event);
		// its width given by the layout (not the one of a new widget)
		if (event->size().width() != event->oldSize().width() && isVisible()) {
			m_placed = true;
			updateGeometry();
		}
	}

private:
	bool m_placed = false;
};

/** Wrapped text as tall as the longest of the texts it shows, so that it does not jump */
class LongestLabel : public QLabel {
public:
	LongestLabel(const QStringList& texts, QWidget* parent) : QLabel(parent), m_texts(texts) {
		m_measure.setWordWrap(true);
		m_measure.setTextFormat(Qt::RichText);
	}
	int heightForWidth(int width) const override {
		const int frame = 2 * (margin() + frameWidth());
		m_measure.setFont(font());
		int height = QLabel::heightForWidth(width);
		for (const QString& text : m_texts) {
			m_measure.setText(text);
			height = std::max(height, m_measure.heightForWidth(width - frame) + frame);
		}
		return height;
	}

private:
	QStringList m_texts;
	mutable QLabel m_measure;
};

QLabel* infoLabel(QWidget* parent, const QString& name) {
	auto* label = new WrapLabel(parent);
	label->setObjectName(name);
	label->setWordWrap(true);
	label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	label->setTextInteractionFlags(Qt::TextSelectableByMouse);
	return label;
}

QLabel* noteLabel(const QString& text, QWidget* parent) {
	auto* note = new WrapLabel(text, parent);
	note->setWordWrap(true);
	note->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	note->setForegroundRole(QPalette::PlaceholderText);
	return note;
}

} // namespace

ProjectSettingsDialog::ProjectSettingsDialog(Project& project, bool open, QWidget* parent) :
	QDialog(parent), m_project(project), m_open(open), m_folder(project.projectDir()), m_initialTitle(project.gameTitle())
{
	setObjectName(QStringLiteral("ProjectSettingsDialog"));
	setWindowTitle(tr("Project Settings"));
	auto* layout = new QVBoxLayout(this);
	auto* tabs = new QTabWidget(this);
	tabs->setObjectName(QStringLiteral("tabs"));
	tabs->addTab(generalPage(project, open), tr("General"));
	tabs->addTab(enginePage(project, open), tr("Engine"));
	tabs->addTab(playerPage(), tr("EasyRPG Player"));
	layout->addWidget(tabs);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	// what the Editor does not show: edited by hand, read again on coming back
	auto* ini = buttons->addButton(tr("Open EasyRPG.ini"), QDialogButtonBox::ActionRole);
	ini->setObjectName(QStringLiteral("pushOpenIni"));
	ini->setToolTip(tr("For the settings not shown here: opens it in the script editor (created if missing)"));
	connect(ini, &QPushButton::clicked, this, &ProjectSettingsDialog::openIni);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);
	// the texts as wide as the window (macOS keeps fields at their size)
	for (auto* form : findChildren<QFormLayout*>()) {
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	}
	setMinimumWidth(600);
	readIni();
}

QWidget* ProjectSettingsDialog::generalPage(Project& project, bool open)
{
	auto* page = new QWidget(this);
	auto* layout = new QVBoxLayout(page);

	// RPG Maker's Rename Project
	auto* names = new QGroupBox(tr("Project"), page);
	auto* form = new QFormLayout(names);
	m_folderName = new QLineEdit(m_folder.dirName(), names);
	m_folderName->setObjectName(QStringLiteral("lineFolderName"));
	form->addRow(tr("Directory Name"), m_folderName);
	m_title = new QLineEdit(project.gameTitle(), names);
	m_title->setObjectName(QStringLiteral("lineGameTitle"));
	form->addRow(tr("Game Name"), m_title);
	layout->addWidget(names);

	// the code page of the texts: [EasyRPG] Encoding of RPG_RT.ini
	auto* text = new QGroupBox(tr("Text"), page);
	auto* text_form = new QFormLayout(text);
	if (project.projectType() == FileFinder::ProjectType::EasyRpg) {
		text_form->addRow(tr("Encoding"), new QLabel(QStringLiteral("UTF-8"), text));
	} else {
		m_encodingSetting = project.encodingSetting();
		m_encoding = new QComboBox(text);
		m_encoding->setObjectName(QStringLiteral("comboEncoding"));
		// the long names in its list only
		m_encoding->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		m_encoding->setMinimumContentsLength(28);
		// detected: what the Editor found reading the game
		const QString found = m_encodingSetting.isEmpty() ? project.encoding() : QString();
		m_encoding->addItem(found.isEmpty() ? tr("Detect automatically")
			: tr("Detect automatically (%1)").arg(describe(found)), QString());
		for (const auto& [number, name] : codePages()) {
			m_encoding->addItem(QStringLiteral("%1 - %2").arg(number).arg(tr(name)), QString::number(number));
		}
		int current = 0;
		if (!m_encodingSetting.isEmpty()) {
			current = m_encoding->findData(QString::number(codePageOf(m_encodingSetting)));
			if (current < 0) {
				// as written, though the Player may not read it
				m_encoding->addItem(m_encodingSetting, m_encodingSetting);
				current = m_encoding->count() - 1;
			}
		}
		m_encoding->setCurrentIndex(current);
		m_encoding->setToolTip(tr("The code page the texts of the game are written in. "
			"A wrong one shows them garbled: the game is read again with the one chosen."));
		text_form->addRow(tr("Encoding"), m_encoding);
		if (open) {
			text_form->addRow(noteLabel(tr("Changing it closes the project (asking to save it) and opens it again."), text));
		}
	}
	layout->addWidget(text);
	layout->addStretch(1);
	return page;
}

QWidget* ProjectSettingsDialog::enginePage(Project& project, bool open)
{
	auto* page = new QWidget(this);
	auto* layout = new QVBoxLayout(page);

	// what runs the game: the format of the project, RPG_RT.exe, and how EasyRPG Player sees it
	auto* engine = new QGroupBox(tr("Engine"), page);
	auto* engine_form = new QFormLayout(engine);
	if (open) {
		auto* row = new QHBoxLayout();
		const bool easyrpg = project.projectType() == FileFinder::ProjectType::EasyRpg;
		const QString name = easyrpg ? tr("EasyRPG (XML files)")
			: project.database().system.ldb_id == 2003 ? tr("RPG Maker 2003") : tr("RPG Maker 2000");
		auto* label = new QLabel(name, engine);
		label->setObjectName(QStringLiteral("labelEngine"));
		row->addWidget(label, 1);
		if (ProjectConvert::canConvert(project)) {
			auto* convert = new QPushButton(tr("Convert to RPG Maker 2003..."), engine);
			convert->setObjectName(QStringLiteral("pushConvert"));
			convert->setToolTip(tr("Writes the settings of this window, then converts the game (a backup is made first)"));
			connect(convert, &QPushButton::clicked, this, [this] {
				m_convert = true;
				accept();
			});
			row->addWidget(convert);
		}
		engine_form->addRow(tr("Project"), row);
	}
	m_runtime = infoLabel(engine, QStringLiteral("labelRuntime"));
	engine_form->addRow(tr("Player"), m_runtime);
	layout->addWidget(engine);

	// the font of the database, and those RPG_RT.exe draws them with
	auto* fonts = new QGroupBox(tr("Fonts"), page);
	auto* fonts_form = new QFormLayout(fonts);
	if (open) {
		m_textFont = new QComboBox(fonts);
		m_textFont->setObjectName(QStringLiteral("comboTextFont"));
		m_textFont->addItem(tr("Font 1 (MS Gothic)"), lcf::rpg::System::Font_gothic);
		m_textFont->addItem(tr("Font 2 (MS Mincho)"), lcf::rpg::System::Font_mincho);
		m_textFont->setCurrentIndex(m_textFont->findData(project.database().system.font_id));
		m_textFont->setToolTip(tr("The font of the game (Database > System), saved in the database"));
		fonts_form->addRow(tr("Text font"), m_textFont);
		m_fontPreview = new QLabel(fonts);
		m_fontPreview->setObjectName(QStringLiteral("labelFontPreview"));
		m_fontPreview->setToolTip(tr("As EasyRPG Player draws it"));
		m_fontPreview->setAlignment(Qt::AlignCenter);
		fonts_form->addRow(m_fontPreview);
		connect(m_textFont, &QComboBox::currentIndexChanged, this, &ProjectSettingsDialog::showFontPreview);
	}
	m_exeFonts = infoLabel(fonts, QStringLiteral("labelExeFonts"));
	m_exeFonts->setToolTip(tr("Fonts of Windows: the ones of font 1 and font 2"));
	fonts_form->addRow(tr("RPG_RT.exe fonts"), m_exeFonts);
	// the glyphs as messages show them: $A to $Z, then $a to $z, crisp at twice the pixels
	const ExFont::Sheet sheet = ExFont::load(project);
	const int columns = ExFont::Count / 2;
	QImage preview(columns * ExFont::GlyphSize * 2, 2 * ExFont::GlyphSize * 2, QImage::Format_ARGB32_Premultiplied);
	preview.fill(Qt::transparent);
	{
		QPainter painter(&preview);
		for (int index = 0; index < ExFont::Count; ++index) {
			const QImage glyph = ExFont::glyph(sheet.image, index, palette().color(QPalette::WindowText));
			painter.drawImage(QRect((index % columns) * ExFont::GlyphSize * 2, (index / columns) * ExFont::GlyphSize * 2,
				ExFont::GlyphSize * 2, ExFont::GlyphSize * 2), glyph);
		}
	}
	QPixmap pixmap = QPixmap::fromImage(preview);
	pixmap.setDevicePixelRatio(2);
	auto* glyphs_row = new QHBoxLayout();
	auto* glyphs = new QLabel(fonts);
	glyphs->setObjectName(QStringLiteral("labelExFont"));
	glyphs->setPixmap(pixmap);
	glyphs->setToolTip(tr("$A to $Z, then $a to $z"));
	glyphs_row->addWidget(glyphs);
	auto* source = noteLabel(sheet.source == QLatin1String("EasyRPG") ? tr("EasyRPG Player's") : sheet.source, fonts);
	source->setObjectName(QStringLiteral("labelExFontSource"));
	glyphs_row->addWidget(source, 1);
	fonts_form->addRow(tr("Symbols"), glyphs_row);
	layout->addWidget(fonts);
	layout->addStretch(1);
	return page;
}

QWidget* ProjectSettingsDialog::playerPage()
{
	auto* page = new QWidget(this);
	auto* layout = new QVBoxLayout(page);
	// settings, not patches applied
	auto* about = noteLabel(tr("Read by EasyRPG Player only (EasyRPG.ini, Font folder). RPG_RT.exe is not changed."), page);
	about->setObjectName(QStringLiteral("labelPlayerAbout"));
	layout->addWidget(about);

	auto* patches = new QGroupBox(tr("Patches"), page);
	auto* patches_layout = new QVBoxLayout(patches);
	const GamePatches::State state = GamePatches::read(m_project);
	m_patchesWereChosen = state.chosen;
	m_detected = GamePatches::detect(m_project);
	m_written = state.chosen ? state.values : m_detected;
	m_chosen = m_written;
	m_patchMode = new QComboBox(patches);
	m_patchMode->setObjectName(QStringLiteral("comboPatchMode"));
	m_patchMode->addItem(tr("Detect from the game files"), Detect);
	m_patchMode->addItem(tr("Choose"), Choose);
	m_patchMode->addItem(tr("As written in EasyRPG.ini"), Refer);
	m_patchMode->setToolTip(tr("Detect: as EasyRPG Player does when EasyRPG.ini sets none. "
		"Choose: the patches checked are written in EasyRPG.ini. "
		"As written: the Editor leaves EasyRPG.ini as it is."));
	auto* mode_row = new QFormLayout();
	mode_row->addRow(tr("Patches"), m_patchMode);
	patches_layout->addLayout(mode_row);
	auto* grid = new QGridLayout();
	int i = 0;
	for (const auto& patch : GamePatches::list()) {
		const QString key = QString::fromLatin1(patch.key);
		auto* box = new QCheckBox(QCoreApplication::translate("GamePatches", patch.name), patches);
		box->setObjectName(QStringLiteral("patch") + key);
		box->setProperty("patch", key);
		box->installEventFilter(this);
		connect(box, &QCheckBox::toggled, this, [this, key](bool on) {
			if (patchMode() == Choose) {
				// the Maniac Patch keeps its version
				const int was = m_chosen.value(key);
				m_chosen[key] = on ? std::max(was, 1) : 0;
			}
			explainPatch(key);
		});
		m_patchBoxes[key] = box;
		grid->addWidget(box, i / 2, i % 2);
		++i;
	}
	patches_layout->addLayout(grid);
	m_otherPatches = noteLabel(QString(), patches);
	m_otherPatches->setObjectName(QStringLiteral("labelOtherPatches"));
	patches_layout->addWidget(m_otherPatches);

	// what the patch pointed at changes
	QStringList explanations;
	for (const auto& patch : GamePatches::list()) {
		explanations << GamePatches::explain(patch);
	}
	m_patchHelp = new LongestLabel(explanations, patches);
	m_patchHelp->setObjectName(QStringLiteral("labelPatchHelp"));
	m_patchHelp->setWordWrap(true);
	m_patchHelp->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_patchHelp->setFrameShape(QFrame::StyledPanel);
	m_patchHelp->setMargin(6);
	patches_layout->addWidget(m_patchHelp);
	explainPatch(QString());
	// greyed boxes get no mouse events: the group tells which one is pointed at
	patches->setMouseTracking(true);
	patches->installEventFilter(this);
	m_patchesPage = patches;
	layout->addWidget(patches);

	m_patchMode->setCurrentIndex(!state.chosen ? Detect : state.others.isEmpty() ? Choose : Refer);
	connect(m_patchMode, &QComboBox::currentIndexChanged, this, &ProjectSettingsDialog::showPatches);
	showPatches();

	// the fonts the Player draws with, in the Font folder of the game
	auto* fonts = new QGroupBox(tr("Fonts"), page);
	auto* fonts_form = new QFormLayout(fonts);
	for (int font = 0; font < 2; ++font) {
		auto* row = new QHBoxLayout();
		m_playerFonts[font] = infoLabel(fonts, QStringLiteral("labelPlayerFont%1").arg(font + 1));
		m_playerFonts[font]->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		m_playerFonts[font]->setToolTip(tr("Font/%1 of the game (.ttf, .otf, .fon...), else a font of the Player")
			.arg(font == 0 ? QStringLiteral("Font") : QStringLiteral("Font2")));
		row->addWidget(m_playerFonts[font], 1);
		auto* choose = new QPushButton(tr("Choose..."), fonts);
		choose->setObjectName(QStringLiteral("pushPlayerFont%1").arg(font + 1));
		choose->setToolTip(tr("Copied in the Font folder of the game on OK"));
		connect(choose, &QPushButton::clicked, this, [this, font] {
			QStringList patterns;
			for (const QString& type : fontTypes()) {
				patterns << QStringLiteral("*.") + type;
			}
			const QString file = QFileDialog::getOpenFileName(this, tr("Font %1 for EasyRPG Player").arg(font + 1),
				QString(), tr("Fonts (%1)").arg(patterns.join(QLatin1Char(' '))));
			if (!file.isEmpty()) {
				m_fontChoice[font] = file;
				showFonts();
			}
		});
		row->addWidget(choose);
		auto* builtin = new QPushButton(tr("Built-in"), fonts);
		builtin->setObjectName(QStringLiteral("pushPlayerFontBuiltin%1").arg(font + 1));
		builtin->setToolTip(tr("The Player's own font; the game's font file goes to the trash on OK"));
		connect(builtin, &QPushButton::clicked, this, [this, font] {
			m_fontChoice[font] = QString();
			showFonts();
		});
		row->addWidget(builtin);
		fonts_form->addRow(tr("Font %1").arg(font + 1), row);
	}
	layout->addWidget(fonts);
	layout->addStretch(1);
	showFonts();
	return page;
}

ProjectSettingsDialog::PatchMode ProjectSettingsDialog::patchMode() const
{
	return static_cast<PatchMode>(m_patchMode->currentData().toInt());
}

void ProjectSettingsDialog::showPatches()
{
	// detected: what the files tell; as written: what EasyRPG.ini says, both greyed; chosen: the boxes
	const PatchMode mode = patchMode();
	const QMap<QString, int>& values = mode == Detect ? m_detected : mode == Choose ? m_chosen : m_written;
	for (auto it = m_patchBoxes.begin(); it != m_patchBoxes.end(); ++it) {
		const QSignalBlocker blocker(it.value());
		it.value()->setChecked(values.value(it.key()) > 0);
		it.value()->setEnabled(mode == Choose);
	}
}

bool ProjectSettingsDialog::eventFilter(QObject* watched, QEvent* event)
{
	// the patch pointed at, or focused, is explained
	if (watched == m_patchesPage) {
		if (event->type() == QEvent::MouseMove) {
			const QWidget* child = m_patchesPage->childAt(static_cast<QMouseEvent*>(event)->position().toPoint());
			if (child && !child->property("patch").isNull()) {
				explainPatch(child->property("patch").toString());
			}
		}
	} else if (event->type() == QEvent::Enter || event->type() == QEvent::FocusIn) {
		explainPatch(watched->property("patch").toString());
	}
	return QDialog::eventFilter(watched, event);
}

void ProjectSettingsDialog::explainPatch(const QString& key)
{
	for (const auto& patch : GamePatches::list()) {
		if (key == QLatin1String(patch.key)) {
			m_patchHelp->setText(GamePatches::explain(patch));
			return;
		}
	}
	m_patchHelp->setText(tr("Point at a patch to see what it changes."));
}

void ProjectSettingsDialog::readIni()
{
	// the engine EasyRPG Player is told, else the one it reads in the program
	const QString exe = GamePatches::engineExe(m_project);
	const QString exe_setting = GamePatches::enginePathSetting(m_project);
	const RpgRtExe::Info info = exe.isEmpty() ? RpgRtExe::Info() : GamePatches::exeInfo(exe);
	// the players in the folder of the game: RPG_RT.exe (RPG Maker's, of which version, or
	// EasyRPG Player renamed), and EasyRPG Player under its own name
	auto version = [](QString text) {
		while (text.endsWith(QLatin1String(".0")) && text.count(QLatin1Char('.')) > 1) {
			text.chop(2);
		}
		return text;
	};
	auto describe = [&](const RpgRtExe::Info& info) {
		QString text;
		if (info.easyrpgPlayer) {
			text = info.version.isEmpty() ? tr("EasyRPG Player") : tr("EasyRPG Player %1").arg(version(info.version));
		} else if (!info.program || !info.engine) {
			text = tr("unknown version");
		} else if (info.version.isEmpty()) {
			text = RpgRtExe::engineName(info.engine);
		} else {
			text = QStringLiteral("%1 %2").arg(info.engine & RpgRtExe::EngineRpg2k3 ? tr("RPG Maker 2003") : tr("RPG Maker 2000"), info.version);
		}
		if (info.maniac) {
			text += QStringLiteral(", ") + (info.maniac > 1 ? tr("Maniac Patch %1").arg(info.maniac) : tr("Maniac Patch"));
		}
		return text;
	};
	QStringList players;
	if (!exe.isEmpty()) {
		players << QStringLiteral("%1: %2").arg(QFileInfo(exe).fileName(), describe(info));
	} else if (!exe_setting.isEmpty()) {
		players << tr("%1: not found").arg(exe_setting);
	}
	for (const QFileInfo& file : m_folder.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
		const QString name = file.fileName();
		if (file.absoluteFilePath() == QFileInfo(exe).absoluteFilePath()) {
			continue;
		}
		if (file.isFile() && file.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0 && file.size() < 64 * 1024 * 1024) {
			const RpgRtExe::Info other = GamePatches::exeInfo(file.absoluteFilePath());
			if (other.easyrpgPlayer) {
				players << QStringLiteral("%1: %2").arg(name, describe(other));
			}
		} else if (file.isDir() && file.suffix() == QLatin1String("app") && name.contains(QLatin1String("EasyRPG"), Qt::CaseInsensitive)) {
			// macOS: the version of its bundle
			const QSettings plist(file.absoluteFilePath() + QStringLiteral("/Contents/Info.plist"), QSettings::NativeFormat);
			const QString number = plist.value(QStringLiteral("CFBundleShortVersionString")).toString();
			players << QStringLiteral("%1: %2").arg(name, number.isEmpty() ? tr("EasyRPG Player") : tr("EasyRPG Player %1").arg(number));
		} else if (file.isFile() && name.compare(QLatin1String("easyrpg-player"), Qt::CaseInsensitive) == 0) {
			players << QStringLiteral("%1: %2").arg(name, tr("EasyRPG Player"));
		}
	}
	const QString runtime = players.isEmpty() ? tr("None in the game folder") : players.join(QLatin1Char('\n'));
	m_runtime->setText(runtime);
	m_runtime->setToolTip(exe);

	const QStringList names = exe.isEmpty() || info.easyrpgPlayer ? QStringList() : RpgRtExe::fontNames(exe);
	m_exeFonts->setText(names.isEmpty() ? tr("Unknown") : names.join(QStringLiteral(", ")));

	// the patches as EasyRPG.ini says now
	const GamePatches::State state = GamePatches::read(m_project);
	m_detected = GamePatches::detect(m_project);
	m_written = state.chosen ? state.values : m_detected;
	auto* model = qobject_cast<QStandardItemModel*>(m_patchMode->model());
	if (!state.others.isEmpty()) {
		// any key of [Patch] stops the detection of the Player
		if (patchMode() == Detect) {
			m_patchMode->setCurrentIndex(Refer);
		}
		model->item(Detect)->setEnabled(false);
		m_otherPatches->setText(tr("Also in EasyRPG.ini: %1").arg(state.others.join(QStringLiteral(", "))));
		m_otherPatches->setVisible(true);
	} else {
		model->item(Detect)->setEnabled(true);
		m_otherPatches->setVisible(false);
	}
	showPatches();
}

void ProjectSettingsDialog::openIni()
{
	const QString path = m_project.findFileOrDefault(QStringLiteral("EasyRPG.ini"));
	if (!QFileInfo::exists(path)) {
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly) || file.write("[Game]\r\n\r\n[Patch]\r\n") < 0) {
			QMessageBox::warning(this, windowTitle(), tr("%1 could not be created.").arg(QDir::toNativeSeparators(path)));
			return;
		}
	}
	if (!ScriptLink::openAt(path, 1)) {
		QMessageBox::warning(this, windowTitle(), tr("The script editor could not open %1.").arg(QDir::toNativeSeparators(path)));
	}
	if (patchMode() == Detect) {
		// edited by hand from now on
		m_patchMode->setCurrentIndex(Refer);
	}
}

void ProjectSettingsDialog::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
		readIni();
	}
	QDialog::changeEvent(event);
}

QString ProjectSettingsDialog::playerFont(int font) const
{
	if (m_fontChoice[font]) {
		return *m_fontChoice[font];
	}
	return fontFiles(m_folder, font).value(0);
}

void ProjectSettingsDialog::showFonts()
{
	for (int font = 0; font < 2; ++font) {
		const QString file = playerFont(font);
		QString text;
		if (file.isEmpty()) {
			const bool cjk = cjkGame();
			text = tr("Built-in (%1)").arg(cjk ? (font == 0 ? QStringLiteral("Shinonome Gothic") : QStringLiteral("Shinonome Mincho"))
				: (font == 0 ? QStringLiteral("ttyp0") : QStringLiteral("RMG2000")));
		} else if (m_fontChoice[font]) {
			text = tr("%1 (copied on OK)").arg(QFileInfo(file).fileName());
		} else {
			text = m_folder.relativeFilePath(file);
		}
		m_playerFonts[font]->setText(text);
	}
	showFontPreview();
}

void ProjectSettingsDialog::showFontPreview()
{
	if (!m_fontPreview) {
		return;
	}
	// the font chosen, as the Player draws it, its pixels doubled
	const int font = m_textFont->currentData().toInt() == lcf::rpg::System::Font_mincho ? 1 : 0;
	const QImage text = GameFont::render(tr("The quick brown fox jumps over the lazy dog."), font,
		playerFont(font), cjkGame(), palette().color(QPalette::WindowText));
	QPixmap pixmap = QPixmap::fromImage(text.scaled(text.size() * 4, Qt::IgnoreAspectRatio, Qt::FastTransformation));
	pixmap.setDevicePixelRatio(2);
	m_fontPreview->setPixmap(pixmap);
}

bool ProjectSettingsDialog::cjkGame() const
{
	// Font::DefaultBitmapFont: by the code page of the game
	const int page = codePageOf(m_project.encoding());
	return page == 932 || page == 936 || page == 949 || page == 950;
}

QString ProjectSettingsDialog::folderName() const
{
	return m_folderName->text().trimmed();
}

QString ProjectSettingsDialog::gameTitle() const
{
	return m_title->text();
}

QString ProjectSettingsDialog::encoding() const
{
	return m_encoding ? m_encoding->currentData().toString() : QString();
}

bool ProjectSettingsDialog::encodingChanged() const
{
	return m_encoding && encoding() != m_encodingSetting;
}

QString ProjectSettingsDialog::writeSettings(Project& project) const
{
	// the font of the game: in the database
	if (m_textFont && m_textFont->currentData().toInt() != project.database().system.font_id) {
		project.database().system.font_id = m_textFont->currentData().toInt();
		if (!project.saveDatabase()) {
			return tr("The database could not be saved.");
		}
	}
	// the code page first: the title is written in it
	if (encodingChanged() && !project.saveEncodingSetting(encoding())) {
		return tr("The encoding could not be written in RPG_RT.ini.");
	}
	// refer: EasyRPG.ini as it is
	const GamePatches::State state = GamePatches::read(project);
	const PatchMode mode = patchMode();
	const bool patches_changed = mode == Choose ? (!state.chosen || state.values != m_chosen)
		: mode == Detect && state.chosen;
	if (patches_changed && !GamePatches::write(project, mode == Choose ? std::optional(m_chosen) : std::nullopt)) {
		return tr("The patches could not be written in EasyRPG.ini.");
	}
	// the fonts of the Player: the old file to the trash, the new one copied
	for (int font = 0; font < 2; ++font) {
		if (!m_fontChoice[font]) {
			continue;
		}
		const QString chosen = m_fontChoice[font]->isEmpty() ? QString() : QFileInfo(*m_fontChoice[font]).absoluteFilePath();
		const QStringList old = fontFiles(project.projectDir(), font);
		if (old.size() == 1 && old.front() == chosen) {
			continue;
		}
		for (const QString& file : old) {
			if (file != chosen && !QFile::moveToTrash(file)) {
				return tr("%1 could not be moved to the trash.").arg(QDir::toNativeSeparators(file));
			}
		}
		if (chosen.isEmpty()) {
			continue;
		}
		QDir game = project.projectDir();
		QString dir = QStringLiteral("Font");
		for (const QString& sub : game.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			if (sub.compare(dir, Qt::CaseInsensitive) == 0) {
				dir = sub;
			}
		}
		const QString target = game.filePath(QStringLiteral("%1/%2.%3").arg(dir,
			font == 0 ? QStringLiteral("Font") : QStringLiteral("Font2"), QFileInfo(chosen).suffix().toLower()));
		if (target == chosen) {
			continue;
		}
		if (!game.mkpath(dir) || !QFile::copy(chosen, target)) {
			return tr("The font could not be copied in %1.").arg(QDir::toNativeSeparators(game.filePath(dir)));
		}
	}
	// as typed: an encoding changed reads the title again, unless it was typed
	if (gameTitle() != m_initialTitle && !project.saveGameTitle(gameTitle())) {
		return tr("The game title could not be written.");
	}
	return {};
}

void ProjectSettingsDialog::accept()
{
	const QString name = folderName();
	// a folder name usable on every system, and not taken
	static const QRegularExpression invalid(QStringLiteral(R"([<>:"/\\|?*]|^\.+$)"));
	if (name.isEmpty() || name.contains(invalid)) {
		QMessageBox::warning(this, windowTitle(), tr("\"%1\" is not a valid folder name.").arg(name));
		m_convert = false;
		return;
	}
	if (name != m_folder.dirName()) {
		QDir parent = m_folder;
		parent.cdUp();
		if (parent.exists(name)) {
			QMessageBox::warning(this, windowTitle(),
				tr("The folder %1 already exists. Choose another folder name.").arg(name));
			m_convert = false;
			return;
		}
	}
	QDialog::accept();
}
