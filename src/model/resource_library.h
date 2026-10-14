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

#include <QAbstractListModel>
#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include "common/filefinder.h"

class ResourceLibraryModel;

/** What a file operation of the Resource Manager did */
struct ResourceResult
{
	Q_GADGET
	Q_PROPERTY(bool ok MEMBER ok)
	Q_PROPERTY(bool cancelled MEMBER cancelled)
	Q_PROPERTY(QString error MEMBER error)
	Q_PROPERTY(QString notice MEMBER notice)
	Q_PROPERTY(QString name MEMBER name)

public:
	/** Done */
	bool ok = false;
	/** Not done: the user said no, nothing to tell */
	bool cancelled = false;
	/** Not done: why, for a warning */
	QString error;
	/** Not done, and nothing wrong (the file is already in the game): for an information */
	QString notice;
	/** The file, without its extension: imported, renamed... */
	QString name;

	static ResourceResult done(const QString& name = QString());
	static ResourceResult failed(const QString& error);
	static ResourceResult skipped(const QString& notice);
	static ResourceResult declined();
};

/**
 * The choices of an image import that are more than a yes or a no: the
 * interface shows its own windows for them (ResourceDialogs: a message with
 * two buttons, the Import Image window). Without one, such an import is
 * cancelled.
 */
class ResourceImportChoices
{
public:
	enum class Colors { Reduce, Keep, Cancel };

	virtual ~ResourceImportChoices() = default;
	/** An image of more than 256 colors (text says so): reduced, kept, or the import cancelled */
	virtual Colors askColors(const QString& title, const QString& text);
	/** The image with its transparent color chosen, a null image to cancel */
	virtual QImage chooseTransparent(const QImage& image, const QString& file_name);
};

/**
 * The folders of RPG Maker 2003's Resource Manager, in its order, with
 * their rules: the files they take, palette color 0 transparent.
 */
class ResourceFolderModel : public QAbstractListModel
{
	Q_OBJECT

public:
	enum Role {
		/** Its name in the game (CharSet...); also DisplayRole */
		NameRole = Qt::UserRole + 1,
		/** What it holds (ToolTipRole: also when the game has none) */
		DescriptionRole,
		/** ResourceLibraryModel::Kind */
		KindRole,
		/** Palette color 0 is transparent in the game */
		TransparentRole,
		/** The game has no such folder yet (FontRole: italic) */
		MissingRole,
		/** The folder of the game, where Import writes */
		PathRole,
	};

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

private:
	friend class ResourceLibraryModel;
	explicit ResourceFolderModel(ResourceLibraryModel* library);
	/** Which folders the game has, after a change of game or an import */
	void update();

	ResourceLibraryModel* m_library;
	QList<bool> m_missing;
};

/**
 * Tools > Resource Manager, without its window: the files of a folder of
 * the game and of the RTP (one row per file, as filtered), the file shown
 * and what it is, and the file operations (Import, Export, Rename, Delete).
 *
 * The game's files hide the RTP's of the same name. The questions go
 * through DialogService (No is the safe answer); the errors come back as
 * a ResourceResult, for the window to show.
 */
class ResourceLibraryModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(QString gamePath READ gamePath WRITE setGamePath NOTIFY gamePathChanged)
	Q_PROPERTY(QString rtpPath READ rtpPath WRITE setRtpPath NOTIFY rtpPathChanged)
	Q_PROPERTY(ResourceFolderModel* folders READ folders CONSTANT)
	Q_PROPERTY(int folder READ folder WRITE setFolder NOTIFY folderChanged)
	Q_PROPERTY(QString folderName READ folderName NOTIFY folderChanged)
	Q_PROPERTY(Kind kind READ kind NOTIFY folderChanged)
	Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
	Q_PROPERTY(QString countText READ countText NOTIFY filesChanged)
	Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentChanged)
	Q_PROPERTY(QString currentName READ currentName NOTIFY currentChanged)
	Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentChanged)
	Q_PROPERTY(bool currentIsRtp READ currentIsRtp NOTIFY currentChanged)
	Q_PROPERTY(QStringList info READ info NOTIFY currentChanged)
	Q_PROPERTY(QString previewNote READ previewNote NOTIFY currentChanged)
	Q_PROPERTY(QImage previewImage READ previewImage NOTIFY currentChanged)
	Q_PROPERTY(bool canImport READ canImport NOTIFY currentChanged)
	Q_PROPERTY(bool canExport READ canExport NOTIFY currentChanged)
	Q_PROPERTY(bool canDelete READ canDelete NOTIFY currentChanged)
	Q_PROPERTY(bool importXyz READ importXyz WRITE setImportXyz NOTIFY importXyzChanged)

