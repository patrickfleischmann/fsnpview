# Agent Instructions for fsnpview

This document provides instructions for setting up the build environment and compiling the `fsnpview` project on a Linux system.
Do not remove any Windows-related stuff just because you are working on a linux system. Use include guards if you need to.
Do not remove debug prints unless you are specifically asked to.

## 1. Dependencies

This project requires a C++ compiler, Qt6 development tools, and the Eigen3 library.

On a Debian-based system (like Ubuntu), you can install these dependencies using the following command:

```bash
sudo apt-get update && sudo apt-get install -y build-essential qt6-base-dev qt6-base-dev-tools libeigen3-dev
```

## 2. Building the Application

Once the dependencies are installed, you can build the application using the following commands from the root of the repository:

1.  **Generate the Makefile:**
    ```bash
    qmake6
    ```

2.  **Compile the project:**
    ```bash
    make
    ```

Alternatively, you can run `./build.sh` to execute both steps at once. On Windows, use the Qt6 `qmake` and your platform's build tool (`nmake` for MSVC or `mingw32-make` for MinGW).

This will produce an executable file named `fsnpview` in the root directory.

## 3. Testing

Run the included test script to verify that the application launches and parses a sample Touchstone file:

```bash
./test.sh
```

## 4. Running the Application

`fsnpview` is a GUI application. To run it in a headless environment, you need to use the `offscreen` Qt platform plugin.

You can run the application and plot a Touchstone file (`.sNp`) by passing the file path as a command-line argument:

```bash
QT_QPA_PLATFORM=offscreen ./fsnpview /path/to/your/file.s2p
```
