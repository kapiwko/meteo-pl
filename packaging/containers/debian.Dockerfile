ARG BASE_IMAGE=debian:trixie-slim
FROM ${BASE_IMAGE}
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --yes --no-install-recommends \
    build-essential ccache cmake ninja-build pkg-config python3 git file binutils \
    dpkg-dev dbus-x11 fonts-dejavu-core desktop-file-utils \
    qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-declarative-dev-tools \
    qt6-positioning-dev libqt6svg6-dev libkirigami-dev libkf6notifications-dev \
    extra-cmake-modules qml6-module-org-kde-kirigami qml6-module-org-kde-desktop \
    qml6-module-qtcore qml6-module-qtpositioning qml6-module-qtqml \
    qml6-module-qtqml-models qml6-module-qtqml-workerscript qml6-module-qtquick \
    qml6-module-qtquick-controls qml6-module-qtquick-dialogs qml6-module-qtquick-layouts \
    qml6-module-qtquick-shapes qml6-module-qtquick-templates qml6-module-qtquick-window \
    breeze-icon-theme libgl1-mesa-dri plasma-workspace \
    && rm -rf /var/lib/apt/lists/*
RUN apt-get update && apt-get install --yes --no-install-recommends xvfb xauth plasma-desktop && rm -rf /var/lib/apt/lists/*
WORKDIR /source
