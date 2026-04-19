# Print Queue Manager

A native Windows desktop application written in C++ / Win32 that talks
directly to the Windows Print Spooler (`winspool.drv`) to let you view and
control every job in every printer's queue.

## Features

- Lists all local and network printers (`EnumPrinters`)
- Shows the full print queue for the selected printer with columns for
  job ID, document, owner, pages, size, status, submission time, and position
- Per-job actions: **Pause**, **Resume**, **Cancel**, **Restart**
- Reorder jobs with **Move Up** / **Move Down** (`SetJob` with `JOB_INFO_1.Position`)
- Per-printer actions: **Pause printer** / **Resume printer**
- Auto-refresh every 3 seconds (Win32 `WM_TIMER`)
- Classic Win32 UI with a ComboBox, ListView, menu bar, context menu, and status bar
- Common Controls v6 and per-monitor DPI awareness via embedded manifest
- Zero third-party dependencies

## Platform

Windows 7 / 8 / 8.1 / 10 / 11 (x86 or x64).

## Project Layout

```
PrintQueueManager/
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp          # wWinMain, message loop
    ├── MainWindow.cpp/.h # Main window: ComboBox, ListView, status bar, actions
    ├── SpoolerAPI.cpp/.h # Thin wrapper around winspool.drv
    ├── resource.rc       # Menus, version info, manifest reference
    ├── resource.h        # Resource IDs
    └── app.manifest      # Common Controls v6 + DPI awareness
```

## Building

### Visual Studio 2019 / 2022 (recommended)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The resulting executable is `build\Release\PrintQueueManager.exe`.

### MinGW-w64

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Usage

1. Launch `PrintQueueManager.exe`.
2. Pick a printer from the drop-down at the top.
3. The list view shows every job currently spooled on that printer; it
   refreshes automatically every 3 seconds.
4. Right-click any job (or use the **Job** menu) to pause, resume, cancel,
   restart, or reorder it.
5. Use the **Printer** menu to pause or resume the whole printer.

### Permissions

Controlling jobs that belong to *other* users, or administering the printer
itself, generally requires elevated rights. If an action fails with an
"Access denied" message, try running the app as Administrator.

## How It Works

All printer/job operations go through `src/SpoolerAPI.cpp`:

| Action                        | Win32 call                                         |
|-------------------------------|----------------------------------------------------|
| Enumerate printers            | `EnumPrinters(PRINTER_ENUM_LOCAL \| _CONNECTIONS)` |
| Enumerate jobs                | `OpenPrinter` + `EnumJobs(level=2)`                |
| Pause / Resume / Cancel / Restart a job | `SetJob(jobId, 0, NULL, JOB_CONTROL_*)`  |
| Move a job in the queue       | `GetJob(level=1)` + `SetJob(level=1, Position=N)`  |
| Pause / Resume a printer      | `SetPrinter(0, NULL, PRINTER_CONTROL_*)`           |

The UI layer (`MainWindow.cpp`) is a plain Win32 window that hosts a
`COMBOBOX`, a `SysListView32`, a `msctls_statusbar32`, and a push button,
with all events handled in a classic `WndProc` dispatcher.