public:
	/** What the files of a folder are */
	enum class Kind { Image, Music, Sound, Movie };
	Q_ENUM(Kind)
	enum class Origin { Game, Rtp };
	Q_ENUM(Origin)

	enum Role {
		/** The file without its extension (DisplayRole: "name (RTP)" for the RTP's) */
		NameRole = Qt::UserRole + 1,
		PathRole,
		/** Origin: the game's file or the RTP's */
		OriginRole,
		/** In bytes */
		SizeRole,
		/** Its extension: PNG, WAV, MID... */
		FormatRole,
	};

	explicit ResourceLibraryModel(QObject* parent = nullptr);
	~ResourceLibraryModel() override;

	// QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	/*
	 * The game and its folders
	 */

	/** The folder of the game, empty when no game is open */
	QString gamePath() const;
	void setGamePath(const QString& path);
	/** The RTP folder of the settings, empty when none */
	QString rtpPath() const;
	void setRtpPath(const QString& path);
	bool hasGame() const;

	ResourceFolderModel* folders() const;
	static int folderCount();
	/** Its name in the game (CharSet...) */
	static QString folderName(int folder);
	/** -1 when there is no such folder */
	static int folderOf(const QString& name);
	static Kind folderKind(int folder);
	static FileFinder::FileType folderType(int folder);
	/** Palette color 0 is transparent in the game */
	static bool folderTransparent(int folder);
	/** What it holds, translated */
	static QString folderDescription(int folder);
	/** The folder of the game, whatever its case (Charset, CHARSET...), else where it would be */
	QString gameFolderPath(int folder) const;
	/** The folder of the RTP, empty when it has none */
	QString rtpFolderPath(int folder) const;

	/** The folder shown (remembered for the next time) */
	int folder() const;
	void setFolder(int folder);
	QString folderName() const;
	Kind kind() const;

	/*
	 * The files of the folder shown
	 */

	/** Only the files whose name has it (the file shown stays) */
	QString filter() const;
	void setFilter(const QString& text);
	/** "Files: 12, from the RTP: 3" */
	QString countText() const;
	/** Lists the files again (and which folders the game has), keeping
	 *  the file shown, else selecting the file of that name */
	Q_INVOKABLE void refresh();
	Q_INVOKABLE void refresh(const QString& select);

	/** The row of the file shown, -1 when none or filtered out */
	int currentRow() const;
	void setCurrentRow(int row);
	/** Shows the file of that name (any case); false when there is none */
	Q_INVOKABLE bool selectName(const QString& name);
	QString currentName() const;
	QString currentPath() const;
	bool currentIsRtp() const;
	/** What the file shown is, line by line (no project: says so) */
	QStringList info() const;
	/** For a file without an image: what to do with it */
	QString previewNote() const;
	/** The image shown, as in the game (palette color 0 transparent where
	 *  the folder makes it so); null when none or not readable */
	QImage previewImage() const;

	/** The file extensions of a kind of file, in the order the Player tries them */
	static QStringList extensions(FileFinder::FileType type);
	/** How an image breaks the sizes of RPG Maker 2003's help (Image
	 *  Specifications) for its folder, empty when it fits */
	static QString sizeRule(const QString& folder, QSize size);
	/** What a resource file is, line by line, as the Resource Manager and
	 *  the pickers show it: where it comes from, its size, an image's
	 *  dimensions and colors. image: the file read (an image), or null. */
	static QStringList fileInfo(const QString& path, bool rtp, const QImage& image = QImage());

	/*
	 * The operations
	 */

	bool canImport() const;
	bool canExport() const;
	/** Delete and Rename: the game's files only */
	bool canDelete() const;

	/** Images are written as XYZ (palette images only), else PNG */
	bool importXyz() const;
	void setImportXyz(bool xyz);
	/** The windows of the image choices; not owned */
	void setImportChoices(ResourceImportChoices* choices);
	/** Deleted files go to the trash of the system when it has one (tests: removed) */
	void setUseTrash(bool trash);

	/** Where the file window of Import starts: the folder of the last import */
	Q_INVOKABLE QString importDirectory() const;
	Q_INVOKABLE void rememberImportDirectory(const QString& file);
	/** The file window's filters: the files of the folder, all files */
	Q_INVOKABLE QStringList importFilters() const;
	/** Where the file window of Export starts: the folder of the last export */
	Q_INVOKABLE QString exportDirectory() const;

	/** Creates the game's folder when it has none, as RPG Maker does */
	Q_INVOKABLE ResourceResult createFolder();
	/** Copies a file into the folder (images: checked, converted to a palette
	 *  and their transparent color chosen); the list is not refreshed */
	Q_INVOKABLE ResourceResult importFile(const QString& source);
	/** createFolder(), importFile() for each, then the list with the last
	 *  one imported: the ResourceResult of each step */
	Q_INVOKABLE QVariantList importFiles(const QStringList& sources);
	/** Writes a copy of the file shown (target without extension: the file's) */
	Q_INVOKABLE ResourceResult exportCurrent(const QString& target);
	/** Renames the game's file shown, keeping its extension */
	Q_INVOKABLE ResourceResult renameCurrent(const QString& name);
	/** Deletes the game's file shown, after asking; the next one is shown */
	Q_INVOKABLE ResourceResult deleteCurrent();
	/** The folder of the file shown, else the game's folder, else the RTP's */
	Q_INVOKABLE QString openFolderPath() const;
	/** Opens it in the file manager of the system */
	Q_INVOKABLE bool openFolder() const;

