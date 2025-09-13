# snpview
fast snp (touchstone) file viewer

## Dependencies

This project has the following dependencies:

*   **Qt6**: A C++ cross-platform development framework. You will need the following modules:
    *   Core
    *   GUI
    *   Widgets
    *   PrintSupport
*   **A C++17 compatible compiler**: Such as GCC 7+, Clang 5+, or MSVC 2017+.
*   **Eigen 3**: A C++ template library for linear algebra, matrix and vector operations.
*   **QCustomPlot**: A Qt C++ widget for plotting. This dependency is included directly in the source code, so no separate installation is required.

## Building

1.  **Install Dependencies**: Make sure you have Qt, a C++17 compiler, and the Eigen library installed on your system.

2.  **Configure the Project**: Open the `fsnpview.pro` file in a text editor. You will need to modify the `INCLUDEPATH` to point to the location of the Eigen library on your system.

    For example, change this line:
    ```
    INCLUDEPATH += "C:\home\projekte\Qt\eigen-3.4.0"
    ```
    to something like this on Linux/macOS:
    ```
    INCLUDEPATH += "/usr/local/include/eigen3"
    ```
    or this on Windows:
    ```
    INCLUDEPATH += "C:/path/to/eigen-3.4.0"
    ```

3.  **Build the Project**:
    ```bash
    qmake
    make
    ```

4.  **Run**: Once the build is complete, you can run the executable.

## Linting

Run `clang-tidy` to analyze the C++ sources:

```bash
/usr/lib/qt6/libexec/uic mainwindow.ui -o ui_mainwindow.h
git ls-files '*.cpp' '*.h' | grep -v '^qcustomplot' | grep -v '^mainwindow' | grep -v '^moc_' | xargs -I{} clang-tidy {} -- -std=c++17 -I/usr/include/x86_64-linux-gnu/qt6 -I/usr/include/x86_64-linux-gnu/qt6/QtWidgets -I/usr/include/x86_64-linux-gnu/qt6/QtCore -I/usr/include/x86_64-linux-gnu/qt6/QtGui -I/usr/include/x86_64-linux-gnu/qt6/QtNetwork -I/usr/include/eigen3 -I.
```

The build will fail if `clang-tidy` reports warnings.
## Usage

After building, `fsnpview` can be launched either with a Touchstone file
as an argument or on its own:

```bash
./fsnpview /path/to/your/file.s2p   # open and plot a specific file
./fsnpview                         # start with an empty session
```

If you start without a file, use **File → Open…** or drag and drop a
`.sNp` file into the window to load it.  The left-hand tables list loaded
network files and the available lumped elements.  Check the box next to
an entry to add or remove its trace from the plot.  You can drag networks
onto the cascade table to evaluate a chain of networks.

The plot area supports typical interactions from `QCustomPlot`: use the
mouse wheel to zoom, drag to pan, and enable measurement cursors or
phase/legend display using the checkboxes above the plot.  Individual
S‑parameters (S11, S21, S12, S22) can also be toggled with their
respective checkboxes.

### Headless environments

`fsnpview` is a GUI application.  When running on a system without a
display (for example, in continuous integration) use Qt’s offscreen
platform plugin:

```bash
QT_QPA_PLATFORM=offscreen ./fsnpview /path/to/your/file.s2p
```

### Windows

On Windows, replace `./fsnpview` with `fsnpview.exe` and use Windows-style
paths when specifying files.
