#!/bin/bash
# Builds the EasyRPG Editor on macOS (Apple silicon or Intel), from nothing
# but Xcode's command line tools and Homebrew:
#
#   builds/macos/build.sh            # dependencies (once), then the Editor
#   builds/macos/build.sh --tests    # also the script runner and the tests
#
# Homebrew gives Qt 6 and most libraries. Four are not in Homebrew; they are
# downloaded and built once into DEPS_DIR. The Editor is built in BUILD_DIR,
# with liblcf cloned into lib/liblcf and built alongside.
#
#   DEPS_DIR   where the four libraries go   (default: ../editor-deps, next to the repository)
#   BUILD_DIR  where the Editor is built     (default: build)
#   JOBS       parallel jobs                 (default: all cores)
#   EDITOR_TEST_PROJECT  the game of the GUI tests (default with --tests: a
#              copy of TestGame-2003 downloaded into BUILD_DIR)
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS_DIR="${DEPS_DIR:-$(dirname "$REPO")/editor-deps}"
BUILD_DIR="${BUILD_DIR:-$REPO/build}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
TESTS=OFF
[ "${1:-}" = "--tests" ] && TESTS=ON

# The versions the Editor is known to build with
KDBINDINGS=1.1.0
KDDOCKWIDGETS=2.4.0
KIRIGAMI=6.23.0
BREEZE_STYLE=6.6.1

step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }

step "Tools"
xcode-select -p >/dev/null 2>&1 || { echo "Install Xcode's command line tools first: xcode-select --install"; exit 1; }
command -v brew >/dev/null || { echo "Install Homebrew first: https://brew.sh"; exit 1; }
BREW="$(brew --prefix)"
# only what is missing: the installed ones are not upgraded
missing=()
for formula in cmake ninja qtbase qtdeclarative qtmultimedia qtsvg qttools qtshadertools \
		extra-cmake-modules glaze icu4c@78 inih expat; do
	brew list --versions "$formula" >/dev/null || missing+=("$formula")
done
if [ ${#missing[@]} -gt 0 ]; then
	brew install "${missing[@]}"
else
	echo "Homebrew packages: all installed"
fi
ICU="$(brew --prefix icu4c@78)"
PREFIX_PATH="$DEPS_DIR/install;$ICU;$BREW"

# download, configure, build and install one library (skipped once installed)
dependency() {
	local name="$1" url="$2" marker="$3"; shift 3
	if [ -e "$DEPS_DIR/install/$marker" ]; then
		echo "$name: already installed"
		return
	fi
	step "$name"
	mkdir -p "$DEPS_DIR/src"
	local src="$DEPS_DIR/src/$name"
	rm -rf "$src" && mkdir -p "$src"
	curl -fsSL "$url" | tar -xz -C "$src" --strip-components 1
	if [ "$name" = qqc2-breeze-style ]; then
		# no Plasma integration: the frameworks it needs are not on macOS
		(cd "$src" && patch -p1 < "$REPO/builds/vcpkg_overlay/qqc2-breeze-style/no-plasma-integration.patch")
	fi
	cmake -S "$src" -B "$src/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$DEPS_DIR/install" -DCMAKE_PREFIX_PATH="$PREFIX_PATH" \
		-DBUILD_TESTING=OFF "$@"
	cmake --build "$src/build" -j "$JOBS"
	cmake --install "$src/build"
}

dependency KDBindings "https://github.com/KDAB/KDBindings/archive/refs/tags/v$KDBINDINGS.tar.gz" \
	lib/cmake/KDBindings
dependency KDDockWidgets "https://github.com/KDAB/KDDockWidgets/archive/refs/tags/v$KDDOCKWIDGETS.tar.gz" \
	lib/cmake/KDDockWidgets-qt6 -DKDDockWidgets_QT6=ON -DKDDockWidgets_FRONTENDS=qtquick \
	-DKDDockWidgets_EXAMPLES=OFF -DKDDockWidgets_PYTHON_BINDINGS=OFF
dependency kirigami "https://github.com/KDE/kirigami/archive/refs/tags/v$KIRIGAMI.tar.gz" \
	lib/cmake/KF6Kirigami -DBUILD_EXAMPLES=OFF
dependency qqc2-breeze-style "https://github.com/KDE/qqc2-breeze-style/archive/refs/tags/v$BREEZE_STYLE.tar.gz" \
	lib/cmake/QQC2BreezeStyle

options=()
if [ "$TESTS" = ON ]; then
	options+=(-DEDITOR_SCRIPTS=ON)
	# the game the GUI tests change: a copy of TestGame-2003, made once
	if [ -z "${EDITOR_TEST_PROJECT:-}" ] && ! grep -qs "^EDITOR_TEST_PROJECT:PATH=." "$BUILD_DIR/CMakeCache.txt"; then
		EDITOR_TEST_PROJECT="$BUILD_DIR/testgame2003"
		if [ ! -d "$EDITOR_TEST_PROJECT" ]; then
			step "TestGame-2003"
			mkdir -p "$BUILD_DIR"
			curl -fsSL https://github.com/EasyRPG/TestGame/archive/refs/heads/master.tar.gz \
				| tar -xz -C "$BUILD_DIR" --strip-components 1 TestGame-master/TestGame-2003
			mv "$BUILD_DIR/TestGame-2003" "$EDITOR_TEST_PROJECT"
		fi
	fi
	[ -n "${EDITOR_TEST_PROJECT:-}" ] && options+=(-DEDITOR_TEST_PROJECT="$EDITOR_TEST_PROJECT")
fi

step "EasyRPG Editor"
cmake -S "$REPO" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DEDITOR_BUILD_LIBLCF=ON -DCMAKE_PREFIX_PATH="$PREFIX_PATH" -DICU_INCLUDE_DIR="$ICU/include" \
	${options[@]+"${options[@]}"}
cmake --build "$BUILD_DIR" -j "$JOBS"

APP="$BUILD_DIR/EasyRPG Editor.app"
step "Done"
echo "The Editor: $APP"
echo "Open it:    open \"$APP\""
[ "$TESTS" = ON ] && echo "Tests:      ctest --test-dir \"$BUILD_DIR\""
exit 0
