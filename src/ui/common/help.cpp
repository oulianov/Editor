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

#include "help.h"
#include "ui/database/variable_widget.h"
#include "ui/database/switch_widget.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QPointer>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>
#include <lcf/rpg/eventcommand.h>

void Help::open(QWidget* from, const QString& topic) {
	// One guide window per window asking for help, next to it: a window
	// parented to a modal dialog can be used while the dialog is open
	QWidget* window = from ? from->window() : nullptr;
	QString section = topic;
	if (window && section == QStringLiteral("database")) {
		if (window->findChild<VariableWidget*>()) section = QStringLiteral("database-variables");
		else if (window->findChild<SwitchWidget*>()) section = QStringLiteral("database-switches");
	}
	auto* dialog = window ? window->findChild<QDialog*>(QStringLiteral("helpWindow"), Qt::FindDirectChildrenOnly) : nullptr;
	if (!dialog) {
		// a dialog, not a plain window: on macOS a window stays under the
		// level of a modal dialog (an event command) and cannot be reached
		dialog = new QDialog(window, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
			| Qt::WindowMinMaxButtonsHint);
		dialog->setObjectName(QStringLiteral("helpWindow"));
		dialog->setWindowTitle(QObject::tr("EasyRPG Editor Help"));
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->resize(640, 720);
		auto* layout = new QVBoxLayout(dialog);
		layout->setContentsMargins(0, 0, 0, 0);
		auto* browser = new QTextBrowser(dialog);
		browser->setObjectName(QStringLiteral("helpBrowser"));
		browser->setOpenExternalLinks(true);
		layout->addWidget(browser);
		auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
		QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
		layout->addWidget(buttons);
	}
	auto* browser = dialog->findChild<QTextBrowser*>(QStringLiteral("helpBrowser"));
	QUrl url(QStringLiteral("qrc:/help/en/index.html"));
	if (!section.isEmpty()) {
		url.setFragment(section);
	}
	browser->setSource(url);
	dialog->show();
	browser->scrollToAnchor(section);
	dialog->raise();
	dialog->activateWindow();
	browser->setFocus();
}

QString Help::commandTopic(int code) {
	const char* name = lcf::rpg::EventCommand::kCodeTags.tag(code);
	return name ? QStringLiteral("cmd-") + QString::fromLatin1(name) : QString();
}
