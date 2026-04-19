#pragma once

#include <windows.h>
#include <winspool.h>
#include <string>
#include <vector>
#include <cstdint>

namespace Spooler {

struct PrinterInfo {
    std::wstring name;
    std::wstring serverName;
    DWORD status = 0;
    DWORD jobCount = 0;
};

struct JobInfo {
    DWORD jobId = 0;
    DWORD position = 0;
    DWORD status = 0;
    DWORD totalPages = 0;
    DWORD pagesPrinted = 0;
    DWORD sizeBytes = 0;
    std::wstring printerName;
    std::wstring documentName;
    std::wstring userName;
    std::wstring machineName;
    std::wstring dataType;
    std::wstring statusText;
    SYSTEMTIME submitted{};
};

enum class JobAction {
    Pause,
    Resume,
    Cancel,
    Restart
};

// Enumerate all local + connection printers visible to the user.
std::vector<PrinterInfo> EnumeratePrinters();

// List all jobs currently in a given printer's queue.
std::vector<JobInfo> EnumerateJobs(const std::wstring& printerName);

// Pause, resume, cancel or restart a single job.
bool ControlJob(const std::wstring& printerName, DWORD jobId, JobAction action);

// Move a job to a new 0-based queue position.
bool SetJobPosition(const std::wstring& printerName, DWORD jobId, DWORD newPosition);

// Pause or resume the whole printer.
bool ControlPrinter(const std::wstring& printerName, DWORD control);

// Human-readable decoding helpers.
std::wstring PrinterStatusToString(DWORD status);
std::wstring JobStatusToString(DWORD status);
std::wstring FormatSystemTime(const SYSTEMTIME& st);
std::wstring FormatLastError(DWORD err);

} // namespace Spooler
