// Shell: kod önizleme - ImGui ile C#/C++ sözdizimi renklendirmesi
//
// Panel SALT OKUNURDUR: kod grafın karşılığıdır, elle düzenlenemez.
// İçerik arka plandaki LiveAnalyzer'dan gelir, yani bir düğüm eklediğinde
// ya da bir kablo bağladığında kendiliğinden güncellenir. Satır numarasına
// tıklamak o satırı üreten düğümü graf tuvalinde seçip odaklar.
#include "shell/panels/CodePanel.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "imgui.h"

#include "shell/App.hpp"
#include "shell/BuildService.hpp"
#include "shell/LiveAnalyzer.hpp"
#include "shell/Panel.hpp"
#include "shell/Theme.hpp"
#include "gpi/Types.hpp"

namespace shell {

namespace {

// --- Basit sözdizimi renklendirme ---
enum class Tok { Plain, Keyword, Type, Str, Num, Comment, Preproc, Func, Punct };

struct Syntax {
  const char* keywords;
  Tok keyword = Tok::Keyword;
  const char* types;
  Tok type = Tok::Type;
  Tok comment = Tok::Comment;
  bool cppPreproc = true;
};

const char* kCsKeywords =
    "using namespace class struct interface enum public private protected internal static readonly "
    "const void new return if else for foreach while do break continue switch case default try "
    "catch finally throw async await var int long double float bool string char byte object true "
    "false null this base override virtual abstract sealed partial get set value event delegate "
    "operator is as in out ref params nameof typeof sizeof checked unchecked";
const char* kCsTypes = "String Int32 Int64 Double Boolean Object List Dictionary Task Math Console";

const char* kCppKeywords =
    "alignas alignof auto bool break case catch char class const constexpr const_cast continue "
    "decltype default delete do double dynamic_cast else enum explicit export extern false float "
    "for friend goto if inline int long mutable namespace new noexcept nullptr operator private "
    "protected public register reinterpret_cast return short signed sizeof static static_assert "
    "static_cast struct switch template this throw true try typedef typeid typename union "
    "unsigned using virtual void volatile while";
const char* kCppTypes = "string vector map unordered_map set deque array ostream istream size_t int64_t uint32_t";

bool inList(const char* list, const std::string& word) {
  std::size_t i = 0;
  const std::size_t n = list ? std::strlen(list) : 0;
  while (i < n) {
    std::size_t j = i;
    while (j < n && list[j] != ' ') ++j;
    if (word.size() == (j - i) && std::memcmp(list + i, word.data(), j - i) == 0) return true;
    i = j + 1;
  }
  return false;
}

bool isIdentChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

ImVec4 levelColor(gpi::ProblemLevel lv) {
  switch (lv) {
    case gpi::ProblemLevel::Error:
      return ImVec4(1.00f, 0.34f, 0.34f, 1.00f);
    case gpi::ProblemLevel::Warning:
      return ImVec4(1.00f, 0.77f, 0.25f, 1.00f);
    default:
      return ImVec4(0.50f, 0.56f, 0.62f, 1.00f);
  }
}

const char* levelText(gpi::ProblemLevel lv) {
  switch (lv) {
    case gpi::ProblemLevel::Error:
      return "HATA";
    case gpi::ProblemLevel::Warning:
      return "UYARI";
    default:
      return "";
  }
}

bool worse(gpi::ProblemLevel a, gpi::ProblemLevel b) {
  return static_cast<int>(a) > static_cast<int>(b);
}

}  // namespace

void CodePanel::setCode(const std::string& code) {
  buildCode_ = code;
  hasBuildCode_ = true;
}

void CodePanel::sync(const LiveAnalyzer::Snapshot& a) {
  const std::string& next = showBuildOutput_ ? buildCode_ : a.code;
  if (next == code_ && !lines_.empty()) return;
  code_ = next;
  lines_.clear();
  std::string cur;
  for (char c : code_) {
    if (c == '\n') {
      lines_.push_back(cur);
      cur.clear();
    } else if (c != '\r') {
      cur += c;
    }
  }
  lines_.push_back(cur);

  // Vurgu haritalarını sıfırla: bunlar kodla birlikte geçerli.
  lineLevels_.clear();
  lineNodes_.clear();
  lineMessages_.clear();
  for (const auto& kv : a.nodeLines) lineNodes_[kv.second] = kv.first;

  for (const auto& d : a.diagnostics) {
    if (d.level == gpi::ProblemLevel::Note) continue;
    // Tanıyı ilgili satırlara bağla. Düğüm biliniyorsa o düğümün satırı,
    // bilinmiyorsa (ör. kablo hatası) kaynak düğümün satırı kullanılır.
    int line = 0;
    if (!d.nodeId.empty()) {
      auto it = a.nodeLines.find(d.nodeId);
      if (it != a.nodeLines.end()) line = it->second;
    }
    if (line == 0 && !d.fromNode.empty()) {
      auto it = a.nodeLines.find(d.fromNode);
      if (it != a.nodeLines.end()) line = it->second;
    }
    if (line <= 0) continue;
    auto lv = lineLevels_.find(line);
    if (lv == lineLevels_.end() || worse(d.level, lv->second)) lineLevels_[line] = d.level;
    lineMessages_[line].push_back(d.message);
  }
}

gpi::ProblemLevel CodePanel::levelAt(std::size_t line) const {
  auto it = lineLevels_.find(static_cast<int>(line) + 1);
  return it == lineLevels_.end() ? gpi::ProblemLevel::Note : it->second;
}

const std::string* CodePanel::nodeAt(std::size_t line) const {
  auto it = lineNodes_.find(static_cast<int>(line) + 1);
  return it == lineNodes_.end() ? nullptr : &it->second;
}

void CodePanel::draw(App& app) {
  // Begin() false dönse bile End() çağrılmak ZORUNDA.
  if (!ImGui::Begin("Kod", &app.showCode())) {
    ImGui::End();
    return;
  }

  const LiveAnalyzer::Snapshot& a = app.analysis();
  sync(a);

  // Kaynak seçici: anlık önizleme (varsayılan) ya da F5 üretimi.
  ImGui::Checkbox("F5 üretimini göster", &showBuildOutput_);
  ImGui::SameLine();
  ImGui::TextDisabled("salt okunur");
  ImGui::SameLine();
  if (!a.ready) {
    ImGui::TextDisabled("bekliyor...");
  } else if (app.analysisPending()) {
    ImGui::TextDisabled("denetleniyor...");
  } else if (a.errorCount > 0) {
    ImGui::TextColored(levelColor(gpi::ProblemLevel::Error), "%d hata", a.errorCount);
  } else {
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.55f, 1.0f), "temiz");
  }

  if (code_.empty()) {
    ImGui::TextDisabled("Kod üretmek için tuvalden en az bir düğüm ekleyin.");
  } else {
    drawHighlighted(app);
  }
  drawProblems(app);

  ImGui::End();
}

