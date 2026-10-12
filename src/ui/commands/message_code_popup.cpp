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

#include "message_code_popup.h"
#include "common/exfont.h"
#include "model/project_data.h"
#include "ui/common/rpg_model.h"
#include "ui/common/search_combobox.h"

#include <QGridLayout>
#include <QLabel>
#include <QPointer>
#include <QScreen>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

/** The list a code's number is chosen from */
enum class Pick { None, Hero, Variable };

struct Code {
	const char* text;
	const char* name;
	const char* tip;
	Pick pick = Pick::None;
};

struct Section {
	std::vector<Code> codes;
};

// the codes of RPG Maker 2000 and 2003 messages: text, look, timing
const std::vector<Section>& sections() {
	static const std::vector<Section> list = {
		{{
			{"\\N[1]", QT_TRANSLATE_NOOP("MessageCodePopup", "Hero name"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "The name of hero n (0: the party leader)"), Pick::Hero},
			{"\\V[1]", QT_TRANSLATE_NOOP("MessageCodePopup", "Variable"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "The value of variable n"), Pick::Variable},
			{"\\$", QT_TRANSLATE_NOOP("MessageCodePopup", "Money"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "Shows the money window")},
			{"\\\\", QT_TRANSLATE_NOOP("MessageCodePopup", "Backslash"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "A backslash: \\")},
			{"\\_", QT_TRANSLATE_NOOP("MessageCodePopup", "Half space"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "A space half as wide")},
		}},
		{{
			{"\\C[0]", QT_TRANSLATE_NOOP("MessageCodePopup", "Color"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "The text after it in color n of the System graphic (0: normal)")},
			{"\\S[1]", QT_TRANSLATE_NOOP("MessageCodePopup", "Speed"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "Text speed: 1 fast to 20 slow")},
			{"\\>", QT_TRANSLATE_NOOP("MessageCodePopup", "Instant on"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "Shows the text after it at once, up to \\< or the end of the line")},
			{"\\<", QT_TRANSLATE_NOOP("MessageCodePopup", "Instant off"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "Back to letter by letter")},
		}},
		{{
			{"\\.", QT_TRANSLATE_NOOP("MessageCodePopup", "Short pause"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "Small pause (a quarter of a second)")},
			{"\\|", QT_TRANSLATE_NOOP("MessageCodePopup", "Long pause"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "Pause of one second")},
			{"\\!", QT_TRANSLATE_NOOP("MessageCodePopup", "Wait for key"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "Waits for a key before the rest")},
			{"\\^", QT_TRANSLATE_NOOP("MessageCodePopup", "Close by itself"),
				QT_TRANSLATE_NOOP("MessageCodePopup", "Closes the message without waiting for a key")},
		}},
	};
	return list;
}

QLabel* header(const QString& text, QWidget* parent) {
	auto* label = new QLabel(text, parent);
	QFont font = label->font();
	font.setBold(true);
	label->setFont(font);
	return label;
}

/** A glyph enlarged with its pixels kept sharp, on high density screens too */
QIcon glyphIcon(const QImage& sheet, int index, const QColor& color, int scale) {
	const QImage glyph = ExFont::glyph(sheet, index, color);
	if (glyph.isNull()) {
		return {};
	}
	QIcon icon;
	for (int ratio : {1, 2}) {
		QPixmap pixmap = QPixmap::fromImage(glyph.scaled(glyph.size() * scale * ratio, Qt::IgnoreAspectRatio, Qt::FastTransformation));
		pixmap.setDevicePixelRatio(ratio);
		icon.addPixmap(pixmap);
	}
	return icon;
}

/** Types text at the cursor, the number between [ and ] selected to be typed over when asked */
void typeCode(QTextEdit* edit, const QString& text, bool selectNumber) {
	QTextCursor cursor = edit->textCursor();
	cursor.insertText(text);
	const int open = text.indexOf(u'[');
	const int close = text.indexOf(u']');
	if (selectNumber && open >= 0 && close > open) {
		const int start = cursor.position() - text.size();
		cursor.setPosition(start + open + 1);
		cursor.setPosition(start + close, QTextCursor::KeepAnchor);
	}
	edit->setTextCursor(cursor);
	edit->setFocus();
}

/** The searchable list of the heroes or variables under anchor: the one chosen is typed as "\\N[id]" */
template <typename LCF>
void pickEntry(QTextEdit* edit, ProjectData& project, QWidget* anchor, const QString& name, const QString& code) {
	// a hidden list of the text box, kept for the next code
	auto* list = edit->findChild<SearchComboBox*>(name);
	if (!list) {
		list = new SearchComboBox(edit);
		list->setObjectName(name);
		list->hide();
		list->setModel(new RpgModel<LCF>(project, list));
	}
	QObject::disconnect(list, &QComboBox::activated, nullptr, nullptr);
	QPointer<QTextEdit> target = edit;
	QObject::connect(list, &QComboBox::activated, list, [list, target, code](int row) {
		if (target && row >= 0) {
			typeCode(target, QStringLiteral("\\%1[%2]").arg(code).arg(list->itemData(row).toInt()), false);
		}
	});
	list->showPopupUnder(anchor);
}

} // namespace

MessageCodePopup::MessageCodePopup(QTextEdit* edit, QWidget* anchor) :
	QFrame(nullptr, Qt::Popup), m_edit(edit), m_anchor(anchor)
{
	setAttribute(Qt::WA_DeleteOnClose);
	// a click on the button closes the pop-up without opening it again
	setAttribute(Qt::WA_NoMouseReplay);
	setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
}

