// Shell: Win32 ortak iletişim kutusu tabanlı dosya seçici
#pragma once

#include <cstddef>

namespace shell {

// Var olan bir dosyayı seçtirir. Çağrıldığında pencere açılır;
// kullanıcı bir dosya seçtiğinde buffer'a yazılır ve true döner.
// İptal edilirse false döner (buffer değişmez).
bool openFileDialog(const char* title, const char* filter, char* buffer, std::size_t bufferSize);

// Kaydedilecek dosya adını sorar. OFN_OVERWRITEPROMPT uygulanır.
// Kullanıcı onaylarsa buffer'a yazılır ve true döner; iptalde false döner.
bool saveFileDialog(const char* title, const char* filter, char* buffer, std::size_t bufferSize);

}  // namespace shell