signals:
	void gamePathChanged();
	void rtpPathChanged();
	void folderChanged();
	void filterChanged();
	/** The files were listed again (countText) */
	void filesChanged();
	/** Another file is shown, or the same one again after a change */
	void currentChanged();
	void importXyzChanged();
	/** A file is about to be removed (a preview playing it stops) */
	void aboutToRemoveFile(const QString& path);

private:
	friend class ResourceFolderModel;
	struct Entry {
		QString name;
		QString path;
		bool rtp = false;
		/** The extension's place in extensions(): the Player's choice */
		int rank = 0;
		qint64 size = 0;
	};

	void reload(const QString& select, bool keep);
	void applyFilter();
	void updateCurrent();
	/** Asks before replacing a game file of the same name; removes the old ones */
	ResourceResult makeRoom(const QString& dir, const QString& name);
	ResourceResult importImage(const QString& source, const QString& dir);
	ResourceResult importOther(const QString& source, const QString& dir);

	ResourceFolderModel* m_folders;
	ResourceImportChoices* m_choices = nullptr;
	QString m_gamePath;
	QString m_rtpPath;
	int m_folder = 0;
	QString m_filter;
	bool m_useTrash = true;
	bool m_importXyz = false;

	/** Every file of the folder, sorted; the rows: those the filter shows */
	QList<Entry> m_entries;
	QList<int> m_rows;
	/** The file shown (in m_entries), -1 when none */
	int m_current = -1;
	QImage m_image;
	QStringList m_info;
	QString m_previewNote;
};
