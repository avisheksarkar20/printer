#include "MainWindow.h"
#include "resource.h"

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

namespace {

constexpr wchar_t kWindowClassName[] = L"PrintQueueManagerMainWindow";
constexpr wchar_t kWindowTitle[]     = L"Print Queue Manager";
constexpr UINT    kAutoRefreshMs     = 3000;

constexpr int kTopBarHeight   = 40;
constexpr int kComboWidth     = 360;
constexpr int kButtonWidth    = 100;
constexpr int kPadding        = 8;

struct Column {
    const wchar_t* title;
    int width;
};

const Column kColumns[] = {
    { L"Job ID",     70  },
    { L"Document",   300 },
    { L"Owner",      140 },
    { L"Pages",      80  },
    { L"Size (KB)",  90  },
    { L"Status",     180 },
    { L"Submitted",  150 },
    { L"Position",   70  },
};

} // namespace

MainWindow::MainWindow() = default;
MainWindow::~MainWindow() = default;

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    hInstance_ = hInstance;

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    ::InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = &MainWindow::WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kWindowClassName;
    wc.hIcon         = ::LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP));
    if (!wc.hIcon) {
        wc.hIcon = ::LoadIcon(nullptr, IDI_APPLICATION);
    }
    wc.hIconSm       = wc.hIcon;

    if (!::RegisterClassExW(&wc)) {
        return false;
    }

    HMENU hMenu = ::LoadMenuW(hInstance, MAKEINTRESOURCEW(IDR_MAINMENU));

    hwnd_ = ::CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1100, 620,
        nullptr, hMenu, hInstance, this);

    if (!hwnd_) {
        return false;
    }

    ::ShowWindow(hwnd_, nCmdShow);
    ::UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(msg, wParam, lParam);
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            OnCreate();
            return 0;
        case WM_SIZE:
            OnSize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_COMMAND:
            OnCommand(LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HWND>(lParam));
            return 0;
        case WM_NOTIFY:
            OnNotify(reinterpret_cast<NMHDR*>(lParam));
            return 0;
        case WM_CONTEXTMENU:
            OnContextMenu(reinterpret_cast<HWND>(wParam),
                          GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_TIMER:
            if (wParam == IDT_AUTO_REFRESH) {
                OnTimer();
            }
            return 0;
        case WM_DESTROY:
            OnDestroy();
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void MainWindow::OnCreate() {
    CreateChildControls();
    LoadPrinters();
    RefreshJobs();
    ::SetTimer(hwnd_, IDT_AUTO_REFRESH, kAutoRefreshMs, nullptr);
}

void MainWindow::CreateChildControls() {
    hCombo_ = ::CreateWindowExW(
        0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        kPadding, kPadding, kComboWidth, 300,
        hwnd_, reinterpret_cast<HMENU>(IDC_PRINTER_COMBO),
        hInstance_, nullptr);

    hRefresh_ = ::CreateWindowExW(
        0, L"BUTTON", L"Refresh",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        kPadding * 2 + kComboWidth, kPadding, kButtonWidth, 24,
        hwnd_, reinterpret_cast<HMENU>(IDC_REFRESH_BTN),
        hInstance_, nullptr);

    hList_ = ::CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
        kPadding, kTopBarHeight, 800, 400,
        hwnd_, reinterpret_cast<HMENU>(IDC_JOB_LIST),
        hInstance_, nullptr);

    ListView_SetExtendedListViewStyle(
        hList_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    for (int i = 0; i < static_cast<int>(sizeof(kColumns) / sizeof(kColumns[0])); ++i) {
        LVCOLUMNW col{};
        col.mask    = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = const_cast<LPWSTR>(kColumns[i].title);
        col.cx      = kColumns[i].width;
        col.iSubItem = i;
        ListView_InsertColumn(hList_, i, &col);
    }

    hStatus_ = ::CreateWindowExW(
        0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwnd_, reinterpret_cast<HMENU>(IDC_STATUS_BAR),
        hInstance_, nullptr);

    HFONT hFont = reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
    ::SendMessageW(hCombo_,   WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    ::SendMessageW(hRefresh_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    ::SendMessageW(hList_,    WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
}

void MainWindow::OnSize(int width, int height) {
    if (hStatus_) {
        ::SendMessageW(hStatus_, WM_SIZE, 0, 0);
    }

    RECT statusRect{};
    if (hStatus_) {
        ::GetWindowRect(hStatus_, &statusRect);
    }
    int statusHeight = statusRect.bottom - statusRect.top;

    int listTop = kTopBarHeight;
    int listHeight = height - listTop - statusHeight - kPadding;
    if (listHeight < 50) listHeight = 50;
    int listWidth = width - 2 * kPadding;
    if (listWidth < 100) listWidth = 100;

    ::SetWindowPos(hList_, nullptr,
                   kPadding, listTop, listWidth, listHeight,
                   SWP_NOZORDER);
}

void MainWindow::OnCommand(WORD id, WORD notifyCode, HWND /*ctrl*/) {
    switch (id) {
        case IDC_PRINTER_COMBO:
            if (notifyCode == CBN_SELCHANGE) {
                RefreshJobs();
            }
            break;
        case IDC_REFRESH_BTN:
        case IDM_FILE_REFRESH:
            LoadPrinters();
            RefreshJobs();
            break;
        case IDM_FILE_EXIT:
            ::DestroyWindow(hwnd_);
            break;
        case IDM_JOB_PAUSE:
            DoJobAction(Spooler::JobAction::Pause);
            break;
        case IDM_JOB_RESUME:
            DoJobAction(Spooler::JobAction::Resume);
            break;
        case IDM_JOB_CANCEL:
            DoJobAction(Spooler::JobAction::Cancel);
            break;
        case IDM_JOB_RESTART:
            DoJobAction(Spooler::JobAction::Restart);
            break;
        case IDM_JOB_MOVEUP:
            DoMoveSelected(-1);
            break;
        case IDM_JOB_MOVEDOWN:
            DoMoveSelected(+1);
            break;
        case IDM_PRINTER_PAUSE:
            DoPrinterPauseResume(true);
            break;
        case IDM_PRINTER_RESUME:
            DoPrinterPauseResume(false);
            break;
        case IDM_HELP_ABOUT:
            ShowAbout();
            break;
    }
}

void MainWindow::OnNotify(NMHDR* nmhdr) {
    if (!nmhdr) return;
    if (nmhdr->hwndFrom == hList_ && nmhdr->code == NM_DBLCLK) {
        DoJobAction(Spooler::JobAction::Pause);
    }
}

void MainWindow::OnContextMenu(HWND target, int x, int y) {
    if (target != hList_) return;

    HMENU hMenu = ::LoadMenuW(hInstance_, MAKEINTRESOURCEW(IDR_JOBMENU));
    if (!hMenu) return;
    HMENU hSub = ::GetSubMenu(hMenu, 0);

    if (x == -1 && y == -1) {
        RECT rc{};
        ::GetWindowRect(hList_, &rc);
        x = rc.left + 20;
        y = rc.top + 20;
    }

    ::TrackPopupMenu(hSub, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                     x, y, 0, hwnd_, nullptr);
    ::DestroyMenu(hMenu);
}

void MainWindow::OnTimer() {
    RefreshJobs();
}

void MainWindow::OnDestroy() {
    ::KillTimer(hwnd_, IDT_AUTO_REFRESH);
}

void MainWindow::LoadPrinters() {
    std::wstring previous;
    int selIdx = static_cast<int>(::SendMessageW(hCombo_, CB_GETCURSEL, 0, 0));
    if (selIdx != CB_ERR && selIdx < static_cast<int>(printers_.size())) {
        previous = printers_[selIdx].name;
    }

    printers_ = Spooler::EnumeratePrinters();
    ::SendMessageW(hCombo_, CB_RESETCONTENT, 0, 0);

    int toSelect = -1;
    for (size_t i = 0; i < printers_.size(); ++i) {
        ::SendMessageW(hCombo_, CB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(printers_[i].name.c_str()));
        if (printers_[i].name == previous) {
            toSelect = static_cast<int>(i);
        }
    }

    if (!printers_.empty()) {
        if (toSelect < 0) toSelect = 0;
        ::SendMessageW(hCombo_, CB_SETCURSEL, toSelect, 0);
    }
}

std::wstring MainWindow::GetSelectedPrinterName() const {
    int idx = static_cast<int>(::SendMessageW(hCombo_, CB_GETCURSEL, 0, 0));
    if (idx == CB_ERR || idx >= static_cast<int>(printers_.size())) {
        return L"";
    }
    return printers_[idx].name;
}

void MainWindow::RefreshJobs() {
    std::wstring printer = GetSelectedPrinterName();

    std::vector<DWORD> previouslySelected = GetSelectedJobIds();

    ListView_DeleteAllItems(hList_);
    jobs_.clear();

    if (!printer.empty()) {
        jobs_ = Spooler::EnumerateJobs(printer);

        for (size_t i = 0; i < jobs_.size(); ++i) {
            const auto& j = jobs_[i];

            wchar_t idBuf[16];
            ::swprintf(idBuf, 16, L"%lu", j.jobId);
            wchar_t pagesBuf[32];
            ::swprintf(pagesBuf, 32, L"%lu / %lu", j.pagesPrinted, j.totalPages);
            wchar_t sizeBuf[32];
            ::swprintf(sizeBuf, 32, L"%lu", j.sizeBytes / 1024);
            wchar_t posBuf[16];
            ::swprintf(posBuf, 16, L"%lu", j.position);

            std::wstring submitted = Spooler::FormatSystemTime(j.submitted);
            std::wstring status    = Spooler::JobStatusToString(j.status);

            LVITEMW item{};
            item.mask    = LVIF_TEXT | LVIF_PARAM;
            item.iItem   = static_cast<int>(i);
            item.iSubItem = 0;
            item.pszText = idBuf;
            item.lParam  = static_cast<LPARAM>(j.jobId);
            int row = ListView_InsertItem(hList_, &item);

            ListView_SetItemText(hList_, row, 1, const_cast<LPWSTR>(j.documentName.c_str()));
            ListView_SetItemText(hList_, row, 2, const_cast<LPWSTR>(j.userName.c_str()));
            ListView_SetItemText(hList_, row, 3, pagesBuf);
            ListView_SetItemText(hList_, row, 4, sizeBuf);
            ListView_SetItemText(hList_, row, 5, const_cast<LPWSTR>(status.c_str()));
            ListView_SetItemText(hList_, row, 6, const_cast<LPWSTR>(submitted.c_str()));
            ListView_SetItemText(hList_, row, 7, posBuf);
        }

        // Restore prior selection by jobId when possible.
        for (DWORD id : previouslySelected) {
            int row = FindJobRowById(id);
            if (row >= 0) {
                ListView_SetItemState(hList_, row,
                                      LVIS_SELECTED | LVIS_FOCUSED,
                                      LVIS_SELECTED | LVIS_FOCUSED);
            }
        }
    }

    UpdateStatusBar();
}

void MainWindow::UpdateStatusBar() {
    std::wstring msg;
    std::wstring printer = GetSelectedPrinterName();
    if (printer.empty()) {
        msg = L"No printer selected.";
    } else {
        // Find status from printers_.
        DWORD status = 0;
        for (const auto& p : printers_) {
            if (p.name == printer) {
                status = p.status;
                break;
            }
        }
        wchar_t buf[512];
        ::swprintf(buf, 512, L"Printer: %ls  |  State: %ls  |  Jobs: %zu",
                   printer.c_str(),
                   Spooler::PrinterStatusToString(status).c_str(),
                   jobs_.size());
        msg = buf;
    }
    ::SendMessageW(hStatus_, SB_SETTEXTW, 0,
                   reinterpret_cast<LPARAM>(msg.c_str()));
}

std::vector<DWORD> MainWindow::GetSelectedJobIds() const {
    std::vector<DWORD> ids;
    int i = -1;
    while ((i = ListView_GetNextItem(hList_, i, LVNI_SELECTED)) != -1) {
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        if (ListView_GetItem(hList_, &item)) {
            ids.push_back(static_cast<DWORD>(item.lParam));
        }
    }
    return ids;
}

int MainWindow::FindJobRowById(DWORD jobId) const {
    int count = ListView_GetItemCount(hList_);
    for (int i = 0; i < count; ++i) {
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = i;
        if (ListView_GetItem(hList_, &item) && static_cast<DWORD>(item.lParam) == jobId) {
            return i;
        }
    }
    return -1;
}

void MainWindow::DoJobAction(Spooler::JobAction action) {
    std::wstring printer = GetSelectedPrinterName();
    if (printer.empty()) return;

    auto ids = GetSelectedJobIds();
    if (ids.empty()) return;

    if (action == Spooler::JobAction::Cancel) {
        int n = static_cast<int>(ids.size());
        wchar_t prompt[128];
        ::swprintf(prompt, 128,
                   L"Cancel %d selected print job%ls?",
                   n, n == 1 ? L"" : L"s");
        if (::MessageBoxW(hwnd_, prompt, L"Confirm cancel",
                          MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
    }

    bool anyFailed = false;
    for (DWORD id : ids) {
        if (!Spooler::ControlJob(printer, id, action)) {
            anyFailed = true;
        }
    }
    if (anyFailed) {
        ReportError(L"One or more jobs could not be updated");
    }
    RefreshJobs();
}

void MainWindow::DoMoveSelected(int delta) {
    std::wstring printer = GetSelectedPrinterName();
    if (printer.empty()) return;

    auto ids = GetSelectedJobIds();
    if (ids.size() != 1) {
        ::MessageBoxW(hwnd_,
                      L"Select exactly one job to move.",
                      L"Move job",
                      MB_OK | MB_ICONINFORMATION);
        return;
    }

    DWORD jobId = ids[0];
    const Spooler::JobInfo* job = nullptr;
    for (const auto& j : jobs_) {
        if (j.jobId == jobId) { job = &j; break; }
    }
    if (!job) return;

    long newPos = static_cast<long>(job->position) + delta;
    if (newPos < 1) newPos = 1;

    if (!Spooler::SetJobPosition(printer, jobId, static_cast<DWORD>(newPos))) {
        ReportError(L"Could not reorder job");
    }
    RefreshJobs();
}

void MainWindow::DoPrinterPauseResume(bool pause) {
    std::wstring printer = GetSelectedPrinterName();
    if (printer.empty()) return;

    DWORD cmd = pause ? PRINTER_CONTROL_PAUSE : PRINTER_CONTROL_RESUME;
    if (!Spooler::ControlPrinter(printer, cmd)) {
        ReportError(pause ? L"Could not pause printer" : L"Could not resume printer");
    }
    LoadPrinters();
    RefreshJobs();
}

void MainWindow::ShowAbout() {
    ::MessageBoxW(hwnd_,
        L"Print Queue Manager\r\n"
        L"A native Win32 tool for managing the Windows print spooler.\r\n\r\n"
        L"Uses: EnumPrinters, EnumJobs, SetJob, SetPrinter.",
        L"About Print Queue Manager",
        MB_OK | MB_ICONINFORMATION);
}

void MainWindow::ReportError(const std::wstring& context) {
    DWORD err = ::GetLastError();
    std::wstring msg = context + L":\r\n" + Spooler::FormatLastError(err);
    ::MessageBoxW(hwnd_, msg.c_str(), L"Print Queue Manager",
                  MB_OK | MB_ICONWARNING);
}
