#!/bin/bash
# Makes the EasyRPG Editor built by build.sh an application that runs on
# other Macs, in a disk image:
#
#   builds/macos/package.sh
#
# 1. copies the application into BUILD_DIR/dist and puts in it what it uses
#    from Qt and Homebrew (Qt, its plugins, the libraries), with macdeployqt,
#    and the QML modules of its interface (Kirigami, Breeze, KDDockWidgets);
# 2. signs it: with SIGN_IDENTITY (a Developer ID Application certificate
#    of the keychain), else for this computer only (ad hoc);
# 3. notarizes it when Apple credentials are given: NOTARY_PROFILE (saved by
#    xcrun notarytool store-credentials), or APPLE_ID, APPLE_TEAM_ID and
#    APPLE_APP_SPECIFIC_PASSWORD;
# 4. puts it in BUILD_DIR/dist/EasyRPG-Editor-VERSION-macOS.dmg, signed and
#    notarized the same way.
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
APP="$DIST/$NAME.app"
DMG="$DIST/EasyRPG-Editor-$VERSION-macOS.dmg"

step "Application"
rm -rf "$DIST"
mkdir -p "$DIST"
ditto "$BUILD_DIR/$NAME.app" "$APP"
# Kirigami and Breeze where build.sh built them: their QML modules, and the
# Kirigami platform plugin of Breeze, which macdeployqt does not know
KDE_LIB="$(sed -n 's/^KF6Kirigami_DIR:PATH=//p' "$BUILD_DIR/CMakeCache.txt")/../.."
KDE_LIB="$(cd "$KDE_LIB" && pwd)"
platform="$APP/Contents/PlugIns/kf6/kirigami/platform"
mkdir -p "$platform"
cp "$KDE_LIB"/plugins/kf6/kirigami/platform/*.so "$platform/"
deploy=(-qmldir="$REPO/src" -qmlimport="$KDE_LIB/qml")
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

# the oldest macOS it runs on: the newest any of its parts needs (Homebrew
# builds its libraries for the macOS it runs on, the official Qt for 13)
newest() { sort -t. -k1,1n -k2,2n -k3,3n | tail -1; }
minos() { vtool -show-build "$1" | awk '/minos/{print $2}' | newest; }
minimum="$(for file in "${machos[@]}"; do minos "$file"; done | newest)"
if [ -n "${QT_DIR:-}" ]; then
	target="${MACOSX_DEPLOYMENT_TARGET:-13.3}"
	if [ "$(printf '%s\n%s\n' "$target" "$minimum" | newest)" != "$target" ]; then
		for file in "${machos[@]}"; do
			needs="$(minos "$file")"
			[ "$(printf '%s\n%s\n' "$target" "$needs" | newest)" = "$target" ] || echo "$file needs macOS $needs"
		done
		echo "The application needs macOS $minimum, not $target"
		exit 1
	fi
fi
plist="$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $minimum" "$plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $VERSION" "$plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $VERSION" "$plist"
echo "Version $VERSION, for macOS $minimum and later ($(lipo -archs "$APP/Contents/MacOS/$NAME"))"

step "Signing ($SIGN_IDENTITY)"
sign=(codesign --force --sign "$SIGN_IDENTITY")
if [ "$SIGN_IDENTITY" != - ]; then
	# the hardened runtime and a secure timestamp: notarization requires them
	sign+=(--options runtime --timestamp)
fi
# from the inside out: the libraries, then the frameworks, then the application
for file in "${machos[@]}"; do
	case "$file" in
		*.framework/Versions/*) ;;
		"$APP/Contents/MacOS/"*) ;;
		*) "${sign[@]}" "$file" ;;
	esac
done
while IFS= read -r -d '' framework; do
	"${sign[@]}" "$framework"
done < <(find "$APP/Contents/Frameworks" -maxdepth 1 -name '*.framework' -print0)
"${sign[@]}" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

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

if [ "$notarize" = true ]; then
	step "Notarizing the application"
	ditto -c -k --keepParent "$APP" "$DIST/notarize.zip"
	notarize "$DIST/notarize.zip"
	rm "$DIST/notarize.zip"
	xcrun stapler staple "$APP"
fi

step "Disk image"
stage="$DIST/dmg"
mkdir -p "$stage"
ditto "$APP" "$stage/$NAME.app"
ln -s /Applications "$stage/Applications"
hdiutil create -quiet -volname "$NAME" -srcfolder "$stage" -format UDZO -ov "$DMG"
rm -rf "$stage"
sign=(codesign --force --sign "$SIGN_IDENTITY")
[ "$SIGN_IDENTITY" = - ] || sign+=(--timestamp)
"${sign[@]}" "$DMG"
if [ "$notarize" = true ]; then
	step "Notarizing the disk image"
	notarize "$DMG"
	xcrun stapler staple "$DMG"
	spctl --assess --type open --context context:primary-signature --verbose=2 "$DMG"
	spctl --assess --type execute --verbose=2 "$APP"
fi

step "Done"
echo "The application: $APP"
echo "The disk image:  $DMG"
[ "$notarize" = true ] || echo "Not notarized: other Macs ask before opening it"
exit 0
