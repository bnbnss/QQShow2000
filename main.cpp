#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <string>
#include "WebView2.h"
#pragma comment(lib, "ole32.lib")

static HWND hWnd = NULL;
static ICoreWebView2Controller* webviewController = NULL;
static ICoreWebView2* webview = NULL;
static std::wstring exeDir;
static HHOOK g_mouseHook = NULL;
static BOOL g_dragging = FALSE;
static int g_dragOffX = 0, g_dragOffY = 0;

std::wstring GetExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring path(buf);
    return path.substr(0, path.find_last_of(L'\\'));
}

class ControllerHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
    ULONG refCount = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef(); return S_OK;
        }
        *ppv = NULL; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG c = --refCount; if (c == 0) delete this; return c; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) override {
        if (controller) {
            webviewController = controller;
            webviewController->AddRef();
            webviewController->get_CoreWebView2(&webview);
            ICoreWebView2_3* wv3 = NULL;
            webview->QueryInterface(IID_ICoreWebView2_3, (void**)&wv3);
            if (wv3) {
                wv3->SetVirtualHostNameToFolderMapping(L"qqshow.local", exeDir.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                wv3->Release();
            }
            webview->Navigate(L"https://qqshow.local/index.html");
            RECT bounds;
            GetClientRect(hWnd, &bounds);
            webviewController->put_Bounds(bounds);

            // Disable dev tools, refresh, F12, etc.
            ICoreWebView2Settings* settings = NULL;
            webview->get_Settings(&settings);
            if (settings) {
                settings->put_AreDevToolsEnabled(FALSE);
                settings->put_AreDefaultContextMenusEnabled(FALSE);
                settings->put_IsStatusBarEnabled(FALSE);
                settings->Release();
            }

            // Block F5/F12/refresh shortcuts via accelerator key
            class AccelHandler : public ICoreWebView2AcceleratorKeyPressedEventHandler {
            public:
                ULONG refCount = 1;
                HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
                    if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2AcceleratorKeyPressedEventHandler)) {
                        *ppv = static_cast<ICoreWebView2AcceleratorKeyPressedEventHandler*>(this);
                        AddRef(); return S_OK;
                    }
                    *ppv = NULL; return E_NOINTERFACE;
                }
                ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
                ULONG STDMETHODCALLTYPE Release() override { ULONG c = --refCount; if (c == 0) delete this; return c; }
                HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2Controller* ctrl, ICoreWebView2AcceleratorKeyPressedEventArgs* args) override {
                    UINT key;
                    args->get_VirtualKey(&key);
                    if (key == VK_F5 || key == VK_F12) {
                        args->put_Handled(TRUE);
                        return S_OK;
                    }
                    BOOL ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    BOOL shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    if (ctrlDown && (key == 'R' || key == 'r')) { args->put_Handled(TRUE); return S_OK; }
                    if (ctrlDown && shiftDown && (key == 'I' || key == 'i')) { args->put_Handled(TRUE); return S_OK; }
                    if (ctrlDown && (key == 'U' || key == 'u')) { args->put_Handled(TRUE); return S_OK; }
                    args->put_Handled(FALSE);
                    return S_OK;
                }
            };
            EventRegistrationToken accelToken;
            webviewController->add_AcceleratorKeyPressed(new AccelHandler(), &accelToken);

            // Handle messages from JS (window drag)
            class MsgHandler : public ICoreWebView2WebMessageReceivedEventHandler {
            public:
                ULONG refCount = 1;
                HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
                    if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2WebMessageReceivedEventHandler)) {
                        *ppv = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
                        AddRef(); return S_OK;
                    }
                    *ppv = NULL; return E_NOINTERFACE;
                }
                ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
                ULONG STDMETHODCALLTYPE Release() override { ULONG c = --refCount; if (c == 0) delete this; return c; }
                HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
                    LPWSTR json = NULL;
                    args->get_WebMessageAsJson(&json);
                    if (json) {
                        std::wstring s(json);
                        CoTaskMemFree(json);
                        if (s.length() >= 2 && s.front() == L'"' && s.back() == L'"')
                            s = s.substr(1, s.length() - 2);
                        if (s == L"close") {
                            DestroyWindow(hWnd);
                        }
                    }
                    return S_OK;
                }
            };
            EventRegistrationToken msgToken;
            webview->add_WebMessageReceived(new MsgHandler(), &msgToken);
        }
        return S_OK;
    }
};

class EnvHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    ULONG refCount = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == __uuidof(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
            *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef(); return S_OK;
        }
        *ppv = NULL; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG c = --refCount; if (c == 0) delete this; return c; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* env) override {
        if (env) {
            env->CreateCoreWebView2Controller(hWnd, new ControllerHandler());
        }
        return S_OK;
    }
};

static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        MSLLHOOKSTRUCT* ms = (MSLLHOOKSTRUCT*)lParam;
        if (wParam == WM_LBUTTONDOWN) {
            if (hWnd) {
                POINT pt = ms->pt;
                ScreenToClient(hWnd, &pt);
                RECT rc;
                GetClientRect(hWnd, &rc);
                if (pt.x >= 0 && pt.x < rc.right && pt.y >= 0 && pt.y < 55) {
                    g_dragging = TRUE;
                    g_dragOffX = ms->pt.x;
                    g_dragOffY = ms->pt.y;
                    RECT wrc;
                    GetWindowRect(hWnd, &wrc);
                    g_dragOffX -= wrc.left;
                    g_dragOffY -= wrc.top;
                }
            }
        } else if (wParam == WM_MOUSEMOVE && g_dragging) {
            SetWindowPos(hWnd, NULL, ms->pt.x - g_dragOffX, ms->pt.y - g_dragOffY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        } else if (wParam == WM_LBUTTONUP) {
            g_dragging = FALSE;
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        if (webviewController) {
            RECT bounds;
            GetClientRect(hwnd, &bounds);
            webviewController->put_Bounds(bounds);
        }
        return 0;
    case WM_DESTROY:
        if (g_mouseHook) { UnhookWindowsHookEx(g_mouseHook); g_mouseHook = NULL; }
        if (webviewController) { webviewController->Release(); webviewController = NULL; }
        if (webview) { webview->Release(); webview = NULL; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hI, HINSTANCE, LPWSTR, int nShow) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    exeDir = GetExeDir();

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.lpszClassName = L"QQShow2000";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    hWnd = CreateWindowExW(0, L"QQShow2000", L"QQ秀2000",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, 269, 465,
        NULL, NULL, hI, NULL);

    // Center on screen
    int sx = (GetSystemMetrics(SM_CXSCREEN) - 269) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - 465) / 2;
    SetWindowPos(hWnd, NULL, sx, sy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    // Install global mouse hook for window dragging
    g_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, hI, 0);

    std::wstring userDataFolder = exeDir + L"\\webview2_data";
    CreateCoreWebView2EnvironmentWithOptions(NULL, userDataFolder.c_str(), NULL, new EnvHandler());

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return (int)msg.wParam;
}
