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

#include <QFont>
#include <QObject>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QUrl>
#include <QVariantList>
#include <QtQmlIntegration>


/**
 * The Editor for QML (the singleton Ez.Editor): its options (EditorSettings,
 * written at once), the languages it has, the guide, the color scheme, and
 * the helpers of the windows (paths of the file dialogs, the Player).
 */
class EditorBridge : public QObject {
	Q_OBJECT
	QML_NAMED_ELEMENT(Editor)
	QML_SINGLETON
	Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY settingsChanged)
	Q_PROPERTY(QString rtpPath READ rtpPath WRITE setRtpPath NOTIFY settingsChanged)
	Q_PROPERTY(QString playerPath READ playerPath WRITE setPlayerPath NOTIFY settingsChanged)
	Q_PROPERTY(QString scriptEditor READ scriptEditor WRITE setScriptEditor NOTIFY settingsChanged)
	Q_PROPERTY(bool reloadExternalChanges READ reloadExternalChanges WRITE setReloadExternalChanges NOTIFY settingsChanged)
	Q_PROPERTY(bool searchCache READ searchCache WRITE setSearchCache NOTIFY settingsChanged)
	/** A newer Editor looked for at start-up and every hour (Ez.Updater) */
	Q_PROPERTY(bool checkForUpdates READ checkForUpdates WRITE setCheckForUpdates NOTIFY settingsChanged)
	Q_PROPERTY(bool animateTiles READ animateTiles WRITE setAnimateTiles NOTIFY settingsChanged)
	Q_PROPERTY(bool commandColors READ commandColors WRITE setCommandColors NOTIFY settingsChanged)
	Q_PROPERTY(bool commandStripes READ commandStripes WRITE setCommandStripes NOTIFY settingsChanged)
	Q_PROPERTY(QString defaultScriptEditor READ defaultScriptEditor CONSTANT)
	/** The languages of the Editor besides the system's: {code, name} in their own language */
	Q_PROPERTY(QVariantList languages READ languages CONSTANT)
	Q_PROPERTY(QString version READ version CONSTANT)
	/** The system shows dark colors */
	Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)
	/** The font of the forms: the small size of the platform on macOS */
	Q_PROPERTY(QFont font READ font CONSTANT)
	/** A fixed-width font of the same size */
	Q_PROPERTY(QFont monospaceFont READ monospaceFont CONSTANT)

public:
	explicit EditorBridge(QObject* parent = nullptr);

	QString language() const;
	void setLanguage(const QString& code);
	QString rtpPath() const;
	void setRtpPath(const QString& path);
	QString playerPath() const;
	void setPlayerPath(const QString& path);
	QString scriptEditor() const;
	void setScriptEditor(const QString& editor);
	bool reloadExternalChanges() const;
	void setReloadExternalChanges(bool on);
	bool searchCache() const;
	void setSearchCache(bool on);
	bool checkForUpdates() const;
	void setCheckForUpdates(bool on);
	bool animateTiles() const;
	void setAnimateTiles(bool on);
	bool commandColors() const;
	void setCommandColors(bool on);
	bool commandStripes() const;
	void setCommandStripes(bool on);
	QString defaultScriptEditor() const;
	QVariantList languages() const;
	QString version() const;
	bool dark() const;
	QFont font() const;
	QFont monospaceFont() const;
	/** The font of the forms (font) */
	static QFont formFont();

	/** Window sizes, tabs, selections... back to their defaults (the preferences and games stay) */
	Q_INVOKABLE void resetLocalState();
	/** The guide at a topic, next to the window of from */
	Q_INVOKABLE void help(const QString& topic = QString(), QQuickItem* from = nullptr);
	/**
	 * Shows a window of the module (an Ez.DialogWindow) over the window of
	 * from, modal, with its properties, and returns its result once closed
	 * (1: OK): Ez.Editor.exec("CurveDialog", {parameters: ...}, button)
	 */
	Q_INVOKABLE int exec(const QString& type, const QVariantMap& properties = {}, QQuickItem* from = nullptr);
	/** The folder of the Player the Editor would run (where to look for another one) */
	Q_INVOKABLE QUrl playerFolder() const;
	/** The program of a file chosen as the Player (an app bundle: its program); empty if none */
	Q_INVOKABLE QString executableOf(const QUrl& file) const;
	/** The path of a file of a file dialog, as the settings keep them */
	Q_INVOKABLE QString localPath(const QUrl& file) const;
	/** A path as a URL for the file dialogs */
	Q_INVOKABLE QUrl url(const QString& path) const;
	/** The guide, as HTML (its anchors are the topics) */
	Q_INVOKABLE QString helpText() const;
	/** Where an anchor (<a name>) is in a rich text: its position, -1 if it has none */
	Q_INVOKABLE int anchorPosition(QQuickTextDocument* document, const QString& anchor) const;
	/** The icon themes of the Editor (ezbright, ezdark) have an icon of this name */
	Q_INVOKABLE bool hasIcon(const QString& name) const;
	/**
	 * The part of the screen of an item the windows can use (without the
	 * menu bar and the dock), in the coordinates of the screens: where a
	 * pop-up of its own window fits (mapToGlobal)
	 */
	Q_INVOKABLE QRectF screenArea(QQuickItem* item) const;

signals:
	void settingsChanged();
	void darkChanged();
};
