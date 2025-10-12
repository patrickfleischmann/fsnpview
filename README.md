<img width="100" height="100" alt="fsnpview" src="https://github.com/user-attachments/assets/d457d2c4-23b8-49bb-a929-e3451aebf44a" />

# fsnpview
Fast SnP file viewer with cascading capability.

About SnP file format: [Wikipedia/Touchstone file](https://en.wikipedia.org/wiki/Touchstone_file)

## Windows Installation and Usage
Download and extract fsnpview.zip. 

To quickly open .snp files from explorer, register the extracted fsnpview.exe with the "open with" dialog.

Multiple files can be opened at once or one by one. All files will be opened in the same instance.

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

## Usage

After building, `fsnpview` can be launched either with a Touchstone file
as an argument or on its own:

```bash
./fsnpview /path/to/your/file.s2p   # open and plot a specific file
./fsnpview                         # start with an empty session
```

If you start without a file, press `Ctrl+O` or drag and drop a
`.sNp` file into the window to load it.  The left-hand tables list loaded
network files and the available lumped elements.  Check the box next to
an entry to add or remove its trace from the plot.  You can drag networks
onto the cascade table to evaluate a chain of networks.

### GUI tips

**Loading and organizing data**

*   Press `Ctrl+O` to browse for Touchstone files without leaving the main window; each file you pick is added to the current session.
*   Drag Touchstone rows or lumped elements from the left-hand tables into the cascade table to build or reorder network chains; both the source tables and the cascade support multi-selection and drag and drop.
*   Press `Ctrl+S` to export the active cascade; the shortcut opens a Touchstone save dialog when the cascade contains any networks.

**Trace selection and measurements**

*   Toggle the individual S-parameters with the row of checkboxes (`s11`–`s33`) above the plot area to show or hide specific traces.
*   The toolbar checkboxes also let you enable the red/blue measurement cursors, the crosshair overlay, and the legend so you can inspect values directly on the chart.
*   Select traces or table rows and press **Delete** to hide the traces, remove cascaded elements, or delete loaded files depending on which pane is focused; the command respects context so you do not have to clear the whole session.
*   Highlight exactly two traces and press the `-` key to create a red math trace representing the point-by-point difference between them.

**Display modes and analysis**

*   Switch between linear and logarithmic frequency axes with the **f Log** checkbox, and turn on **Phase**, **gdelay**, **VSWR**, **Smith**, or **TDR** views to swap the plot into the matching analysis mode; enabling one of these mutually exclusive views automatically disables the others to keep the display coherent.
*   Use **Unwrap** to keep phase plots continuous and toggle **Gate** to activate time-domain gating; the start/stop distance and effective dielectric fields immediately reapply when edited.

**Frequency grids and mouse-wheel helpers**

*   Edit the `f min`, `f max`, and `pt` fields above the lumped-element table to resample cascades onto a new frequency grid; every change is validated and applied as soon as you finish editing the field.
*   Roll the mouse wheel while hovering over the frequency, point-count, or gating fields to nudge the values up or down with engineering notation updates, or over the `*` multiplier box to clamp a new mouse-wheel gain between 1.0001× and 10× for fine or coarse adjustments.
*   Scroll over lumped-element parameter cells (in either the component library or the cascade) to scale the highlighted value by the configured multiplier—handy for quick tuning sweeps.

**Plot styling and grids**

*   Right-click and release on the plot to open the Plot Settings dialog without disturbing the current zoom; the shortcut works even while cursors are active.
*   In the Plot Settings dialog you can set exact axis limits, reposition markers, choose grid and subgrid line styles/colors, and override the automatic major/minor tick spacing when the axes use linear scales.

To add more Touchstone files after launch, press `Ctrl+O` or drag files from your desktop into the window; each chosen file is appended to the current session so you can compare multiple networks side by side.

### Command-line interface

`fsnpview` also exposes a non-interactive CLI for batch processing:

```bash
fsnpview [files...] [options]
```

Common options include:

*   `-c, --cascade <items>` — Build a cascade before showing the GUI (or
    when running headless).  Provide a sequence of Touchstone files or
    lumped element names with optional parameter/value overrides.
*   `-f, --freq <fmin> <fmax> <points>` — Resample the cascade onto a new
    frequency grid.
*   `-s, --save <file>` — Write the resulting cascaded network to a
    Touchstone file.
*   `-n, --nogui` — Run without starting the GUI (useful together with
    `-s` in scripts).
*   `-h, --help` — Show the full help text, including the list of
    available lumped elements and their default units.

For example, the following command cascades a measured network with a
75 Ω series resistor, resamples the result, and saves it to disk without
opening the GUI:

```bash
fsnpview example.s2p -c example.s2p R_series R 75 -f 1e6 1e9 1001 -s result.s2p -n
```

The CLI understands the same lumped elements that are available in the
GUI (**R/C/L**, lossy and lossless transmission lines, and the RLC
combinations) and accepts both positional arguments and explicit
`name=value` overrides.  Combine `-n`, `-c`, and `-s` to automate
repeatable cascades as part of a data-processing pipeline.

### Headless environments

`fsnpview` is a GUI application.  When running on a system without a
display (for example, in continuous integration) use Qt’s offscreen
platform plugin:

```bash
QT_QPA_PLATFORM=offscreen ./fsnpview /path/to/your/file.s2p
```

## Testing

Run `./setup.sh` first to install the required system and Python dependencies (the script uses `sudo`).
After the dependencies are available, `./test.sh` builds the application and executes the full test suite, including the offscreen GUI plot comparison against the baseline `tests/gui_plot_baseline.csv` and the regression test that checks the lumped-network models against scikit-rf.
To update the baseline after intentional changes, rebuild and run:

```bash
QT_QPA_PLATFORM=offscreen UPDATE_BASELINE=1 ./gui_plot_tests
```

Commit the regenerated baseline file.

### Windows

On Windows, replace `./fsnpview` with `fsnpview.exe` and use Windows-style
paths when specifying files.

## Screenshots
<img width="1447" height="917" alt="Screenshot 2025-10-01 220438" src="https://github.com/user-attachments/assets/32dcd518-a45f-46c7-81ae-a08afdf10f01" />
