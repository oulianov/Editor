#!/bin/sh
# In the container (see run.sh): builds the Flatpak from the Editor's folder
# (/src, read-only) with builds/flatpak/org.easyrpg.editor.yml, installs it,
# then runs it on a virtual screen shown at http://localhost:6080/vnc.html.
#   start.sh          build, install and run
#   start.sh run      run what was installed last
#   start.sh shell    a shell in the container
set -e
mode="${1:-build}"
[ $# -gt 0 ] && shift  # the rest: arguments of the Editor (a game: /games/...)
[ "$mode" = shell ] && exec /bin/bash

flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

if [ "$mode" = build ]; then
	# the manifest builds the folder it is in (../../): here /src, without
	# the local builds (build/: gigabytes, made on macOS)
	sed -e 's|^\( *\)path: "\.\./\.\./"$|\1path: /src\n\1skip: [build, lib/liblcf/build, .flatpak-builder]|' \
		/src/builds/flatpak/org.easyrpg.editor.yml > /tmp/org.easyrpg.editor.yml
	grep -q "path: /src" /tmp/org.easyrpg.editor.yml || { echo "the manifest's source path changed: update start.sh"; exit 1; }
	# runtimes in /var/lib/flatpak, builds and ccache in /var/cache/fb: volumes kept between runs
	flatpak-builder --system --install --install-deps-from=flathub --ccache --force-clean \
		--state-dir=/var/cache/fb/state /var/cache/fb/build /tmp/org.easyrpg.editor.yml
fi

# the screen, its window manager, and the browser's way to it
export DISPLAY=:1
Xvfb :1 -screen 0 "${SCREEN:-1600x1000x24}" -nolisten tcp &
until xdpyinfo >/dev/null 2>&1; do sleep 0.2; done
openbox >/dev/null 2>&1 &
x11vnc -display :1 -forever -shared -nopw -quiet -rfbport 5900 >/dev/null 2>&1 &
websockify --web /usr/share/novnc 6080 localhost:5900 >/dev/null 2>&1 &
echo
echo "The Editor: http://localhost:6080/vnc.html (Connect). Your games: /games. Ctrl+C stops."
echo

export XDG_RUNTIME_DIR=/run/user/0
mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"
# no GPU in the container: Mesa's software rendering
exec dbus-run-session -- flatpak run --env=LIBGL_ALWAYS_SOFTWARE=1 org.easyrpg.editor "$@"
