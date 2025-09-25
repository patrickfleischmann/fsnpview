#!/bin/bash

# This script builds the test binaries.
set -euo pipefail
set -x

MOC=/usr/lib/qt6/libexec/moc
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
