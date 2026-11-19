# Local development only: builds the Flatpak of the Editor in a Linux container
# of this computer's processor and shows it in the browser (noVNC). See run.sh.
FROM fedora:42

RUN dnf install -y --setopt=install_weak_deps=False \
        flatpak flatpak-builder git ccache python3 \
        xorg-x11-server-Xvfb x11vnc novnc python3-websockify openbox \
        dbus-daemon dbus-tools xdpyinfo \
    && dnf clean all

COPY start.sh /usr/local/bin/start.sh
EXPOSE 6080
ENTRYPOINT ["/usr/local/bin/start.sh"]
