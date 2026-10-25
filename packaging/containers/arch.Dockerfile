FROM archlinux:base-devel AS builder
RUN pacman -Syu --noconfirm --needed \
    ccache cmake ninja pkgconf python git file binutils libarchive zstd gettext ki18n \
    xorg-server-xvfb xorg-xauth dbus ttf-dejavu desktop-file-utils qt6-base qt6-declarative qt6-positioning qt6-svg \
    kirigami knotifications qqc2-desktop-style extra-cmake-modules breeze-icons \
    mesa plasma-workspace plasma-desktop \
    && rm -rf /var/cache/pacman/pkg/*
WORKDIR /source
RUN echo 'en_US.UTF-8 UTF-8' >> /etc/locale.gen && locale-gen

# Installation checks must not inherit compiler or development packages.
FROM archlinux:base AS runtime
RUN pacman -Syu --noconfirm --needed python dbus ttf-dejavu desktop-file-utils \
    xorg-server-xvfb xorg-xauth plasma-desktop \
    && rm -rf /var/cache/pacman/pkg/*
WORKDIR /out
