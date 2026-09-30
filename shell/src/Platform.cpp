// Shell: Win32 pencere ve DirectX 11 aygıtı
#include "shell/Platform.hpp"

#include "imgui.h"
#include "imgui_impl_win32.h"

#include "shell/App.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

// Başlangıç akışı izleme yardımcısı (main.cpp içindeki traceStep ile aynı dosya).
// Varsayılan KAPALI: sadece "/trace" komut bayrağı verilirse yazılır.
namespace shell {
bool g_gpiTraceEnabled = false;
void GpiTraceStep(const std::string& what);
inline bool GpiTraceEnabled() { return g_gpiTraceEnabled; }
}  // namespace shell

// Win32 arka ucu başlıkta extern bildirimi sunuyor; bağlantı burada.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                            LPARAM lParam);

namespace shell {

void GpiTraceStep(const std::string& what) {
  // Yalnızca /trace bayrağı verildiğinde yaz (bkz. main.cpp: setTraceEnabled).
  if (!GpiTraceEnabled()) return;
  std::ofstream os(std::filesystem::path(L"build/startup.log"),
                   std::ios::app | std::ios::binary);
  if (os) {
    os << what << "\n";
    os.flush();
  }
}

Platform::~Platform() { destroy(); }

bool Platform::create(const wchar_t* title, int width, int height) {
  GpiTraceStep("Platform::create: sinif kaydi");
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_CLASSDC;
  wc.lpfnWndProc = &Platform::wndProc;
  wc.hInstance = ::GetModuleHandleW(nullptr);
  wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
  wc.lpszClassName = L"GpiShellWindow";
  ::RegisterClassExW(&wc);

  RECT r{0, 0, width, height};
  ::AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  hwnd_ = ::CreateWindowW(wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                          CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr,
                          wc.hInstance, this);
  if (!hwnd_) return false;
  GpiTraceStep("Platform::create: pencere olustu");

  if (!createDevice()) return false;
  GpiTraceStep("Platform::create: D3D11 aygiti olustu");
  return true;
}

bool Platform::createDevice() {
  GpiTraceStep("Platform::createDevice: giris");
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd_;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  D3D_FEATURE_LEVEL got{};
  HRESULT hr = ::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels,
                                              2, D3D11_SDK_VERSION, &sd, &swapChain_, &device_,
                                              &got, &context_);
  if (hr == DXGI_ERROR_UNSUPPORTED) {
    // Yazılım rasterleştiriciye düş
    hr = ::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                        D3D11_SDK_VERSION, &sd, &swapChain_, &device_, &got,
                                        &context_);
  }
  if (FAILED(hr)) return false;

  createRenderTarget();
  return true;
}

void Platform::createRenderTarget() {
  ID3D11Texture2D* back = nullptr;
  swapChain_->GetBuffer(0, IID_PPV_ARGS(&back));
  device_->CreateRenderTargetView(back, nullptr, &rtv_);
  back->Release();
}

void Platform::releaseRenderTarget() {
  if (rtv_) {
    rtv_->Release();
    rtv_ = nullptr;
  }
}

void Platform::present() { swapChain_->Present(1, 0); }

void Platform::resize() {
  releaseRenderTarget();
  UINT w = 0, h = 0;
  RECT rc{};
  ::GetClientRect(hwnd_, &rc);
  w = rc.right - rc.left;
  h = rc.bottom - rc.top;
  if (w == 0 || h == 0) return;  // minimize
  swapChain_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
  createRenderTarget();
}

void Platform::show() const { ::ShowWindow(hwnd_, SW_SHOW); ::UpdateWindow(hwnd_); }

void Platform::destroy() {
  releaseRenderTarget();
  if (swapChain_) {
    swapChain_->Release();
    swapChain_ = nullptr;
  }
  if (context_) {
    context_->Release();
    context_ = nullptr;
  }
  if (device_) {
    device_->Release();
    device_ = nullptr;
  }
  if (hwnd_) {
    ::DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

LRESULT CALLBACK Platform::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  // Pencere oluşturulurken ImGui henüz başlatılmamış olabilir; arka uç
  // bağlamı yoksa doğrudan Win32'ye düş.
  if (ImGui::GetCurrentContext() != nullptr) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return 1;
  }

  Platform* self = reinterpret_cast<Platform*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!self && msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = static_cast<Platform*>(cs->lpCreateParams);
    ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }

  switch (msg) {
    case WM_SIZE:
      if (self && wParam != SIZE_MINIMIZED) self->resized_ = true;
      return 0;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      return 0;
    default:
      return ::DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

}  // namespace shell