void CodePanel::drawHighlighted(App& app) {
  const bool cs = app.state().language() == gpi::TargetLang::CSharp;
  const Syntax sx{cs ? kCsKeywords : kCppKeywords, Tok::Keyword, cs ? kCsTypes : kCppTypes, Tok::Type,
                  Tok::Comment, !cs};

  const ImVec4 colPlain(0.85f, 0.88f, 0.92f, 1.0f);
  const ImVec4 colKey(0.45f, 0.72f, 1.0f, 1.0f);
  const ImVec4 colType(0.35f, 0.85f, 0.75f, 1.0f);
  const ImVec4 colStr(0.95f, 0.75f, 0.45f, 1.0f);
  const ImVec4 colNum(0.85f, 0.60f, 0.95f, 1.0f);
  const ImVec4 colCom(0.50f, 0.56f, 0.62f, 1.0f);
  const ImVec4 colPre(0.70f, 0.55f, 0.85f, 1.0f);
  const ImVec4 colFunc(0.95f, 0.85f, 0.55f, 1.0f);
  const ImVec4 colPunct(0.70f, 0.75f, 0.82f, 1.0f);

  const std::size_t bad = static_cast<std::size_t>(lineLevels_.size());
  if (bad > 0) {
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu işaretli satır)", bad);
  }
  ImGui::Separator();

  // Sabit genişlikli kod fontu.
  ImFont* const codeFont = shell::theme::codeFont();
  const float lh = codeFont ? codeFont->LegacySize : ImGui::GetFontSize();

  // En uzun satırın genişliğini ölç: yatay kaydırma çubuğunun aralığı
  // görünür alandan değil İÇERİKTEN gelir. Ölçmezsek çubuk hiç "kıpırdamaz".
  float maxLineW = 0.0f;
  for (const auto& line : lines_) {
    float w = 0.0f;
    if (codeFont) {
      w = codeFont
              ->CalcTextSizeA(codeFont->LegacySize, FLT_MAX, -1.0f, line.c_str(),
                              line.c_str() + line.size())
              .x;
    } else {
      w = ImGui::CalcTextSize(line.c_str()).x;
    }
    if (w > maxLineW) maxLineW = w;
  }

  constexpr float kGutter = 50.0f;  // satır numarası sütunu
  constexpr float kGap = 8.0f;
  const float pitch = lh + ImGui::GetStyle().ItemSpacing.y;
  // İçerik boyutunu önceden bildir → yatay kaydırma doğru aralık kazanır.
  ImGui::SetNextWindowContentSize(
      ImVec2(kGutter + kGap + maxLineW, pitch * static_cast<float>(lines_.size())));
  ImGui::BeginChild("##kod", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

  const bool fontPushed = (codeFont != nullptr);
  if (fontPushed) ImGui::PushFont(codeFont);

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 childStart = ImGui::GetCursorScreenPos();
  const float codeWidth = ImGui::GetContentRegionAvail().x;
  const ImVec2 mouse = ImGui::GetMousePos();

  // --- Satırlar: sol kenar çubuğu (numara) + aynı satırda renkli kod ---
  // Her satır TEK satır olarak yazılır; tokenlar arasında SameLine(0) ile
  // yapıştırılır ve NewLine ile satır kapatılır. Böylece uzun satır alt
  // satırlara bölünmez ve yatay kaydırma çubuğu gerçek genişliği görür.
  int focusLine = 0;
  for (std::size_t i = 0; i < lines_.size(); ++i) {
    const float y0 = childStart.y + static_cast<float>(i) * pitch;
    const ImVec2 bandMin(childStart.x, y0);
    const ImVec2 bandMax(childStart.x + codeWidth, y0 + lh);
    const gpi::ProblemLevel lv = levelAt(i);

    // Hata/uyarı satırı arka planı
    if (lv != gpi::ProblemLevel::Note) {
      const ImVec4 c = levelColor(lv);
      dl->AddRectFilled(bandMin, bandMax, IM_COL32(static_cast<int>(c.x * 255 * 0.18f),
                                                   static_cast<int>(c.y * 255 * 0.18f),
                                                   static_cast<int>(c.z * 255 * 0.18f), 255));
      dl->AddLine(ImVec2(bandMin.x, bandMin.y), ImVec2(bandMin.x, bandMax.y),
                  IM_COL32(static_cast<int>(c.x * 255), static_cast<int>(c.y * 255),
                           static_cast<int>(c.z * 255), 255));
    }
    // Fare satırın üstündeyse hafif vurgu; tıklama o satırın düğümünü odaklar.
    const bool overRow = mouse.x >= bandMin.x && mouse.x < bandMax.x && mouse.y >= y0 &&
                         mouse.y < y0 + lh;
    if (overRow) {
      dl->AddRectFilled(bandMin, bandMax, IM_COL32(255, 255, 255, 8));
      if (lv == gpi::ProblemLevel::Note && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (const std::string* nid = nodeAt(i)) focusLine = static_cast<int>(i) + 1;
      }
    }

    // Sol kenar çubuğu
    ImGui::PushID(static_cast<int>(i));
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%4d", static_cast<int>(i + 1));
    if (lv != gpi::ProblemLevel::Note) ImGui::PushStyleColor(ImGuiCol_Text, levelColor(lv));
    const bool hit = ImGui::Selectable(buf, false, 0, ImVec2(kGutter, 0));
    if (lv != gpi::ProblemLevel::Note) ImGui::PopStyleColor();
    if (hit) focusLine = static_cast<int>(i) + 1;
    if (lv != gpi::ProblemLevel::Note && ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::TextColored(levelColor(lv), "%s", levelText(lv));
      auto mit = lineMessages_.find(static_cast<int>(i) + 1);
      if (mit != lineMessages_.end()) {
        for (const auto& m : mit->second) ImGui::TextWrapped("%s", m.c_str());
      }
      if (const std::string* nid = nodeAt(i)) {
        ImGui::Separator();
        ImGui::TextDisabled("düğüm: %s", nid->c_str());
      }
      ImGui::EndTooltip();
    }
    ImGui::PopID();

    // Kodu satır numarasının yanına yaz.
    ImGui::SameLine(0.0f, kGap);
    const std::string& line = lines_[i];
    std::size_t i2 = 0;
    const std::size_t n = line.size();
    bool firstToken = true;
    auto tok = [&](const ImVec4& c, const char* s) {
      if (!firstToken) ImGui::SameLine(0.0f, 0.0f);
      ImGui::TextColored(c, "%s", s);
      firstToken = false;
    };
    while (i2 < n) {
      if (line[i2] == '/' && i2 + 1 < n && line[i2 + 1] == '/') {
        tok(colCom, line.c_str() + i2);
        i2 = n;
        break;
      }
      if (line[i2] == '"') {
        std::size_t j = i2 + 1;
        while (j < n && line[j] != '"') {
          if (line[j] == '\\') ++j;
          ++j;
        }
        if (j < n) ++j;
        tok(colStr, line.substr(i2, j - i2).c_str());
        i2 = j;
        continue;
      }
      if (line[i2] == '\'') {
        std::size_t j = i2 + 1;
        while (j < n && line[j] != '\'') {
          if (line[j] == '\\') ++j;
          ++j;
        }
        if (j < n) ++j;
        tok(colStr, line.substr(i2, j - i2).c_str());
        i2 = j;
        continue;
      }
      if (sx.cppPreproc && line[i2] == '#' && (i2 == 0 || line[i2 - 1] == ' ')) {
        tok(colPre, line.c_str() + i2);
        i2 = n;
        break;
      }
      if (std::isdigit(static_cast<unsigned char>(line[i2]))) {
        std::size_t j = i2;
        while (j < n && (std::isdigit(static_cast<unsigned char>(line[j])) || line[j] == '.' ||
                         line[j] == 'x' || line[j] == 'X' ||
                         std::isxdigit(static_cast<unsigned char>(line[j])) || line[j] == 'L' ||
                         line[j] == 'f' || line[j] == 'd')) {
          ++j;
        }
        tok(colNum, line.substr(i2, j - i2).c_str());
        i2 = j;
        continue;
      }
      if (std::isalpha(static_cast<unsigned char>(line[i2])) || line[i2] == '_') {
        std::size_t j = i2;
        while (j < n && isIdentChar(line[j])) ++j;
        const std::string word = line.substr(i2, j - i2);
        if (inList(sx.keywords, word)) {
          tok(colKey, word.c_str());
        } else if (inList(sx.types, word)) {
          tok(colType, word.c_str());
        } else if (j < n && (line[j] == '(' || line[j] == ' ')) {
          tok(colFunc, word.c_str());
        } else {
          tok(colPlain, word.c_str());
        }
        i2 = j;
        continue;
      }
      if (std::strchr("{}()[];,.:", line[i2])) {
        char b[2] = {line[i2], '\0'};
        tok(colPunct, b);
        ++i2;
        continue;
      }
      char b[2] = {line[i2], '\0'};
      tok(colPlain, b);
      ++i2;
    }
    // Satırı kapat; bir sonraki satır yeni satıra geçsin.
    ImGui::NewLine();
  }

  // ÖNEMLİ: PopFont, EndChild'DAN ÖNCE olmalı. ImGui her pencerenin End'inde
  // "Begin'de kaydedilen FontStack boyutuna dönüldü mü?" diye bakar; fontu alt
  // pencereden ÇIKTIKTAN sonra pop edersek alt pencerenin End()'i "Missing
  // PopFont()" assertion'ıyla patlar.
  if (fontPushed) ImGui::PopFont();
  ImGui::EndChild();

  // Satır -> düğüm: graf tuvalinde odakla.
  if (focusLine > 0) {
    auto it = lineNodes_.find(focusLine);
    if (it != lineNodes_.end()) {
      app.focusNode(it->second);
    } else {
      app.setStatus("Bu satır bir düğüme karşılık gelmiyor (sabit kod).");
    }
  }
}

