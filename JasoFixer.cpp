#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <commctrl.h>
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "normaliz.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace std;

// Configuration
const COLORREF COLOR_BG = RGB(28, 28, 30);
const COLORREF COLOR_TEXT = RGB(242, 242, 247);
const COLORREF COLOR_ACCENT = RGB(10, 132, 255);
const COLORREF COLOR_CLOSE_HOVER = RGB(255, 59, 48);

// Global State
bool g_isHoveringClose = false;
bool g_isDragging = false;

// Function to normalize NFD (Mac) to NFC (Windows)
wstring NormalizeToNFC(const wstring& input) {
    int size = NormalizeString(NormalizationC, input.c_str(), -1, NULL, 0);
    if (size <= 0) return input;
    vector<wchar_t> buffer(size);
    NormalizeString(NormalizationC, input.c_str(), -1, buffer.data(), size);
    return wstring(buffer.data());
}

// Function to fix jaso in filenames recursively
void ProcessPath(const wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) return;

    if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) {
        wstring searchPath = path + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                    ProcessPath(path + L"\\" + fd.cFileName);
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }
    }

    // Process the name itself
    size_t lastBackslash = path.find_last_of(L"\\/");
    wstring directory = (lastBackslash == wstring::npos) ? L"" : path.substr(0, lastBackslash);
    wstring filename = (lastBackslash == wstring::npos) ? path : path.substr(lastBackslash + 1);

    if (filename == L"." || filename == L"..") return;

    wstring normalizedFilename = NormalizeToNFC(filename);
    if (filename != normalizedFilename) {
        wstring newPath = directory.empty() ? normalizedFilename : directory + L"\\" + normalizedFilename;
        // MoveFileExW is more reliable
        MoveFileExW(path.c_str(), newPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static RECT closeRect = { 270, 5, 295, 30 };

    switch (message) {
    case WM_CREATE: {
        DragAcceptFiles(hWnd, TRUE);
        break;
    }
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        for (UINT i = 0; i < fileCount; i++) {
            wchar_t filePath[MAX_PATH];
            DragQueryFileW(hDrop, i, filePath, MAX_PATH);
            ProcessPath(filePath);
        }
        DragFinish(hDrop);
        InvalidateRect(hWnd, NULL, TRUE);
        MessageBoxW(hWnd, L"모든 파일의 자소 합치기가 완료되었습니다.", L"완료", MB_OK | MB_ICONINFORMATION);
        break;
    }
    case WM_NCHITTEST: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hWnd, &pt);
        if (PtInRect(&closeRect, pt)) return HTCLIENT; // Important: Return HTCLIENT for the button area
        
        LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
        if (hit == HTCLIENT) return HTCAPTION;
        return hit;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        bool wasHovering = g_isHoveringClose;
        g_isHoveringClose = PtInRect(&closeRect, pt);
        if (wasHovering != g_isHoveringClose) {
            InvalidateRect(hWnd, &closeRect, FALSE);
            
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        if (g_isHoveringClose) {
            g_isHoveringClose = false;
            InvalidateRect(hWnd, &closeRect, FALSE);
        }
        break;
    }
    case WM_LBUTTONUP: {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        if (PtInRect(&closeRect, pt)) {
            SendMessage(hWnd, WM_CLOSE, 0, 0);
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rect;
        GetClientRect(hWnd, &rect);

        // Double buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        SelectObject(memDC, memBitmap);

        // 1. Background
        HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG);
        FillRect(memDC, &rect, hBgBrush);
        DeleteObject(hBgBrush);

        // Border
        HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 65));
        SelectObject(memDC, hBorderPen);
        MoveToEx(memDC, 0, 0, NULL);
        LineTo(memDC, rect.right - 1, 0);
        LineTo(memDC, rect.right - 1, rect.bottom - 1);
        LineTo(memDC, 0, rect.bottom - 1);
        LineTo(memDC, 0, 0);
        DeleteObject(hBorderPen);

        // 2. Custom Close Button
        if (g_isHoveringClose) {
            HBRUSH hCloseBrush = CreateSolidBrush(COLOR_CLOSE_HOVER);
            FillRect(memDC, &closeRect, hCloseBrush);
            DeleteObject(hCloseBrush);
        }
        SetTextColor(memDC, RGB(255, 255, 255));
        SetBkMode(memDC, TRANSPARENT);
        DrawTextW(memDC, L"×", -1, &closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 3. Main Text
        HFONT hTitleFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, L"Malgun Gothic");
        HFONT hMainFont = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, L"Malgun Gothic");

        RECT textRect = rect;
        textRect.top = 45;
        
        SelectObject(memDC, hTitleFont);
        SetTextColor(memDC, COLOR_ACCENT);
        DrawTextW(memDC, L"한글 자소 합성기", -1, &textRect, DT_CENTER | DT_TOP | DT_SINGLELINE);

        textRect.top = 85;
        SelectObject(memDC, hMainFont);
        SetTextColor(memDC, COLOR_TEXT);
        
        // Use separate strings to avoid multi-line literal encoding issues
        const wchar_t* line1 = L"맥(Mac)에서 가져온 파일이나";
        const wchar_t* line2 = L"폴더를 여기에 드롭하세요";
        
        RECT r1 = textRect;
        DrawTextW(memDC, line1, -1, &r1, DT_CENTER | DT_TOP | DT_SINGLELINE);
        r1.top += 25;
        DrawTextW(memDC, line2, -1, &r1, DT_CENTER | DT_TOP | DT_SINGLELINE);

        DeleteObject(hTitleFont);
        DeleteObject(hMainFont);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
        
        DeleteObject(memBitmap);
        DeleteDC(memDC);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"JasoFixerPremium";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));

    RegisterClassW(&wc);

    int width = 300;
    int height = 180;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        CLASS_NAME,
        L"Jaso Fixer",
        WS_POPUP,
        (screenWidth - width) / 2,
        (screenHeight - height) / 2,
        width, height,
        NULL, NULL, hInstance, NULL
    );

    if (hWnd == NULL) return 0;

    // Set transparency/opacity (optional, but looks premium)
    SetLayeredWindowAttributes(hWnd, 0, 245, LWA_ALPHA);
    ShowWindow(hWnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
