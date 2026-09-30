// Shell: düğüm kataloğu paneli.
//
// Üç şeyi kolaylaştırır:
//   1) TEK arama kutusu: ad, kimlik, kategori ve açıklama aynı anda aranır.
//   2) Kategori SEKMELERI: açılır liste yerine yatay sekmeler, her sekmede
//      kaç düğüm olduğu yazılıdır.
//   3) Favoriler: yıldıza tıklayınca o düğüm "Favoriler" sekmesinde toplanır.
// Bir düğümü tuvalden seçmek için tek tık yeterli; çift tık yeni bir örnek
// daha ekler (ör. iki Toplama düğümü).
#include "shell/panels/CatalogPanel.hpp"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "imgui.h"

#include "shell/App.hpp"
#include "shell/Panel.hpp"
#include "shell/Platform.hpp"
#include "shell/panels/CanvasPanel.hpp"

namespace shell {

namespace {

constexpr const char* kAllTab = "*";
constexpr const char* kFavTab = "@fav";

std::string lower(std::string v) {
  for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return v;
}

std::filesystem::path favoritesPath() {
  // Uygulama dosyasının yanına yazmak en az izin gerektiren yol.
  wchar_t buf[MAX_PATH] = {0};
  const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return std::filesystem::path(L"favorites.txt");
  std::filesystem::path p(buf);
  p.remove_filename();
  return p / L"favorites.txt";
}

}  // namespace

void CatalogPanel::loadFavorites() {
  favorites_.clear();
  std::ifstream in(favoritesPath());
  if (!in) return;
  std::string line;
  while (std::getline(in, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (!line.empty() && line[0] != '#') favorites_.insert(line);
  }
}

void CatalogPanel::saveFavorites() const {
  std::ofstream out(favoritesPath());
  if (!out) return;
  for (const auto& f : favorites_) out << f << "\n";
}

std::vector<const gpi::NodeType*> CatalogPanel::visible(const gpi::NodeCatalog& cat) const {
  const std::string q = lower(query_);
  std::vector<const gpi::NodeType*> out;
  for (const auto& [id, t] : cat.all()) {
    if (activeTab_ == kFavTab) {
      if (!favorites_.count(t.id)) continue;
    } else if (activeTab_ != kAllTab) {
      if (t.category != activeTab_) continue;
    }
    if (!q.empty()) {
      const std::string hay = lower(t.displayName + " " + t.id + " " + t.category + " " +
                                    t.subcategory + " " + t.description);
      if (hay.find(q) == std::string::npos) continue;
    }
    out.push_back(&t);
  }
  return out;
}

void CatalogPanel::addNode(App& app, const gpi::NodeType& t) {
  EditorState& s = app.state();
  const ImVec2 center = app.canvas().viewCenter();
  float gx = 0.0f, gy = 0.0f;
  s.screenToGraph(center, gx, gy);
  s.beginTransaction("Düğüm ekle: " + t.displayName);
  const std::string newId = s.addNode(t.id, gx - 60.0f, gy - 20.0f);
  s.commitTransaction();
  s.select(newId);
  app.setStatus("Eklendi: " + t.displayName);
}

void CatalogPanel::drawRow(App& app, const gpi::NodeType& t) {
  ImGui::PushID(t.id.c_str());

  const bool fav = favorites_.count(t.id) > 0;
  const ImVec4 accent = hexToColor(t.accentColor);

  // Yıldız: favorileri aç/kapa
  if (fav) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.82f, 0.30f, 1.0f));
  if (ImGui::SmallButton(fav ? "*" : "+")) {
    if (fav) {
      favorites_.erase(t.id);
    } else {
      favorites_.insert(t.id);
    }
    saveFavorites();
  }
  if (fav) ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(fav ? "Favorilerden cikar" : "Favorilere ekle");
  }
  ImGui::SameLine();

  // Ad: tek tıkla ekle, çift tıkla ikinci örneği ekle.
  const bool clicked = ImGui::Selectable(t.displayName.c_str(), false, 0, ImVec2(0, 0));
  if (clicked) addNode(app, t);
  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) addNode(app, t);

  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::TextColored(accent, "%s", t.displayName.c_str());
    ImGui::TextDisabled("%s  -  %s", t.id.c_str(), t.category.c_str());
    if (!t.description.empty()) ImGui::TextWrapped("%s", t.description.c_str());
    if (!t.deprecated) {
      ImGui::Separator();
      std::string ports;
      for (const auto& p : t.inputs) {
        if (!ports.empty()) ports += ", ";
        ports += p.name;
      }
      if (!ports.empty()) ImGui::Text("girdi: %s", ports.c_str());
      ports.clear();
      for (const auto& p : t.outputs) {
        if (!ports.empty()) ports += ", ";
        ports += p.name;
      }
      if (!ports.empty()) ImGui::Text("cikti: %s", ports.c_str());
      ImGui::TextDisabled("tikla: ekle  -  cift tik: bir tane daha ekle");
    }
    ImGui::EndTooltip();
  }

  // Sağda kategori + saf/deprecated rozeti
  ImGui::SameLine();
  ImGui::TextDisabled("%s", t.category.c_str());
  if (t.deprecated) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "eski");
  }
  ImGui::PopID();
}

