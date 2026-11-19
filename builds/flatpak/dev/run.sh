#!/bin/sh
# Local development only: builds the Editor's Flatpak in Docker (a Linux
# container of this computer's processor) and runs it, shown in the browser at
# http://localhost:6080/vnc.html. Needs Docker running; nothing to install.
#
#   builds/flatpak/dev/run.sh [build|run|shell] [GAMES]
#     build   (default) build the Editor as it is in this folder, then run it
#     run     run the Editor built last
#     shell   a shell in the container
#     GAMES   the folder shown in the container as /games (default: ../TestGame)
#
# The first build downloads the KDE runtime and SDK (about 2 GB) and builds the
# libraries of the manifest; they stay in two Docker volumes
# (easyrpg-editor-flatpak, easyrpg-editor-flatpak-cache), so the next builds
# only build the Editor (with ccache). To remove them:
#   docker volume rm easyrpg-editor-flatpak easyrpg-editor-flatpak-cache
set -e
here="$(cd "$(dirname "$0")" && pwd)"
editor="$(cd "$here/../../.." && pwd)"
mode="${1:-build}"
games="${2:-$editor/../TestGame}"
games="$(cd "$games" && pwd)"

docker build -q -t easyrpg-editor-flatpak-dev "$here" >/dev/null
tty=""
[ -t 0 ] && tty="-it"
# privileged: flatpak and flatpak-builder sandbox with bubblewrap
exec docker run --rm $tty --name easyrpg-editor-flatpak-dev --privileged -p 6080:6080 \
	-v easyrpg-editor-flatpak:/var/lib/flatpak \
	-v easyrpg-editor-flatpak-cache:/var/cache/fb \
	-v "$editor":/src:ro \
	-v "$games":/games \
	easyrpg-editor-flatpak-dev "$mode"
