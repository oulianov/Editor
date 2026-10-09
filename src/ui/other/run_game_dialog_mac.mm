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

#include "run_game_dialog.h"
#include <QCoreApplication>
#include <QTimer>
#import <AppKit/AppKit.h>

void RunGameDialog::activatePlayer(qint64 pid) {
	// The detached process has started, but SDL may not have registered its
	// application yet. Keep the Editor responsive while waiting for it.
	auto* timer = new QTimer(QCoreApplication::instance());
	QObject::connect(timer, &QTimer::timeout, timer, [timer, pid, attempts = 0]() mutable {
		NSRunningApplication* player = [NSRunningApplication runningApplicationWithProcessIdentifier:static_cast<pid_t>(pid)];
		if (player.isTerminated || ++attempts >= 100) {
			timer->deleteLater();
			return;
		}
		if (!player.isFinishedLaunching || player.activationPolicy == NSApplicationActivationPolicyProhibited) {
			return;
		}
		// macOS 14 and later require the foreground app to yield activation.
		BOOL activated;
		if (@available(macOS 14.0, *)) {
			[NSApp yieldActivationToApplication:player];
			activated = [player activateFromApplication:NSRunningApplication.currentApplication options:NSApplicationActivateAllWindows];
		} else {
			activated = [player activateWithOptions:NSApplicationActivateAllWindows];
		}
		if (activated) {
			timer->deleteLater();
		}
	});
	timer->start(100);
}
