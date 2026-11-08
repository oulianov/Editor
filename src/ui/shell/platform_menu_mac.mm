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

// arrangeApplicationMenu() of platform_menu.h, whose QML types editor_core
// does not build with
#import <AppKit/AppKit.h>

namespace {

// Qt's items of the application menu, found the first time
struct ApplicationMenu {
	NSMenu* menu = nil;
	NSMenuItem* about = nil;
	NSMenuItem* updates = nil;
	NSMenuItem* preferences = nil;
	NSMenuItem* services = nil;
	NSMenuItem* quit = nil;
	// Hide, Hide Others, Show All
	NSArray<NSMenuItem*>* hiding = nil;
};

ApplicationMenu& applicationMenu()
{
	static ApplicationMenu menu;
	return menu;
}

// Qt makes the application menu once, in this order: About, About Qt (Check
// for Updates), a separator, Settings, a separator, Services, a separator,
// Hide, Hide Others, Show All, a separator, Quit. False when it is not (yet)
// that menu.
bool findItems(ApplicationMenu& found)
{
	// the menu of Services is in it (the first menu of the bar can be
	// another before Qt moves the application menu there)
	NSMenu* menu = NSApp.servicesMenu.supermenu;
	if (!menu || menu.numberOfItems < 12)
		return false;
	NSMutableArray<NSMenuItem*>* hiding = [NSMutableArray array];
	NSMenuItem* services = nil;
	for (NSMenuItem* item in menu.itemArray) {
		if (item.action == @selector(hide:) || item.action == @selector(hideOtherApplications:)
				|| item.action == @selector(unhideAllApplications:))
			[hiding addObject:item];
		if (item.submenu == NSApp.servicesMenu)
			services = item;
	}
	NSMenuItem* quit = [menu itemAtIndex:menu.numberOfItems - 1];
	if (hiding.count != 3 || !services || [menu itemAtIndex:0].separatorItem || [menu itemAtIndex:1].separatorItem
			|| ![menu itemAtIndex:2].separatorItem || [menu itemAtIndex:3].separatorItem
			|| ![quit.keyEquivalent isEqualToString:@"q"])
		return false;
	found.menu = menu;
	found.about = [menu itemAtIndex:0];
	found.updates = [menu itemAtIndex:1];
	found.preferences = [menu itemAtIndex:3];
	found.services = services;
	found.quit = quit;
	found.hiding = hiding;
	return true;
}

// About first, then (after a separator) Settings, the application's items
// (Test Play Options) and Check for Updates
void arrange(const ApplicationMenu& items)
{
	NSMenu* menu = items.menu;
	// the application's items (Test Play Options), which Qt puts after the
	// last item it made, wherever Check for Updates has been moved
	NSMutableArray<NSMenuItem*>* own = [NSMutableArray array];
	for (NSMenuItem* item in menu.itemArray) {
		if (!item.separatorItem && item != items.about && item != items.updates && item != items.preferences
				&& item != items.services && item != items.quit && ![items.hiding containsObject:item])
			[own addObject:item];
	}
	// already in place (checked whenever a menu opens): left as it is
	const NSInteger settings = [menu indexOfItem:items.preferences];
	const NSInteger count = static_cast<NSInteger>(own.count);
	bool placed = [menu indexOfItem:items.about] == 0 && [menu indexOfItem:items.updates] == settings + 1 + count;
	for (NSInteger i = 0; placed && i < count; ++i)
		placed = [menu indexOfItem:own[static_cast<NSUInteger>(i)]] == settings + 1 + i;
	if (placed)
		return;

	for (NSMenuItem* item in own)
		[menu removeItem:item];
	if ([menu indexOfItem:items.about] != 0) {
		[menu removeItem:items.about];
		[menu insertItem:items.about atIndex:0];
	}
	[menu removeItem:items.updates];
	NSInteger at = [menu indexOfItem:items.preferences] + 1;
	for (NSMenuItem* item in own)
		[menu insertItem:item atIndex:at++];
	[menu insertItem:items.updates atIndex:at];
}

}

void arrangeApplicationMenu()
{
	ApplicationMenu& items = applicationMenu();
	if (!items.menu) {
		if (!findItems(items))
			return;
		// Qt adds the application items when it updates its menus: checked
		// again whenever a menu opens
		[[NSNotificationCenter defaultCenter] addObserverForName:NSMenuDidBeginTrackingNotification
			object:nil queue:nil usingBlock:^(NSNotification*) {
				arrange(applicationMenu());
			}];
	}
	arrange(items);
}
