#!/bin/bash
# This script installs the dependencies for the fsnpview project on Debian-based systems.
set -e
sudo apt-get update && sudo apt-get install -y build-essential qt6-base-dev qt6-base-dev-tools libeigen3-dev