void MessageCodePopup::showCodes(QTextEdit* edit, ProjectData& project, QWidget* anchor) {
	auto* popup = new MessageCodePopup(edit, anchor);
	popup->setObjectName(QStringLiteral("messageCodes"));
	popup->addCodes(project);
	popup->place();
	popup->show();
}

void MessageCodePopup::showSymbols(QTextEdit* edit, ProjectData& project, QWidget* anchor) {
	auto* popup = new MessageCodePopup(edit, anchor);
	popup->setObjectName(QStringLiteral("messageSymbols"));
	popup->addSymbols(project);
	popup->place();
	popup->show();
}

void MessageCodePopup::addCodes(ProjectData& project) {
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 8, 8);
	constexpr int columns = 4;

	auto button = [this](const QString& code, const QString& name, const QString& tip) {
		auto* b = new QToolButton(this);
		b->setObjectName(QStringLiteral("code ") + code);
		b->setText(code + QLatin1Char('\n') + name);
		b->setToolTip(QStringLiteral("<b>%1</b> %2").arg(code.toHtmlEscaped(), tip.toHtmlEscaped()));
		b->setAutoRaise(true);
		b->setToolButtonStyle(Qt::ToolButtonTextOnly);
		b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		return b;
	};

	for (const auto& section : sections()) {
		// one block of buttons per kind of code, without a title
		const bool first = &section == &sections().front();
		auto* grid = new QGridLayout();
		grid->setSpacing(2);
		int i = 0;
		for (const auto& code : section.codes) {
			const QString text = QString::fromLatin1(code.text);
			auto* b = button(text, tr(code.name), tr(code.tip));
			ProjectData* p = &project;
			const Pick pick = code.pick;
			connect(b, &QToolButton::clicked, this, [this, text, pick, p] {
				QPointer<QTextEdit> edit = m_edit;
				QWidget* anchor = m_anchor;
				close();
				if (!edit) {
					return;
				}
				if (pick == Pick::Hero) {
					pickEntry<lcf::rpg::Actor>(edit, *p, anchor, QStringLiteral("heroList"), QStringLiteral("N"));
				} else if (pick == Pick::Variable) {
					pickEntry<lcf::rpg::Variable>(edit, *p, anchor, QStringLiteral("variableList"), QStringLiteral("V"));
				} else {
					typeCode(edit, text, true);
				}
			});
			grid->addWidget(b, i / columns, i % columns);
			++i;
		}
		if (first) {
			// the symbols of the game, chosen in a second pop-up
			const ExFont::Sheet sheet = ExFont::load(project.project());
			auto* b = button(QStringLiteral("$A…"), tr("Symbol"),
				tr("A symbol of the game's font, $A to $z (the list of them)"));
			b->setObjectName(QStringLiteral("codeSymbol"));
			// the glyph as large as in the game: the button no taller than the others
			b->setIcon(glyphIcon(sheet.image, 0, palette().color(QPalette::ButtonText), 1));
			b->setIconSize(QSize(ExFont::GlyphSize, ExFont::GlyphSize));
			b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
			b->setText(tr("Symbol…"));
			ProjectData* p = &project;
			connect(b, &QToolButton::clicked, this, [this, p] {
				QPointer<QTextEdit> edit = m_edit;
				QWidget* anchor = m_anchor;
				close();
				if (edit) {
					showSymbols(edit, *p, anchor);
				}
			});
			grid->addWidget(b, i / columns, i % columns);
		}
		layout->addLayout(grid);
	}
}

void MessageCodePopup::addSymbols(ProjectData& project) {
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(8, 8, 8, 8);
	const ExFont::Sheet sheet = ExFont::load(project.project());
	layout->addWidget(header(tr("Symbols (%1)").arg(sheet.source), this));
	auto* grid = new QGridLayout();
	grid->setSpacing(1);
	const QColor color = palette().color(QPalette::ButtonText);
	for (int index = 0; index < ExFont::Count; ++index) {
		const QString code = ExFont::code(index);
		auto* b = new QToolButton(this);
		b->setObjectName(QStringLiteral("symbol ") + code);
		b->setAutoRaise(true);
		b->setIcon(glyphIcon(sheet.image, index, color, 3));
		b->setIconSize(QSize(36, 36));
		b->setToolTip(code);
		b->setAccessibleName(code);
		connect(b, &QToolButton::clicked, this, [this, code] {
			insert(code);
			close();
		});
		grid->addWidget(b, index / ExFont::Columns, index % ExFont::Columns);
	}
	layout->addLayout(grid);
}

void MessageCodePopup::insert(const QString& text) {
	if (m_edit) {
		typeCode(m_edit, text, true);
	}
}

void MessageCodePopup::place() {
	adjustSize();
	const QRect screen = m_anchor->screen()->availableGeometry();
	QPoint position = m_anchor->mapToGlobal(QPoint(0, m_anchor->height()));
	if (position.y() + height() > screen.bottom()) {
		position.setY(m_anchor->mapToGlobal(QPoint(0, 0)).y() - height());
	}
	position.setX(qBound(screen.left(), position.x(), qMax(screen.left(), screen.right() - width())));
	position.setY(qMax(screen.top(), position.y()));
	move(position);
}