void CatalogPanel::draw(App& app) {
  // Begin() false dönse bile End() çağrılmalı.
  if (!ImGui::Begin("Katalog", &app.showCatalog())) {
    ImGui::End();
    return;
  }

  EditorState& s = app.state();
  const gpi::NodeCatalog& cat = s.catalog();
  if (favorites_.empty() && !favoritesLoaded_) {
    favoritesLoaded_ = true;
    loadFavorites();
  }

  // --- Tek arama kutusu ---
  char buf[160];
  std::snprintf(buf, sizeof(buf), "%s", query_.c_str());
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::InputTextWithHint("##ara", "Operator ara (ad, kimlik, aciklama)...", buf, sizeof(buf))) {
    query_ = buf;
  }
  if (!query_.empty()) {
    ImGui::SameLine();
    if (ImGui::SmallButton("x")) query_.clear();
  }

  // --- Kategori sekmeleri ---
  // Yatay kaydırılabilir sekme şeridi; "Tümü" ve "Favoriler" her zaman var,
  // ardından katalogdaki kategoriler sırayla gelir.
  std::vector<Tab> all;
  all.push_back({"Tümü", kAllTab});
  all.push_back({"Favoriler", kFavTab});
  for (const auto& c : s.categories()) {
    bool dup = false;
    for (const auto& t : all) {
      if (t.label == c.displayName) dup = true;
    }
    if (!dup) all.push_back({c.displayName, c.displayName});
  }

  if (ImGui::BeginTabBar("##sekmeler", ImGuiTabBarFlags_Reorderable)) {
    for (const auto& t : all) {
      if (ImGui::BeginTabItem(t.label.c_str(), nullptr,
                              activeTab_ == t.id ? ImGuiTabItemFlags_SetSelected
                                                 : ImGuiTabItemFlags_None)) {
        activeTab_ = t.id;
        ImGui::EndTabItem();
      }
    }
  }
  ImGui::EndTabBar();

  const std::vector<const gpi::NodeType*> list = visible(cat);
  ImGui::TextDisabled("%zu / %zu operator", list.size(), cat.all().size());

  ImGui::BeginChild("##liste", ImVec2(0, 0), ImGuiChildFlags_Borders);
  for (const gpi::NodeType* t : list) drawRow(app, *t);
  if (list.empty()) {
    if (activeTab_ == kFavTab) {
      ImGui::TextDisabled("Favori yok. Bir operatorun yanindaki + isaretine tikla.");
    } else {
      ImGui::TextDisabled("Eslesen operator yok.");
    }
  }
  ImGui::EndChild();

  ImGui::End();
}

}  // namespace shell
