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
UIC=""

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

    return 1
}

find_uic() {
    if command -v qtpaths6 >/dev/null 2>&1; then
        local qt_bins
        qt_bins="$(qtpaths6 --query QT_INSTALL_BINS || true)"
        if [ -n "$qt_bins" ] && [ -x "$qt_bins/uic" ]; then
            UIC="$qt_bins/uic"
            return 0
        fi
        local qt_libexec
        qt_libexec="$(qtpaths6 --query QT_INSTALL_LIBEXECS || true)"
        if [ -n "$qt_libexec" ] && [ -x "$qt_libexec/uic" ]; then
            UIC="$qt_libexec/uic"
            return 0
        fi
    fi

    for candidate in uic-qt6 uic; do
        if command -v "$candidate" >/dev/null 2>&1; then
            UIC="$(command -v "$candidate")"
            return 0
        fi
    done

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

if ! find_uic; then
    if install_qt_dependencies && find_uic; then
        :
    else
        echo "error: Qt 'uic' executable not found. Install qt6-base-dev or run ./setup.sh." >&2
        exit 1
    fi
fi

# Generate ui header
if [ -z "$UIC" ]; then
    echo "error: Qt 'uic' executable not found even after setup." >&2
    exit 1
fi
"$UIC" mainwindow.ui -o ui_mainwindow.h

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
$MOC $MOC_INCLUDES networkitemmodel.h -o moc_networkitemmodel.cpp
$MOC $MOC_INCLUDES mainwindow.h -o moc_mainwindow.cpp
$MOC $MOC_INCLUDES server.h -o moc_server.cpp
$MOC $MOC_INCLUDES qcustomplot.h -o moc_qcustomplot.cpp
$MOC $MOC_INCLUDES parameterstyledialog.h -o moc_parameterstyledialog.cpp
$MOC $MOC_INCLUDES plotsettingsdialog.h -o moc_plotsettingsdialog.cpp

# Build GUI plot test
g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/gui_plot_tests.cpp plotmanager.cpp plotsettingsdialog.cpp network.cpp networkfile.cpp \
    networklumped.cpp networkcascade.cpp parser_touchstone.cpp qcustomplot.cpp \
    tdrcalculator.cpp \
    moc_plotmanager.cpp moc_plotsettingsdialog.cpp moc_network.cpp moc_networkfile.cpp moc_networklumped.cpp \
    moc_networkcascade.cpp moc_qcustomplot.cpp \
    -o gui_plot_tests $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport)

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/networkcascade_tests.cpp parser_touchstone.cpp network.cpp networkfile.cpp \
    networklumped.cpp networkcascade.cpp tdrcalculator.cpp \
    moc_network.cpp moc_networkfile.cpp moc_networklumped.cpp moc_networkcascade.cpp \
    -o networkcascade_tests $(pkg-config --cflags --libs Qt6Core Qt6Gui)

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/cascadeio_tests.cpp cascadeio.cpp parser_touchstone.cpp network.cpp networkfile.cpp \
    networklumped.cpp networkcascade.cpp tdrcalculator.cpp \
    moc_network.cpp moc_networkfile.cpp moc_networklumped.cpp moc_networkcascade.cpp \
    -o cascadeio_tests $(pkg-config --cflags --libs Qt6Core Qt6Gui)

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/network_plot_style_tests.cpp network.cpp \
    moc_network.cpp \
    -o network_plot_style_tests $(pkg-config --cflags --libs Qt6Core Qt6Gui)

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/parameter_style_dialog_tests.cpp parameterstyledialog.cpp network.cpp \
    moc_parameterstyledialog.cpp moc_network.cpp \
    -o parameter_style_dialog_tests $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core)

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/plotmanager_selection_tests.cpp plotmanager.cpp plotsettingsdialog.cpp network.cpp networklumped.cpp \
    networkcascade.cpp parser_touchstone.cpp qcustomplot.cpp tdrcalculator.cpp \
    moc_plotmanager.cpp moc_plotsettingsdialog.cpp moc_network.cpp moc_networklumped.cpp moc_networkcascade.cpp moc_qcustomplot.cpp \
    -o plotmanager_selection_tests $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport)

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/plotmanager_mathplot_tests.cpp plotmanager.cpp plotsettingsdialog.cpp network.cpp networklumped.cpp \
    networkcascade.cpp parser_touchstone.cpp qcustomplot.cpp tdrcalculator.cpp \
    moc_plotmanager.cpp moc_plotsettingsdialog.cpp moc_network.cpp moc_networklumped.cpp moc_networkcascade.cpp moc_qcustomplot.cpp \
    -o plotmanager_mathplot_tests $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport)

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/cascade_wheel_tests.cpp mainwindow.cpp networkitemmodel.cpp plotmanager.cpp plotsettingsdialog.cpp \
    parameterstyledialog.cpp network.cpp networkfile.cpp networklumped.cpp networkcascade.cpp parser_touchstone.cpp \
    qcustomplot.cpp tdrcalculator.cpp server.cpp cascadeio.cpp \
    moc_mainwindow.cpp moc_networkitemmodel.cpp moc_plotmanager.cpp moc_plotsettingsdialog.cpp \
    moc_parameterstyledialog.cpp moc_network.cpp moc_networkfile.cpp moc_networklumped.cpp moc_networkcascade.cpp \
    moc_qcustomplot.cpp moc_server.cpp \
    -o cascade_wheel_tests $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport Qt6Network)

g++ -std=c++17 -I/usr/include/eigen3 -I. \
    tests/plotsettingsdialog_tests.cpp plotmanager.cpp plotsettingsdialog.cpp network.cpp networklumped.cpp \
    networkcascade.cpp parser_touchstone.cpp qcustomplot.cpp tdrcalculator.cpp \
    moc_plotmanager.cpp moc_plotsettingsdialog.cpp moc_network.cpp moc_networklumped.cpp moc_networkcascade.cpp moc_qcustomplot.cpp \
    -o plotsettingsdialog_tests $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport)
