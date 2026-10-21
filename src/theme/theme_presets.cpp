#include "theme/theme_presets.h"

#include "cli/schema_msg.h"
#include "config/atomic_file.h"
#include "core/log.h"
#include "ipc/ipc_service.h"
#include "util/string_utils.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <system_error>

namespace paw2shell::theme {

  namespace {

    constexpr Logger kLog("theme-presets");

    ThemePreset presetFromJson(const nlohmann::json& object) {
      ThemePreset preset;
      const auto text = [&](const char* key) {
        const auto it = object.find(key);
        return it != object.end() && it->is_string() ? it->get<std::string>() : std::string{};
      };
      preset.id = text("id");
      preset.title = text("title");
      preset.source = enumFromKey(kPaletteSources, text("source")).value_or(PaletteSource::Builtin);
      preset.palette = text("palette");
      if (const std::string scheme = text("wallpaperScheme"); !scheme.empty()) {
        preset.wallpaperScheme = scheme;
      }
      if (const auto it = object.find("wallpapers"); it != object.end() && it->is_array()) {
        for (const auto& path : *it) {
          if (path.is_string() && !path.get<std::string>().empty()) {
            preset.wallpapers.push_back(path.get<std::string>());
          }
        }
      }
      // Early versions of the plugin held a single wallpaper.
      if (const std::string legacy = text("wallpaperPath"); preset.wallpapers.empty() && !legacy.empty()) {
        preset.wallpapers.push_back(legacy);
      }
      if (const auto it = object.find("wallpaperIndex"); it != object.end() && it->is_number_unsigned()) {
        preset.wallpaperIndex = it->get<std::size_t>();
      }
      preset.screenshotPath = text("screenshotPath");
      return preset;
    }

    nlohmann::json presetToJson(const ThemePreset& preset) {
      return {
          {"id", preset.id},
          {"title", preset.title},
          {"source", std::string(enumToKey(kPaletteSources, preset.source))},
          {"palette", preset.palette},
          {"wallpaperScheme", preset.wallpaperScheme},
          {"wallpapers", preset.wallpapers},
          {"wallpaperIndex", preset.wallpaperIndex},
          {"screenshotPath", preset.screenshotPath},
      };
    }

