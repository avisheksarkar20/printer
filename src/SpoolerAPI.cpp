#include "SpoolerAPI.h"

#include <vector>
#include <string>
#include <memory>

namespace Spooler {

namespace {

// Small RAII helper around OpenPrinter / ClosePrinter.
class PrinterHandle {
public:
    explicit PrinterHandle(const std::wstring& name) {
        // OpenPrinter takes LPWSTR, not LPCWSTR - copy to a mutable buffer.
        std::vector<wchar_t> buffer(name.begin(), name.end());
        buffer.push_back(L'\0');
        if (!::OpenPrinterW(buffer.data(), &handle_, nullptr)) {
            handle_ = nullptr;
        }
    }
    ~PrinterHandle() {
        if (handle_) {
            ::ClosePrinter(handle_);
        }
    }
    PrinterHandle(const PrinterHandle&) = delete;
    PrinterHandle& operator=(const PrinterHandle&) = delete;

    HANDLE get() const { return handle_; }
    bool valid() const { return handle_ != nullptr; }

private:
    HANDLE handle_ = nullptr;
};

std::wstring SafeCopy(LPCWSTR p) {
    return p ? std::wstring(p) : std::wstring();
}

} // namespace

std::vector<PrinterInfo> EnumeratePrinters() {
    std::vector<PrinterInfo> result;

    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    DWORD needed = 0;
    DWORD returned = 0;

    ::EnumPrintersW(flags, nullptr, 2, nullptr, 0, &needed, &returned);
    if (needed == 0) {
        return result;
    }

    std::vector<BYTE> buffer(needed);
    if (!::EnumPrintersW(flags, nullptr, 2,
                         buffer.data(), needed, &needed, &returned)) {
        return result;
    }

    auto* info = reinterpret_cast<PRINTER_INFO_2W*>(buffer.data());
    result.reserve(returned);
    for (DWORD i = 0; i < returned; ++i) {
        PrinterInfo p;
        p.name = SafeCopy(info[i].pPrinterName);
        p.serverName = SafeCopy(info[i].pServerName);
        p.status = info[i].Status;
        p.jobCount = info[i].cJobs;
        result.push_back(std::move(p));
    }
    return result;
}

std::vector<JobInfo> EnumerateJobs(const std::wstring& printerName) {
    std::vector<JobInfo> result;
    PrinterHandle h(printerName);
    if (!h.valid()) {
        return result;
    }

    DWORD needed = 0;
    DWORD returned = 0;
    // Probe for required buffer size, querying up to 1024 jobs.
    ::EnumJobsW(h.get(), 0, 1024, 2, nullptr, 0, &needed, &returned);
    if (needed == 0) {
        return result;
    }

    std::vector<BYTE> buffer(needed);
    if (!::EnumJobsW(h.get(), 0, 1024, 2,
                     buffer.data(), needed, &needed, &returned)) {
        return result;
    }

    auto* jobs = reinterpret_cast<JOB_INFO_2W*>(buffer.data());
    result.reserve(returned);
    for (DWORD i = 0; i < returned; ++i) {
        JobInfo j;
        j.jobId = jobs[i].JobId;
        j.position = jobs[i].Position;
        j.status = jobs[i].Status;
        j.totalPages = jobs[i].TotalPages;
        j.pagesPrinted = jobs[i].PagesPrinted;
        j.sizeBytes = jobs[i].Size;
        j.printerName = SafeCopy(jobs[i].pPrinterName);
        j.documentName = SafeCopy(jobs[i].pDocument);
        j.userName = SafeCopy(jobs[i].pUserName);
        j.machineName = SafeCopy(jobs[i].pMachineName);
        j.dataType = SafeCopy(jobs[i].pDatatype);
        j.statusText = SafeCopy(jobs[i].pStatus);
        j.submitted = jobs[i].Submitted;
        result.push_back(std::move(j));
    }
    return result;
}

bool ControlJob(const std::wstring& printerName, DWORD jobId, JobAction action) {
    PrinterHandle h(printerName);
    if (!h.valid()) {
        return false;
    }

    DWORD cmd = 0;
    switch (action) {
        case JobAction::Pause:   cmd = JOB_CONTROL_PAUSE; break;
        case JobAction::Resume:  cmd = JOB_CONTROL_RESUME; break;
        case JobAction::Cancel:  cmd = JOB_CONTROL_DELETE; break;
        case JobAction::Restart: cmd = JOB_CONTROL_RESTART; break;
    }
    return ::SetJobW(h.get(), jobId, 0, nullptr, cmd) != FALSE;
}

bool SetJobPosition(const std::wstring& printerName, DWORD jobId, DWORD newPosition) {
    PrinterHandle h(printerName);
    if (!h.valid()) {
        return false;
    }

    // Query current JOB_INFO_1 for this job.
    DWORD needed = 0;
    ::GetJobW(h.get(), jobId, 1, nullptr, 0, &needed);
    if (needed == 0) {
        return false;
    }

    std::vector<BYTE> buffer(needed);
    if (!::GetJobW(h.get(), jobId, 1, buffer.data(), needed, &needed)) {
        return false;
    }

    auto* info = reinterpret_cast<JOB_INFO_1W*>(buffer.data());
    info->Position = newPosition;
    return ::SetJobW(h.get(), jobId, 1, buffer.data(), 0) != FALSE;
}

bool ControlPrinter(const std::wstring& printerName, DWORD control) {
    PrinterHandle h(printerName);
    if (!h.valid()) {
        return false;
    }
    return ::SetPrinterW(h.get(), 0, nullptr, control) != FALSE;
}

std::wstring PrinterStatusToString(DWORD status) {
    if (status == 0) return L"Ready";
    std::wstring out;
    auto add = [&](const wchar_t* s) {
        if (!out.empty()) out += L", ";
        out += s;
    };
    if (status & PRINTER_STATUS_PAUSED)          add(L"Paused");
    if (status & PRINTER_STATUS_ERROR)           add(L"Error");
    if (status & PRINTER_STATUS_PENDING_DELETION)add(L"Deleting");
    if (status & PRINTER_STATUS_PAPER_JAM)       add(L"Paper jam");
    if (status & PRINTER_STATUS_PAPER_OUT)       add(L"Out of paper");
    if (status & PRINTER_STATUS_MANUAL_FEED)     add(L"Manual feed");
    if (status & PRINTER_STATUS_PAPER_PROBLEM)   add(L"Paper problem");
    if (status & PRINTER_STATUS_OFFLINE)         add(L"Offline");
    if (status & PRINTER_STATUS_IO_ACTIVE)       add(L"I/O active");
    if (status & PRINTER_STATUS_BUSY)            add(L"Busy");
    if (status & PRINTER_STATUS_PRINTING)        add(L"Printing");
    if (status & PRINTER_STATUS_OUTPUT_BIN_FULL) add(L"Output bin full");
    if (status & PRINTER_STATUS_NOT_AVAILABLE)   add(L"Not available");
    if (status & PRINTER_STATUS_WAITING)         add(L"Waiting");
    if (status & PRINTER_STATUS_PROCESSING)      add(L"Processing");
    if (status & PRINTER_STATUS_INITIALIZING)    add(L"Initializing");
    if (status & PRINTER_STATUS_WARMING_UP)      add(L"Warming up");
    if (status & PRINTER_STATUS_TONER_LOW)       add(L"Toner low");
    if (status & PRINTER_STATUS_NO_TONER)        add(L"No toner");
    if (status & PRINTER_STATUS_PAGE_PUNT)       add(L"Page punt");
    if (status & PRINTER_STATUS_USER_INTERVENTION) add(L"User intervention required");
    if (status & PRINTER_STATUS_OUT_OF_MEMORY)   add(L"Out of memory");
    if (status & PRINTER_STATUS_DOOR_OPEN)       add(L"Door open");
    if (status & PRINTER_STATUS_SERVER_UNKNOWN)  add(L"Server unknown");
    if (status & PRINTER_STATUS_POWER_SAVE)      add(L"Power save");
    return out.empty() ? L"Ready" : out;
}

std::wstring JobStatusToString(DWORD status) {
    if (status == 0) return L"Queued";
    std::wstring out;
    auto add = [&](const wchar_t* s) {
        if (!out.empty()) out += L", ";
        out += s;
    };
    if (status & JOB_STATUS_PAUSED)        add(L"Paused");
    if (status & JOB_STATUS_ERROR)         add(L"Error");
    if (status & JOB_STATUS_DELETING)      add(L"Deleting");
    if (status & JOB_STATUS_SPOOLING)      add(L"Spooling");
    if (status & JOB_STATUS_PRINTING)      add(L"Printing");
    if (status & JOB_STATUS_OFFLINE)       add(L"Offline");
    if (status & JOB_STATUS_PAPEROUT)      add(L"Paper out");
    if (status & JOB_STATUS_PRINTED)       add(L"Printed");
    if (status & JOB_STATUS_DELETED)       add(L"Deleted");
    if (status & JOB_STATUS_BLOCKED_DEVQ)  add(L"Blocked");
    if (status & JOB_STATUS_USER_INTERVENTION) add(L"User intervention");
    if (status & JOB_STATUS_RESTART)       add(L"Restart");
#ifdef JOB_STATUS_COMPLETE
    if (status & JOB_STATUS_COMPLETE)      add(L"Complete");
#endif
#ifdef JOB_STATUS_RETAINED
    if (status & JOB_STATUS_RETAINED)      add(L"Retained");
#endif
    return out.empty() ? L"Queued" : out;
}

std::wstring FormatSystemTime(const SYSTEMTIME& st) {
    if (st.wYear == 0) return L"-";
    // SYSTEMTIME from spooler is UTC; convert to local for display.
    SYSTEMTIME local{};
    FILETIME ft{}, localFt{};
    if (::SystemTimeToFileTime(&st, &ft) &&
        ::FileTimeToLocalFileTime(&ft, &localFt) &&
        ::FileTimeToSystemTime(&localFt, &local)) {
        // ok
    } else {
        local = st;
    }

    wchar_t buf[64];
    ::swprintf(buf, 64, L"%04u-%02u-%02u %02u:%02u:%02u",
               local.wYear, local.wMonth, local.wDay,
               local.wHour, local.wMinute, local.wSecond);
    return buf;
}

std::wstring FormatLastError(DWORD err) {
    LPWSTR msg = nullptr;
    DWORD len = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&msg), 0, nullptr);
    std::wstring out;
    if (len && msg) {
        out.assign(msg, len);
        while (!out.empty() && (out.back() == L'\r' || out.back() == L'\n' || out.back() == L' ')) {
            out.pop_back();
        }
    }
    if (msg) ::LocalFree(msg);
    if (out.empty()) {
        wchar_t buf[32];
        ::swprintf(buf, 32, L"Error %lu", err);
        out = buf;
    }
    return out;
}

} // namespace Spooler
