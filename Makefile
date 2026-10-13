# EasyRPG Editor: `make` builds it for this computer, with what it needs.
#
#   make          build (the first time, also install or build what is missing)
#   make run      open the Editor
#   make test     build with the tests, then run them
#   make clean    remove the build
#
#   QT_DIR=...    an official Qt 6 (qt.io or aqt), e.g. ~/Qt/6.10.1/macos
#                 (macOS: instead of Homebrew's; Linux: required unless
#                 CMake finds a Qt 6.10 itself)
#
# See docs/Building.md for what each platform needs and the options.

BUILD_DIR ?= build
OS := $(shell uname -s)

ifeq ($(OS),Darwin)
APP := $(BUILD_DIR)/EasyRPG Editor.app
TEST_DIR := $(BUILD_DIR)
else
# Linux: the vcpkg preset of upstream; vcpkg builds zlib, inih and expat
PRESET := vcpkg-liblcf-release
QT_PREFIX := $(if $(QT_DIR),-DCMAKE_PREFIX_PATH="$(QT_DIR)")
TEST_DIR := build/$(PRESET)
# the game the GUI tests change: a copy of TestGame-2003, downloaded once
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
