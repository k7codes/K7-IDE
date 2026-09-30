// Shell: derleme/çalıştırma çıktısı paneli
#pragma once

#include <string>

#include "shell/Panel.hpp"

namespace shell {

class OutputPanel : public Panel {
 public:
  void draw(App& app) override;
};

}  // namespace shell
