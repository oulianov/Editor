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

#include "resource_windows.h"
#include "core.h"
#include "model/audio_library.h"
#include "model/project.h"
#include "model/transparent_color.h"
#include "qml_dialog_service.h"
#include "qml_host.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QSettings>

namespace {

// QQuickDialogButtonBox::StandardButton and ButtonRole (the values of QPlatformDialogHelper)
constexpr int Cancel = 0x00400000;
constexpr int AcceptRole = 0;

const char RESOURCE_MANAGER_SIZE_KEY[] = "resource_manager/size";
const char JUKEBOX_SIZE_KEY[] = "jukebox_size";

QPointer<QQuickWindow> jukebox_window;
QPointer<AudioLibraryModel> jukebox_library;

/** The size a window opens at: as last time, else its default, on the screen of over */
QVariantMap sizeProperties(const char* key, QSize size, QWindow* over) {
	const QSize remembered = QSettings().value(QLatin1String(key)).toSize();
	if (remembered.isValid()) {
		size = remembered;
	}
	if (const QScreen* screen = over ? over->screen() : nullptr) {
		size = size.boundedTo(screen->availableGeometry().size() * 0.95);
	}
	return {{QStringLiteral("preferredWidth"), size.width()}, {QStringLiteral("preferredHeight"), size.height()}};
}

void saveSize(const QWindow* window, const char* key) {
	QSettings().setValue(QLatin1String(key), window->size());
}

/** The files of the project open, and of the RTP */
void useProject(AudioLibraryModel* library) {
	const auto& project = core().project();
	library->setGamePath(project ? project->projectDir().absolutePath() : QString());
	library->setRtpPath(core().rtpPath(QString()));
	if (project) {
		library->setSystem(project->database().system);
	} else {
		library->clearSystem();
	}
	library->refresh();
}

} // namespace

ResourceDialogs::ResourceDialogs(QObject* parent) : QObject(parent)
{
}

ResourceDialogs::~ResourceDialogs()
{
	setLibrary(nullptr);
}

ResourceLibraryModel* ResourceDialogs::library() const
{
	return m_library;
}

void ResourceDialogs::setLibrary(ResourceLibraryModel* library)
{
	if (library == m_library) {
		return;
	}
	if (m_library) {
		m_library->setImportChoices(nullptr);
	}
	m_library = library;
	if (m_library) {
		m_library->setImportChoices(this);
	}
	emit libraryChanged();
}

QWindow* ResourceDialogs::window() const
{
	return m_window;
}

void ResourceDialogs::setWindow(QWindow* window)
{
	if (window != m_window) {
		m_window = window;
		emit windowChanged();
	}
}

bool ResourceDialogs::report(const QString& title, const ResourceResult& result)
{
	QmlDialogService dialogs(m_window);
	if (!result.error.isEmpty()) {
		dialogs.warning(title, result.error);
	} else if (!result.notice.isEmpty()) {
		dialogs.information(title, result.notice);
	}
	return result.ok;
}

ResourceImportChoices::Colors ResourceDialogs::askColors(const QString& title, const QString& text)
{
	const QVariantList buttons = {
		QVariantMap{{QStringLiteral("answer"), QStringLiteral("reduce")}, {QStringLiteral("role"), AcceptRole},
			{QStringLiteral("text"), QCoreApplication::translate("ResourceManagerDialog", "Reduce to 256 colors")}},
		QVariantMap{{QStringLiteral("answer"), QStringLiteral("keep")}, {QStringLiteral("role"), AcceptRole},
			{QStringLiteral("text"), QCoreApplication::translate("ResourceManagerDialog", "Keep all colors")}},
	};
	QString answer;
	QmlHost::exec(QStringLiteral("MessageWindow"), m_window, {
		{QStringLiteral("title"), title},
		{QStringLiteral("text"), text},
		{QStringLiteral("standardButtons"), Cancel},
		{QStringLiteral("extraButtons"), buttons},
	}, [&answer](QQuickWindow* window) {
		answer = window->property("answer").toString();
	});
	if (answer == QLatin1String("reduce")) {
		return Colors::Reduce;
	}
	return answer == QLatin1String("keep") ? Colors::Keep : Colors::Cancel;
}

QImage ResourceDialogs::chooseTransparent(const QImage& image, const QString& file_name)
{
	return ResourceWindows::chooseTransparent(m_window, image,
		QCoreApplication::translate("ResourceManagerDialog", "Transparent color of %1").arg(file_name));
}

void ResourceWindows::resourceManager(QWindow* over)
{
	const auto& project = core().project();
	QVariantMap properties = sizeProperties(RESOURCE_MANAGER_SIZE_KEY, QSize(860, 540), over);
	properties.insert(QStringLiteral("gamePath"), project ? project->projectDir().absolutePath() : QString());
	properties.insert(QStringLiteral("rtpPath"), core().rtpPath(QString()));
	QmlHost::exec(QStringLiteral("ResourceManagerWindow"), over, properties, [](QQuickWindow* window) {
		saveSize(window, RESOURCE_MANAGER_SIZE_KEY);
	});
}

QQuickWindow* ResourceWindows::jukebox(QWindow* over)
{
	// one library for the window of each opening: the list shown, the line
	// selected and the settings stay
	if (!jukebox_library) {
		jukebox_library = new AudioLibraryModel(QCoreApplication::instance());
	}
	useProject(jukebox_library);
	if (jukebox_window && jukebox_window->isVisible()) {
		jukebox_window->raise();
		jukebox_window->requestActivate();
		return jukebox_window;
	}
	QVariantMap properties = sizeProperties(JUKEBOX_SIZE_KEY, QSize(820, 540), over);
	properties.insert(QStringLiteral("library"), QVariant::fromValue(jukebox_library.data()));
	jukebox_window = QmlHost::open(QStringLiteral("JukeboxWindow"), over, properties);
	if (jukebox_window) {
		QObject::connect(jukebox_window, &QWindow::visibleChanged, jukebox_window, [window = jukebox_window.data()](bool visible) {
			// closed: silence
			if (!visible) {
				if (jukebox_library) {
					jukebox_library->stop();
				}
				saveSize(window, JUKEBOX_SIZE_KEY);
			}
		});
	}
	return jukebox_window;
}

void ResourceWindows::closeJukebox()
{
	if (jukebox_window) {
		jukebox_window->close();
	}
}

QImage ResourceWindows::chooseTransparent(QWindow* over, const QImage& image, const QString& title)
{
	TransparentColorChoice choice;
	choice.setImage(image);
	QVariantMap properties{{QStringLiteral("choice"), QVariant::fromValue(&choice)}};
	if (!title.isEmpty()) {
		properties.insert(QStringLiteral("title"), title);
	}
	return QmlHost::exec(QStringLiteral("ImportImageWindow"), over, properties) == 1 ? choice.result() : QImage();
}

QQuickWindow* ResourceWindows::openImportImage(QWindow* over, const QString& path)
{
	auto* choice = new TransparentColorChoice();
	choice->load(path);
	QQuickWindow* window = QmlHost::open(QStringLiteral("ImportImageWindow"), over,
		{{QStringLiteral("choice"), QVariant::fromValue(choice)}});
	if (window) {
		choice->setParent(window);
	} else {
		delete choice;
	}
	return window;
}
