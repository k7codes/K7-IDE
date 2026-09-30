// Shell: araç zinciri keşif paneli
#pragma once

#include <string>

#include "gpi/Toolchain.hpp"
#include "shell/Panel.hpp"

namespace shell {

class ToolchainPanel : public Panel {
 public:
  void draw(App& app) override;
  void refresh();

 private:
  gpi::ToolchainReport report_;
  bool probed_ = false;
};

}  // namespace shell
