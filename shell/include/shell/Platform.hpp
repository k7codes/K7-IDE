// Shell: Win32 + DirectX 11 altyapısı
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

namespace shell {

class Platform {
 public:
  ~Platform();

  // Pencereyi ve DX11 aygıtını oluşturur.
  bool create(const wchar_t* title, int width, int height);
  void destroy();
  // Pencereyi görünür yapar (create sonrası çağrılır).
  void show() const;

  HWND window() const { return hwnd_; }
  ID3D11Device* device() const { return device_; }
  ID3D11DeviceContext* context() const { return context_; }
  ID3D11RenderTargetView* target() const { return rtv_; }
  void present();
  void resize();
  // Boyut değiştiyse yeniden boyutlandırma gerekli.
  bool consumeResized() {
    const bool r = resized_;
    resized_ = false;
    return r;
  }
  bool quitRequested() const { return quit_; }

 private:
  bool createDevice();
  void createRenderTarget();
  void releaseRenderTarget();
  static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
  HWND hwnd_ = nullptr;
  IDXGISwapChain* swapChain_ = nullptr;
  ID3D11Device* device_ = nullptr;
  ID3D11DeviceContext* context_ = nullptr;
  ID3D11RenderTargetView* rtv_ = nullptr;
  bool resized_ = false;
  bool quit_ = false;
};

}  // namespace shell
