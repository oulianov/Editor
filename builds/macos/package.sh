#!/bin/bash
# Makes the EasyRPG Editor built by build.sh an application that runs on
# other Macs, in a disk image:
#
#   builds/macos/package.sh
#
# 1. copies the application into BUILD_DIR/dist and puts in it what it uses
#    from Qt and Homebrew (Qt, its plugins, the libraries), with macdeployqt,
#    and the QML modules of its interface (Kirigami, Breeze, KDDockWidgets),
#    without what the Editor never loads (the other styles of Qt Quick
#    Controls, the QML modules nothing imports, the libraries only they use);
# 2. makes an application per kind of Mac it was built for (ARCH):
#    BUILD_DIR/dist/ARCH/EasyRPG Editor.app, its programs and libraries
#    thinned to that kind (a universal build is twice as big);
# 3. signs each: with SIGN_IDENTITY (a Developer ID Application certificate
#    of the keychain), else for this computer only (ad hoc);
# 4. notarizes it when Apple credentials are given: NOTARY_PROFILE (saved by
#    xcrun notarytool store-credentials), or APPLE_ID, APPLE_TEAM_ID and
#    APPLE_APP_SPECIFIC_PASSWORD;
# 5. puts it in BUILD_DIR/dist/EasyRPG-Editor-VERSION-macOS-ARCH.dmg
#    (arm64: Apple silicon, x86_64: Intel), signed and notarized the same
#    way. The Editor's updater picks the disk image of its kind of Mac.
#
#   BUILD_DIR   where the Editor was built     (default: build)
#   QT_DIR      the official Qt build.sh used, if any
#   MACOSX_DEPLOYMENT_TARGET  with QT_DIR: the oldest macOS it must run on
#               (default: 13.3); a part needing a newer one is an error
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO/build}"
SIGN_IDENTITY="${SIGN_IDENTITY:--}"
NAME="EasyRPG Editor"

step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }

[ -d "$BUILD_DIR/$NAME.app" ] || { echo "No $BUILD_DIR/$NAME.app: run builds/macos/build.sh first"; exit 1; }
VERSION="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$BUILD_DIR/CMakeCache.txt")"

notarize=false
if [ -n "${NOTARY_PROFILE:-}" ]; then
	notary_auth=(--keychain-profile "$NOTARY_PROFILE")
	notarize=true
