#!/bin/bash
# Builds the EasyRPG Editor on macOS (Apple silicon or Intel), from nothing
# but Xcode's command line tools and Homebrew:
#
#   builds/macos/build.sh            # the Editor
#   builds/macos/build.sh --tests    # also the start-up test (and its game)
#
# Homebrew gives Qt 6 and the libraries. The Editor is built in BUILD_DIR,
# with liblcf cloned into lib/liblcf and built alongside. The libraries of its
# QML interface, KDE's Kirigami with its Breeze style and KDDockWidgets, are
# built once per Qt into DEPS_DIR/kde (they use Qt's private headers).
#
# Homebrew builds for the macOS it runs on: an Editor made with its Qt runs on
# that macOS and later only. With QT_DIR, an official Qt (from qt.io or aqt),
# ICU and inih are built once into DEPS_DIR, and they and the Editor are for
# MACOSX_DEPLOYMENT_TARGET and later, for both kinds of Mac.
#
#   DEPS_DIR   where Kirigami, Breeze and KDDockWidgets go, and with QT_DIR
#              ICU, inih and glaze (default: ../editor-deps, next to the repository)
#   BUILD_DIR  where the Editor is built     (default: build)
#   JOBS       parallel jobs                 (default: all cores)
#   QT_DIR     an official Qt, e.g. ~/Qt/6.11.2/macos (default: Homebrew's)
#   MACOSX_DEPLOYMENT_TARGET  with QT_DIR: the oldest macOS (default: 13.3)
#   ARCHS      with QT_DIR: the kinds of Mac of the Editor (default: this
#              Mac's; "arm64;x86_64" for both). ICU and inih are for both
#   EDITOR_TEST_PROJECT  the game of the start-up test (default with --tests: a
#              copy of TestGame-2003 downloaded into BUILD_DIR)
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
DEPS_DIR="${DEPS_DIR:-$(dirname "$REPO")/editor-deps}"
BUILD_DIR="${BUILD_DIR:-$REPO/build}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
TESTS=OFF
[ "${1:-}" = "--tests" ] && TESTS=ON

# The versions the Editor is known to build with
ICU_VERSION=78.3
INIH=r62
GLAZE=v9.0.0
# as upstream (builds/vcpkg_overlay)
KDDOCKWIDGETS=v2.4.0
KIRIGAMI=v6.23.0
BREEZE=v6.6.1

step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }

step "Tools"
xcode-select -p >/dev/null 2>&1 || { echo "Install Xcode's command line tools first: xcode-select --install"; exit 1; }
command -v brew >/dev/null || { echo "Install Homebrew first: https://brew.sh"; exit 1; }
BREW="$(brew --prefix)"
QT_DIR="${QT_DIR:-}"
# KDE's CMake modules: also for an official Qt (CMake files only)
formulas=(cmake ninja extra-cmake-modules)
if [ -z "$QT_DIR" ]; then
	formulas+=(qtbase qtdeclarative qtshadertools qtmultimedia qtsvg qttools icu4c@78 inih expat glaze)
fi
# only what is missing: the installed ones are not upgraded
missing=()
for formula in "${formulas[@]}"; do
	brew list --versions "$formula" >/dev/null || missing+=("$formula")
