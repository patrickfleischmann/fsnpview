# Agent Instructions for fsnpview

This document provides instructions for setting up the build environment and compiling the `fsnpview` project.

## 1. Dependencies

This project requires a C++ compiler, Qt5 development tools, and the Eigen3 library.

On a Debian-based system (like Ubuntu), you can install these dependencies using the following commands:

```bash
sudo apt-get update
sudo apt-get install -y build-essential qt5-qmake qtbase5-dev libeigen3-dev
```

## 2. Configuration

The Qt project file, `fsnpview.pro`, contains a hardcoded path to the Eigen3 library that is specific to a Windows environment. This needs to be corrected for the build to work on other systems.

**Action:**

In the file `fsnpview.pro`, find the following line:

```
INCLUDEPATH += "C:\home\projekte\Qt\eigen-3.4.0"
```

And replace it with the standard include path for Eigen3 on a Debian-based system:

```
INCLUDEPATH += /usr/include/eigen3
```

## 3. Building the Application

Once the dependencies are installed and the configuration is corrected, you can build the application using the following commands from the root of the repository:

1.  **Generate the Makefile:**
    ```bash
    qmake
    ```

2.  **Compile the project:**
    ```bash
    make
    ```

This will produce an executable file named `fsnpview` in the root directory.

## 4. Running the Application

`fsnpview` is a GUI application. To run it in a headless environment (like the one this agent operates in), you need to use the `offscreen` Qt platform plugin.

You can run the application and plot a Touchstone file (`.sNp`) by passing the file path as a command-line argument:

```bash
QT_QPA_PLATFORM=offscreen ./fsnpview /path/to/your/file.s2p
```
