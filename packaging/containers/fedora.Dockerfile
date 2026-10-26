ARG BASE_IMAGE=fedora:44
FROM ${BASE_IMAGE} AS builder
RUN dnf install -y \
    gcc-c++ ccache cmake ninja-build pkgconf-pkg-config python3 git file binutils gettext kf6-ki18n-devel glibc-langpack-en \
    xorg-x11-server-Xvfb xorg-x11-xauth rpm-build dbus-daemon dejavu-sans-fonts desktop-file-utils \
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtpositioning-devel qt6-qtsvg-devel \
    kf6-kirigami-devel kf6-knotifications-devel kf6-qqc2-desktop-style kf6-krunner-devel kf6-kconfig-devel \
    extra-cmake-modules kf6-breeze-icons mesa-dri-drivers plasma-workspace plasma-desktop \
    && dnf clean all
WORKDIR /source

FROM ${BASE_IMAGE} AS runtime
RUN dnf install -y python3 dbus-daemon dejavu-sans-fonts desktop-file-utils \
    xorg-x11-server-Xvfb xorg-x11-xauth plasma-desktop && dnf clean all
WORKDIR /out
