FROM archlinux:base-devel
RUN pacman -Syu --noconfirm --needed \
    ccache cmake ninja pkgconf python git file binutils libarchive zstd \
    xorg-server-xvfb xorg-xauth dbus ttf-dejavu desktop-file-utils qt6-base qt6-declarative qt6-positioning qt6-svg \
    kirigami knotifications qqc2-desktop-style extra-cmake-modules breeze-icons \
    mesa plasma-workspace plasma-desktop \
    && rm -rf /var/cache/pacman/pkg/*
WORKDIR /source
