# The Editor is a Qt Quick application without Qt Widgets: fails for each
# include of a Qt Widgets header in the sources (DIRS, separated by "|")
# and each use of the Qt6::Widgets target in the CMake files (CMAKE_FILES).
# QAction, QActionGroup, QShortcut, QUndoStack and QFileSystemModel are
# Qt GUI's in Qt 6.
#
#   cmake -DDIRS="src|tests" -DCMAKE_FILES="CMakeLists.txt" -P check_no_widgets.cmake
string(REPLACE "|" ";" dirs "${DIRS}")
string(REPLACE "|" ";" cmake_files "${CMAKE_FILES}")
# the classes of Qt Widgets a file could include by their own header
set(widgets
	QAbstractButton QAbstractItemDelegate QAbstractItemView QAbstractScrollArea
	QAbstractSlider QAbstractSpinBox QApplication QBoxLayout QButtonGroup
	QCheckBox QColorDialog QColumnView QComboBox QCommandLinkButton QCompleter
	QDataWidgetMapper QDateEdit QDateTimeEdit QDial QDialog QDialogButtonBox
	QDockWidget QDoubleSpinBox QErrorMessage QFileDialog QFileIconProvider
	QFocusFrame QFontComboBox QFontDialog QFormLayout QFrame QGesture
	QGraphicsEffect QGraphicsItem QGraphicsLayout QGraphicsProxyWidget
	QGraphicsScene QGraphicsSceneEvent QGraphicsView QGraphicsWidget
	QGridLayout QGroupBox QHBoxLayout QHeaderView QInputDialog QItemDelegate
	QKeySequenceEdit QLabel QLayout QLayoutItem QLCDNumber QLineEdit
	QListView QListWidget QMainWindow QMdiArea QMdiSubWindow QMenu QMenuBar
	QMessageBox QPlainTextEdit QProgressBar QProgressDialog QPushButton
	QRadioButton QRubberBand QScrollArea QScrollBar QScroller QSizeGrip
	QSizePolicy QSlider QSpinBox QSplashScreen QSplitter QStackedLayout
	QStackedWidget QStatusBar QStyle QStyleFactory QStyleOption QStylePainter
	QStyledItemDelegate QSystemTrayIcon QTabBar QTableView QTableWidget
	QTabWidget QTextBrowser QTextEdit QTimeEdit QToolBar QToolBox QToolButton
	QToolTip QTreeView QTreeWidget QTreeWidgetItemIterator QUndoView
	QVBoxLayout QWhatsThis QWidget QWidgetAction QWizard QWizardPage
	QProxyStyle QCommonStyle)
list(JOIN widgets "|" alternatives)
set(found 0)
foreach(dir IN LISTS dirs)
	file(GLOB_RECURSE sources "${dir}/*.h" "${dir}/*.hpp" "${dir}/*.cpp" "${dir}/*.mm")
	foreach(file IN LISTS sources)
		file(STRINGS "${file}" lines REGEX "#[ \t]*include")
		foreach(line IN LISTS lines)
			if(line MATCHES "#[ \t]*include[ \t]*[<\"](QtWidgets(/[^>\"]*)?|(${alternatives}))[>\"]")
				message(WARNING "${file}: a Qt Widgets header: ${line}")
				math(EXPR found "${found} + 1")
			endif()
		endforeach()
	endforeach()
endforeach()
foreach(file IN LISTS cmake_files)
	# the target, or the component of find_package(Qt6 COMPONENTS ...)
	file(STRINGS "${file}" lines REGEX "Qt6?::Widgets|(^|[ \t])Widgets([ \t)]|$)")
	foreach(line IN LISTS lines)
		# not in a comment
		if(NOT line MATCHES "^[ \t]*#")
			message(WARNING "${file}: Qt Widgets: ${line}")
			math(EXPR found "${found} + 1")
		endif()
	endforeach()
endforeach()
if(found GREATER 0)
	message(FATAL_ERROR "${found} use(s) of Qt Widgets")
endif()
message(STATUS "no Qt Widgets")
