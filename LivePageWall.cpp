#define UNICODE
#define NOMINMAX
#include "WebView2.h"
#include <Shlwapi.h>
#include <shellapi.h>
#include <string>
#include <windows.h>
#include <wrl.h>
#pragma comment(lib, "Shlwapi.lib")
extern "C" {
extern int __argc;
extern wchar_t **__wargv;
}

using Microsoft::WRL::ComPtr;

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000
#endif
constexpr UINT kSwpPosNoZNoAct =
    SWP_NOZORDER | SWP_NOACTIVATE; // 位置/サイズのみ変更
constexpr UINT kSwpZOnlyNoAct =
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE; // Z順のみ変更

static HWND gProgman = nullptr, gWorkerW = nullptr, gDefView = nullptr;
static BOOL gIsRaised = FALSE;
static ComPtr<ICoreWebView2Environment> gEnv;
static ComPtr<ICoreWebView2Controller> gCtl;
static ComPtr<ICoreWebView2> gWV;
static HWND gHost = nullptr;

static BOOL HasEx(HWND h, DWORD ex) {
  return (GetWindowLongPtrW(h, GWL_EXSTYLE) & ex) == ex;
}

static BOOL CALLBACK EnumWinProc(HWND top, LPARAM) {
  HWND dv = FindWindowExW(top, nullptr, L"SHELLDLL_DefView", nullptr);
  if (dv) {
    gDefView = dv;
    gWorkerW = FindWindowExW(nullptr, top, L"WorkerW", nullptr);
  }
  return TRUE;
}

static void SetupDesktopHandles() {
  gProgman = FindWindowW(L"Progman", nullptr);
  if (!gProgman)
    return;
  gIsRaised = HasEx(gProgman, WS_EX_NOREDIRECTIONBITMAP);

  // 0x052CでWorkerW生成を促す
  SendMessageTimeoutW(gProgman, 0x052C, (WPARAM)0xD, (LPARAM)0x1, SMTO_NORMAL,
                      1000, nullptr);

  // DefView/WorkerWを探す
  gDefView = gWorkerW = nullptr;
  EnumWindows(EnumWinProc, 0);
  if (gIsRaised) {
    if (!gWorkerW)
      gWorkerW = FindWindowExW(gProgman, nullptr, L"WorkerW", nullptr);
    if (!gDefView)
      gDefView = FindWindowExW(gProgman, nullptr, L"SHELLDLL_DefView", nullptr);
  }
}

static void FitAndOrder(HWND hwnd) {
  HWND parent = gIsRaised ? gProgman : (gWorkerW ? gWorkerW : gProgman);
  if (!parent)
    return;
  if (gIsRaised) {
    RECT r{};
    GetWindowRect(gWorkerW, &r);                             // 画面座標
    MapWindowPoints(HWND_DESKTOP, gProgman, (LPPOINT)&r, 2); // Progman基準へ
    SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left,
                 r.bottom - r.top, kSwpPosNoZNoAct);
    if (gDefView) {
      SetWindowPos(hwnd, gDefView, 0, 0, 0, 0, kSwpZOnlyNoAct); // DefView直下
    }
  } else {
    RECT rc{};
    GetClientRect(parent, &rc);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right, rc.bottom, kSwpPosNoZNoAct);
  }
}

static void ResizeWebViewTo(HWND hTarget) {
  if (!gCtl)
    return;
  RECT rc{};
  GetClientRect(hTarget, &rc);
  gCtl->put_Bounds(rc);
  gCtl->NotifyParentWindowPositionChanged();
}

static std::wstring GetExeDir() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring p(buf);
  size_t pos = p.find_last_of(L"\\/");
  return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

static std::wstring MakeFileUrl(const std::wstring &path) {
  wchar_t url[4096];
  DWORD cch = _countof(url);
  if (SUCCEEDED(UrlCreateFromPathW(path.c_str(), url, &cch, 0))) {
    return std::wstring(url);
  }
  std::wstring q = path;
  for (auto &ch : q)
    if (ch == L'\\')
      ch = L'/';
  return L"file:///" + q;
}

static std::wstring ResolveStartUri() {
  const std::wstring exeDir = GetExeDir();
  auto to_abs_fileurl = [&](std::wstring p) -> std::wstring {
    if (PathIsRelativeW(p.c_str()))
      p = exeDir + L"\\" + p;
    wchar_t full[MAX_PATH];
    DWORD n = GetFullPathNameW(p.c_str(), MAX_PATH, full, nullptr);
    return MakeFileUrl(n ? std::wstring(full, n) : p);
  };

  if (__argc >= 2) {
    std::wstring a = __wargv[1];
    if (PathIsURLW(a.c_str()))
      return a;                          // http(s), file:, data: など
    return to_abs_fileurl(std::move(a)); // ローカルファイル（相対も可）
  }
  return to_abs_fileurl(L"resources/usage.html"); // 引数なしデフォルト
}

static void InitWebView2(HWND parent) {
  auto createCtl = [parent](ICoreWebView2Environment *env) -> HRESULT {
    return env->CreateCoreWebView2Controller(
        parent, Microsoft::WRL::Callback<
                    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [](HRESULT hr, ICoreWebView2Controller *ctl) -> HRESULT {
                      if (FAILED(hr))
                        return hr;
                      gCtl = ctl;
                      gCtl->get_CoreWebView2(&gWV);
                      gCtl->put_IsVisible(TRUE);
                      ResizeWebViewTo(gHost);

                      std::wstring start = ResolveStartUri();
                      gWV->Navigate(start.c_str());

                      return S_OK;
                    })
                    .Get());
  };
  if (gEnv) {
    createCtl(gEnv.Get());
    return;
  }
  CreateCoreWebView2EnvironmentWithOptions(
      nullptr, nullptr, nullptr,
      Microsoft::WRL::Callback<
          ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [createCtl](HRESULT hr, ICoreWebView2Environment *env) -> HRESULT {
            if (FAILED(hr))
              return hr;
            gEnv = env;
            return createCtl(env);
          })
          .Get());
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(h, &ps);
    EndPaint(h, &ps);
    return 0;
  }
  case WM_SIZE:
    ResizeWebViewTo(h);
    return 0;
  case WM_DISPLAYCHANGE:
  case WM_SETTINGCHANGE:
    FitAndOrder(h);
    ResizeWebViewTo(h);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

static HWND CreateHost(HINSTANCE hi) {
  WNDCLASSW wc{};
  wc.hInstance = hi;
  wc.lpszClassName = L"WVHost";
  wc.lpfnWndProc = WndProc;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  HWND hwnd =
      CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP, CW_USEDEFAULT,
                      CW_USEDEFAULT, 100, 100, nullptr, nullptr, hi, nullptr);
  if (!hwnd)
    return nullptr;
  // 子化
  LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
  SetWindowLongPtrW(hwnd, GWL_STYLE,
                    (style & ~WS_POPUP) | (WS_CHILD | WS_VISIBLE));
  HWND parent = gIsRaised ? gProgman : (gWorkerW ? gWorkerW : gProgman);
  SetParent(hwnd, parent);
  FitAndOrder(hwnd);
  return hwnd;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  SetupDesktopHandles();
  if (!gProgman)
    return 1;

  gHost = CreateHost(hi);
  if (!gHost)
    return 2;

  InitWebView2(gHost);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  gWV.Reset();
  gCtl.Reset();
  gEnv.Reset();
  CoUninitialize();
  return 0;
}
