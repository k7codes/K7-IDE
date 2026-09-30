// Shell: koyu tema, yazı tipleri ve ortak çizim yardımcıları
#pragma once

struct ImFont;

namespace shell {
namespace theme {
void applyDark();

// Arayüz yazı tipi (ekran varsayılanı) ve kod yazı tipi.
// Kod paneli sabit genişlikli olması için codeFont() kullanır.
ImFont* uiFont();
void setUiFont(ImFont* f);
ImFont* codeFont();
void setCodeFont(ImFont* f);
}
}  // namespace shell