void CodePanel::drawProblems(App& app) {
  // Anlık denetim tanıları + F5 derleme tanıları birlikte listelenir.
  const LiveAnalyzer::Snapshot& a = app.analysis();
  const std::size_t live = a.diagnostics.size();
  const auto& build = app.build().diagnostics();
  if (live == 0 && build.empty()) return;

  ImGui::Separator();
  ImGui::Text("Sorunlar (%zu anlık, %zu derleme)", live, build.size());

  for (const auto& d : a.diagnostics) {
    if (d.level == gpi::ProblemLevel::Note) continue;
    const ImVec4 c = levelColor(d.level);
    ImGui::PushStyleColor(ImGuiCol_Text, c);
    std::string where;
    if (!d.nodeId.empty()) {
      auto it = a.nodeLines.find(d.nodeId);
      if (it != a.nodeLines.end()) where = "satır " + std::to_string(it->second);
    }
    if (where.empty()) where = d.nodeId;
    // Tıklanınca ilgili düğümü graf tuvalinde odakla.
    if (!d.nodeId.empty() && ImGui::Selectable(d.message.c_str(), false, 0)) {
      app.focusNode(d.nodeId);
    } else {
      ImGui::TextWrapped("%s %s: %s", levelText(d.level), where.c_str(), d.message.c_str());
    }
    ImGui::PopStyleColor();
  }
  for (const auto& d : build) {
    const ImVec4 c = d.severity == gpi::Severity::Error ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f)
                                                         : ImVec4(1.0f, 0.80f, 0.35f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, c);
    ImGui::TextWrapped("derleme %s: %s",
                       d.line > 0 ? ("satır " + std::to_string(d.line)).c_str() : "", d.message.c_str());
    ImGui::PopStyleColor();
  }
}

}  // namespace shell
