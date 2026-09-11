#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objbase.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include "WebView2.h"
#pragma comment(lib, "ole32.lib")

static HWND hWnd = NULL;
static ICoreWebView2Controller* webviewController = NULL;
static ICoreWebView2* webview = NULL;
static std::wstring exeDir;

std::wstring GetExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring path(buf);
    size_t pos = path.find_last_of(L"\\");
    return path.substr(0, pos);
}

class ControllerCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
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

            // Map virtual host to local assets folder
            ICoreWebView2_3* wv3 = NULL;
            webview->QueryInterface(IID_ICoreWebView2_3, (void**)&wv3);
            if (wv3) {
                wv3->SetVirtualHostNameToFolderMapping(
                    L"qqshow.local",
                    exeDir.c_str(),
                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                wv3->Release();
            }

            // Navigate via virtual host
            webview->Navigate(L"https://qqshow.local/index.html");

            RECT bounds;
            GetClientRect(hWnd, &bounds);
            webviewController->put_Bounds(bounds);
        }
        return S_OK;
    }
};

class EnvCompletedHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
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
            auto handler = new ControllerCompletedHandler();
            env->CreateCoreWebView2Controller(hWnd, handler);
        }
        return S_OK;
    }
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (webviewController) {
            RECT bounds;
            GetClientRect(hwnd, &bounds);
            webviewController->put_Bounds(bounds);
        }
        return 0;
    case WM_DESTROY:
        if (webviewController) { webviewController->Release(); webviewController = NULL; }
        if (webview) { webview->Release(); webview = NULL; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nShow) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    exeDir = GetExeDir();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"QQShow2000";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    hWnd = CreateWindowExW(0, L"QQShow2000", L"QQ秀2000",
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 269, 465,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    auto handler = new EnvCompletedHandler();
    CreateCoreWebView2EnvironmentWithOptions(NULL, NULL, NULL, handler);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return (int)msg.wParam;
}
