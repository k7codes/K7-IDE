#include "shell/LiveAnalyzer.hpp"

#include <utility>

namespace shell {

LiveAnalyzer::LiveAnalyzer() {
  catalog_.loadBuiltins();
  thread_ = std::thread([this] { worker(); });
}

LiveAnalyzer::~LiveAnalyzer() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    stop_ = true;
  }
  cv_.notify_all();
  if (thread_.joinable()) thread_.join();
}

void LiveAnalyzer::request(const gpi::Graph& g, const gpi::ProjectOptions& opts,
                           std::uint64_t revision) {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    // Aynı sürüm için tekrar çalıştırma: sürüm yalnızca graf değiştiğinde artar.
    if (hasPending_ && pendingRev_ == revision) return;
    if (result_.ready && result_.revision == revision) return;
    pending_ = g;
    pendingOpts_ = opts;
    pendingRev_ = revision;
    hasPending_ = true;
  }
  cv_.notify_one();
}

void LiveAnalyzer::reset() {
  {
    std::lock_guard<std::mutex> lock(mtx_);
    hasPending_ = false;
    result_ = Snapshot{};
  }
  cv_.notify_one();
}

LiveAnalyzer::Snapshot LiveAnalyzer::snapshot() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return result_;
}

void LiveAnalyzer::worker() {
  for (;;) {
    gpi::Graph graph;
    gpi::ProjectOptions opts;
    std::uint64_t rev = 0;
    {
      std::unique_lock<std::mutex> lock(mtx_);
      cv_.wait(lock, [this] { return stop_ || hasPending_; });
      if (stop_) return;
      graph = std::move(pending_);
      opts = pendingOpts_;
      rev = pendingRev_;
      hasPending_ = false;
      running_ = true;
    }

    // --- asıl iş: derleme değil, yalnızca denetim + kod metni üretimi ---
    Snapshot out;
    out.revision = rev;
    out.ready = true;
    {
      gpi::CodeGenerator gen(catalog_);
      const gpi::GenResult r = gen.generate(graph, opts);
      out.ok = r.ok;
      out.code = r.code;
      out.diagnostics = r.diagnostics;
      out.nodeLines = r.nodeLines;
      for (const auto& d : r.diagnostics) {
        if (d.isError()) {
          ++out.errorCount;
        } else if (d.isWarning()) {
          ++out.warningCount;
        }
      }
    }

    {
      std::lock_guard<std::mutex> lock(mtx_);
      result_ = std::move(out);
      running_ = false;
    }
  }
}

}  // namespace shell
