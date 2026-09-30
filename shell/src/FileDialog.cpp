// Shell: Win32 ortak iletişim kutusu tabanlı dosya seçici
#include "shell/FileDialog.hpp"

#include <windows.h>
#include <commdlg.h>

#include <cstring>
#include <string>

#pragma comment(lib, "comdlg32.lib")

namespace shell {

namespace {

std::wstring widen(const char* s) {
  if (!s) return {};
  const int n = ::MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
  if (n <= 1) return {};
  std::wstring w(static_cast<std::size_t>(n - 1), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
  return w;
}

void narrowTo(const std::wstring& w, char* out, std::size_t cap) {
  if (!out || cap == 0) return;
  const int n =
      ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out, static_cast<int>(cap), nullptr, nullptr);
  if (n <= 0) out[0] = 0;
}

// Ortak kurulum. mode=false -> açma, mode=true -> kaydetme.
// Başarı true döner; iptal/işlem hatasında false döner ve buffer DEĞİŞMEZ.
bool runDialog(bool saveMode, const char* title, const char* filter, char* buffer,
               std::size_t bufferSize) {
  if (!buffer || bufferSize == 0) return false;

  wchar_t fileBuf[MAX_PATH * 4] = {0};
  if (buffer[0]) {
    const std::wstring initial = widen(buffer);
    if (initial.size() < std::size(fileBuf)) {
      std::memcpy(fileBuf, initial.c_str(), (initial.size() + 1) * sizeof(wchar_t));
    }
  }

  const std::wstring wFilter = L"Dosyalar\0" + widen(filter) + L"\0Tüm dosyalar\0*.*\0\0";
  const std::wstring wTitle = widen(title);

  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = ::GetActiveWindow();
  ofn.lpstrFilter = wFilter.c_str();
  ofn.lpstrFile = fileBuf;
  ofn.nMaxFile = MAX_PATH * 4;
  ofn.lpstrTitle = wTitle.c_str();
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_NOCHANGEDIR;
  // Bayraklar moda göre ayrılmalı: " Aç " penceresinde üzerine yazma
  // istemi, "Kaydet" penceresinde varlık denetimi görünmez.
  if (saveMode) {
    ofn.Flags |= OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  } else {
    ofn.Flags |= OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
  }

  const BOOL ok = saveMode ? ::GetSaveFileNameW(&ofn) : ::GetOpenFileNameW(&ofn);
  if (!ok) return false;  // iptal edildi -> buffer olduğu gibi kalır

  narrowTo(fileBuf, buffer, bufferSize);
  return true;
}

}  // namespace

bool openFileDialog(const char* title, const char* filter, char* buffer, std::size_t bufferSize) {
  return runDialog(/*saveMode=*/false, title, filter, buffer, bufferSize);
}

bool saveFileDialog(const char* title, const char* filter, char* buffer, std::size_t bufferSize) {
  return runDialog(/*saveMode=*/true, title, filter, buffer, bufferSize);
}

}  // namespace shell
