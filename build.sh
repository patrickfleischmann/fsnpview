#!/bin/bash
# This script builds the parser and network cascade tests.
set -e

g++ -std=c++17 -I/usr/include/eigen3 -I. tests/parser_touchstone_tests.cpp parser_touchstone.cpp -o parser_touchstone_tests

/usr/lib/qt6/libexec/moc network.h -o /tmp/moc_network.cpp
/usr/lib/qt6/libexec/moc networkfile.h -o /tmp/moc_networkfile.cpp
/usr/lib/qt6/libexec/moc networkcascade.h -o /tmp/moc_networkcascade.cpp

g++ -std=c++17 -I/usr/include/eigen3 -I. tests/networkcascade_tests.cpp parser_touchstone.cpp network.cpp networkfile.cpp networkcascade.cpp /tmp/moc_network.cpp /tmp/moc_networkfile.cpp /tmp/moc_networkcascade.cpp $(pkg-config --cflags --libs Qt6Core Qt6Gui) -o networkcascade_tests
