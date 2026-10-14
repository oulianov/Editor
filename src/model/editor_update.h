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

#include <QFile>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>

class DialogService;
class QNetworkAccessManager;
class QNetworkReply;
class QProcessEnvironment;
class QTemporaryDir;

/**
 * Where the new versions of the Editor come from, and how this copy of the
 * Editor is replaced by one.
 */
namespace EditorUpdates {
	/**
	 * The GitHub repository ("owner/name") whose latest release is the
	 * newest Editor: EDITOR_UPDATE_REPOSITORY of CMake; empty: no updates
	 */
	QString repository();
	/**
	 * The latest release, as GitHub's REST API gives it; EDITOR_UPDATE_FEED
	 * (a URL, file:// too, or a local file) replaces it: mirrors and tests.
	 * Empty without either
	 */
	QUrl feedUrl();
	/** Updates are looked for (a repository or a feed) */
	bool enabled();
	/** A tag as a version: "v0.10.0" is "0.10.0" */
	QString versionOf(const QString& tag);
	/** Below 0, 0, above 0 as version a is older than b, the same, newer
	 *  (numbers by numbers: 0.10.0 is newer than 0.9.1; a leading v is skipped) */
	int compareVersions(const QString& a, const QString& b);

	/** How this copy is replaced, as it runs (see the docs) */
	enum class Method {
		/** macOS: the application bundle, from the disk image (.dmg) */
		MacBundle,
		/** Windows: the folder of the zip ("EasyRPG Editor", the program in its bin) */
		WindowsFolder,
		/** Linux: the AppImage file ($APPIMAGE) */
		AppImage,
		/** Flatpak, a build from source...: the page of the release, in the browser */
		Browser,
	};
	Method method();
	/** What the update replaces: the bundle, the folder, the AppImage; empty for Browser */
	QString target();
	/** The package of a release for a method, among the names of its files; -1 for none */
	int pickAsset(const QStringList& names, Method method);

	struct Release {
		/** "0.10.0" */
		QString version;
		/** Its page on GitHub */
		QUrl page;
		/** The package for this computer (pickAsset), empty for none */
		QString assetName;
		QUrl assetUrl;
		qint64 assetSize = 0;
	};
	/**
	 * The release of a feed: GitHub's latest release, or a list of releases
	 * (the first published, neither a draft nor a pre-release)
	 */
	std::optional<Release> parseRelease(const QByteArray& json, Method method, QString* error = nullptr);
}

/**
 * Looks for a newer Editor (the latest release of EditorUpdates::feedUrl())
 * at start-up and every hour (EditorSettings::checkForUpdates), or when asked
 * (Help > Check for Updates); then installs it: downloaded with its
 * progress, made ready next to this copy (EditorUpdates::Method), and put in
 * its place by a small helper once the Editor has quit, which starts it
 * again after a restart. Where this copy cannot be replaced, the release is
 * opened instead (its page, or the disk image on macOS).
 *
 * The questions and failures go through the DialogService; the automatic
 * checks say nothing. One for the whole Editor (instance()): the menu, the
 * toolbar and the Settings show it.
 */
class EditorUpdater : public QObject
{
	Q_OBJECT
	/** Updates are looked for (EditorUpdates::enabled) */
	Q_PROPERTY(bool enabled READ isEnabled CONSTANT)
	Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
	/** The newest version found, empty before one is */
	Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY stateChanged)
	/** A newer version is known: Install Update, the toolbar's Update button */
	Q_PROPERTY(bool updateAvailable READ isUpdateAvailable NOTIFY stateChanged)
	/** Being downloaded or made ready */
	Q_PROPERTY(bool busy READ isBusy NOTIFY stateChanged)
	/** Ready: installed when the Editor quits (Restart to Update) */
	Q_PROPERTY(bool ready READ isReady NOTIFY stateChanged)
	/** It replaces this copy; false: its page opens in the browser */
	Q_PROPERTY(bool installsInPlace READ installsInPlace CONSTANT)
	/** Of the download, in percent; -1 while unknown */
	Q_PROPERTY(int progress READ progress NOTIFY progressChanged)

public:
	enum class State { Idle, Checking, Available, Downloading, Preparing, Ready };
	Q_ENUM(State)

	static EditorUpdater& instance();
	~EditorUpdater() override;

	bool isEnabled() const;
	QString currentVersion() const;
	QString latestVersion() const;
	State state() const;
	bool isUpdateAvailable() const;
	bool isBusy() const;
	bool isReady() const;
	bool installsInPlace() const;
	int progress() const;

	/** Where the questions go (nullptr: the installed DialogService) */
	void setDialogService(DialogService* dialogs);

	/**
	 * The checks of the settings, from now on: a few seconds after this, then
	 * every hour. automation: a script's run, which checks only a feed of
	 * its own (EDITOR_UPDATE_FEED), not GitHub
	 */
	void startAutomaticChecks(bool automation = false);
	/** Looks for a newer version now; interactive: says what it found */
	Q_INVOKABLE void check(bool interactive = true);
	/** What the menu item and the toolbar button do now: check, install, restart */
	Q_INVOKABLE void activate();
	/** The version found, downloaded and made ready (or its page opened) */
	Q_INVOKABLE void install();
	/** Ready: asks the window to quit (restartRequested), the update installed then */
	Q_INVOKABLE void restart();
	/** The window quits for the update: started again once installed */
	void setRelaunchOnQuit(bool relaunch);

	/** The text of the menu item (Check for Updates...: empty), translated */
	QString actionText() const;
	/** The menu item can be chosen now */
	bool actionEnabled() const;

signals:
	void stateChanged();
	void progressChanged();
	/** The window should quit (asking about unsaved changes) for the update */
	void restartRequested();

private:
	explicit EditorUpdater(QObject* parent);

	DialogService& dialogs() const;
	void setState(State state);
	void setProgress(int percent);
	void scheduleChecks();
	void checked(QNetworkReply* reply, bool interactive);
	void startDownload(const QString& file);
	void downloaded();
	void prepare();
	void prepareMac();
	void verifyMac();
	void prepareWindows();
	void prepareAppImage();
	void detachImage(std::function<void()> then);
	void readyToRestart();
	/** The install stops: the error told, what was downloaded removed */
	void fail(const QString& error);
	void cleanUp();
	void openPage();
	/** Starts the helper that puts the update in place (aboutToQuit) */
	void installOnQuit();
	/** Runs a program, then done(exit code, -1 when it did not run; its output) */
	void run(const QString& program, const QStringList& args, std::function<void(int, const QString&)> done,
		const QProcessEnvironment* env = nullptr);

	DialogService* m_dialogs = nullptr;
	QNetworkAccessManager* m_network = nullptr;
	QPointer<QNetworkReply> m_reply;
	QTimer m_timer;
	bool m_automatic = false;
	bool m_automation = false;
	State m_state = State::Idle;
	int m_progress = -1;
	std::optional<EditorUpdates::Release> m_release;
	/** What is replaced, the folder next to it the update is made ready in,
	 *  and the update made ready there */
	QString m_target;
	QString m_staging;
	QString m_staged;
	/** macOS, a bundle that cannot be replaced: the disk image is opened */
	bool m_manual = false;
	bool m_relaunch = false;
	QFile m_file;
	std::unique_ptr<QTemporaryDir> m_mount;
};
