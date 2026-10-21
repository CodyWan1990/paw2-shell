#include "theme/theme_presets.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

using paw2shell::theme::ThemePreset;
using paw2shell::theme::ThemePresetService;

namespace {

  bool check(bool cond, const char* msg) {
    if (!cond) {
      std::cerr << "FAIL: " << msg << '\n';
    }
    return cond;
  }

  // Records what applying asked the shell to do, in order.
  struct Recorder {
    std::vector<std::string> calls;

    ThemePresetService::Hooks hooks() {
      return {
          .setWallpaper =
              [this](const std::string& path) {
                calls.push_back("wallpaper " + path);
                return true;
              },
          .setColorScheme =
              [this](PaletteSource source, std::string_view value) {
                calls.push_back(std::string(enumToKey(kPaletteSources, source)) + " " + std::string(value));
                return true;
              },
      };
    }
  };

} // namespace

int main() {
  bool ok = true;
  const auto dir = std::filesystem::temp_directory_path() / ("theme-presets-test-" + std::to_string(getpid()));
  std::filesystem::create_directories(dir);
  const auto store = dir / "themes.json";

  // A file the Theme Switcher plugin wrote, including its legacy single-wallpaper field.
  {
    std::ofstream file(store);
    file << R"({"version":1,"lastAppliedId":"rose-pine","themes":[
      {"id":"rose-pine","title":"Rose Pine","source":"builtin","palette":"Rosé Pine",
       "wallpapers":["/w/a.png","/w/b.png"],"wallpaperIndex":1,"screenshotPath":"/s/rose.png"},
      {"id":"dusk","title":"Dusk","source":"wallpaper","wallpaperScheme":"m3-tonal-spot","wallpaperPath":"/w/dusk.jpg"},
      {"id":"tokyo-night","title":"Tokyo Night","source":"builtin","palette":"Tokyo Night"}
    ]})";
  }

  Recorder recorder;
  ThemePresetService service(store, recorder.hooks());
  service.load();
  ok &= check(service.presets().size() == 3, "plugin file loads");
  ok &= check(service.lastAppliedId() == "rose-pine", "last applied loads");
  const auto* rose = service.byId("rose-pine");
  ok &= check(rose != nullptr && rose->currentWallpaper() == "/w/b.png", "wallpaper index honoured");
  ok &= check(rose != nullptr && rose->screenshotPath == "/s/rose.png", "screenshot loads");
  const auto* dusk = service.byId("dusk");
  ok &= check(dusk != nullptr && dusk->wallpapers == std::vector<std::string>{"/w/dusk.jpg"}, "legacy wallpaper");
  ok &= check(dusk != nullptr && dusk->paletteValue() == "m3-tonal-spot", "wallpaper source uses its scheme");

  // Lookup: exact id, then exact title, then an unambiguous partial match.
  ok &= check(service.find("DUSK") == dusk, "id ignores case");
  ok &= check(service.find("rose pine") == rose, "title ignores case");
  ok &= check(service.find("tokyo") == service.byId("tokyo-night"), "partial match");
  ok &= check(service.find("o") == nullptr, "ambiguous partial match finds nothing");
  ok &= check(service.find("  ") == nullptr, "blank query");

  // Applying sets the wallpaper before the palette.
  ok &= check(service.apply("dusk"), "apply");
  ok &= check(
      recorder.calls == std::vector<std::string>{"wallpaper /w/dusk.jpg", "wallpaper m3-tonal-spot"},
      "wallpaper first, then palette"
  );
  ok &= check(service.lastAppliedId() == "dusk", "apply records the theme");

  // Next/previous wrap around the list.
  ok &= check(service.applyAdjacent(1) && service.lastAppliedId() == "tokyo-night", "next");
  ok &= check(service.applyAdjacent(1) && service.lastAppliedId() == "rose-pine", "next wraps");
  ok &= check(service.applyAdjacent(-1) && service.lastAppliedId() == "tokyo-night", "previous wraps");

  // Wallpaper cycling stays within the applied theme and leaves the palette alone.
  (void)service.apply("rose-pine");
  recorder.calls.clear();
  ok &= check(service.cycleWallpaper(1), "cycle wallpaper");
  ok &= check(recorder.calls == std::vector<std::string>{"wallpaper /w/a.png"}, "cycle wraps, no palette change");
  ok &= check(service.randomWallpaper() && service.applied()->currentWallpaper() == "/w/b.png", "random avoids current");
  (void)service.apply("tokyo-night");
  ok &= check(!service.cycleWallpaper(1), "a theme without wallpapers has none to cycle");

  // Saving gives a fresh, unique slug; the store survives a reload.
  ThemePreset copy;
  copy.title = "Rose Pine!";
  copy.palette = "Rosé Pine Dawn";
  const std::string id = service.save(copy);
  ok &= check(id == "rose-pine-2", "slug is unique");
  ok &= check(service.move(id, -10) && service.presets().front().id == id, "move to front");
  ok &= check(service.remove("dusk"), "remove");

  ThemePresetService reloaded(store, recorder.hooks());
  reloaded.load();
  ok &= check(reloaded.presets() == service.presets(), "round trip");
  ok &= check(reloaded.lastAppliedId() == "tokyo-night", "applied id round trips");

  std::filesystem::remove_all(dir);
  return ok ? 0 : 1;
}
