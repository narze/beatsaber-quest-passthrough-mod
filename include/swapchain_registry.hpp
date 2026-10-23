#pragma once

// Bookkeeping for OpenXR swapchains: which GL texture each image is, which
// image the game holds now, and which swapchains feed projection layers (the
// eye buffers). No GL or OpenXR calls, so it is unit tested on the host.

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace passthrough {

struct SwapchainInfo {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t arraySize = 0;
  int64_t format = 0; // GL internal format
};

struct ReleaseTargetInfo {
  uint32_t texture;
  SwapchainInfo info;
};

class SwapchainRegistry {
public:
  using Handle = uint64_t;

  void OnCreate(Handle h, SwapchainInfo info) {
    std::lock_guard lock(mutex_);
    Entry e;
    e.info = info;
    entries_[h] = std::move(e);
  }

  void OnImages(Handle h, std::vector<uint32_t> textures) {
    std::lock_guard lock(mutex_);
    if (auto it = entries_.find(h); it != entries_.end()) it->second.textures = std::move(textures);
  }

  void OnAcquire(Handle h, uint32_t index) {
    std::lock_guard lock(mutex_);
    if (auto it = entries_.find(h); it != entries_.end()) it->second.acquired = index;
  }

  void MarkProjection(Handle h) {
    std::lock_guard lock(mutex_);
    if (auto it = entries_.find(h); it != entries_.end()) it->second.projection = true;
  }

  void OnDestroy(Handle h) {
    std::lock_guard lock(mutex_);
    entries_.erase(h);
  }

  // The texture to process before the game releases `h`, if `h` is an eye
  // buffer with a known, acquired image.
  std::optional<ReleaseTargetInfo> ReleaseTarget(Handle h) const {
    std::lock_guard lock(mutex_);
    auto it = entries_.find(h);
    if (it == entries_.end()) return std::nullopt;
    const Entry &e = it->second;
    if (!e.projection || !e.acquired || *e.acquired >= e.textures.size()) return std::nullopt;
    return ReleaseTargetInfo{e.textures[*e.acquired], e.info};
  }

private:
  struct Entry {
    SwapchainInfo info;
    std::vector<uint32_t> textures;
    std::optional<uint32_t> acquired;
    bool projection = false;
  };
  mutable std::mutex mutex_;
  std::unordered_map<Handle, Entry> entries_;
};

} // namespace passthrough
