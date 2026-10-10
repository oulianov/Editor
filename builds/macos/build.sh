#!/bin/bash
# Builds the EasyRPG Editor on macOS (Apple silicon or Intel), from nothing
# but Xcode's command line tools and Homebrew:
#
#   builds/macos/build.sh            # the Editor
#   builds/macos/build.sh --tests    # also the script runner and the tests
#
# Homebrew gives Qt 6 and the libraries. The Editor is built in BUILD_DIR,
# with liblcf cloned into lib/liblcf and built alongside.
#
# Homebrew builds for the macOS it runs on: an Editor made with its Qt runs on
# that macOS and later only. With QT_DIR, an official Qt (from qt.io or aqt),
# ICU and inih are built once into DEPS_DIR, and they and the Editor are for
# MACOSX_DEPLOYMENT_TARGET and later, for both kinds of Mac.
#
#   DEPS_DIR   with QT_DIR: where ICU and inih go (default: ../editor-deps, next to the repository)
#   BUILD_DIR  where the Editor is built     (default: build)
#   JOBS       parallel jobs                 (default: all cores)
#   QT_DIR     an official Qt, e.g. ~/Qt/6.10.1/macos (default: Homebrew's)
#   MACOSX_DEPLOYMENT_TARGET  with QT_DIR: the oldest macOS (default: 13.3)
#   ARCHS      with QT_DIR: the kinds of Mac of the Editor (default: this
#              Mac's; "arm64;x86_64" for both). ICU and inih are for both
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
ICU_VERSION=78.3
INIH=r62

step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }

step "Tools"
xcode-select -p >/dev/null 2>&1 || { echo "Install Xcode's command line tools first: xcode-select --install"; exit 1; }
command -v brew >/dev/null || { echo "Install Homebrew first: https://brew.sh"; exit 1; }
BREW="$(brew --prefix)"
QT_DIR="${QT_DIR:-}"
formulas=(cmake ninja)
if [ -z "$QT_DIR" ]; then
	formulas+=(qtbase qtmultimedia qtsvg qttools icu4c@78 inih expat)
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

options=()
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
	-DEDITOR_BUILD_LIBLCF=ON -DCMAKE_PREFIX_PATH="$PREFIX_PATH" "${options[@]}"
cmake --build "$BUILD_DIR" -j "$JOBS"

APP="$BUILD_DIR/EasyRPG Editor.app"
step "Done"
echo "The Editor: $APP"
echo "Open it:    open \"$APP\""
[ "$TESTS" = ON ] && echo "Tests:      ctest --test-dir \"$BUILD_DIR\""
exit 0
