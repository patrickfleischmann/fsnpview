#!/bin/bash
# This script builds the parser tests.
set -e
g++ -std=c++17 -I/usr/include/eigen3 -I. tests/parser_touchstone_tests.cpp parser_touchstone.cpp -o parser_touchstone_tests