done
if [ ${#missing[@]} -gt 0 ]; then
	brew install "${missing[@]}"
else
	echo "Homebrew packages: all installed"
fi

portable=()
if [ -n "$QT_DIR" ]; then
	[ -x "$QT_DIR/bin/qmake" ] || { echo "No Qt in QT_DIR=$QT_DIR"; exit 1; }
	TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.3}"
	ARCHS="${ARCHS:-$(uname -m)}"
	ICU="$DEPS_DIR/install"
	# nothing of Homebrew: its libraries are for this macOS only
	PREFIX_PATH="$QT_DIR;$DEPS_DIR/install"
	portable=(-DCMAKE_OSX_DEPLOYMENT_TARGET="$TARGET" -DCMAKE_IGNORE_PREFIX_PATH="$BREW")
	echo "Qt: $QT_DIR, for macOS $TARGET and later"
else
	ICU="$(brew --prefix icu4c@78)"
	PREFIX_PATH="$ICU;$BREW"
fi

# download one library's sources into DEPS_DIR/src/NAME
download() {
	local name="$1" url="$2"
	mkdir -p "$DEPS_DIR/src"
	rm -rf "${DEPS_DIR:?}/src/$name" && mkdir -p "$DEPS_DIR/src/$name"
	curl -fsSL "$url" | tar -xz -C "$DEPS_DIR/src/$name" --strip-components 1
}

# ICU and inih: for both kinds of Mac, whatever the Editor is built for
if [ -n "$QT_DIR" ]; then
	flags="-arch arm64 -arch x86_64 -mmacosx-version-min=$TARGET"
	if [ -e "$DEPS_DIR/install/lib/libicuuc.a" ]; then
		echo "ICU: already installed"
	else
		step "ICU"
		download icu "https://github.com/unicode-org/icu/releases/download/release-$ICU_VERSION/icu4c-$ICU_VERSION-sources.tgz"
		# static, with its data inside: liblcf converts the encodings of games
		(cd "$DEPS_DIR/src/icu/source" \
			&& CFLAGS="$flags" CXXFLAGS="$flags" LDFLAGS="$flags" CPP="clang -E" CXXCPP="clang++ -E" \
				./runConfigureICU MacOSX --prefix="$DEPS_DIR/install" --enable-static --disable-shared \
				--disable-tests --disable-samples --disable-extras --with-data-packaging=static \
			&& make -j "$JOBS" && make install)
	fi
	if [ -e "$DEPS_DIR/install/share/glaze/glazeConfig.cmake" ]; then
		echo "glaze: already installed"
	else
		step "glaze"
		download glaze "https://github.com/stephenberry/glaze/archive/refs/tags/$GLAZE.tar.gz"
		# headers only
		cmake -S "$DEPS_DIR/src/glaze" -B "$DEPS_DIR/src/glaze/build" -G Ninja \
			-DCMAKE_INSTALL_PREFIX="$DEPS_DIR/install" -Dglaze_DEVELOPER_MODE=OFF -Dglaze_BUILD_EXAMPLES=OFF
		cmake --install "$DEPS_DIR/src/glaze/build"
	fi
	if [ -e "$DEPS_DIR/install/lib/libinih.a" ]; then
		echo "inih: already installed"
	else
		step "inih"
		download inih "https://github.com/benhoyt/inih/archive/refs/tags/$INIH.tar.gz"
		mkdir -p "$DEPS_DIR/install/include" "$DEPS_DIR/install/lib"
		# shellcheck disable=SC2086
		clang $flags -O2 -c "$DEPS_DIR/src/inih/ini.c" -o "$DEPS_DIR/src/inih/ini.o"
		ar rcs "$DEPS_DIR/install/lib/libinih.a" "$DEPS_DIR/src/inih/ini.o"
		cp "$DEPS_DIR/src/inih/ini.h" "$DEPS_DIR/install/include"
	fi
fi

# The QML interface: KDE's Kirigami and its Breeze style, and the docks of
# KDDockWidgets. They use Qt's private headers: built once per Qt and kinds of
# Mac, into a folder of their own
QMAKE="${QT_DIR:-$BREW}/bin/qmake"
QT_VERSION="$("$QMAKE" -query QT_VERSION)"
if [ -n "$QT_DIR" ]; then
	KDE_DIR="$DEPS_DIR/kde/qt$QT_VERSION-${ARCHS//;/+}-macos$TARGET"
else
	KDE_DIR="$DEPS_DIR/kde/qt$QT_VERSION-homebrew"
fi
ECM_DIR="$(brew --prefix extra-cmake-modules)/share/ECM/cmake"
# the patches of Breeze too: a changed one builds them again
breeze_patches="$REPO/builds/vcpkg_overlay/qqc2-breeze-style"
kde_stamp="$KDDOCKWIDGETS $KIRIGAMI $BREEZE $(cat "$breeze_patches"/*.patch | shasum | cut -c1-12) $("$QMAKE" -query QT_INSTALL_PREFIX)"
if [ "$(cat "$KDE_DIR/stamp" 2>/dev/null)" = "$kde_stamp" ]; then
	echo "Kirigami, Breeze, KDDockWidgets: already built for Qt $QT_VERSION"
else
	rm -rf "$KDE_DIR"
	kde_build() {
		local name="$1"
		shift
		local arch=()
		[ -n "$QT_DIR" ] && arch=(-DCMAKE_OSX_ARCHITECTURES="$ARCHS")
		cmake -S "$DEPS_DIR/src/$name" -B "$DEPS_DIR/src/$name/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_INSTALL_PREFIX="$KDE_DIR" -DCMAKE_PREFIX_PATH="$PREFIX_PATH;$KDE_DIR" -DECM_DIR="$ECM_DIR" \
			-DBUILD_TESTING=OFF ${portable[@]+"${portable[@]}"} ${arch[@]+"${arch[@]}"} "$@"
		cmake --build "$DEPS_DIR/src/$name/build" -j "$JOBS"
		cmake --install "$DEPS_DIR/src/$name/build"
	}
	step "KDDockWidgets"
	download "kddockwidgets-$KDDOCKWIDGETS" "https://github.com/KDAB/KDDockWidgets/archive/refs/tags/$KDDOCKWIDGETS.tar.gz"
	# Qt Quick only; KDBindings and nlohmann_json come with it
	kde_build "kddockwidgets-$KDDOCKWIDGETS" -DKDDockWidgets_QT6=ON -DKDDockWidgets_FRONTENDS=qtquick \
		-DKDDockWidgets_TESTS=OFF -DKDDockWidgets_EXAMPLES=OFF -DKDDockWidgets_PYTHON_BINDINGS=OFF \
		-DCMAKE_DISABLE_FIND_PACKAGE_spdlog=ON -DCMAKE_DISABLE_FIND_PACKAGE_fmt=ON
	step "Kirigami"
	download "kirigami-$KIRIGAMI" "https://github.com/KDE/kirigami/archive/refs/tags/$KIRIGAMI.tar.gz"
	# without its qmllint settings, as builds/vcpkg_overlay/kirigami
	sed -i '' 's/^configure_file(qmllint.ini.in/#&/' "$DEPS_DIR/src/kirigami-$KIRIGAMI/CMakeLists.txt"
	kde_build "kirigami-$KIRIGAMI" -DBUILD_EXAMPLES=OFF -DBUILD_QCH=OFF
	step "Breeze style"
	download "qqc2-breeze-style-$BREEZE" "https://github.com/KDE/qqc2-breeze-style/archive/refs/tags/$BREEZE.tar.gz"
	# without the Plasma desktop integration, as builds/vcpkg_overlay/qqc2-breeze-style
	patch -d "$DEPS_DIR/src/qqc2-breeze-style-$BREEZE" -p1 < "$breeze_patches/no-plasma-integration.patch"
	# the icon and text of its buttons deleted with them (they leaked)
	patch -d "$DEPS_DIR/src/qqc2-breeze-style-$BREEZE" -p1 < "$breeze_patches/icon-label-items-deleted.patch"
	kde_build "qqc2-breeze-style-$BREEZE"
	echo "$kde_stamp" > "$KDE_DIR/stamp"
fi
PREFIX_PATH="$PREFIX_PATH;$KDE_DIR"

options=(-DECM_DIR="$ECM_DIR")
if [ -n "$QT_DIR" ]; then
	options+=("${portable[@]}" -DCMAKE_OSX_ARCHITECTURES="$ARCHS" -DICU_ROOT="$ICU")
else
	options+=(-DICU_INCLUDE_DIR="$ICU/include")
fi
# a second build compiles only what changed since any build before
if command -v ccache >/dev/null; then
	options+=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi
if [ "$TESTS" = ON ]; then
	# the game the start-up test opens: a copy of TestGame-2003, made once. The
	# one of an earlier build is kept while its folder exists
	if [ -z "${EDITOR_TEST_PROJECT:-}" ]; then
		cached="$(sed -n 's/^EDITOR_TEST_PROJECT:PATH=//p' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null || true)"
		if [ -n "$cached" ] && [ -d "$cached" ]; then
			EDITOR_TEST_PROJECT="$cached"
		else
			EDITOR_TEST_PROJECT="$BUILD_DIR/testgame2003"
		fi
	fi
	"$REPO/builds/testgame.sh" "$EDITOR_TEST_PROJECT"
	options+=(-DEDITOR_TEST_PROJECT="$EDITOR_TEST_PROJECT")
fi

step "EasyRPG Editor"
cmake -S "$REPO" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DEDITOR_BUILD_LIBLCF=ON -DCMAKE_PREFIX_PATH="$PREFIX_PATH" "${options[@]}"
cmake --build "$BUILD_DIR" -j "$JOBS"

APP="$BUILD_DIR/EasyRPG Editor.app"
step "Done"
echo "The Editor: $APP"
echo "Open it:    open \"$APP\""
[ "$TESTS" = ON ] && echo "Tests:      ctest --test-dir \"$BUILD_DIR\""
exit 0
