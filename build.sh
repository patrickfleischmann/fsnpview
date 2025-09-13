#!/bin/bash
# This script builds the tests.
set -e

MOC=/usr/lib/qt6/libexec/moc
MOC_INCLUDES="-I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtWidgets -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtPrintSupport"

g++ -std=c++17 -I/usr/include/eigen3 -I. tests/parser_touchstone_tests.cpp parser_touchstone.cpp -o parser_touchstone_tests

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
    moc_plotmanager.cpp moc_network.cpp moc_networkfile.cpp moc_networklumped.cpp \
    moc_networkcascade.cpp moc_qcustomplot.cpp \
    -o gui_plot_tests $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core Qt6PrintSupport)
