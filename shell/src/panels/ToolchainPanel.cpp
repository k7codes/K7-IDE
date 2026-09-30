// Shell: araç zinciri paneli - hangi derleyiciler bulundu
#include "shell/panels/ToolchainPanel.hpp"

#include "imgui.h"

#include "shell/App.hpp"
#include "shell/Panel.hpp"

namespace shell {

void ToolchainPanel::refresh() {
  gpi::ToolchainDiscovery d;
  report_ = d.probe();
  probed_ = true;
}

void ToolchainPanel::draw(App& app) {
  // Begin() false dönse bile End() çağrılmak ZORUNDA.
  if (!ImGui::Begin("Araç Zinciri", &app.showToolchain())) {
    ImGui::End();
    return;
  }
  if (!probed_) refresh();

  if (ImGui::Button("Yeniden Tara")) {
    refresh();
    app.setStatus("Araç zinciri tarandı");
  }
  ImGui::Separator();

  ImGui::Text("C#:  %s", report_.canBuildCSharp() ? "Hazır" : "Yok");
  ImGui::Text("C++: %s", report_.canBuildCpp() ? "Hazır" : "Yok");
  ImGui::Text(".NET SDK: %s", report_.dotnetSdkVersion.c_str());
  ImGui::Text("Windows SDK: %s", report_.windowsSdkVersion.c_str());
  if (!report_.nugetCachePath.empty()) {
    ImGui::Text("NuGet: %s", report_.nugetCachePath.c_str());
  }

  ImGui::Separator();
  ImGui::TextDisabled("Araçlar");
  if (ImGui::BeginTable("##araclar", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Araç");
    ImGui::TableSetupColumn("Sürüm");
    ImGui::TableSetupColumn("Yol");
    ImGui::TableHeadersRow();
    for (const auto& t : report_.tools) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(t.displayName.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(t.version.c_str());
      ImGui::TableNextColumn();
      ImGui::TextDisabled("%s", t.exePath.c_str());
    }
    ImGui::EndTable();
  }

  for (const auto& e : report_.errors) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
    ImGui::TextWrapped("%s", e.c_str());
    ImGui::PopStyleColor();
  }

  ImGui::End();
}

}  // namespace shell
