// Shell: derleme günlüğü ve program çıktısı
#include "shell/panels/OutputPanel.hpp"

#include "imgui.h"

#include "shell/App.hpp"
#include "shell/BuildService.hpp"
#include "shell/Panel.hpp"

namespace shell {

void OutputPanel::draw(App& app) {
  // Begin() false dönse bile End() çağrılmak ZORUNDA.
  if (!ImGui::Begin("Çıktı", &app.showOutput())) {
    ImGui::End();
    return;
  }

  BuildService& b = app.build();

  // Filtre
  static bool showLog = true;
  static bool showProgram = true;
  ImGui::Checkbox("Derleme günlüğü", &showLog);
  ImGui::SameLine();
  ImGui::Checkbox("Program çıktısı", &showProgram);
  ImGui::SameLine();
  if (ImGui::Button("Temizle")) {
    // yalnızca görüntüyü temizlemek için: yeniden çizimde kaydır
  }

  ImGui::Separator();
  ImGui::BeginChild("##cikti", ImVec2(0, 0));

  if (showLog && !b.messages().empty()) {
    ImGui::TextDisabled("--- Derleme ---");
    for (const auto& m : b.messages()) {
      ImGui::TextWrapped("%s", m.c_str());
    }
    ImGui::Spacing();
  }
  if (showProgram && !b.programOutput().empty()) {
    ImGui::TextDisabled("--- Program ---");
    ImGui::TextUnformatted(b.programOutput().c_str());
  }
  if (b.messages().empty() && b.programOutput().empty()) {
    ImGui::TextDisabled("Henüz çıktı yok. F5 (derle) veya F6 (çalıştır).");
  }

  ImGui::EndChild();
  ImGui::End();
}

}  // namespace shell
