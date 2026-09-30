// Shell: Çözüm Gezgini paneli.
//
// Projenin neyden oluştuğunu tek bakışta gösterir: grafin kendisi, ondan
// üretilen kaynak dosyası ve derlenen çıktı. Tıklanan satır ilgili paneli
// öne getirir. Bu bir dosya GÖRÜNTÜSÜDÜR; düzenleme yapılmaz.
#include "shell/panels/SolutionPanel.hpp"

#include <filesystem>
#include <string>

#include "imgui.h"

#include "shell/App.hpp"

namespace shell {

namespace {

constexpr ImVec4 kErrCol(1.00f, 0.34f, 0.34f, 1.00f);
constexpr ImVec4 kWarnCol(1.00f, 0.77f, 0.25f, 1.00f);
constexpr ImVec4 kOkCol(0.35f, 0.85f, 0.55f, 1.00f);
constexpr ImVec4 kDimCol(0.58f, 0.63f, 0.72f, 1.00f);
constexpr ImVec4 kTextCol(0.86f, 0.89f, 0.94f, 1.00f);

// ASCII ok: kapali / aci
const char* caret(bool open) { return open ? "v" : ">"; }

ImVec4 levelColor(gpi::ProblemLevel lv) {
  switch (lv) {
    case gpi::ProblemLevel::Error:
      return kErrCol;
    case gpi::ProblemLevel::Warning:
      return kWarnCol;
    default:
      return kDimCol;
  }
}

}  // namespace

bool SolutionPanel::isOpen(const std::string& key) const {
  // Ilk acilista tum klasorler acik olsun.
  return openFolders_.empty() || openFolders_.count(key) > 0;
}

void SolutionPanel::toggle(const std::string& key) {
  // openFolders_ bosken "hepsi acik" modundayiz; ilk toggle ile acik
  // klasorlerin tamamini yazip sonra kapatilani cikaririz.
  if (openFolders_.empty()) {
    openFolders_ = {"@proje", "@kaynak", "@uretulen", "@cikti", "@denetim"};
  }
  if (openFolders_.count(key) > 0) {
    openFolders_.erase(key);
  } else {
    openFolders_.insert(key);
  }
}

void SolutionPanel::buildRows(App& app) {
  rows_.clear();
  const EditorState& s = app.state();
  const LiveAnalyzer::Snapshot& a = app.analysis();
  const bool cs = s.language() == gpi::TargetLang::CSharp;
  const std::string projName = s.graph().name.empty() ? "Program" : s.graph().name;

  Row root;
  root.folder = true;
  root.key = "@proje";
  root.label = projName;
  root.detail = std::string(cs ? "C#" : "C++") + "  -  " +
                std::to_string(s.graph().nodes.size()) + " dugum  -  " +
                std::to_string(s.graph().edges.size()) + " kablo";
  rows_.push_back(root);
  if (!isOpen("@proje")) return;

  // ---------------------------------------------------------- Kaynak
  Row src;
  src.depth = 1;
  src.folder = true;
  src.key = "@kaynak";
  src.label = "Kaynak";
  rows_.push_back(src);
  if (isOpen("@kaynak")) {
    Row g;
    g.depth = 2;
    g.label = graphFileName_;
    g.action = "canvas";
    const std::string& fp = s.filePath();
    if (fp.empty()) {
      g.detail = "kaydedilmedi";
      g.muted = true;
    } else {
      std::error_code ec;
      g.exists = std::filesystem::exists(std::filesystem::path(fp), ec) && !ec;
      g.detail = g.exists ? (s.dirty() ? "kaydedilmemis degisiklik" : "kayitli") : "bulunamadi";
      g.muted = !g.exists;
    }
    rows_.push_back(g);
  }

  // ------------------------------------------------------ Uretilen kod
  Row gen;
  gen.depth = 1;
  gen.folder = true;
  gen.key = "@uretulen";
  gen.label = "Uretilen Kod";
  rows_.push_back(gen);
  if (isOpen("@uretilen")) {
    Row c;
    c.depth = 2;
    c.label = cs ? "Program.g.cs" : "main.cpp";
    c.action = "code";
    c.exists = !a.code.empty();
    if (a.code.empty()) {
      c.detail = "uretilmedi";
      c.muted = true;
    } else {
      std::size_t lines = 1;
      for (char ch : a.code) {
        if (ch == '\n') ++lines;
      }
      c.detail = std::to_string(lines) + " satir  -  anlik";
    }
    rows_.push_back(c);

    if (cs) {
      Row csproj;
      csproj.depth = 2;
      csproj.label = projName + ".csproj";
      csproj.detail = "F5 sonrasi olusur";
      csproj.muted = true;
      csproj.action = "output";
      rows_.push_back(csproj);
    }
  }

  // ------------------------------------------------------------ Cikti
  Row out;
  out.depth = 1;
  out.folder = true;
  out.key = "@cikti";
  out.label = "Cikti";
  rows_.push_back(out);
  if (isOpen("@cikti")) {
    Row exe;
    exe.depth = 2;
    exe.label = (cs ? projName : "GpiProgram") + ".exe";
    exe.detail = "F5 ile derlenir, F6 ile calisir";
    exe.muted = true;
    exe.action = "output";
    rows_.push_back(exe);
  }

  // ----------------------------------------------------------- Denetim
  Row diag;
  diag.depth = 1;
  diag.folder = true;
  diag.key = "@denetim";
  diag.label = "Denetim";
  diag.detail = a.ready ? "anlik" : "bekliyor";
  rows_.push_back(diag);
  if (isOpen("@denetim")) {
    if (a.errorCount > 0) {
      Row e;
      e.depth = 2;
      e.label = std::to_string(a.errorCount) + " hata";
      e.level = gpi::ProblemLevel::Error;
      e.action = "canvas";
      rows_.push_back(e);
    }
    if (a.warningCount > 0) {
      Row w;
      w.depth = 2;
      w.label = std::to_string(a.warningCount) + " uyari";
      w.level = gpi::ProblemLevel::Warning;
      w.action = "canvas";
      rows_.push_back(w);
    }
    if (a.errorCount == 0 && a.warningCount == 0) {
      Row g2;
      g2.depth = 2;
      g2.label = a.ready ? "Sorun yok" : "Henuz denetlenmedi";
      g2.exists = a.ready;
      g2.muted = !a.ready;
      rows_.push_back(g2);
    }
  }
}

void SolutionPanel::draw(App& app) {
  if (!ImGui::Begin("Cozum Gezgini", &app.showSolution())) {
    ImGui::End();
    return;
  }

  const EditorState& s = app.state();
  const LiveAnalyzer::Snapshot& a = app.analysis();

  // Ozet satiri: proje adi + canli denetim sonucu.
  const std::string projName = s.graph().name.empty() ? "Program" : s.graph().name;
  ImGui::TextUnformatted(projName.c_str());
  ImGui::SameLine();
  if (a.ready && app.analysisPending()) {
    ImGui::TextColored(kDimCol, "... denetleniyor");
  } else if (a.errorCount > 0) {
    ImGui::TextColored(kErrCol, "%d hata", a.errorCount);
  } else if (a.warningCount > 0) {
    ImGui::TextColored(kWarnCol, "%d uyari", a.warningCount);
  } else {
    ImGui::TextColored(kOkCol, "sorun yok");
  }

  // Eylem seridi
  if (ImGui::SmallButton("Kaydet")) {
    app.saveRequested() = true;
    if (app.pendingPath().empty()) app.pendingPath() = graphFileName_;
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Derle")) {
    app.buildRequested() = true;
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Calistir")) {
    app.runRequested() = true;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("  F5: derle  -  F6: calistir");

  ImGui::Separator();

  buildRows(app);
  for (std::size_t i = 0; i < rows_.size(); ++i) {
    const Row& r = rows_[i];

    ImVec4 col = r.muted ? kDimCol : kTextCol;
    if (r.level != gpi::ProblemLevel::Note) col = levelColor(r.level);
    else if (r.exists && r.folder == false && r.muted == false) col = kOkCol;

    ImGui::PushID(static_cast<int>(i));
    const float pad = static_cast<float>(r.depth) * 12.0f;
    bool acted = false;

    if (r.folder) {
      // Klasor: ok + ad. Tikmak ac/kapa yapar.
      ImGui::Indent(pad);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(caret(isOpen(r.key)));
      ImGui::SameLine();
      acted = ImGui::Selectable(r.label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
      ImGui::Unindent(pad);
    } else {
      ImGui::Indent(pad);
      ImGui::TextUnformatted("|");
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Text, col);
      acted = ImGui::Selectable(r.label.c_str(), false, 0);
      ImGui::PopStyleColor();
      if (!r.detail.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", r.detail.c_str());
      }
      ImGui::Unindent(pad);
    }

    if (acted) {
      if (r.folder) {
        toggle(r.key);
      } else if (r.action == "code") {
        app.revealInCode();
      } else if (r.action == "output") {
        app.revealInOutput();
      } else if (r.action == "canvas") {
        app.revealInCatalog();
      }
    }
    ImGui::PopID();
  }

  ImGui::End();
}

}  // namespace shell