    std::string slugOf(std::string_view title) {
      std::string slug;
      for (const char ch : StringUtils::toLower(title)) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
          slug.push_back(ch);
        } else if (!slug.empty() && slug.back() != '-') {
          slug.push_back('-');
        }
      }
      while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
      }
      slug.resize(std::min<std::size_t>(slug.size(), 48));
      return slug.empty() ? std::string("theme") : slug;
    }

    std::string ok(std::string_view detail = {}) {
      return detail.empty() ? std::string("ok\n") : "ok " + std::string(detail) + "\n";
    }

  } // namespace

  const std::string& ThemePreset::paletteValue() const {
    return source == PaletteSource::Wallpaper ? wallpaperScheme : palette;
  }

  std::string ThemePreset::currentWallpaper() const {
    if (wallpapers.empty()) {
      return {};
    }
    return wallpapers[wallpaperIndex < wallpapers.size() ? wallpaperIndex : 0];
  }

  ThemePresetService::ThemePresetService(std::filesystem::path storePath, Hooks hooks)
      : m_storePath(std::move(storePath)), m_hooks(std::move(hooks)) {}

  void ThemePresetService::load() {
    m_presets.clear();
    m_lastAppliedId.clear();
    std::ifstream file(m_storePath);
    if (!file) {
      return;
    }
    const auto document = nlohmann::json::parse(file, nullptr, false);
    if (!document.is_object()) {
      kLog.warn("ignoring unreadable theme store {}", m_storePath.string());
      return;
    }
    if (const auto it = document.find("themes"); it != document.end() && it->is_array()) {
      for (const auto& object : *it) {
        if (!object.is_object()) {
          continue;
        }
        auto preset = presetFromJson(object);
        if (preset.id.empty()) {
          preset.id = uniqueSlug(preset.title);
        }
        if (byId(preset.id) == nullptr) {
          m_presets.push_back(std::move(preset));
        }
      }
    }
    if (const auto it = document.find("lastAppliedId"); it != document.end() && it->is_string()) {
      m_lastAppliedId = it->get<std::string>();
    }
  }

  const ThemePreset* ThemePresetService::byId(std::string_view id) const {
    const auto it = std::ranges::find(m_presets, id, &ThemePreset::id);
    return it != m_presets.end() ? &*it : nullptr;
  }

  ThemePreset* ThemePresetService::mutableById(std::string_view id) {
    return const_cast<ThemePreset*>(std::as_const(*this).byId(id));
  }

  const ThemePreset* ThemePresetService::find(std::string_view query) const {
    const std::string wanted = StringUtils::trim(query);
    if (wanted.empty()) {
      return nullptr;
    }
    const std::string lower = StringUtils::toLower(wanted);
    for (const auto& preset : m_presets) {
      if (StringUtils::toLower(preset.id) == lower) {
        return &preset;
      }
    }
    for (const auto& preset : m_presets) {
      if (StringUtils::toLower(preset.title) == lower) {
        return &preset;
      }
    }
    const ThemePreset* match = nullptr;
    for (const auto& preset : m_presets) {
      if (StringUtils::toLower(preset.title).contains(lower) || StringUtils::toLower(preset.id).contains(lower)) {
        if (match != nullptr) {
          return nullptr;
        }
        match = &preset;
      }
    }
    return match;
  }

  std::string ThemePresetService::uniqueSlug(std::string_view title) const {
    const std::string base = slugOf(title);
    std::string slug = base;
    for (int n = 2; byId(slug) != nullptr; ++n) {
      slug = base + "-" + std::to_string(n);
    }
    return slug;
  }

  std::string ThemePresetService::save(ThemePreset preset) {
    if (preset.id.empty()) {
      preset.id = uniqueSlug(preset.title);
    }
    if (preset.wallpaperIndex >= preset.wallpapers.size()) {
      preset.wallpaperIndex = 0;
    }
    std::string id = preset.id;
    if (auto* existing = mutableById(id)) {
      *existing = std::move(preset);
    } else {
      m_presets.push_back(std::move(preset));
    }
    persist();
    notify();
    return id;
  }

  bool ThemePresetService::remove(std::string_view id) {
    const auto removed = std::erase_if(m_presets, [&](const ThemePreset& preset) { return preset.id == id; });
    if (removed == 0) {
      return false;
    }
    if (m_lastAppliedId == id) {
      m_lastAppliedId.clear();
    }
    persist();
    notify();
    return true;
  }

  bool ThemePresetService::move(std::string_view id, int delta) {
    const auto it = std::ranges::find(m_presets, id, &ThemePreset::id);
    if (it == m_presets.end()) {
      return false;
    }
    const auto from = static_cast<long>(it - m_presets.begin());
    const auto to = std::clamp(from + delta, 0L, static_cast<long>(m_presets.size()) - 1);
    if (to == from) {
      return false;
    }
    auto preset = std::move(*it);
    m_presets.erase(it);
    m_presets.insert(m_presets.begin() + to, std::move(preset));
    persist();
    notify();
    return true;
  }

  bool ThemePresetService::apply(std::string_view id) {
    const ThemePreset* preset = byId(id);
    if (preset == nullptr || preset->paletteValue().empty()) {
      return false;
    }
    if (const std::string wallpaper = preset->currentWallpaper(); !wallpaper.empty() && m_hooks.setWallpaper) {
      if (!m_hooks.setWallpaper(wallpaper)) {
        kLog.warn("theme \"{}\": wallpaper {} could not be set", preset->title, wallpaper);
      }
    }
    if (!m_hooks.setColorScheme || !m_hooks.setColorScheme(preset->source, preset->paletteValue())) {
      kLog.warn("theme \"{}\": palette {} could not be set", preset->title, preset->paletteValue());
      return false;
    }
    m_lastAppliedId = preset->id;
    persist();
    notify();
    return true;
  }

  bool ThemePresetService::applyAdjacent(int direction) {
    if (m_presets.empty()) {
      return false;
    }
    const auto count = static_cast<long>(m_presets.size());
    const auto it = std::ranges::find(m_presets, m_lastAppliedId, &ThemePreset::id);
    // With nothing applied yet, "next" starts at the first preset and
    // "previous" at the last.
    const long current = it != m_presets.end() ? static_cast<long>(it - m_presets.begin()) : (direction > 0 ? -1 : 0);
    const long next = ((current + direction) % count + count) % count;
    return apply(m_presets[static_cast<std::size_t>(next)].id);
  }

  bool ThemePresetService::showWallpaper(std::size_t index) {
    ThemePreset* preset = mutableById(m_lastAppliedId);
    if (preset == nullptr || index >= preset->wallpapers.size()) {
      return false;
    }
    preset->wallpaperIndex = index;
    if (m_hooks.setWallpaper) {
      (void)m_hooks.setWallpaper(preset->wallpapers[index]);
    }
    persist();
    notify();
    return true;
  }

  bool ThemePresetService::cycleWallpaper(int direction) {
    const ThemePreset* preset = applied();
    if (preset == nullptr || preset->wallpapers.empty()) {
      return false;
    }
    const auto count = static_cast<long>(preset->wallpapers.size());
    const auto current = static_cast<long>(preset->wallpaperIndex < preset->wallpapers.size() ? preset->wallpaperIndex : 0);
    return showWallpaper(static_cast<std::size_t>(((current + direction) % count + count) % count));
  }

  bool ThemePresetService::randomWallpaper() {
    const ThemePreset* preset = applied();
    if (preset == nullptr || preset->wallpapers.empty()) {
      return false;
    }
    // Never the one already showing, when there is another to pick.
    std::vector<std::size_t> candidates;
    for (std::size_t index = 0; index < preset->wallpapers.size(); ++index) {
      if (index != preset->wallpaperIndex || preset->wallpapers.size() == 1) {
        candidates.push_back(index);
      }
    }
    static std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    return showWallpaper(candidates[pick(generator)]);
  }

  void ThemePresetService::notify() const { m_changed.emit(); }

  void ThemePresetService::persist() const {
    if (m_storePath.empty()) {
      return;
    }
    nlohmann::json themes = nlohmann::json::array();
    for (const auto& preset : m_presets) {
      themes.push_back(presetToJson(preset));
    }
    const nlohmann::json document{{"version", 1}, {"themes", themes}, {"lastAppliedId", m_lastAppliedId}};

    if (!writeTextFileAtomic(m_storePath, document.dump(2) + "\n")) {
      kLog.warn("could not write theme store {}", m_storePath.string());
    }
  }

  void ThemePresetService::registerIpc(IpcService& ipc) {
    ipc.bind(paw2shell::cli::msg::themeList, [this](const std::string&) -> std::string {
      std::ostringstream out;
      for (const auto& preset : m_presets) {
        out << (preset.id == m_lastAppliedId ? "* " : "  ") << preset.id << '\t' << preset.title << '\n';
      }
      return out.str();
    });
    ipc.bind(paw2shell::cli::msg::themeApply, [this](const std::string& args) -> std::string {
      const ThemePreset* preset = find(args);
      if (preset == nullptr) {
        return "error: no theme matches \"" + StringUtils::trim(args) + "\" (see theme-list)\n";
      }
      return apply(preset->id) ? ok(preset->id) : "error: theme could not be applied\n";
    });
    ipc.bind(paw2shell::cli::msg::themeNext, [this](const std::string&) -> std::string {
      return applyAdjacent(1) ? ok(m_lastAppliedId) : "error: no themes saved\n";
    });
    ipc.bind(paw2shell::cli::msg::themePrevious, [this](const std::string&) -> std::string {
      return applyAdjacent(-1) ? ok(m_lastAppliedId) : "error: no themes saved\n";
    });
    ipc.bind(paw2shell::cli::msg::themeWallpaperNext, [this](const std::string&) -> std::string {
      return cycleWallpaper(1) ? ok() : "error: the applied theme has no wallpapers\n";
    });
    ipc.bind(paw2shell::cli::msg::themeWallpaperPrevious, [this](const std::string&) -> std::string {
      return cycleWallpaper(-1) ? ok() : "error: the applied theme has no wallpapers\n";
    });
    ipc.bind(paw2shell::cli::msg::themeWallpaperRandom, [this](const std::string&) -> std::string {
      return randomWallpaper() ? ok() : "error: the applied theme has no wallpapers\n";
    });
  }

} // namespace paw2shell::theme
