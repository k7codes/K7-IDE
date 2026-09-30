// Shell: düğüm kataloğu paneli - arama, kategori sekmeleri, favoriler
#pragma once

#include <set>
#include <string>
#include <vector>

#include "gpi/NodeCatalog.hpp"
#include "shell/Panel.hpp"

namespace shell {

class CatalogPanel : public Panel {
 public:
  void draw(App& app) override;

  // Favoriler uygulama dizinindeki favorites.txt dosyasına yazılır, böylece
  // kapatıp açtığında kaybolmaz.
  void loadFavorites();
  void saveFavorites() const;

 private:
  // Görünen kategori sekmeleri (ilk öğe her zaman "Tümü").
  struct Tab {
    std::string label;  // görünen ad
    std::string id;     // "*" = hepsi, "@fav" = favoriler
  };
  // Sekmeye + aramaya göre eşleşen düğümler.
  std::vector<const gpi::NodeType*> visible(const gpi::NodeCatalog& cat) const;
  void addNode(App& app, const gpi::NodeType& t);
  void drawRow(App& app, const gpi::NodeType& t);

  std::string query_;
  // Seçili sekme: "*" (tümü), "@fav" (favoriler) ya da kategori görünen adı.
  std::string activeTab_ = "*";
  std::set<std::string> favorites_;
  bool favoritesLoaded_ = false;
};

}  // namespace shell
