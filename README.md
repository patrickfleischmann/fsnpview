# snpview
fast snp (touchstone) file viewer

## Dependencies

This project has the following dependencies:

*   **Qt5** or **Qt6**: A C++ cross-platform development framework. You will need the following modules:
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
