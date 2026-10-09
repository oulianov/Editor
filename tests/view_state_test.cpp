#include "ui/common/view_state.h"
#include "common/editor_settings.h"
#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QEventLoop>
#include <QHeaderView>
#include <QLineEdit>
#include <QMainWindow>
#include <QSettings>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <iostream>
#include <cstdlib>

void require(bool condition, const char* message) {
	if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

void settle() {
	QEventLoop loop;
	QTimer::singleShot(100, &loop, &QEventLoop::quit);
	loop.exec();
}

int main(int argc, char** argv) {
	QApplication app(argc, argv);
	require(argc == 3, "Expected write/read and an isolated settings directory");
	app.setOrganizationName("EasyRPG Tests");
	app.setApplicationName("View State");
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QString::fromLocal8Bit(argv[2]));
	QSettings settings;
	const QString mode = QString::fromLocal8Bit(argv[1]);
	const bool write = mode == "write";
	if (write) settings.clear();

	QMainWindow window;
	window.resize(900, 650);
	window.setCentralWidget(new QWidget(&window));
	auto* palette = new QDockWidget(&window);
	palette->setObjectName("palette");
	palette->setWidget(new QWidget(palette));
	auto* maps = new QDockWidget(&window);
	maps->setObjectName("maps");
	maps->setWidget(new QWidget(maps));
	window.addDockWidget(Qt::LeftDockWidgetArea, palette);
	window.addDockWidget(Qt::LeftDockWidgetArea, maps);
	window.show();
	if (write) {
		window.resize(1100, 730);
		window.resizeDocks({palette, maps}, {370, 370}, Qt::Horizontal);
		window.resizeDocks({palette, maps}, {260, 450}, Qt::Vertical);
		settle();
		settings.setValue("main/geometry", window.saveGeometry());
		settings.setValue("main/layout", ViewState::captureLayout(&window));
		settings.setValue("main/paletteSize", palette->size());
		settings.setValue("main/mapsSize", maps->size());
	} else {
		window.restoreGeometry(settings.value("main/geometry").toByteArray());
		ViewState::restoreLayout(&window, settings.value("main/layout").toMap());
		settle();
		require(palette->size() == settings.value("main/paletteSize").toSize(), "Dock width/height changed across launches");
		require(maps->size() == settings.value("main/mapsSize").toSize(), "Map dock dimensions changed across launches");
	}

	QDialog dialog;
	dialog.setObjectName("layout-test");
	dialog.resize(700, 620);
	auto* layout = new QVBoxLayout(&dialog);
	auto* tabs = new QTabWidget(&dialog);
	layout->addWidget(tabs);
	QList<QSplitter*> splitters;
	QList<QTableWidget*> tables;
	// Identical names on separate tabs must not share a persisted width.
	for (int i = 0; i < 3; ++i) {
		auto* page = new QWidget(tabs);
		page->setObjectName("page");
		auto* pageLayout = new QVBoxLayout(page);
		auto* splitter = new QSplitter(page);
		splitter->setObjectName("splitter");
		splitter->addWidget(new QLineEdit("Current game data", splitter));
		auto* table = new QTableWidget(10, 3, splitter);
		table->setObjectName("table");
		table->horizontalHeader()->setSectionsMovable(true);
		splitter->addWidget(table);
		pageLayout->addWidget(splitter);
		tabs->addTab(page, QString::number(i));
		splitters.append(splitter);
		tables.append(table);
	}
	ViewState::rememberDialog(&dialog);
	dialog.show();
	settle();
	if (!write) require(tabs->currentIndex() == (mode == "peek" ? 2 : 0), "Dialog did not reopen its last tab");
	if (write) {
		settings.setValue("expected/defaultSplitter", QVariant::fromValue(splitters[0]->sizes()).toList());
		settings.setValue("expected/defaultColumn", tables[0]->columnWidth(0));
	}
	for (int i = 0; i < (mode == "peek" ? 1 : tabs->count()); ++i) {
		tabs->setCurrentIndex(i);
		settle();
		if (write) {
			splitters[i]->setSizes({210 + i * 120, 620 - i * 120});
			tables[i]->horizontalHeader()->resizeSection(0, 180 + i * 60);
			tables[i]->horizontalHeader()->moveSection(2, 0);
			settle();
			settings.setValue(QString("expected/splitter%1").arg(i), QVariant::fromValue(splitters[i]->sizes()).toList());
			settings.setValue(QString("expected/column%1").arg(i), tables[i]->columnWidth(0));
		} else {
			const QVariantList expected = settings.value(QString("expected/splitter%1").arg(i)).toList();
			if (QVariant::fromValue(splitters[i]->sizes()).toList() != expected)
				std::cerr << "Tab " << i << ": sizes " << splitters[i]->sizes()[0] << "," << splitters[i]->sizes()[1]
					<< " expected " << expected[0].toInt() << "," << expected[1].toInt() << "; dialog width " << dialog.width() << '\n';
			require(QVariant::fromValue(splitters[i]->sizes()).toList() == settings.value(QString("expected/splitter%1").arg(i)).toList(), "Splitter width was lost or shared between tabs");
			require(tables[i]->columnWidth(0) == settings.value(QString("expected/column%1").arg(i)).toInt(), "Table column width was lost");
			require(tables[i]->horizontalHeader()->visualIndex(2) == 0, "Table column order was lost");
		}
		require(splitters[i]->findChild<QLineEdit*>()->text() == "Current game data", "Layout restoration changed game data");
	}
	if (mode == "read") {
		EditorSettings::instance().resetLocalState();
		settle();
		require(!settings.contains("window_states/QDialog/layout-test"), "Reset retained saved layout state");
		require(tabs->currentIndex() == 0, "Reset did not select the default tab");
		require(QVariant::fromValue(splitters[0]->sizes()).toList() == settings.value("expected/defaultSplitter").toList(), "Reset did not restore the default splitter immediately");
		require(tables[0]->columnWidth(0) == settings.value("expected/defaultColumn").toInt() && tables[0]->horizontalHeader()->visualIndex(2) == 2,
			"Reset did not restore default table columns immediately");
	}
	dialog.reject();
	settings.sync();
	return 0;
}