elif [ -n "${APPLE_ID:-}" ] && [ -n "${APPLE_TEAM_ID:-}" ] && [ -n "${APPLE_APP_SPECIFIC_PASSWORD:-}" ]; then
	notary_auth=(--apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD")
	notarize=true
fi
if [ "$notarize" = true ] && [ "$SIGN_IDENTITY" = - ]; then
	echo "Notarizing needs a Developer ID certificate: set SIGN_IDENTITY"
	exit 1
fi

DIST="$BUILD_DIR/dist"
# the application of every kind of Mac, before it is split
APP="$DIST/all/$NAME.app"

step "Application"
rm -rf "$DIST"
mkdir -p "$DIST/all"
ditto "$BUILD_DIR/$NAME.app" "$APP"
# Kirigami and Breeze where build.sh built them: their QML modules, and the
# Kirigami platform plugin of Breeze, which macdeployqt does not know
KDE_LIB="$(sed -n 's/^KF6Kirigami_DIR:PATH=//p' "$BUILD_DIR/CMakeCache.txt")/../.."
KDE_LIB="$(cd "$KDE_LIB" && pwd)"
platform="$APP/Contents/PlugIns/kf6/kirigami/platform"
mkdir -p "$platform"
cp "$KDE_LIB"/plugins/kf6/kirigami/platform/*.so "$platform/"
deploy=(-qmldir="$REPO/src" -qmlimport="$KDE_LIB/qml")
# the offscreen platform beside cocoa (macdeployqt copies cocoa only): the
# packaged Editor runs scripts without windows (QT_QPA_PLATFORM=offscreen)
mkdir -p "$APP/Contents/PlugIns/platforms"
cp "$("${QT_DIR:+$QT_DIR/bin/}qmake" -query QT_INSTALL_PLUGINS)/platforms/libqoffscreen.dylib" "$APP/Contents/PlugIns/platforms/"
deploy+=(-executable="$APP/Contents/PlugIns/platforms/libqoffscreen.dylib")
# the Breeze style is chosen in C++ (QQuickStyle): no QML file imports it
mkdir -p "$APP/Contents/Resources/qml/org/kde"
cp -R "$KDE_LIB/qml/org/kde/breeze" "$APP/Contents/Resources/qml/org/kde/"
# the libraries macdeployqt did not find itself: pointed at the Qt inside
while IFS= read -r -d '' library; do
	deploy+=(-executable="$library")
done < <(find "$platform" "$APP/Contents/Resources/qml/org/kde/breeze" \( -name '*.so' -o -name '*.dylib' \) -print0)
"${QT_DIR:+$QT_DIR/bin/}macdeployqt" "$APP" "${deploy[@]}"
# the style of Qt Widgets macdeployqt always adds, and Qt Widgets with it:
# the Editor has no widgets (Qt Quick only)
rm -rf "$APP/Contents/PlugIns/styles" "$APP/Contents/Frameworks/QtWidgets.framework"
# the database drivers, there for QtQuick.LocalStorage (macdeployqt copies
# every module under QtQuick): the Editor uses no database, and the drivers
# of the official Qt need client libraries the application does not carry
rm -rf "$APP/Contents/PlugIns/sqldrivers"
# the QML modules nothing of the Editor imports (macdeployqt copies every
# module of QtQuick): the styles of Qt Quick Controls but Basic (that Breeze
# builds on) and Fusion (the fallback style), the native style they use,
# and modules for other uses (particles, timelines, vector images, Qt
# Design Studio, databases...); with their plugins
qml="$APP/Contents/Resources/qml"
for module in QtQuick/Controls/{Material,Imagine,Universal,FluentWinUI3,iOS,macOS,designer} \
		QtQuick/NativeStyle QtQuick/Particles QtQuick/Timeline QtQuick/VectorImage QtQuick/LocalStorage \
		QtQuick/Shapes/DesignHelpers QtQuick/tooling QtQml/XmlListModel Qt/labs/folderlistmodel; do
	[ -d "$qml/$module" ] || continue
	while IFS= read -r -d '' plugin; do
		rm -f "$(dirname "$plugin")/$(readlink "$plugin")"
	done < <(find "$qml/$module" -type l -name '*.dylib' -print0)
	rm -rf "$qml/$module"
done
# then the libraries no program, library or plugin left uses (the
# frameworks of those modules, what only they use...), by the names of
# their files: QtSql.framework/Versions/A/QtSql, libdbus-1.3.dylib
uses() {
	local file id
	while IFS= read -r -d '' file; do
		file -b "$file" | grep -q '^Mach-O' || continue
		# not itself: a library's own name is among its libraries
		id="$(otool -D "$file" | awk '!/:$/ {print; exit}')"
		otool -L "$file" | awk -v id="$id" '/^\t/ && $1 != id {print $1}'
	done < <(find "$APP" -type f -print0)
}
while true; do
	used="$(uses | sed 's#.*/##' | sort -u)"
	removed=0
	for library in "$APP/Contents/Frameworks/"*; do
		base="$(basename "$library")"
		if ! grep -qxF "${base%.framework}" <<< "$used"; then
			echo "Unused: $base"
			rm -rf "$library"
			removed=1
		fi
	done
	[ "$removed" = 1 ] || break
done

# every program and library of the application
machos=()
while IFS= read -r -d '' file; do
	if file -b "$file" | grep -q '^Mach-O'; then
		machos+=("$file")
	fi
done < <(find "$APP" -type f -print0)

# nothing outside the application but the system
outside=0
for file in "${machos[@]}"; do
	# a universal file has a "FILE (architecture X):" line per kind of Mac:
	# the library's own name and its libraries are the other lines
	id="$(otool -D "$file" | awk '!/:$/ {print; exit}')"
	while IFS= read -r lib; do
		case "$lib" in
			/System/*|/usr/lib/*|@*|"$id") ;;
			*) echo "$file uses $lib"; outside=1 ;;
		esac
	done < <(otool -L "$file" | awk '/^\t/ {sub(/^\t/, ""); sub(/ \(compatibility version .*/, ""); print}' | sort -u)
done
[ "$outside" = 0 ] || { echo "The application uses libraries outside it"; exit 1; }
# nor Qt Widgets
widgets=0
for file in "${machos[@]}"; do
	if otool -L "$file" | grep -q 'QtWidgets'; then
		echo "$file uses Qt Widgets"
		widgets=1
	fi
done
[ "$widgets" = 0 ] || { echo "The application uses Qt Widgets"; exit 1; }

# the newest of versions, and the macOS a program or library needs
newest() { sort -t. -k1,1n -k2,2n -k3,3n | tail -1; }
minos() { vtool -show-build "$1" | awk '/minos/{print $2}' | newest; }

archs="$(lipo -archs "$APP/Contents/MacOS/$NAME")"

