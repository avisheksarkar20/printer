#pragma once

#include <windows.h>
#include <string>
#include <vector>

#include "SpoolerAPI.h"

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInstance, int nCmdShow);
    HWND Handle() const { return hwnd_; }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnCreate();
    void OnSize(int width, int height);
    void OnCommand(WORD id, WORD notifyCode, HWND ctrl);
    void OnNotify(NMHDR* nmhdr);
    void OnContextMenu(HWND target, int x, int y);
    void OnTimer();
    void OnDestroy();

    void CreateChildControls();
    void LoadPrinters();
    void RefreshJobs();
    void UpdateStatusBar();

    std::wstring GetSelectedPrinterName() const;
    std::vector<DWORD> GetSelectedJobIds() const;
    int FindJobRowById(DWORD jobId) const;

    void DoJobAction(Spooler::JobAction action);
    void DoMoveSelected(int delta);
    void DoPrinterPauseResume(bool pause);
    void ShowAbout();
    void ReportError(const std::wstring& context);

    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND hCombo_ = nullptr;
    HWND hRefresh_ = nullptr;
    HWND hList_ = nullptr;
    HWND hStatus_ = nullptr;

    std::vector<Spooler::PrinterInfo> printers_;
    std::vector<Spooler::JobInfo> jobs_;
};
