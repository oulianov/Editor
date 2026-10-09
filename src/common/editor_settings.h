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

#pragma once

#include <QObject>
#include <QString>

/**
 * The options of the Editor (the Settings window), saved with QSettings.
 * Windows that depend on an option connect to changed() and read it again.
 */
class EditorSettings : public QObject
{
	Q_OBJECT

public:
	static EditorSettings& instance();

	/** Language code ("en", "fr"...), empty: the system's; used at the next start */
	QString language() const;
	void setLanguage(const QString& code);

	QString rtpPath() const;
	void setRtpPath(const QString& path);

	/** The EasyRPG Player for test play; empty: searched next to the Editor and in the PATH */
	QString playerPath() const;
	void setPlayerPath(const QString& path);

	/** Read again the files that other programs change in the project (off by default) */
	bool reloadExternalChanges() const;
	void setReloadExternalChanges(bool on);

	/** Water and animated tiles move in the map view */
	bool animateTiles() const;
	void setAnimateTiles(bool on);

	/** Event commands colored by kind, as in RPG Maker 2003 */
	bool commandColors() const;
	void setCommandColors(bool on);

	/** Every other line of the event commands on a tinted background */
	bool commandStripes() const;
	void setCommandStripes(bool on);

	/** Search results kept between searches */
	bool searchCache() const;
	void setSearchCache(bool on);

signals:
	void changed();

private:
	EditorSettings() = default;
	QVariant value(const char* key, const QVariant& fallback) const;
	void setValue(const char* key, const QVariant& value);
};