notarize() {
	local file="$1"
	local result
	result="$(xcrun notarytool submit "$file" "${notary_auth[@]}" --wait --output-format plist)"
	local id status
	id="$(/usr/libexec/PlistBuddy -c "Print :id" /dev/stdin <<< "$result")"
	status="$(/usr/libexec/PlistBuddy -c "Print :status" /dev/stdin <<< "$result")"
	echo "Notarization $id: $status"
	if [ "$status" != Accepted ]; then
		xcrun notarytool log "$id" "${notary_auth[@]}"
		exit 1
	fi
}

# The application of one kind of Mac, signed, notarized, in its disk image
package() {
	local arch="$1"
	local app="$DIST/$arch/$NAME.app"
	local dmg="$DIST/EasyRPG-Editor-$VERSION-macOS-$arch.dmg"
	step "Application for $arch"
	mkdir -p "$DIST/$arch"
	# the programs and libraries thinned to that kind of Mac
	ditto --arch "$arch" "$APP" "$app"
	local file
	local files=()
	while IFS= read -r -d '' file; do
		if file -b "$file" | grep -q '^Mach-O'; then
			files+=("$file")
			[ "$(lipo -archs "$file")" = "$arch" ] || { echo "$file is not for $arch only: $(lipo -archs "$file")"; exit 1; }
		fi
	done < <(find "$app" -type f -print0)

	# the oldest macOS it runs on: the newest any of its parts needs
	# (Homebrew builds its libraries for the macOS it runs on, the official
	# Qt for 13)
	local minimum
	minimum="$(for file in "${files[@]}"; do minos "$file"; done | newest)"
	if [ -n "${QT_DIR:-}" ]; then
		local target="${MACOSX_DEPLOYMENT_TARGET:-13.3}"
		if [ "$(printf '%s\n%s\n' "$target" "$minimum" | newest)" != "$target" ]; then
			for file in "${files[@]}"; do
				local needs
				needs="$(minos "$file")"
				[ "$(printf '%s\n%s\n' "$target" "$needs" | newest)" = "$target" ] || echo "$file needs macOS $needs"
			done
			echo "The application needs macOS $minimum, not $target"
			exit 1
		fi
	fi
	local plist="$app/Contents/Info.plist"
	/usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $minimum" "$plist"
	/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $VERSION" "$plist"
	/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $VERSION" "$plist"
	echo "Version $VERSION, for macOS $minimum and later ($arch), $(du -sh "$app" | cut -f1)"

	step "Signing for $arch ($SIGN_IDENTITY)"
	local sign=(codesign --force --sign "$SIGN_IDENTITY")
	if [ "$SIGN_IDENTITY" != - ]; then
		# the hardened runtime and a secure timestamp: notarization requires them
		sign+=(--options runtime --timestamp)
	fi
	# from the inside out: the libraries, then the frameworks, then the application
	for file in "${files[@]}"; do
		case "$file" in
			*.framework/Versions/*) ;;
			"$app/Contents/MacOS/"*) ;;
			*) "${sign[@]}" "$file" ;;
		esac
	done
	local framework
	while IFS= read -r -d '' framework; do
		"${sign[@]}" "$framework"
	done < <(find "$app/Contents/Frameworks" -maxdepth 1 -name '*.framework' -print0)
	"${sign[@]}" "$app"
	codesign --verify --deep --strict --verbose=2 "$app"

	if [ "$notarize" = true ]; then
		step "Notarizing the application for $arch"
		ditto -c -k --keepParent "$app" "$DIST/notarize.zip"
		notarize "$DIST/notarize.zip"
		rm "$DIST/notarize.zip"
		xcrun stapler staple "$app"
	fi

	step "Disk image for $arch"
	local stage="$DIST/dmg"
	mkdir -p "$stage"
	ditto "$app" "$stage/$NAME.app"
	ln -s /Applications "$stage/Applications"
	hdiutil create -quiet -volname "$NAME" -srcfolder "$stage" -format UDZO -ov "$dmg"
	rm -rf "$stage"
	sign=(codesign --force --sign "$SIGN_IDENTITY")
	[ "$SIGN_IDENTITY" = - ] || sign+=(--timestamp)
	"${sign[@]}" "$dmg"
	if [ "$notarize" = true ]; then
		step "Notarizing the disk image for $arch"
		notarize "$dmg"
		xcrun stapler staple "$dmg"
		spctl --assess --type open --context context:primary-signature --verbose=2 "$dmg"
		spctl --assess --type execute --verbose=2 "$app"
	fi
	echo "The application: $app"
	echo "The disk image:  $dmg ($(du -h "$dmg" | cut -f1))"
}

for arch in $archs; do
	package "$arch"
done
rm -rf "$DIST/all"

step "Done"
[ "$notarize" = true ] || echo "Not notarized: other Macs ask before opening it"
exit 0
