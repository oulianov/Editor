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

#include "editor_settings.h"
#include "defines.h"
#include "ui/other/run_game_dialog.h"
#include <QSettings>

namespace {
	constexpr const char* ANIMATE_TILES_KEY = "animate_tiles";
	constexpr const char* COMMAND_COLORS_KEY = "event_command_colors";
	constexpr const char* COMMAND_STRIPES_KEY = "event_command_stripes";
	constexpr const char* SEARCH_CACHE_KEY = "search_cache";
}

EditorSettings& EditorSettings::instance() {
	static EditorSettings settings;
	return settings;
}

QVariant EditorSettings::value(const char* key, const QVariant& fallback) const {
	return QSettings().value(QLatin1String(key), fallback);
}

void EditorSettings::setValue(const char* key, const QVariant& value) {
	QSettings settings;
	if (settings.value(QLatin1String(key)) == value) {
		return;
	}
	settings.setValue(QLatin1String(key), value);
	emit changed();
}

QString EditorSettings::language() const { return value(LANGUAGE_KEY, QString()).toString(); }
void EditorSettings::setLanguage(const QString& code) { setValue(LANGUAGE_KEY, code); }

QString EditorSettings::rtpPath() const { return value(RTP_KEY, QString()).toString(); }
void EditorSettings::setRtpPath(const QString& path) { setValue(RTP_KEY, path); }

QString EditorSettings::playerPath() const { return value(PLAYER_PATH_KEY, QString()).toString(); }
void EditorSettings::setPlayerPath(const QString& path) { setValue(PLAYER_PATH_KEY, path); }

bool EditorSettings::reloadExternalChanges() const { return value(RELOAD_EXTERNAL_KEY, false).toBool(); }
void EditorSettings::setReloadExternalChanges(bool on) { setValue(RELOAD_EXTERNAL_KEY, on); }

bool EditorSettings::animateTiles() const { return value(ANIMATE_TILES_KEY, true).toBool(); }
void EditorSettings::setAnimateTiles(bool on) { setValue(ANIMATE_TILES_KEY, on); }

bool EditorSettings::commandColors() const { return value(COMMAND_COLORS_KEY, true).toBool(); }
void EditorSettings::setCommandColors(bool on) { setValue(COMMAND_COLORS_KEY, on); }

bool EditorSettings::commandStripes() const { return value(COMMAND_STRIPES_KEY, true).toBool(); }
void EditorSettings::setCommandStripes(bool on) { setValue(COMMAND_STRIPES_KEY, on); }

bool EditorSettings::searchCache() const { return value(SEARCH_CACHE_KEY, false).toBool(); }
void EditorSettings::setSearchCache(bool on) { setValue(SEARCH_CACHE_KEY, on); }
