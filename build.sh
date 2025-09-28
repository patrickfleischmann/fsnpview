#!/bin/bash

# This script builds the test binaries.
set -euo pipefail
set -x

# Locate the Qt meta-object compiler. Different distributions install it
# under different names/locations (e.g. moc, moc-qt6). If it is missing we
# attempt to install the required Qt development packages automatically on
# Debian based systems so that the script works out-of-the-box in CI images
# that start without Qt preinstalled.
MOC=""

find_moc() {
    if command -v qtpaths6 >/dev/null 2>&1; then
        local qt_libexec
        qt_libexec="$(qtpaths6 --query QT_INSTALL_LIBEXECS || true)"
        if [ -n "$qt_libexec" ] && [ -x "$qt_libexec/moc" ]; then
            MOC="$qt_libexec/moc"
            return 0
        fi
        local qt_bins
        qt_bins="$(qtpaths6 --query QT_INSTALL_BINS || true)"
        if [ -n "$qt_bins" ] && [ -x "$qt_bins/moc" ]; then
            MOC="$qt_bins/moc"
            return 0
        fi
    fi

    if command -v moc-qt6 >/dev/null 2>&1; then
        MOC="$(command -v moc-qt6)"
    elif command -v moc >/dev/null 2>&1; then
        MOC="$(command -v moc)"
    fi

    if [ -z "$MOC" ]; then
        return 1
    fi

    return 0
}

install_qt_dependencies() {
    if [ "${FSNPVIEW_NO_AUTO_SETUP:-}" = "1" ]; then
        return 1
    fi

    if ! command -v apt-get >/dev/null 2>&1; then
        return 1
    fi

    echo "info: Qt development tools not found, installing dependencies via apt-get." >&2

    # Prefer sudo when available (e.g. for non-root users) but fall back to
    # running the commands directly when already running as root.
    if command -v sudo >/dev/null 2>&1; then
        sudo apt-get update
        sudo apt-get install -y build-essential qt6-base-dev qt6-base-dev-tools libeigen3-dev
    else
        apt-get update
        apt-get install -y build-essential qt6-base-dev qt6-base-dev-tools libeigen3-dev
    fi

    return 0
}

if ! find_moc; then
    if install_qt_dependencies && find_moc; then
        :
    else
        echo "error: Qt 'moc' executable not found. Install qt6-base-dev or run ./setup.sh." >&2
        exit 1
    fi
fi

# Ensure pkg-config can locate the Qt6 .pc files. Debian based systems install
# them in a qt6 specific subdirectory that is not part of the default search
# path.
qt_pkgconfig_dirs=(
    /usr/lib/x86_64-linux-gnu/qt6/pkgconfig
    /usr/lib/qt6/pkgconfig
    /usr/local/lib/qt6/pkgconfig
    /usr/lib64/qt6/pkgconfig
)

for dir in "${qt_pkgconfig_dirs[@]}"; do
    if [ -d "$dir" ]; then
        if [ -z "${PKG_CONFIG_PATH:-}" ]; then
            export PKG_CONFIG_PATH="$dir"
        elif [[ ":$PKG_CONFIG_PATH:" != *":$dir:"* ]]; then
            export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$dir"
        fi
    fi
done

if ! pkg-config --exists Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport; then
    echo "error: Qt6 pkg-config files were not found. Install qt6-base-dev or set PKG_CONFIG_PATH." >&2
    exit 1
fi

MOC_INCLUDES="$(pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport)"

g++ -std=c++17 -I/usr/include/eigen3 -I. tests/parser_touchstone_tests.cpp parser_touchstone.cpp -o parser_touchstone_tests

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/tdrcalculator_tests.cpp tdrcalculator.cpp \
    -o tdrcalculator_tests $(pkg-config --cflags --libs Qt6Core)

# Generate moc files for Qt classes
$MOC $MOC_INCLUDES plotmanager.h -o moc_plotmanager.cpp
$MOC $MOC_INCLUDES network.h -o moc_network.cpp
$MOC $MOC_INCLUDES networkfile.h -o moc_networkfile.cpp
$MOC $MOC_INCLUDES networklumped.h -o moc_networklumped.cpp
$MOC $MOC_INCLUDES networkcascade.h -o moc_networkcascade.cpp
$MOC $MOC_INCLUDES qcustomplot.h -o moc_qcustomplot.cpp

# Build GUI plot test
g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/gui_plot_tests.cpp plotmanager.cpp network.cpp networkfile.cpp \
    networklumped.cpp networkcascade.cpp parser_touchstone.cpp qcustomplot.cpp \
    tdrcalculator.cpp \
    moc_plotmanager.cpp moc_network.cpp moc_networkfile.cpp moc_networklumped.cpp \
    moc_networkcascade.cpp moc_qcustomplot.cpp \
    -o gui_plot_tests $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport)

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/networkcascade_tests.cpp parser_touchstone.cpp network.cpp networkfile.cpp \
    networklumped.cpp networkcascade.cpp tdrcalculator.cpp \
    moc_network.cpp moc_networkfile.cpp moc_networklumped.cpp moc_networkcascade.cpp \
    -o networkcascade_tests $(pkg-config --cflags --libs Qt6Core Qt6Gui)
