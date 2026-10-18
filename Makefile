# EasyRPG Editor: `make` builds it for this computer, with what it needs.
#
#   make          build (the first time, also install or build what is missing)
#   make run      open the Editor
#   make test     build with the tests, then run them
#   make clean    remove the build
#
#   QT_DIR=...    an official Qt 6 (qt.io or aqt), e.g. ~/Qt/6.11.2/macos
#                 (macOS: instead of Homebrew's; Linux: required unless
#                 CMake finds a Qt 6.10 or later itself)
#
# See docs/Building.md for what each platform needs and the options.

BUILD_DIR ?= build
OS := $(shell uname -s)

ifeq ($(OS),Darwin)
APP := $(BUILD_DIR)/EasyRPG Editor.app
TEST_DIR := $(BUILD_DIR)
else
# Linux: the vcpkg preset of upstream; vcpkg builds zlib, inih, expat, glaze,
# Kirigami, Breeze and KDDockWidgets
PRESET := vcpkg-liblcf-release
QT_PREFIX := $(if $(QT_DIR),-DCMAKE_PREFIX_PATH="$(QT_DIR)")
# liblcf takes the system's ICU, whose headers are found: its libraries too,
# not the other version Qt carries (searched first under CMAKE_PREFIX_PATH)
ICU_LIB := /usr/lib/$(shell gcc -dumpmachine 2>/dev/null)
QT_PREFIX += $(if $(QT_DIR),-DICU_UC_LIBRARY_RELEASE=$(ICU_LIB)/libicuuc.so \
	-DICU_I18N_LIBRARY_RELEASE=$(ICU_LIB)/libicui18n.so \
	-DICU_DATA_LIBRARY_RELEASE=$(ICU_LIB)/libicudata.so)
# the KDE ports of vcpkg (builds/vcpkg_overlay) build against this Qt too
ifneq ($(QT_DIR),)
export Qt6_Path := $(abspath $(QT_DIR))
endif
TEST_DIR := build/$(PRESET)
# the game the start-up test opens: a copy of TestGame-2003, downloaded once
EDITOR_TEST_PROJECT ?= $(TEST_DIR)/testgame2003
endif

.PHONY: all run test clean help

all:
ifeq ($(OS),Darwin)
	@BUILD_DIR="$(BUILD_DIR)" QT_DIR="$(QT_DIR)" builds/macos/build.sh
else ifeq ($(OS),Linux)
	@test -n "$$VCPKG_ROOT" || { echo "Set VCPKG_ROOT to a vcpkg checkout (https://github.com/microsoft/vcpkg): see docs/Building.md"; exit 1; }
	cmake --preset $(PRESET) $(QT_PREFIX)
	cmake --build --preset $(PRESET)
else
	@echo "$(OS): see docs/Building.md (Windows: the windows-* CMake presets)"; exit 1
endif

run: all
ifeq ($(OS),Darwin)
	open "$(APP)"
else
	@echo "The Editor is in $(TEST_DIR)"
endif

test:
ifeq ($(OS),Darwin)
	@BUILD_DIR="$(BUILD_DIR)" QT_DIR="$(QT_DIR)" builds/macos/build.sh --tests
else
	@test -n "$$VCPKG_ROOT" || { echo "Set VCPKG_ROOT to a vcpkg checkout (https://github.com/microsoft/vcpkg): see docs/Building.md"; exit 1; }
	builds/testgame.sh "$(EDITOR_TEST_PROJECT)"
	cmake --preset $(PRESET) $(QT_PREFIX) -DEDITOR_TEST_PROJECT="$(abspath $(EDITOR_TEST_PROJECT))"
	cmake --build --preset $(PRESET)
endif
	ctest --test-dir "$(TEST_DIR)" -j 8 --output-on-failure

clean:
	rm -rf "$(BUILD_DIR)"

help:
	@sed -n "1,12p" Makefile
