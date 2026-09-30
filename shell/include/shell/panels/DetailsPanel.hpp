// Shell: seçili düğümün ayrıntı paneli (literal ve özellik düzenleme)
#pragma once

#include "shell/Panel.hpp"

namespace shell {

class DetailsPanel : public Panel {
 public:
  void draw(App& app) override;
};

}  // namespace shell
