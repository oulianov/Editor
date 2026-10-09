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
#include <QDialog>
#include <QPointer>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>
#include <lcf/rpg/eventcommand.h>

void Help::open(QWidget* from, const QString& topic) {
	// One guide window per window asking for help, next to it: a window
	// parented to a modal dialog can be used while the dialog is open
	QWidget* window = from ? from->window() : nullptr;
	auto* dialog = window ? window->findChild<QDialog*>(QStringLiteral("helpWindow"), Qt::FindDirectChildrenOnly) : nullptr;
	if (!dialog) {
		dialog = new QDialog(window, Qt::Window);
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
	}
	auto* browser = dialog->findChild<QTextBrowser*>(QStringLiteral("helpBrowser"));
	QUrl url(QStringLiteral("qrc:/help/en/index.html"));
	if (!topic.isEmpty()) {
		url.setFragment(topic);
	}
	browser->setSource(url);
	dialog->show();
	dialog->raise();
	dialog->activateWindow();
}

QString Help::commandTopic(int code) {
	const char* name = lcf::rpg::EventCommand::kCodeTags.tag(code);
	return name ? QStringLiteral("cmd-") + QString::fromLatin1(name) : QString();
}
