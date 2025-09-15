#!/bin/bash

# This script builds the test binaries.
set -e

g++ -std=c++17 -I/usr/include/eigen3 -I. tests/parser_touchstone_tests.cpp parser_touchstone.cpp -o parser_touchstone_tests

QT_PACKAGES=(Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport)
QT_AVAILABLE=true

if ! command -v pkg-config >/dev/null; then
    QT_AVAILABLE=false
elif ! pkg-config --exists "${QT_PACKAGES[@]}"; then
    QT_AVAILABLE=false
fi

MOC_BIN=""
if [ "$QT_AVAILABLE" = true ]; then
    for candidate in moc-qt6 moc; do
        if command -v "$candidate" >/dev/null; then
            MOC_BIN="$(command -v "$candidate")"
            break
        fi
    done
    if [ -z "$MOC_BIN" ] && [ -x /usr/lib/qt6/libexec/moc ]; then
        MOC_BIN=/usr/lib/qt6/libexec/moc
    fi
    if [ -z "$MOC_BIN" ]; then
        echo "Qt6 moc tool not found; skipping GUI/network cascade tests." >&2
        QT_AVAILABLE=false
    fi
fi

if [ "$QT_AVAILABLE" = true ]; then
    MOC_INCLUDES="$(pkg-config --cflags "${QT_PACKAGES[@]}")"

    # Generate moc files for Qt classes
    "$MOC_BIN" $MOC_INCLUDES plotmanager.h -o moc_plotmanager.cpp
    "$MOC_BIN" $MOC_INCLUDES network.h -o moc_network.cpp
    "$MOC_BIN" $MOC_INCLUDES networkfile.h -o moc_networkfile.cpp
    "$MOC_BIN" $MOC_INCLUDES networklumped.h -o moc_networklumped.cpp
    "$MOC_BIN" $MOC_INCLUDES networkcascade.h -o moc_networkcascade.cpp
    "$MOC_BIN" $MOC_INCLUDES qcustomplot.h -o moc_qcustomplot.cpp

    # Build GUI plot test
    g++ -std=c++17 -I/usr/include/eigen3 -I. \
        tests/gui_plot_tests.cpp plotmanager.cpp network.cpp networkfile.cpp \
        networklumped.cpp networkcascade.cpp parser_touchstone.cpp qcustomplot.cpp \
        moc_plotmanager.cpp moc_network.cpp moc_networkfile.cpp moc_networklumped.cpp \
        moc_networkcascade.cpp moc_qcustomplot.cpp \
        -o gui_plot_tests $(pkg-config --cflags --libs "${QT_PACKAGES[@]}")

    g++ -std=c++17 -I/usr/include/eigen3 -I. \
        tests/networkcascade_tests.cpp parser_touchstone.cpp network.cpp networkfile.cpp \
        networkcascade.cpp moc_network.cpp moc_networkfile.cpp moc_networkcascade.cpp \
        -o networkcascade_tests $(pkg-config --cflags --libs Qt6Core Qt6Gui)
else
    echo "Qt6 development files not found; skipping GUI/network cascade tests." >&2
fi
