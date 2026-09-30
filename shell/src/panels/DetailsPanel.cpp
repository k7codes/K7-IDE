// Shell: seçili düğümün ayrıntıları - literal alanları, özellikler, işlemler
#include "shell/panels/DetailsPanel.hpp"

#include <cstring>
#include <map>

#include "imgui.h"

#include "shell/App.hpp"
#include "shell/Panel.hpp"

namespace shell {

void DetailsPanel::draw(App& app) {
  // Begin() false dönse bile End() çağrılmak ZORUNDA.
  if (!ImGui::Begin("Ayrıntılar", &app.showDetails())) {
    ImGui::End();
    return;
  }

  EditorState& s = app.state();
  const std::string& id = s.selected();

  if (id.empty()) {
    ImGui::TextDisabled("Bir düğüm seçin.");
    ImGui::Spacing();
    ImGui::TextWrapped("%s", "İpucu: Katalogdan bir düğüm seçip tuvale tıklayarak ekleyin.");
    ImGui::End();
    return;
  }

  gpi::NodeInstance* n = s.graph().findNode(id);
  if (!n) {
    ImGui::TextDisabled("Düğüm bulunamadı.");
    ImGui::End();
    return;
  }
  const gpi::NodeType* t = s.catalog().find(n->typeId);
  if (!t) {
    ImGui::Text("Bilinmeyen tip: %s", n->typeId.c_str());
    ImGui::End();
    return;
  }

  // Başlık
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertFloat4ToU32(hexToColor(t->accentColor)));
  ImGui::TextUnformatted(t->displayName.c_str());
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::TextDisabled("(%s)", t->id.c_str());
  if (!t->description.empty()) {
    ImGui::TextWrapped("%s", t->description.c_str());
  }

  ImGui::Separator();

  // Düzenleme, geri alınabilir olmalı: odak alırken işlem açılır,
  // odak bırakılınca kapatılır (her tuşta ayrı kayıt oluşmaz).
  bool editing = false;

  // Bağlı olmayan giriş portlarının literal değerleri
  for (const auto& port : t->inputs) {
    if (port.kind == gpi::PortKind::Exec) continue;
    if (s.graph().edgeInto(gpi::PortKey{id, port.name})) {
      ImGui::TextDisabled("%s:  <- bağlı", port.displayName.c_str());
      continue;
    }
    // Metin portlarını özel ele
    if (port.type.kind == gpi::TypeKind::String) {
      std::string cur = n->literals[port.name];
      if (cur.size() >= 2 && cur.front() == '"' && cur.back() == '"') cur = cur.substr(1, cur.size() - 2);
      // std::string'i ImGui'ye güvenli biçimde bağla (arabellek taşması olmasın)
      cur.resize(512);
      if (ImGui::InputText(("##lit" + port.name).c_str(), cur.data(), cur.size() + 1)) {
        // C# string literal'ına çevir (kaçışlar korunur)
        std::string esc;
        for (char ch : cur) {
          if (ch == '"') {
            esc += "\\\"";
          } else {
            esc += ch;
          }
        }
        n->literals[port.name] = "\"" + esc + "\"";
        editing = true;
      }
      if (ImGui::IsItemActivated()) s.beginTransaction("Metin değiştir");
      if (ImGui::IsItemDeactivatedAfterEdit()) s.commitTransaction();
      ImGui::SameLine();
      ImGui::TextDisabled("%s", port.displayName.c_str());
    } else if (port.type.kind == gpi::TypeKind::Bool) {
      bool v = (n->literals[port.name] == "true");
      if (ImGui::Checkbox(port.displayName.c_str(), &v)) {
        if (ImGui::IsItemActivated()) s.beginTransaction("Değişken değiştir");
        n->literals[port.name] = v ? "true" : "false";
        s.markDirty();
        if (ImGui::IsItemDeactivatedAfterEdit()) s.commitTransaction();
      }
    } else {
      std::string cur = n->literals[port.name];
      cur.resize(256);
      ImGui::SetNextItemWidth(120.0f);
      if (ImGui::InputText(("##lit" + port.name).c_str(), cur.data(), cur.size() + 1)) {
        n->literals[port.name] = cur.c_str();
        editing = true;
      }
      if (ImGui::IsItemActivated()) s.beginTransaction("Değer değiştir");
      if (ImGui::IsItemDeactivatedAfterEdit()) s.commitTransaction();
      ImGui::SameLine();
      ImGui::TextDisabled("%s", port.displayName.c_str());
    }
  }

  // Tipteki özel özellikler (karşılaştırma türü vb.)
  for (const auto& [k, v] : t->properties) {
    if (k == "control") continue;  // üretici iç bilgisi
    std::string cur = n->properties.count(k) ? n->properties[k] : v;
    cur.resize(256);
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputText(("##prop" + k).c_str(), cur.data(), cur.size() + 1)) {
      n->properties[k] = cur.c_str();
      editing = true;
    }
    if (ImGui::IsItemActivated()) s.beginTransaction("Özellik değiştir");
    if (ImGui::IsItemDeactivatedAfterEdit()) s.commitTransaction();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", k.c_str());
  }

  if (editing) {
    s.markDirty();
  }

  ImGui::Separator();

  // Düğümü sil: n işaretçisi geçersizleşeceği için ÖNCE tüm alanlar okunmalı.
  const bool wasBypassed = n->bypassed;
  bool deletePressed = ImGui::Button("Düğümü Sil");
  if (deletePressed) {
    s.beginTransaction("Düğüm sil");
    s.removeNode(id);
    s.commitTransaction();
    app.setStatus("Düğüm silindi");
    // n artık geçersiz: aşağıdaki arayüze dokunulmadan çık.
    ImGui::End();
    return;
  }
  ImGui::SameLine();
  {
    bool bypass = wasBypassed;
    if (ImGui::Checkbox("Aktar (pasif)", &bypass)) {
      if (n) {  // güvenlik: düğüm hâlâ mevcutsa
        n->bypassed = bypass;
        s.markDirty();
      }
    }
  }
  char comment[256];
  std::snprintf(comment, sizeof(comment), "%s", n->comment.c_str());
  if (ImGui::InputTextWithHint("##yorum", "Yorum...", comment, sizeof(comment))) {
    n->comment = comment;
    s.markDirty();
  }

  ImGui::End();
}

}  // namespace shell
