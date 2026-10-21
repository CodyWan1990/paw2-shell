#include "shell/settings/settings_registry.h"

#include "compositors/paw2/paw2_config_writer.h"
#include "config/config_types.h"
#include "config/schema/config_schema.h"
#include "config/schema/ranges.h"
#include "core/files/directory_scanner.h"
#include "core/log.h"
#include "core/process/process.h"
#include "i18n/i18n.h"
#include "launcher/panel_catalog.h"
#include "pipewire/sound_player.h"
#include "shell/bar/widget_gesture.h"
#include "shell/bar/widget_gesture_defaults.h"
#include "shell/control_center/control_center_panel.h"
#include "shell/control_center/shortcut_registry.h"
#include "shell/panel/panel_manager.h"
#include "shell/settings/color_spec_picker.h"
#include "shell/settings/font_weight_catalog.h"
#include "shell/settings/settings_paw2.h"
#include "shell/wallpaper/wallpaper_paths.h"
#include "system/sysmon_threshold_profile.h"
#include "theme/builtin_palettes.h"
#include "theme/builtin_templates.h"
#include "ui/app_icon_colorization.h"
#include "util/file_utils.h"
#include "util/string_utils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace settings {
  namespace {

    constexpr int kBarMarginMax = 4096;
    constexpr float kBarCornerRadiusMax = 80.0F;

    // Launcher providers that expose a configurable prefix, keyed by the config name
    // (the lowercased provider id). Placeholders and global-search defaults mirror the
    // provider's own defaultPrefix()/defaultIncludeInGlobalSearch().
    struct LauncherProviderSettingSpec {
      std::string_view name;
      std::string_view prefixPlaceholder;
      bool globalByDefault = false;
    };

    constexpr auto kLauncherProviderSettings = std::to_array<LauncherProviderSettingSpec>({
        {.name = "calculator", .prefixPlaceholder = "calc", .globalByDefault = true},
        {.name = "emoji", .prefixPlaceholder = "emo"},
        {.name = "panels", .prefixPlaceholder = "pan"},
        {.name = "session", .prefixPlaceholder = "session"},
        {.name = "wallpaper", .prefixPlaceholder = "wall"},
        {.name = "windows", .prefixPlaceholder = "win"},
    });

    [[nodiscard]] SliderSetting barCornerSlider(std::int32_t value) {
      SliderSetting s{value, 0.0F, kBarCornerRadiusMax, 1.0F, true};
      return s;
    }

    // What the dead zone does for a gesture nobody has bound, shown as the row's placeholder so an
    // empty field never reads as "does nothing".
    [[nodiscard]] std::string deadZoneDefault(noctalia::bar::Gesture gesture) {
      for (const auto& binding : noctalia::bar::deadZoneGestureDefaults()) {
        if (binding.gesture == gesture) {
          return std::string(binding.action);
        }
      }
      return {};
    }

    [[nodiscard]] SliderSetting barReservedSlider(double value, double maxValue, double step, bool integer) {
      return SliderSetting{value, 0.0F, maxValue, step, integer};
    }

    [[nodiscard]] std::vector<KeyChord>
    effectiveKeybindItems(const std::vector<KeyChord>& configured, KeybindAction action) {
      if (!configured.empty()) {
        return configured;
      }
      return defaultKeybindSet(action);
    }

    constexpr std::array<SettingsSectionDescriptor, 23> kSettingsSections{{
        {SettingsSection::Appearance, "appearance", "adjustments-horizontal"},
        {SettingsSection::Wallpaper, "wallpaper", "paint"},
        {SettingsSection::Templates, "templates", "color-swatch"},
        {SettingsSection::Desktop, "desktop", "layout-board"},
        {SettingsSection::Dock, "dock", "layout-bottombar-inactive"},
        {SettingsSection::Panels, "panels", "layout-bottombar"},
        {SettingsSection::Launcher, "launcher", "rocket"},
        {SettingsSection::ControlCenter, "control-center", "adjustments"},
        {SettingsSection::Notifications, "notifications", "bell"},
        {SettingsSection::Osd, "osd", "message-circle"},
        {SettingsSection::Screenshot, "screenshot", "screenshot"},
        {SettingsSection::Security, "security", "shield-lock"},
        {SettingsSection::System, "system", "activity-heartbeat"},
        {SettingsSection::Services, "services", "stack-2"},
        {SettingsSection::Location, "location", "map-pin"},
        {SettingsSection::Calendar, "calendar", "calendar"},
        {SettingsSection::Power, "power", "bolt"},
        {SettingsSection::Hooks, "hooks", "link"},
        {SettingsSection::Paw2, "paw2", "paw"},
        {SettingsSection::Paw2Displays, "paw2-displays", "device-desktop"},
        {SettingsSection::Paw2Shortcuts, "paw2-shortcuts", "keyboard"},
        {SettingsSection::Bar, "bar", "crop-3-2", false},
        {SettingsSection::Plugins, "plugins", "puzzle", true, true},
    }};

    const SettingsSectionDescriptor& descriptorFor(SettingsSection section) {
      const auto it = std::ranges::find(kSettingsSections, section, &SettingsSectionDescriptor::section);
      if (it == kSettingsSections.end()) {
        std::abort();
      }
      return *it;
    }

    // Builds a slider whose bounds come from the shared schema Range, the same
    // constant the parser clamps with. This keeps the UI range and the config clamp
    // one source. `integerValue` (write as int64) stays explicit: it is a UI/write
    // choice, not implied by the range's numeric type (e.g. transition_duration).
    template <typename V, typename T>
    SliderSetting sliderFor(V value, const noctalia::config::schema::Range<T>& range, bool integerValue) {
      return SliderSetting{
          static_cast<double>(value), static_cast<double>(range.min.value()), static_cast<double>(range.max.value()),
          static_cast<double>(range.step.value()), integerValue
      };
    }

    SelectSetting asSegmented(SelectSetting setting) {
      setting.segmented = true;
      return setting;
    }

    StepperSetting barMarginStepper(std::int32_t value) {
      return StepperSetting{
          .value = static_cast<int>(value),
          .minValue = 0,
          .maxValue = kBarMarginMax,
          .step = 1,
          .valueSuffix = "px",
      };
    }

    StepperSetting barPanelOverlapStepper(std::int32_t value) {
      return StepperSetting{
          .value = static_cast<int>(value),
          .minValue = -2,
          .maxValue = 3,
          .step = 1,
          .valueSuffix = "px",
      };
    }

    std::optional<int> radiusStepperValue(const std::optional<double>& value) {
      if (!value.has_value()) {
        return std::nullopt;
      }
      return std::clamp(static_cast<int>(std::lround(*value)), 0, 80);
    }

    int radiusStepperFallback(const std::optional<double>& value) { return radiusStepperValue(value).value_or(8); }

    template <typename T, std::size_t N> SelectSetting enumSelect(const EnumOption<T> (&options)[N], T selected) {
      std::vector<SelectOption> opts;
      opts.reserve(N);
      std::string selectedValue;
      for (const auto& option : options) {
        std::string key(option.key);
        if (option.value == selected) {
          selectedValue = key;
        }
        opts.push_back(SelectOption{std::move(key), i18n::tr(option.labelKey)});
      }
      if (selectedValue.empty() && N > 0) {
        selectedValue = std::string(options[0].key);
      }
      return SelectSetting{std::move(opts), std::move(selectedValue)};
    }

    SelectSetting
    plainSelect(std::initializer_list<std::pair<std::string_view, std::string_view>> items, std::string_view selected) {
      std::vector<SelectOption> opts;
      opts.reserve(items.size());
      for (const auto& [value, labelKey] : items) {
        opts.push_back(SelectOption{std::string(value), i18n::tr(labelKey)});
      }
      return SelectSetting{std::move(opts), std::string(selected)};
    }

    [[nodiscard]] std::string barAutoHideMode(bool autoHide, bool smartAutoHide) {
      if (smartAutoHide) {
        return "smart";
      }
      if (autoHide) {
        return "on";
      }
      return "off";
    }

    [[nodiscard]] SelectSetting autoHideModeSelect(std::string_view mode, std::vector<std::string> smartPath) {
      auto select = asSegmented(plainSelect(
          {{"off", "settings.options.bar.auto-hide.off"},
           {"on", "settings.options.bar.auto-hide.on"},
           {"smart", "settings.options.bar.auto-hide.smart"}},
          mode
      ));
      select.linkedPath = std::move(smartPath);
      select.groupedCommit = [](std::string_view value, const std::vector<std::string>& primaryPath) {
        auto companionPath = primaryPath;
        companionPath.back() = "smart_auto_hide";
        return std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>>{
            {primaryPath, ConfigOverrideValue{value == "on"}},
            {std::move(companionPath), ConfigOverrideValue{value == "smart"}},
        };
      };
      return select;
    }

    ColorSwatchPreview palettePreviewFromPalette(const ::Palette& palette) {
      return ColorSwatchPreview{
          .surface = fixedColorSpec(palette.surface),
          .swatches = {
              fixedColorSpec(palette.primary),
              fixedColorSpec(palette.secondary),
              fixedColorSpec(palette.tertiary),
              fixedColorSpec(palette.error),
          },
      };
    }

    ColorSwatchPreview builtinPalettePreview(const noctalia::theme::BuiltinPalette& palette, ThemeMode mode) {
      return palettePreviewFromPalette(mode == ThemeMode::Light ? palette.light.palette : palette.dark.palette);
    }

    SelectSetting builtinPaletteSelect(std::string_view selected, ThemeMode mode) {
      std::vector<SelectOption> opts;
      opts.reserve(noctalia::theme::builtinPalettes().size());
      for (const auto& palette : noctalia::theme::builtinPalettes()) {
        opts.push_back(
            SelectOption{
                .value = std::string(palette.name),
                .label = std::string(palette.name),
                .description = {},
                .preview = builtinPalettePreview(palette, mode),
            }
        );
      }
      return SelectSetting{
          .options = std::move(opts), .selectedValue = std::string(selected), .preferredWidth = 240.0F
      };
    }

    SelectSetting wallpaperSchemeSelect(std::string_view selected) {
      return plainSelect(
          {{"m3-content", "theme.scheme.m3-content"},
           {"m3-tonal-spot", "theme.scheme.m3-tonal-spot"},
           {"m3-fruit-salad", "theme.scheme.m3-fruit-salad"},
           {"m3-rainbow", "theme.scheme.m3-rainbow"},
           {"m3-monochrome", "theme.scheme.m3-monochrome"},
           {"vibrant", "theme.scheme.vibrant"},
           {"faithful", "theme.scheme.faithful"},
           {"soft", "theme.scheme.soft"},
           {"dysfunctional", "theme.scheme.dysfunctional"},
           {"muted", "theme.scheme.muted"}},
          selected
      );
    }

    std::vector<SelectOption> controlCenterShortcutOptions(const Config& cfg) {
      auto catalog = ShortcutRegistry::catalog();
      std::vector<SelectOption> opts;
      opts.reserve(catalog.size());
      for (auto& shortcut : catalog) {
        if (!ShortcutRegistry::isAvailable(shortcut.type, cfg)) {
          continue;
        }
        std::string label = shortcut.literalLabel ? std::move(shortcut.labelKey) : i18n::tr(shortcut.labelKey);
        opts.push_back(SelectOption{std::move(shortcut.type), std::move(label)});
      }
      return opts;
    }

    SelectSetting languageSelect(std::string_view selected) {
      std::vector<SelectOption> opts;
      opts.reserve(i18n::kSupportedLanguages.size() + 1);
      opts.push_back(SelectOption{"", i18n::tr("common.states.auto")});
      for (const auto& language : i18n::kSupportedLanguages) {
        opts.push_back(SelectOption{std::string(language.code), std::string(language.displayName)});
      }
      return SelectSetting{std::move(opts), std::string(selected)};
    }

    ColorSpecPickerSetting
    colorSpecPicker(const std::optional<ColorSpec>& selected, bool allowNone, std::string noneLabel = {}) {
      return ColorSpecPickerSetting{
          .roles = {},
          .selectedValue = optionalColorSpecConfigValue(selected),
          .allowNone = allowNone,
          .allowCustomColor = true,
          .noneLabel = std::move(noneLabel),
      };
    }

    ColorSpecPickerSetting
    colorSpecPicker(const ColorSpec& selected, bool allowNone = false, std::string noneLabel = {}) {
      return colorSpecPicker(std::optional<ColorSpec>{selected}, allowNone, std::move(noneLabel));
    }

    // The bar settings paw2-shell's own Bar tab had; every other bar setting is Noctalia's
    // fine-tuning and shows only with Advanced on. Keys are the last element of a bar entry's
    // path (`bar.<name>.<key>`, or `bar.<name>.monitor.<match>.<key>` for an override).
    void moveBarFineTuningToAdvanced(std::vector<SettingEntry>& entries) {
      static constexpr std::array<std::string_view, 15> kBasicBarKeys{
          "enabled",   "position",           "auto_hide", "auto_hide_delay_ms", "show_on_workspace_switch",
          "thickness", "background_opacity", "margin_edge", "radius",           "shadow",
          "capsule",   "start",              "center",      "end",              "smart_auto_hide",
      };
      for (auto& entry : entries) {
        if (entry.section != SettingsSection::Bar || entry.path.size() < 3 || entry.path.front() != "bar") {
          continue;
        }
        const bool barLevel = entry.path.size() == 3;
        const bool monitorLevel = entry.path.size() == 5 && entry.path[2] == "monitor";
        const bool basic = (barLevel || monitorLevel) && std::ranges::contains(kBasicBarKeys, entry.path.back());
        if (!basic) {
          entry.advanced = true;
        }
      }
    }

    SettingEntry makeEntry(
        SettingsSection section, std::string group, std::string title, std::string subtitle,
        std::vector<std::string> path, SettingControl control, std::string tags = {}, bool advanced = false
    ) {
      std::string searchText = std::string(settingsSectionId(section))
          + " "
          + group
          + " "
          + title
          + " "
          + subtitle
          + " "
          + StringUtils::join(path, ".")
          + " "
          + tags;
      if (advanced) {
        searchText += " advanced";
      }
      return SettingEntry{
          .section = section,
          .group = std::move(group),
          .title = std::move(title),
          .subtitle = std::move(subtitle),
          .path = std::move(path),
          .control = std::move(control),
          .advanced = advanced,
          .searchText = StringUtils::toLower(searchText),
      };
    }

    // paw2's settings as its pages show them. paw2's config message carries only what its config
    // files set, so anything missing reads as the default paw2 compiles in (its src/config.rs).
    class Paw2Values {
    public:
      explicit Paw2Values(const RegistryEnvironment& env) : m_env(env) {}

      [[nodiscard]] double number(std::string_view path, double fallback) const {
        const auto value = m_env.paw2Value(path);
        return value.has_value() && value->is_number() ? value->get<double>() : fallback;
      }
      [[nodiscard]] bool boolean(std::string_view path, bool fallback) const {
        const auto value = m_env.paw2Value(path);
        return value.has_value() && value->is_boolean() ? value->get<bool>() : fallback;
      }
      [[nodiscard]] std::string text(std::string_view path, std::string fallback = {}) const {
        const auto value = m_env.paw2Value(path);
        return value.has_value() && value->is_string() ? value->get<std::string>() : std::move(fallback);
      }
      [[nodiscard]] std::vector<std::string> list(std::string_view path, std::vector<std::string> fallback) const {
        const auto value = m_env.paw2Value(path);
        if (!value.has_value() || !value->is_array()) {
          return fallback;
        }
        std::vector<std::string> items;
        for (const auto& item : *value) {
          if (item.is_string()) {
            items.push_back(item.get<std::string>());
          }
        }
        return items;
      }

    private:
      const RegistryEnvironment& m_env;
    };

    // makeEntry titled from settings.schema.<key>.label and .description.
    SettingEntry keyedEntry(
        SettingsSection section, std::string group, std::string_view key, std::vector<std::string> path,
        SettingControl control, std::string tags = {}, bool advanced = false
    ) {
      const std::string prefix = "settings.schema." + std::string(key);
      return makeEntry(
          section, std::move(group), i18n::tr(prefix + ".label"), i18n::tr(prefix + ".description"), std::move(path),
          std::move(control), std::move(tags), advanced
      );
    }

    // One row of a paw2 page. Its path is {"paw2", <paw2's dotted path, split>}, and its text lives
    // under settings.schema.paw2.<dotted path>.
    SettingEntry makePaw2Entry(
        SettingsSection section, std::string group, std::string_view dottedPath, SettingControl control,
        std::string tags = {}, bool advanced = false
    ) {
      std::vector<std::string> path{"paw2"};
      std::string key = "settings.schema.paw2";
      std::size_t start = 0;
      while (start <= dottedPath.size()) {
        const auto dot = dottedPath.find('.', start);
        const auto part =
            dottedPath.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start);
        path.emplace_back(part);
        key += "." + i18n::keySegment(part);
        if (dot == std::string_view::npos) {
          break;
        }
        start = dot + 1;
      }
      return makeEntry(
          section, std::move(group), i18n::tr(key + ".label"), i18n::tr(key + ".description"), std::move(path),
          std::move(control), "paw2 compositor " + tags, advanced
      );
    }

    [[nodiscard]] SelectSetting
    paw2Select(std::string_view baseKey, std::span<const std::string_view> values, std::string selected) {
      SelectSetting select;
      for (const auto value : values) {
        select.options.push_back(
            SelectOption{
                .value = std::string(value), .label = i18n::tr(std::string(baseKey) + "." + i18n::keySegment(value))
            }
        );
      }
      select.selectedValue = std::move(selected);
      return select;
    }

    [[nodiscard]] SearchPickerSetting paw2ThemePicker(DesktopThemeKind kind, std::string selected) {
      SearchPickerSetting picker;
      for (auto& name : installedDesktopThemes(kind)) {
        picker.options.push_back(SelectOption{.value = name, .label = name});
      }
      picker.selectedValue = std::move(selected);
      picker.placeholder = i18n::tr("settings.schema.paw2.theme.system-default");
      return picker;
    }

    // Without paw2's config there is nothing to show; each page says why, and offers the files.
    void appendPaw2Unavailable(std::vector<SettingEntry>& entries, const RegistryEnvironment& env) {
      for (const auto section :
           {SettingsSection::Paw2, SettingsSection::Paw2Displays, SettingsSection::Paw2Shortcuts}) {
        entries.push_back(makeEntry(
            section, "paw2-status", i18n::tr("settings.schema.paw2.unavailable.label"),
            i18n::tr("settings.schema.paw2.unavailable.description"), {},
            ButtonSetting{
                .label = i18n::tr("settings.schema.paw2.unavailable.open-folder"),
                .action =
                    [dir = env.paw2ConfigDir]() { (void)process::runAsync(std::vector<std::string>{"xdg-open", dir}); },
                .glyph = "folder",
            },
            "paw2 compositor"
        ));
      }
    }

    // A mode as a person reads it: "2560 × 1440 @ 165.08 Hz".
    [[nodiscard]] std::string paw2ModeLabel(std::int64_t width, std::int64_t height, double refresh) {
      std::string hz = std::format("{:.2f}", refresh);
      while (hz.ends_with('0')) {
        hz.pop_back();
      }
      if (hz.ends_with('.')) {
        hz.pop_back();
      }
      return std::format("{} × {} @ {} Hz", width, height, hz);
    }

    // The monitor's mode choice: paw2's preferred mode, then each advertised mode once, plus the
    // configured mode if the display no longer offers it.
    [[nodiscard]] SelectSetting
    paw2ModeSelect(const compositors::paw2::Paw2Output& output, const nlohmann::json& configured) {
      SelectSetting select;
      select.options.push_back(
          SelectOption{.value = std::string(kPaw2AutoMode), .label = i18n::tr("settings.schema.paw2.monitor.mode.auto")}
      );
      select.selectedValue = std::string(kPaw2AutoMode);
      for (const auto& mode : output.modes) {
        const std::string value = paw2ModeValue(mode.width, mode.height, mode.refresh);
        if (std::ranges::contains(select.options, value, &SelectOption::value)) {
          continue;
        }
        select.options.push_back(
            SelectOption{
                .value = value,
                .label = paw2ModeLabel(mode.width, mode.height, mode.refresh),
                .description = mode.current ? i18n::tr("settings.schema.paw2.monitor.mode.current") : std::string{},
            }
        );
      }

      const auto resolution = configured.find("resolution");
      if (resolution == configured.end() || !resolution->is_array() || resolution->size() != 2) {
        return select;
      }
      const std::int64_t width = (*resolution)[0].get<std::int64_t>();
      const std::int64_t height = (*resolution)[1].get<std::int64_t>();
      // Without a refresh paw2 takes the fastest mode at that size, which the list has first.
      const double wanted = configured.value("refresh", 0.0);
      for (const auto& mode : output.modes) {
        if (mode.width == width && mode.height == height && (wanted == 0.0 || std::abs(mode.refresh - wanted) < 0.01)) {
          select.selectedValue = paw2ModeValue(mode.width, mode.height, mode.refresh);
          return select;
        }
      }
      select.selectedValue = paw2ModeValue(width, height, wanted);
      select.options.push_back(
          SelectOption{.value = select.selectedValue, .label = paw2ModeLabel(width, height, wanted)}
      );
      return select;
    }

    // "Super+h Super+Left" -> {"Super+h", "Super+Left"}
    void appendPaw2ShortcutEntries(std::vector<SettingEntry>& entries, const RegistryEnvironment& env) {
      constexpr auto section = SettingsSection::Paw2Shortcuts;
      const std::string text = "settings.schema.paw2.bindings";

      // Where the shortcuts live, since editing one here moves all of them out of config.toml.
      if (env.paw2OwnsShortcuts) {
        entries.push_back(makeEntry(
            section, "paw2-keys-file", i18n::tr(text + ".file.owned.label"), i18n::tr(text + ".file.owned.description"),
            {},
            ButtonSetting{
                .label = i18n::tr(text + ".file.owned.release"),
                .action = env.paw2ReleaseShortcuts,
                .glyph = "arrow-back-up",
                .variant = ButtonVariant::Destructive,
            },
            "paw2 shortcuts keybinds config.toml keybinds.toml"
        ));
      } else {
        entries.push_back(makeEntry(
            section, "paw2-keys-file", i18n::tr(text + ".file.config.label"),
            i18n::tr(text + ".file.config.description"), {},
            ButtonSetting{
                .label = i18n::tr("settings.schema.paw2.unavailable.open-folder"),
                .action =
                    [dir = env.paw2ConfigDir]() { (void)process::runAsync(std::vector<std::string>{"xdg-open", dir}); },
                .glyph = "folder",
            },
            "paw2 shortcuts keybinds config.toml keybinds.toml"
        ));
      }

      StringMapSetting commands{
          .entries = {},
          .suggestedKeys = {},
          .keyPlaceholder = "Super+Return",
          .valuePlaceholder = "kitty --single-instance",
          .recordKeys = true,
      };
      if (const auto map = env.paw2Value("bindings.commands"); map.has_value() && map->is_object()) {
        for (const auto& [trigger, argv] : map->items()) {
          if (argv.is_array()) {
            commands.entries.emplace(trigger, joinCommandLine(argv.get<std::vector<std::string>>()));
          }
        }
      }
      entries.push_back(makeEntry(
          section, "paw2-keys-commands", i18n::tr(text + ".commands.label"), i18n::tr(text + ".commands.description"),
          {"paw2", "bindings", "commands"}, std::move(commands), "paw2 shortcuts keybinds launch run command app"
      ));

      for (const auto& action : paw2BindingActions()) {
        std::vector<std::string> path{"paw2", "bindings"};
        for (auto& part : compositors::paw2::splitPath(action.key)) {
          path.push_back(std::move(part));
        }
        std::string key(action.key);
        std::ranges::replace(key, '.', '-');
        key = text + ".actions." + i18n::keySegment(key);

        // Unset, paw2 uses its default keys for the action.
        std::vector<std::string> keys;
        if (const auto value = env.paw2Value("bindings." + std::string(action.key));
            value.has_value() && value->is_array()) {
          keys = value->get<std::vector<std::string>>();
        } else {
          keys = StringUtils::splitWhitespace(action.defaults);
        }
        const std::string defaults = action.defaults.empty()
            ? i18n::tr(text + ".no-default")
            : i18n::tr(
                  text + ".default", "keys", StringUtils::join(StringUtils::splitWhitespace(action.defaults), ", ")
              );
        entries.push_back(makeEntry(
            section, std::string(action.group), i18n::tr(key), defaults, std::move(path),
            ListSetting{.items = std::move(keys), .recordKeys = true},
            "paw2 shortcuts keybinds " + std::string(action.key)
        ));
      }

      // Shell actions, bound as commands that call back into the shell.
      const auto commandTable = env.paw2Value("bindings.commands").value_or(nlohmann::json::object());
      for (const auto& shortcut : shellShortcuts()) {
        entries.push_back(makeEntry(
            section, std::string(shortcut.group), i18n::tr(text + ".shell." + std::string(shortcut.name)),
            i18n::tr(text + ".shell.runs", "command", joinCommandLine(shellShortcutArgv(shortcut))),
            {"paw2", "bindings", "shell", std::string(shortcut.name)},
            ListSetting{.items = shellShortcutKeys(commandTable, shortcut), .recordKeys = true},
            "paw2 shortcuts keybinds themes wallpaper " + std::string(shortcut.name)
        ));
      }
    }

    void appendPaw2DisplayEntries(std::vector<SettingEntry>& entries, const RegistryEnvironment& env) {
      constexpr auto section = SettingsSection::Paw2Displays;
      const nlohmann::json monitors = env.paw2Value("monitors").value_or(nlohmann::json::array());
      constexpr std::array<std::string_view, 8> kTransforms{"normal",  "90",         "180",         "270",
                                                            "flipped", "flipped-90", "flipped-180", "flipped-270"};
      constexpr std::array<std::string_view, 3> kVrrPolicies{"automatic", "always", "never"};

      for (const auto& output : env.paw2Outputs) {
        nlohmann::json configured = nlohmann::json::object();
        for (const auto& monitor : monitors) {
          if (monitor.is_object() && monitor.value("connector", std::string{}) == output.name) {
            configured = monitor;
          }
        }
        // The Displays page labels each group with its connector (see groupLabel in settings_content.cpp).
        const std::string group = "paw2-output:" + output.name;
        const auto add = [&](std::string_view key, SettingControl control, std::string tags, bool advanced = false) {
          const std::string text = "settings.schema.paw2.monitor." + i18n::keySegment(key);
          entries.push_back(makeEntry(
              section, group, i18n::tr(text + ".label"), i18n::tr(text + ".description"),
              {"paw2", "monitors", output.name, std::string(key)}, std::move(control),
              "paw2 display monitor output " + output.name + " " + tags, advanced
          ));
        };

        std::string position;
        if (const auto it = configured.find("position"); it != configured.end() && it->is_array() && it->size() == 2) {
          position = std::format("{}, {}", (*it)[0].get<std::int64_t>(), (*it)[1].get<std::int64_t>());
        }
        add("mode", paw2ModeSelect(output, configured), "mode resolution refresh rate hz");
        add("scale", SliderSetting{configured.value("scale", 1.0), 0.5, 3.0, 0.05, false}, "scale scaling hidpi");
        add("transform",
            paw2Select(
                "settings.schema.paw2.monitor.transform.options", kTransforms,
                configured.value("transform", std::string("normal"))
            ),
            "transform rotate rotation flip");
        add("vrr",
            paw2Select(
                "settings.schema.paw2.monitor.vrr.options", kVrrPolicies,
                configured.value("vrr", std::string("automatic"))
            ),
            "vrr variable refresh adaptive sync freesync gsync");
        add("position",
            TextSetting{
                .value = position, .placeholder = i18n::tr("settings.schema.paw2.monitor.position.placeholder")
            },
            "position layout arrangement", true);
        add("focus_at_startup", ToggleSetting{configured.value("focus_at_startup", false)}, "focus startup primary",
            true);
      }
    }

    // Pushes a paw2 row; `visible` hides one that only matters while another value is on, e.g.
    // blur strength while blur is off.
    void pushPaw2Entry(std::vector<SettingEntry>& entries, SettingEntry entry, bool visible = true) {
      if (!visible) {
        entry.visibleWhen = [](const Config&) { return false; };
      }
      entries.push_back(std::move(entry));
    }

    // paw2's look sits on the Appearance page beside the shell's: each helper below fills one of
    // that page's groups, and is called where the group sits.

    void appendPaw2WindowEntries(std::vector<SettingEntry>& entries, const Paw2Values& paw2) {
      constexpr auto section = SettingsSection::Appearance;
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "windows", "appearance.gaps", SliderSetting{paw2.number("appearance.gaps", 0), 0, 64, 1, true},
              "gaps spacing margin"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "windows", "appearance.focus_ring_width",
              SliderSetting{paw2.number("appearance.focus_ring_width", 0), 0, 16, 1, true}, "focus ring border width"
          )
      );

      // paw2 draws these for apps that let it; GTK apps keep their own header bars.
      const bool titlebarsOn = paw2.boolean("appearance.titlebars", false);
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "windows", "appearance.titlebars", ToggleSetting{titlebarsOn},
              "titlebar title bar decorations close button float server side"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "windows", "appearance.titlebar_height",
              SliderSetting{paw2.number("appearance.titlebar_height", 24), 16, 48, 1, true}, "titlebar height size",
              true
          ),
          titlebarsOn
      );
    }

    void appendPaw2ShadowEntries(std::vector<SettingEntry>& entries, const Paw2Values& paw2) {
      constexpr auto section = SettingsSection::Appearance;
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "shadows", "appearance.shadow_radius",
              SliderSetting{paw2.number("appearance.shadow_radius", 0), 0, 64, 1, true}, "window shadow drop size"
          )
      );
    }

    void appendPaw2TransparencyEntries(std::vector<SettingEntry>& entries, const Paw2Values& paw2) {
      constexpr auto section = SettingsSection::Appearance;
      const bool transparencyOn = paw2.boolean("appearance.transparency", false);
      const bool blurOn = paw2.boolean("appearance.blur", false);
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "transparency", "appearance.transparency", ToggleSetting{transparencyOn},
              "transparency translucent"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "transparency", "appearance.opacity",
              SliderSetting{paw2.number("appearance.opacity", 1.0), 0.1, 1.0, 0.05, false}, "window opacity alpha"
          ),
          transparencyOn
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "transparency", "appearance.blur", ToggleSetting{blurOn}, "blur backdrop frosted bar dock panel"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "transparency", "appearance.blur_radius",
              SliderSetting{paw2.number("appearance.blur_radius", 0), 0, 64, 1, true}, "blur radius strength"
          ),
          blurOn
      );
    }

    void appendPaw2DesktopThemeEntries(std::vector<SettingEntry>& entries, const Paw2Values& paw2) {
      constexpr auto section = SettingsSection::Appearance;
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "desktop-theme", "theme.cursor_theme",
              paw2ThemePicker(DesktopThemeKind::Cursor, paw2.text("theme.cursor_theme")), "cursor pointer theme"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "desktop-theme", "theme.cursor_size",
              SliderSetting{paw2.number("theme.cursor_size", 24), 8, 96, 1, true}, "cursor pointer size"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "desktop-theme", "theme.icon_theme",
              paw2ThemePicker(DesktopThemeKind::Icon, paw2.text("theme.icon_theme")), "icon theme"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "desktop-theme", "theme.gtk_theme",
              paw2ThemePicker(DesktopThemeKind::Gtk, paw2.text("theme.gtk_theme")), "gtk theme"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "desktop-theme", "theme.qt_style",
              TextSetting{.value = paw2.text("theme.qt_style"), .placeholder = "Breeze"}, "qt style kvantum"
          )
      );
    }

    // What stays on the paw2 page: how windows behave rather than how they look.
    void appendPaw2BehaviourEntries(std::vector<SettingEntry>& entries, const Paw2Values& paw2) {
      constexpr auto section = SettingsSection::Paw2;
      constexpr std::array<std::string_view, 4> kLayouts{"scrolling", "dwindle", "master_stack", "monocle"};
      ListSetting layouts{.items = paw2.list("layout.enabled_layouts", {kLayouts.begin(), kLayouts.end()})};
      for (const auto layout : kLayouts) {
        layouts.suggestedOptions.push_back(
            SelectOption{
                .value = std::string(layout),
                .label = i18n::tr("settings.schema.paw2.layout.names." + i18n::keySegment(layout))
            }
        );
      }
      pushPaw2Entry(
          entries, makePaw2Entry(section, "paw2-tiling", "layout.enabled_layouts", std::move(layouts), "cycle layouts")
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "paw2-tiling", "layout.column_width",
              SliderSetting{paw2.number("layout.column_width", 0.6), 0.1, 1.0, 0.05, false}, "column width scrolling"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "paw2-tiling", "layout.center_focused_column",
              ToggleSetting{paw2.boolean("layout.center_focused_column", false)}, "center viewport scrolling"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "paw2-tiling", "layout.tab_grouping", ToggleSetting{paw2.boolean("layout.tab_grouping", false)},
              "tabs tab bar group"
          )
      );

      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "paw2-overview", "overview.zoom",
              SliderSetting{paw2.number("overview.zoom", 0.5), 0.2, 0.75, 0.05, false}, "overview zoom size"
          )
      );
      constexpr std::array<std::string_view, 5> kScratchpadPositions{"centered", "top", "bottom", "left", "right"};
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "paw2-overview", "overview.scratchpad_position",
              paw2Select(
                  "settings.schema.paw2.overview.scratchpad-position.options", kScratchpadPositions,
                  paw2.text("overview.scratchpad_position", "centered")
              ),
              "overview scratchpad strip position"
          )
      );

      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "paw2-input", "input.focus_follows_mouse",
              ToggleSetting{paw2.boolean("input.focus_follows_mouse", false)}, "focus follows mouse pointer hover"
          )
      );
      pushPaw2Entry(
          entries,
          makePaw2Entry(
              section, "paw2-input", "input.middle_click_paste",
              ToggleSetting{paw2.boolean("input.middle_click_paste", true)}, "middle click paste primary selection"
          )
      );
    }

  } // namespace

  const BarConfig* findBar(const Config& cfg, std::string_view name) {
    for (const auto& bar : cfg.bars) {
      if (bar.name == name) {
        return &bar;
      }
    }
    return nullptr;
  }

  const BarMonitorOverride* findMonitorOverride(const BarConfig& bar, std::string_view match) {
    for (const auto& ovr : bar.monitorOverrides) {
      if (ovr.match == match) {
        return &ovr;
      }
    }
    return nullptr;
  }

  const DockMonitorOverride* findDockMonitorOverride(const DockConfig& dock, std::string_view tableName) {
    const auto it = std::ranges::find(dock.monitorOverrides, tableName, &DockMonitorOverride::tableName);
    return it != dock.monitorOverrides.end() ? &*it : nullptr;
  }

  std::vector<std::string> barNames(const Config& cfg) {
    std::vector<std::string> names;
    names.reserve(cfg.bars.size());
    for (const auto& bar : cfg.bars) {
      names.push_back(bar.name);
    }
    return names;
  }

  std::string normalizedSettingQuery(std::string_view query) { return StringUtils::toLower(query); }

  bool matchesNormalizedSettingQuery(const SettingEntry& entry, std::string_view normalizedQuery) {
    if (normalizedQuery.empty()) {
      return true;
    }
    return entry.searchText.contains(normalizedQuery);
  }


  bool isBarMonitorOverrideSettingPath(const std::vector<std::string>& path) {
    return path.size() >= 5 && path[0] == "bar" && path[2] == "monitor";
  }

  bool isDockMonitorOverrideSettingPath(const std::vector<std::string>& path) {
    return path.size() >= 4 && path[0] == "dock" && path[1] == "monitor";
  }

  bool settingEntryMatchesBarNavigation(
      const SettingEntry& entry, std::string_view selectedBarName, std::string_view selectedMonitorOverride
  ) {
    if (entry.section != SettingsSection::Bar || entry.path.size() < 2 || entry.path[0] != "bar") {
      return false;
    }
    if (selectedBarName.empty() || entry.path[1] != selectedBarName) {
      return false;
    }
    const bool monitorEntry = isBarMonitorOverrideSettingPath(entry.path);
    if (selectedMonitorOverride.empty()) {
      return !monitorEntry;
    }
    return monitorEntry && entry.path[3] == selectedMonitorOverride;
  }

  bool settingEntryMatchesDockNavigation(const SettingEntry& entry, std::string_view selectedMonitorOverride) {
    if (entry.section != SettingsSection::Dock || entry.path.empty() || entry.path[0] != "dock") {
      return false;
    }
    const bool monitorEntry = isDockMonitorOverrideSettingPath(entry.path);
    if (selectedMonitorOverride.empty()) {
      return !monitorEntry;
    }
    return monitorEntry && entry.path[2] == selectedMonitorOverride;
  }

  std::string barSettingContentSectionKey(const SettingEntry& entry) {
    if (entry.section == SettingsSection::Dock) {
      std::string key = "dock";
      if (isDockMonitorOverrideSettingPath(entry.path)) {
        key += ":monitor:" + entry.path[2];
      }
      return key;
    }
    if (entry.section != SettingsSection::Bar || entry.path.size() < 2) {
      return std::string(settingsSectionId(entry.section));
    }
    std::string key = "bar:" + entry.path[1];
    if (isBarMonitorOverrideSettingPath(entry.path)) {
      key += ":monitor:" + entry.path[3];
    }
    return key;
  }

  std::span<const SettingsSectionDescriptor> settingsSectionDescriptors() { return kSettingsSections; }

  std::string_view settingsSectionId(SettingsSection section) { return descriptorFor(section).id; }

  std::string settingsSectionLabelKey(SettingsSection section) {
    return "settings.navigation.sections." + std::string(settingsSectionId(section));
  }

  std::string_view sectionGlyph(SettingsSection section) { return descriptorFor(section).glyph; }

  std::optional<SettingsSection> settingsSectionFromId(std::string_view id) {
    const auto it = std::ranges::find(kSettingsSections, id, &SettingsSectionDescriptor::id);
    if (it == kSettingsSections.end()) {
      return std::nullopt;
    }
    return it->section;
  }

  std::vector<SettingEntry> buildSettingsRegistry(
      const Config& cfg, const BarConfig* selectedBar, const BarMonitorOverride* selectedMonitorOverride,
      const RegistryEnvironment& env
  ) {
    (void)selectedBar;
    (void)selectedMonitorOverride;
    using i18n::tr;
    const auto positionSelect = [](std::string_view selected) {
      return asSegmented(plainSelect(
          {{"top", "settings.options.edge.top"},
           {"bottom", "settings.options.edge.bottom"},
           {"left", "settings.options.edge.left"},
           {"right", "settings.options.edge.right"}},
          selected
      ));
    };
    std::vector<SettingEntry> entries;

    // Appearance
    // Saved themes come first: picking one sets the palette and wallpaper rows below.
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "theme", "appearance.saved-themes", {},
        ButtonSetting{
            .label = tr("settings.schema.appearance.saved-themes.open"),
            .action = []() { PanelManager::instance().togglePanel("themes"); },
            .glyph = "palette",
        },
        "themes presets saved palette wallpaper switcher"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "theme", "appearance.theme-mode", {"theme", "mode"},
        asSegmented(enumSelect(kThemeModes, cfg.theme.mode)), "dark light auto colors"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "theme", "appearance.palette-source", {"theme", "source"},
        asSegmented(enumSelect(kPaletteSources, cfg.theme.source)), "palette colors"
    ));
    if (cfg.theme.source == PaletteSource::Builtin) {
      entries.push_back(keyedEntry(
          SettingsSection::Appearance, "theme", "appearance.builtin-palette", {"theme", "builtin"},
          builtinPaletteSelect(cfg.theme.builtinPalette, cfg.theme.mode), "builtin palette colors"
      ));
    } else if (cfg.theme.source == PaletteSource::Wallpaper) {
      entries.push_back(keyedEntry(
          SettingsSection::Appearance, "theme", "appearance.wallpaper-generation-scheme", {"theme", "wallpaper_scheme"},
          wallpaperSchemeSelect(cfg.theme.wallpaperScheme), "wallpaper palette generator scheme material you m3 colors"
      ));
    } else if (cfg.theme.source == PaletteSource::Community) {
      SettingControl communityPaletteControl =
          TextSetting{.value = cfg.theme.communityPalette, .placeholder = "Oxocarbon", .browseFileExtensions = {}};
      if (!env.communityPalettes.empty()) {
        communityPaletteControl = SearchPickerSetting{
            .options = env.communityPalettes,
            .selectedValue = cfg.theme.communityPalette,
            .placeholder = tr("settings.schema.appearance.community-palette.search-placeholder"),
            .emptyText = tr("ui.controls.search-picker.empty"),
            .preferredHeight = 240.0F,
        };
      }
      entries.push_back(keyedEntry(
          SettingsSection::Appearance, "theme", "appearance.community-palette", {"theme", "community_palette"},
          std::move(communityPaletteControl), "community palette colors"
      ));
    } else if (cfg.theme.source == PaletteSource::Custom) {
      SettingControl customPaletteControl =
          TextSetting{.value = cfg.theme.customPalette, .placeholder = "", .browseFileExtensions = {}};
      if (!env.customPalettes.empty()) {
        customPaletteControl = SearchPickerSetting{
            .options = env.customPalettes,
            .selectedValue = cfg.theme.customPalette,
            .placeholder = tr("settings.schema.appearance.custom-palette.search-placeholder"),
            .emptyText = tr("ui.controls.search-picker.empty"),
            .preferredHeight = 240.0F,
        };
      }
      entries.push_back(keyedEntry(
          SettingsSection::Appearance, "theme", "appearance.custom-palette", {"theme", "custom_palette"},
          std::move(customPaletteControl), "custom palette colors"
      ));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "theme", "appearance.palette-inspector", {},
        ButtonSetting{
            .label = tr("settings.schema.appearance.palette-inspector.button"),
            .action = {},
            .glyph = "color-swatch",
        },
        "palette inspector template tokens colors swatches hex terminal ansi"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "theme", "appearance.pure-black-dark", {"theme", "pure_black_dark"},
        ToggleSetting{cfg.theme.pureBlackDark}, "oled amoled true black background contrast"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "theme", "appearance.sync-color-scheme", {"theme", "sync_color_scheme"},
        ToggleSetting{cfg.theme.syncColorScheme}, "gnome gtk portal dark light prefer color scheme apps"
    ));
    // paw2's look sits beside the shell's on this page, while paw2 shares its config with the shell.
    const std::optional<Paw2Values> paw2 = env.paw2Value ? std::optional<Paw2Values>(std::in_place, env) : std::nullopt;
    {
      SettingControl fontFamilyControl =
          TextSetting{.value = cfg.shell.fontFamily, .placeholder = "sans-serif", .browseFileExtensions = {}};
      if (!env.fontFamilies.empty()) {
        fontFamilyControl = SearchPickerSetting{
            .options = env.fontFamilies,
            .selectedValue = cfg.shell.fontFamily,
            .placeholder = "sans-serif",
            .emptyText = tr("ui.controls.search-picker.empty"),
            .preferredHeight = 280.0F,
        };
      }
      entries.push_back(keyedEntry(
          SettingsSection::Appearance, "interface", "appearance.font-family", {"shell", "font_family"},
          std::move(fontFamilyControl), "typeface"
      ));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "interface", "appearance.language", {"shell", "lang"},
        languageSelect(cfg.shell.lang), "locale translation", true
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "interface", "appearance.corner-roundness", {"shell", "corner_radius_scale"},
        sliderFor(cfg.shell.cornerRadiusScale, noctalia::config::schema::kCornerRadiusScaleRange, false),
        "rounded corners radius"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "interface", "appearance.app-icon-colorize", {"shell", "app_icon_colorize"},
        ToggleSetting{cfg.shell.appIconColorize}, "tint all application icons"
    ));
    {
      const SettingVisibility colorizeOn = [](const Config& c) { return c.shell.appIconColorize; };
      ShellConfig colorizeShell = cfg.shell;
      colorizeShell.appIconColorize = true;
      const ColorSpec pickerColor =
          cfg.shell.appIconColor.value_or(*effectiveShellAppIconColorizationTint(colorizeShell));
      auto e = keyedEntry(
          SettingsSection::Appearance, "interface", "appearance.app-icon-color", {"shell", "app_icon_color"},
          colorSpecPicker(pickerColor), "color role dock tray application icons"
      );
      e.visibleWhen = colorizeOn;
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "interface", "appearance.show-close-buttons", {"shell", "show_close_buttons"},
        ToggleSetting{cfg.shell.showCloseButtons},
        "close button cross dismiss hide panel notification toast popup dialog sheet settings window"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "interface", "shell.settings-window-translucent",
        {"shell", "settings_window_translucent"}, ToggleSetting{cfg.shell.settingsWindowTranslucent},
        "settings window background alpha transparency translucent opacity"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "interface", "shell.settings-expand-all-groups",
        {"shell", "settings_expand_all_groups"}, ToggleSetting{cfg.shell.settingsExpandAllGroups},
        "settings window groups expand collapse expanded"
    ));
    if (paw2) {
      appendPaw2WindowEntries(entries, *paw2);
    }
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "borders", "appearance.button-borders", {"shell", "button_borders"},
        ToggleSetting{cfg.shell.buttonBorders}, "button outline border flat minimal"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "borders", "appearance.input-borders", {"shell", "input_borders"},
        ToggleSetting{cfg.shell.inputBorders}, "input text box field outline border flat minimal"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "borders", "appearance.popup-borders", {"shell", "popup_borders"},
        ToggleSetting{cfg.shell.popupBorders}, "popup menu dropdown outline border flat minimal"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "borders", "appearance.card-borders", {"shell", "card_borders"},
        ToggleSetting{cfg.shell.cardBorders}, "card section outline border flat minimal"
    ));
    entries.push_back(makeEntry(
        SettingsSection::Appearance, "shadows", tr("settings.schema.shared.shadow-direction.label"),
        tr("settings.schema.appearance.global-shadow-direction.description"), {"shell", "shadow", "direction"},
        enumSelect(kShadowDirections, cfg.shell.shadow.direction), "shadow direction"
    ));
    entries.push_back(makeEntry(
        SettingsSection::Appearance, "shadows", tr("settings.schema.shared.shadow-alpha.label"),
        tr("settings.schema.appearance.global-shadow-alpha.description"), {"shell", "shadow", "alpha"},
        sliderFor(cfg.shell.shadow.alpha, noctalia::config::schema::kUnitRange, false), "shadow opacity", true
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "shadows", "appearance.popup-shadows", {"shell", "popup_shadows"},
        ToggleSetting{cfg.shell.popupShadows}, "popup menu dropdown drop shadow depth"
    ));
    if (paw2) {
      appendPaw2ShadowEntries(entries, *paw2);
      appendPaw2TransparencyEntries(entries, *paw2);
    }
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "motion", "appearance.animations", {"shell", "animation", "enabled"},
        // It switches window animations too, so it reads on only while both are on.
        ToggleSetting{cfg.shell.animation.enabled && (!paw2 || paw2->boolean("animations.enabled", false))},
        "motion window"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "motion", "appearance.animation-speed", {"shell", "animation", "speed"},
        sliderFor(cfg.shell.animation.speed, noctalia::config::schema::kAnimationSpeedRange, false), "motion"
    ));
    if (paw2) {
      appendPaw2DesktopThemeEntries(entries, *paw2);
    }
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "accessibility", "appearance.ui-scale", {"accessibility", "ui_scale"},
        sliderFor(cfg.accessibility.uiScale, noctalia::config::schema::kScaleRange, false), "size scale text panels"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "accessibility", "accessibility.high-contrast", {"accessibility", "high_contrast"},
        ToggleSetting{cfg.accessibility.highContrast}, "accessibility high contrast visually impaired"
    ));

    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "profile", "shell.avatar-path", {"shell", "avatar_path"},
        TextSetting{
            .value = env.shellAvatarPath,
            .placeholder = tr("settings.schema.shell.avatar-path.placeholder"),
            .browseMode = TextSettingBrowseMode::OpenFile,
            .browseFileExtensions = DirectoryScanner::imageExtensionFilter(true)
        },
        "image picture"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "formats", "shell.time-format", {"shell", "time_format"},
        TextSetting{.value = cfg.shell.timeFormat, .placeholder = "{:%H:%M}", .browseFileExtensions = {}},
        "clock time format strftime chrono"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "formats", "shell.date-format", {"shell", "date_format"},
        TextSetting{.value = cfg.shell.dateFormat, .placeholder = "%A, %x", .browseFileExtensions = {}},
        "calendar date format strftime chrono"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Appearance, "formats", "shell.keyboard-layout-custom-labels",
        {"shell", "keyboard_layout", "custom_labels"},
        StringMapSetting{
            .entries = cfg.shell.keyboardLayout.customLabels,
            .suggestedKeys = env.keyboardLayoutNames,
            .keyPlaceholder = tr("settings.widgets.map-placeholders.layout-name"),
            .valuePlaceholder = tr("settings.widgets.map-placeholders.value"),
        },
        "keyboard layout nickname alias label"
    ));
    // Wallpaper
    entries.push_back(makeEntry(
        SettingsSection::Wallpaper, "general", tr("settings.schema.shared.enabled.label"),
        tr("settings.schema.wallpaper.enabled.description"), {"wallpaper", "enabled"},
        ToggleSetting{cfg.wallpaper.enabled}, "background image"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "general", "wallpaper.fill-mode", {"wallpaper", "fill_mode"},
        asSegmented(enumSelect(kWallpaperFillModes, cfg.wallpaper.fillMode)), "scale aspect"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "general", "wallpaper.fill-color", {"wallpaper", "fill_color"},
        colorSpecPicker(cfg.wallpaper.fillColor, true), "background solid color"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "directories", "wallpaper.directory", {"wallpaper", "directory"},
        TextSetting{
            .value = cfg.wallpaper.directory,
            .placeholder = FileUtils::defaultPicturesDirectory().string(),
            .browseMode = TextSettingBrowseMode::SelectFolder,
            .browseFileExtensions = {}
        },
        "folder path"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "directories", "wallpaper.directory-light", {"wallpaper", "directory_light"},
        TextSetting{
            .value = cfg.wallpaper.directoryLight,
            .placeholder = tr("settings.schema.wallpaper.directory-light.placeholder"),
            .browseMode = TextSettingBrowseMode::SelectFolder,
            .browseFileExtensions = {}
        },
        "folder path light theme", true
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "directories", "wallpaper.directory-dark", {"wallpaper", "directory_dark"},
        TextSetting{
            .value = cfg.wallpaper.directoryDark,
            .placeholder = tr("settings.schema.wallpaper.directory-dark.placeholder"),
            .browseMode = TextSettingBrowseMode::SelectFolder,
            .browseFileExtensions = {}
        },
        "folder path dark theme", true
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "directories", "wallpaper.per-monitor-directories",
        {"wallpaper", "per_monitor_directories"}, ToggleSetting{cfg.wallpaper.perMonitorDirectories},
        "per display folder"
    ));
    for (const auto& outputOpt : env.availableOutputs) {
      const std::string& connector = outputOpt.value;
      if (connector.empty()) {
        continue;
      }
      const std::vector<std::string> root = {"wallpaper", "monitor", connector};
      auto monitorPath = [&](std::string key) {
        std::vector<std::string> p = root;
        p.push_back(std::move(key));
        return p;
      };
      constexpr SettingsSection section = SettingsSection::Wallpaper;
      const WallpaperMonitorOverride* ovr = nullptr;
      for (const auto& candidate : cfg.wallpaper.monitorOverrides) {
        if (candidate.match == connector) {
          ovr = &candidate;
          break;
        }
      }
      auto perMonOn = SettingVisibility{[](const Config& c) { return c.wallpaper.perMonitorDirectories; }};
      {
        auto e = makeEntry(
            section, "monitors", connector, tr("settings.schema.wallpaper.fill-color.label"), monitorPath("fill_color"),
            colorSpecPicker(
                ovr != nullptr ? ovr->fillColor : std::optional<ColorSpec>{}, true, tr("common.states.inherit")
            ),
            "monitor background solid color", true
        );
        entries.push_back(std::move(e));
      }
      {
        auto e = makeEntry(
            section, "monitors", connector, tr("settings.schema.wallpaper.monitor-directory.label"),
            monitorPath("directory"),
            TextSetting{
                .value = ovr != nullptr && ovr->directory.has_value() ? *ovr->directory : "",
                .placeholder = FileUtils::defaultPicturesDirectory().string(),
                .browseMode = TextSettingBrowseMode::SelectFolder,
                .browseFileExtensions = {}
            },
            "monitor folder"
        );
        e.visibleWhen = perMonOn;
        entries.push_back(std::move(e));
      }
      entries.push_back(makeEntry(
                            section, "monitors", connector,
                            tr("settings.schema.wallpaper.monitor-directory-light.label"),
                            monitorPath("directory_light"),
                            TextSetting{
                                .value = ovr != nullptr && ovr->directoryLight.has_value() ? *ovr->directoryLight : "",
                                .placeholder = tr("settings.schema.wallpaper.monitor-directory-light.placeholder"),
                                .browseMode = TextSettingBrowseMode::SelectFolder,
                                .browseFileExtensions = {}
                            },
                            "monitor light folder", true
      )
                            .shownWhen(perMonOn));
      entries.push_back(makeEntry(
                            section, "monitors", connector,
                            tr("settings.schema.wallpaper.monitor-directory-dark.label"), monitorPath("directory_dark"),
                            TextSetting{
                                .value = ovr != nullptr && ovr->directoryDark.has_value() ? *ovr->directoryDark : "",
                                .placeholder = tr("settings.schema.wallpaper.monitor-directory-dark.placeholder"),
                                .browseMode = TextSettingBrowseMode::SelectFolder,
                                .browseFileExtensions = {}
                            },
                            "monitor dark folder", true
      )
                            .shownWhen(perMonOn));
    }
    {
      MultiSelectSetting transitions;
      transitions.options.reserve(std::size(kWallpaperTransitions));
      for (const auto& opt : kWallpaperTransitions) {
        transitions.options.push_back(SelectOption{std::string(opt.key), tr(opt.labelKey)});
      }
      transitions.selectedValues.reserve(cfg.wallpaper.transitions.size());
      for (const auto& t : cfg.wallpaper.transitions) {
        transitions.selectedValues.emplace_back(enumToKey(kWallpaperTransitions, t));
      }
      entries.push_back(keyedEntry(
          SettingsSection::Wallpaper, "transition", "wallpaper.transitions", {"wallpaper", "transition"},
          std::move(transitions), "effects animation pool"
      ));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "transition", "wallpaper.transition-duration", {"wallpaper", "transition_duration"},
        sliderFor(
            cfg.wallpaper.transitionDurationMs, noctalia::config::schema::kWallpaperTransitionDurationRange, true
        ),
        "fade animation"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "transition", "wallpaper.edge-smoothness", {"wallpaper", "edge_smoothness"},
        sliderFor(cfg.wallpaper.edgeSmoothness, noctalia::config::schema::kUnitRange, false), "transition feathering",
        true
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "transition", "wallpaper.transition-on-startup",
        {"wallpaper", "transition_on_startup"}, ToggleSetting{cfg.wallpaper.transitionOnStartup}, "startup animation"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "automation", "wallpaper.automation", {"wallpaper", "automation", "enabled"},
        ToggleSetting{cfg.wallpaper.automation.enabled}, "rotate slideshow"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "automation", "wallpaper.automation-interval",
        {"wallpaper", "automation", "interval_seconds"},
        StepperSetting{
            .value = cfg.wallpaper.automation.intervalSeconds,
            .minValue = static_cast<int>(noctalia::config::schema::kWallpaperAutomationIntervalRange.min.value()),
            .maxValue = static_cast<int>(noctalia::config::schema::kWallpaperAutomationIntervalRange.max.value()),
            .step = static_cast<int>(noctalia::config::schema::kWallpaperAutomationIntervalRange.step.value()),
            .valueSuffix = "s",
        },
        "rotate slideshow"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "automation", "wallpaper.automation-order", {"wallpaper", "automation", "order"},
        asSegmented(enumSelect(kWallpaperAutomationOrders, cfg.wallpaper.automation.order)), "rotate slideshow"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Wallpaper, "automation", "wallpaper.automation-recursive",
        {"wallpaper", "automation", "recursive"}, ToggleSetting{cfg.wallpaper.automation.recursive}, "subdirectories",
        true
    ));

    // Templates
    const auto builtInTemplatesOn =
        SettingVisibility{[](const Config& c) { return c.theme.templates.enableBuiltinTemplates; }};
    const auto communityTemplatesOn =
        SettingVisibility{[](const Config& c) { return c.theme.templates.enableCommunityTemplates; }};
    entries.push_back(keyedEntry(
        SettingsSection::Templates, "built-in", "templates.enable-builtins",
        {"theme", "templates", "enable_builtin_templates"}, ToggleSetting{cfg.theme.templates.enableBuiltinTemplates},
        "theme templates"
    ));
    {
      const auto availableTemplates = noctalia::theme::availableTemplates();
      std::vector<SelectOption> templateOptions;
      templateOptions.reserve(availableTemplates.size());
      for (const auto& t : availableTemplates) {
        templateOptions.push_back(
            SelectOption{
                .value = t.id,
                .label = t.displayName,
                .description = t.category,
                .tooltip = noctalia::theme::formatTemplateTooltip(t)
            }
        );
      }
      auto e = keyedEntry(
          SettingsSection::Templates, "built-in", "templates.builtin-ids", {"theme", "templates", "builtin_ids"},
          TemplateGridSetting{
              .options = std::move(templateOptions),
              .selectedValues = cfg.theme.templates.builtinIds,
              .emptyText = tr("settings.schema.templates.builtin-ids.empty"),
          },
          "theme templates apps foot walker gtk"
      );
      e.visibleWhen = builtInTemplatesOn;
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Templates, "community", "templates.enable-community-templates",
        {"theme", "templates", "enable_community_templates"},
        ToggleSetting{cfg.theme.templates.enableCommunityTemplates}, "theme templates community"
    ));
    {
      const std::size_t enabledCount = cfg.theme.templates.communityIds.size();
      auto e = makeEntry(
          SettingsSection::Templates, "community", tr("settings.schema.templates.community-ids.label"),
          enabledCount == 0 ? tr("settings.schema.templates.community-ids.description")
                            : tr("settings.templates.store.enabled-count", "count", std::to_string(enabledCount)),
          {},
          ButtonSetting{
              .label = tr("settings.templates.store.browse"),
              .glyph = "apps",
          },
          "theme templates community apps discord fuzzel vscode walker store browse"
      );
      e.visibleWhen = communityTemplatesOn;
      entries.push_back(std::move(e));
    }

    // Dock and per-monitor dock pages share the same controls. Override pages display effective values while
    // writing to their own table, so untouched fields continue to inherit from the global dock.
    const auto addDockSettings = [&](const DockConfig& dock, std::vector<std::string> root,
                                     std::optional<std::string> monitorTable) {
      const auto path = [&root](std::string key) {
        auto result = root;
        result.push_back(std::move(key));
        return result;
      };
      const auto currentDock = [monitorTable](const Config& current) {
        if (monitorTable) {
          if (const auto* override = findDockMonitorOverride(current.dock, *monitorTable)) {
            return resolveDockMonitorOverride(current.dock, *override);
          }
        }
        return current.dock;
      };

      entries.push_back(makeEntry(
          SettingsSection::Dock, "general", tr("settings.schema.shared.enabled.label"),
          tr("settings.schema.dock.enabled.description"), path("enabled"), ToggleSetting{dock.enabled}, "launcher apps"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "general", "dock.active-monitor-only", path("active_monitor_only"),
          ToggleSetting{dock.activeMonitorOnly}, "monitor"
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "behavior", tr("settings.schema.shared.auto-hide.label"),
          tr("settings.schema.dock.auto-hide.description"), path("auto_hide"),
          autoHideModeSelect(barAutoHideMode(dock.autoHide, dock.smartAutoHide), path("smart_auto_hide")),
          "autohide smart workspace"
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "behavior", tr("settings.schema.shared.reserve-space.label"),
          tr("settings.schema.dock.reserve-space.description"), path("reserve_space"), ToggleSetting{dock.reserveSpace},
          "exclusive zone"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "behavior", "dock.layer", path("layer"),
          asSegmented(plainSelect(
              {{"top", "settings.options.layer.top"}, {"overlay", "settings.options.layer.overlay"}}, dock.layer
          )),
          "layer shell z-order"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "behavior", "dock.show-running", path("show_running"), ToggleSetting{dock.showRunning},
          "windows"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "behavior", "dock.show-dots", path("show_dots"), ToggleSetting{dock.showDots},
          "running dots"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "behavior", "dock.show-instance-count", path("show_instance_count"),
          ToggleSetting{dock.showInstanceCount}, "badge windows"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "layout", "dock.launcher-position", path("launcher_position"),
          asSegmented(enumSelect(kDockLauncherPositions, dock.launcherPosition)), "launcher apps grid"
      ));
      const SettingVisibility dockLauncherEnabled = [currentDock](const Config& current) {
        const auto effective = currentDock(current);
        return effective.launcherPosition == DockLauncherPosition::Start
            || effective.launcherPosition == DockLauncherPosition::End;
      };
      entries.push_back(
          keyedEntry(
              SettingsSection::Dock, "layout", "dock.launcher-icon", path("launcher_icon"),
              TextSetting{.value = dock.launcherIcon, .placeholder = "grid-dots", .browseFileExtensions = {}},
              "launcher apps icon glyph"
          )
              .shownWhen(dockLauncherEnabled)
      );
      entries.push_back(keyedEntry(
                            SettingsSection::Dock, "layout", "dock.launcher-custom-image",
                            path("launcher_custom_image"),
                            TextSetting{
                                .value = dock.launcherCustomImage,
                                .placeholder = tr("settings.schema.dock.launcher-custom-image.placeholder"),
                                .browseMode = TextSettingBrowseMode::OpenFile,
                                .browseFileExtensions = DirectoryScanner::imageExtensionFilter(true),
                                .browseFallbackDirectory = "/usr/share/icons",
                            },
                            "launcher apps image picture logo"
      )
                            .shownWhen(dockLauncherEnabled));
      entries.push_back(keyedEntry(
                            SettingsSection::Dock, "layout", "dock.launcher-custom-image-colorize",
                            path("launcher_custom_image_colorize"), ToggleSetting{dock.launcherCustomImageColorize},
                            "launcher apps image tint color"
      )
                            .shownWhen(dockLauncherEnabled));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "layout", tr("settings.schema.shared.position.label"),
          tr("settings.schema.dock.position.description"), path("position"),
          asSegmented(enumSelect(kDockEdges, dock.position)), "edge"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "layout", "dock.icon-size", path("icon_size"),
          sliderFor(dock.iconSize, noctalia::config::schema::kDockIconSizeRange, true), "apps"
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "layout", tr("settings.schema.shared.main-axis-padding.label"),
          tr("settings.schema.dock.main-axis-padding.description"), path("main_axis_padding"),
          sliderFor(dock.mainAxisPadding, noctalia::config::schema::kDockPaddingRange, true), "inset"
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "layout", tr("settings.schema.shared.cross-axis-padding.label"),
          tr("settings.schema.dock.cross-axis-padding.description"), path("cross_axis_padding"),
          sliderFor(dock.crossAxisPadding, noctalia::config::schema::kDockPaddingRange, true), "inset"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "layout", "dock.item-spacing", path("item_spacing"),
          sliderFor(dock.itemSpacing, noctalia::config::schema::kDockItemSpacingRange, true), "gap"
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "layout", tr("settings.schema.shared.ends-margin.label"),
          tr("settings.schema.dock.ends-margin.description"), path("margin_ends"),
          sliderFor(dock.marginEnds, noctalia::config::schema::kDockMarginEndsRange, true), "gap inset"
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "layout", tr("settings.schema.shared.edge-margin.label"),
          tr("settings.schema.dock.edge-margin.description"), path("margin_edge"),
          sliderFor(dock.marginEdge, noctalia::config::schema::kDockMarginEdgeRange, true), "gap inset"
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "shape", tr("settings.schema.shared.corner-radius.label"),
          tr("settings.schema.dock.corner-radius.description"), path("radius"),
          sliderFor(dock.radius, noctalia::config::schema::kDockRadiusRange, true), "rounded"
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "shape", tr("settings.schema.shared.corner-top-left.label"),
          tr("settings.schema.dock.corner-top-left.description"), path("radius_top_left"),
          sliderFor(dock.radiusTopLeft, noctalia::config::schema::kDockRadiusRange, true), "rounded corner", true
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "shape", tr("settings.schema.shared.corner-top-right.label"),
          tr("settings.schema.dock.corner-top-right.description"), path("radius_top_right"),
          sliderFor(dock.radiusTopRight, noctalia::config::schema::kDockRadiusRange, true), "rounded corner", true
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "shape", tr("settings.schema.shared.corner-bottom-left.label"),
          tr("settings.schema.dock.corner-bottom-left.description"), path("radius_bottom_left"),
          sliderFor(dock.radiusBottomLeft, noctalia::config::schema::kDockRadiusRange, true), "rounded corner", true
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "shape", tr("settings.schema.shared.corner-bottom-right.label"),
          tr("settings.schema.dock.corner-bottom-right.description"), path("radius_bottom_right"),
          sliderFor(dock.radiusBottomRight, noctalia::config::schema::kDockRadiusRange, true), "rounded corner", true
      ));
      {
        auto entry = keyedEntry(
            SettingsSection::Dock, "shape", "dock.concave-edge-corners", path("concave_edge_corners"),
            ToggleSetting{dock.concaveEdgeCorners}, "rounded corner carve"
        );
        entry.visibleWhen = [currentDock](const Config& current) { return currentDock(current).marginEdge == 0; };
        entries.push_back(std::move(entry));
      }
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "shape", "dock.border", path("border"), colorSpecPicker(dock.border), "outline color",
          true
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "shape", "dock.border-width", path("border_width"),
          sliderFor(dock.borderWidth, noctalia::config::schema::kDockBorderWidthRange, false), "outline stroke", true
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "effects", tr("settings.schema.shared.background-opacity.label"),
          tr("settings.schema.dock.background-opacity.description"), path("background_opacity"),
          sliderFor(dock.backgroundOpacity, noctalia::config::schema::kUnitRange, false),
          "alpha transparency translucent opacity"
      ));
      entries.push_back(makeEntry(
          SettingsSection::Dock, "effects", tr("settings.schema.shared.shadow.label"),
          tr("settings.schema.dock.shadow.description"), path("shadow"), ToggleSetting{dock.shadow}, "shadow"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "focus-styling", "dock.magnification", path("magnification"),
          ToggleSetting{dock.magnification}, "magnify zoom mac"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "focus-styling", "dock.magnification-scale", path("magnification_scale"),
          sliderFor(dock.magnificationScale, noctalia::config::schema::kDockMagnificationScaleRange, false),
          "magnify zoom"
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "focus-styling", "dock.active-icon-scale", path("active_scale"),
          sliderFor(dock.activeScale, noctalia::config::schema::kDockActiveScaleRange, false), "focused", true
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "focus-styling", "dock.inactive-icon-scale", path("inactive_scale"),
          sliderFor(dock.inactiveScale, noctalia::config::schema::kDockInactiveScaleRange, false), "unfocused", true
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "focus-styling", "dock.active-icon-opacity", path("active_opacity"),
          sliderFor(dock.activeOpacity, noctalia::config::schema::kUnitRange, false), "focused alpha", true
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "focus-styling", "dock.inactive-icon-opacity", path("inactive_opacity"),
          sliderFor(dock.inactiveOpacity, noctalia::config::schema::kUnitRange, false), "unfocused alpha", true
      ));
      entries.push_back(keyedEntry(
          SettingsSection::Dock, "pinned-apps", "dock.pinned-apps", path("pinned"), ListSetting{.items = dock.pinned},
          "pinned apps dock"
      ));
    };

    addDockSettings(cfg.dock, {"dock"}, std::nullopt);
    for (const auto& override : cfg.dock.monitorOverrides) {
      addDockSettings(
          resolveDockMonitorOverride(cfg.dock, override), {"dock", "monitor", override.tableName}, override.tableName
      );
    }

    // Panels
    {
      SelectSetting anchorBarSelect;
      for (const auto& name : barNames(cfg)) {
        anchorBarSelect.options.push_back(SelectOption{name, name});
      }
      anchorBarSelect.selectedValue = cfg.shell.panelAnchorBar;
      anchorBarSelect.allowEmptySelection = true;
      entries.push_back(keyedEntry(
          SettingsSection::Panels, "general", "panels.panel-anchor-bar", {"shell", "panel_anchor_bar"},
          std::move(anchorBarSelect), "anchor attach bar panel wallpaper launcher"
      ));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "general", "panels.floating-layer", {"shell", "panel", "floating_layer"},
        asSegmented(plainSelect(
            {{"top", "settings.options.layer.top"}, {"overlay", "settings.options.layer.overlay"}},
            cfg.shell.panel.floatingLayer
        )),
        "floating detached panel layer shell z-order input method popup fullscreen"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "effects", "panels.transparency-mode", {"shell", "panel", "transparency_mode"},
        asSegmented(enumSelect(kPanelTransparencyModes, cfg.shell.panel.transparencyMode)),
        "glass alpha transparency translucent opacity cards blur"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "effects", "panels.borders", {"shell", "panel", "borders"},
        ToggleSetting{cfg.shell.panel.borders}, "outline border shell edge"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "effects", "panels.border-color", {"shell", "panel", "border_color"},
        colorSpecPicker(cfg.shell.panel.borderColor), "outline border color theme accent"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "effects", "panels.border-width", {"shell", "panel", "border_width"},
        sliderFor(cfg.shell.panel.borderWidth, noctalia::config::schema::kPanelBorderWidthRange, false),
        "outline border width thickness"
    ));
    entries.push_back(makeEntry(
        SettingsSection::Panels, "effects", tr("settings.schema.shared.shadow.label"),
        tr("settings.schema.panels.shadow.description"), {"shell", "panel", "shadow"},
        ToggleSetting{cfg.shell.panel.shadow}, "shadow depth"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "effects", "panels.list-item-background", {"shell", "panel", "list_item_background"},
        ToggleSetting{cfg.shell.panel.listItemBackground}, "list item background fill row card launcher clipboard"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "effects", "panels.floating-offset", {"shell", "panel", "floating_offset"},
        StepperSetting{
            .value = static_cast<int>(cfg.shell.panel.floatingOffset),
            .minValue = 0,
            .maxValue = 100,
            .step = 1,
            .valueSuffix = "px",
        },
        "floating detached panel gap offset distance bar"
    ));
    // Floating-position select for a panel, shown only when its placement is Floating.
    const auto panelPositionEntry = [&](SettingsSection section, std::string group, std::string_view panelKey,
                                        std::string_view labelKey, std::string_view descKey, const std::string& current,
                                        PanelPlacement ShellConfig::PanelConfig::* placement) {
      auto e = makeEntry(
          section, group, tr(labelKey), tr(descKey),
          {std::string("shell"), std::string("panel"), std::string(panelKey) + "_position"},
          plainSelect(
              {{"auto", "settings.options.panel-position.auto"},
               {"center", "settings.options.screen-position.center"},
               {"top_left", "settings.options.screen-position.top-left"},
               {"top_center", "settings.options.screen-position.top-center"},
               {"top_right", "settings.options.screen-position.top-right"},
               {"center_left", "settings.options.screen-position.center-left"},
               {"center_right", "settings.options.screen-position.center-right"},
               {"bottom_left", "settings.options.screen-position.bottom-left"},
               {"bottom_center", "settings.options.screen-position.bottom-center"},
               {"bottom_right", "settings.options.screen-position.bottom-right"}},
              current
          ),
          "panel position screen anchor corner edge floating bottom right"
      );
      e.visibleWhen = [placement](const Config& c) { return c.shell.panel.*placement == PanelPlacement::Floating; };
      return e;
    };

    const auto panelBarAlignmentEntry = [](SettingsSection section, std::string group, std::string_view panelKey,
                                           std::string_view labelKey, std::string_view descKey, bool nearTrigger,
                                           PanelPlacement ShellConfig::PanelConfig::* placement,
                                           std::string ShellConfig::PanelConfig::* position) {
      auto alignment = plainSelect(
          {{"false", "settings.options.panel-bar-alignment.centered"},
           {"true", "settings.options.panel-bar-alignment.near-trigger"}},
          nearTrigger ? "true" : "false"
      );
      alignment.segmented = true;
      alignment.valueType = SelectValueType::Boolean;
      auto e = makeEntry(
          section, std::move(group), tr(labelKey), tr(descKey),
          {"shell", "panel", std::string("open_near_click_") + std::string(panelKey)}, std::move(alignment),
          "along bar centered near trigger widget click position anchor"
      );
      e.visibleWhen = [placement, position](const Config& c) {
        const auto& panel = c.shell.panel;
        return panel.*placement == PanelPlacement::Attached
            || (panel.*placement == PanelPlacement::Floating && panel.*position == "auto");
      };
      return e;
    };

    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "launcher", "panels.placement-launcher", {"shell", "panel", "launcher_placement"},
        asSegmented(enumSelect(kPanelPlacements, cfg.shell.panel.launcherPlacement)),
        "attached floating bar panel position"
    ));
    entries.push_back(panelPositionEntry(
        SettingsSection::Launcher, "launcher", "launcher", "settings.schema.panels.position-launcher.label",
        "settings.schema.panels.position-launcher.description", cfg.shell.panel.launcherPosition,
        &ShellConfig::PanelConfig::launcherPlacement
    ));
    entries.push_back(panelBarAlignmentEntry(
        SettingsSection::Launcher, "launcher", "launcher", "settings.schema.panels.open-near-click-launcher.label",
        "settings.schema.panels.open-near-click-launcher.description", cfg.shell.panel.openNearClickLauncher,
        &ShellConfig::PanelConfig::launcherPlacement, &ShellConfig::PanelConfig::launcherPosition
    ));
    const SettingVisibility launcherFull = [](const Config& c) {
      return c.shell.launcher.layout == ShellConfig::LauncherLayout::Full;
    };
    const SettingVisibility launcherSimple = [](const Config& c) {
      return c.shell.launcher.layout == ShellConfig::LauncherLayout::Simple;
    };
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "launcher", "panels.launcher-layout", {"shell", "launcher", "layout"},
        asSegmented(enumSelect(ShellConfig::kLauncherLayouts, cfg.shell.launcher.layout)),
        "launcher layout simple full classic minimal compact toolbar tabs"
    ));
    {
      auto width = sliderFor(cfg.shell.launcher.width, noctalia::config::schema::kLauncherWidthRange, true);
      width.valueSuffix = "px";
      auto e = keyedEntry(
          SettingsSection::Launcher, "launcher", "panels.launcher-width", {"shell", "launcher", "width"},
          std::move(width), "launcher panel width size"
      );
      e.visibleWhen = launcherFull;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Launcher, "launcher", "panels.launcher-search-position",
          {"shell", "launcher", "search_position"},
          asSegmented(enumSelect(ShellConfig::kLauncherSearchPositions, cfg.shell.launcher.searchPosition)),
          "launcher search toolbar position top bottom"
      );
      e.visibleWhen = launcherFull;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Launcher, "launcher", "panels.launcher-default-view", {"shell", "launcher", "default_view"},
          enumSelect(ShellConfig::kLauncherDefaultViews, cfg.shell.launcher.defaultView),
          "launcher default view recent pinned all apps categories"
      );
      e.visibleWhen = launcherFull;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Launcher, "launcher", "panels.launcher-categories", {"shell", "launcher", "categories"},
          ToggleSetting{cfg.shell.launcher.categories}, "launcher categories chips filter recent all"
      );
      e.visibleWhen = launcherSimple;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Launcher, "launcher", "panels.launcher-category-icons-only",
          {"shell", "launcher", "category_icons_only"}, ToggleSetting{cfg.shell.launcher.categoryIconsOnly},
          "launcher app category icons labels text"
      );
      e.visibleWhen = [](const Config& c) {
        return c.shell.launcher.layout == ShellConfig::LauncherLayout::Full || c.shell.launcher.categories;
      };
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Launcher, "launcher", "panels.launcher-show-settings-button",
          {"shell", "launcher", "show_settings_button"}, ToggleSetting{cfg.shell.launcher.showSettingsButton},
          "launcher settings shortcut button"
      );
      e.visibleWhen = launcherFull;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Launcher, "launcher", "panels.launcher-show-session-button",
          {"shell", "launcher", "show_session_button"}, ToggleSetting{cfg.shell.launcher.showSessionButton},
          "launcher session power shortcut button"
      );
      e.visibleWhen = launcherFull;
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "display", "panels.launcher-show-icons", {"shell", "launcher", "show_icons"},
        ToggleSetting{cfg.shell.launcher.showIcons}, "launcher app icons hide"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "display", "panels.launcher-app-origin-indicator",
        {"shell", "launcher", "show_app_origin_indicator"}, ToggleSetting{cfg.shell.launcher.showAppOriginIndicator},
        "launcher app origin indicator"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "display", "panels.launcher-grid", {"shell", "launcher", "app_grid"},
        ToggleSetting{cfg.shell.launcher.appGrid}, "launcher grid icons view layout"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "display", "panels.launcher-compact", {"shell", "launcher", "compact"},
        ToggleSetting{cfg.shell.launcher.compact}, "launcher compact rows dense"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "behavior", "panels.launcher-app-actions", {"shell", "launcher", "show_app_actions"},
        ToggleSetting{cfg.shell.launcher.showAppActions}, "launcher app actions show"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "behavior", "panels.launcher-currency-exchange",
        {"shell", "launcher", "fetch_exchange_rates"}, ToggleSetting{cfg.shell.launcher.fetchExchangeRates},
        "launcher currency exchange rates fetch online conversion"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "behavior", "panels.launcher-auto-paste", {"shell", "launcher", "auto_paste"},
        enumSelect(kClipboardAutoPasteModes, cfg.shell.launcher.autoPaste), "launcher auto paste"
    ));
    entries.push_back(makeEntry(
        SettingsSection::Launcher, "behavior", tr("settings.schema.shell.launch-apps-as-systemd-services.label"),
        env.systemdUserManaged ? tr("settings.schema.shell.launch-apps-as-systemd-services.description")
                               : tr("settings.schema.shell.launch-apps-as-systemd-services.requires-systemd-session"),
        {"shell", "launch_apps_as_systemd_services"},
        ToggleSetting{
            .checked = cfg.shell.launchAppsAsSystemdServices,
            // Keep a leftover `true` switchable off even when the session cannot honor it.
            .enabled = env.systemdUserManaged || cfg.shell.launchAppsAsSystemdServices
        }
    ));
    {
      auto e = keyedEntry(
          SettingsSection::Launcher, "behavior", "shell.launch-apps-custom-command",
          {"shell", "launch_apps_custom_command"},
          TextSetting{
              .value = cfg.shell.launchAppsCustomCommand,
              .placeholder = tr("settings.schema.shell.launch-apps-custom-command.placeholder"),
              .width = 320.0F,
              .browseFileExtensions = {},
          },
          "app command custom launcher dock taskbar"
      );
      e.visibleWhen = [managed = env.systemdUserManaged](const Config& c) {
        return !(c.shell.launchAppsAsSystemdServices && managed);
      };
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "history", "panels.launcher-track-usage", {"shell", "launcher", "track_usage"},
        ToggleSetting{cfg.shell.launcher.trackUsage}, "launcher track usage recent history frequency privacy"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "history", "panels.launcher-sort-by-usage", {"shell", "launcher", "sort_by_usage"},
        ToggleSetting{cfg.shell.launcher.sortByUsage, cfg.shell.launcher.trackUsage},
        "launcher sort usage recently used frequency"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "pinned-apps", "panels.launcher-pinned-apps", {"shell", "launcher", "pinned"},
        ListSetting{.items = cfg.shell.launcher.pinned}, "launcher pinned apps"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Launcher, "providers", "panels.launcher-prefix-character",
        {"shell", "launcher", "provider_prefix"},
        TextSetting{.value = cfg.shell.launcher.providerPrefix, .placeholder = "/"}, "launcher common prefix character"
    ));
    for (const auto& provider : kLauncherProviderSettings) {
      const auto it = std::ranges::find(cfg.shell.launcher.providers, provider.name, &LauncherProviderConfig::name);
      const LauncherProviderConfig* stored = it != cfg.shell.launcher.providers.end() ? &*it : nullptr;

      const std::string prefix = stored != nullptr ? stored->prefix : std::string();
      const bool global =
          stored != nullptr ? stored->global.value_or(provider.globalByDefault) : provider.globalByDefault;

      const std::string prefixKey = std::format("settings.schema.panels.launcher-prefix-{}", provider.name);
      const std::string globalKey = std::format("settings.schema.panels.launcher-global-{}", provider.name);

      entries.push_back(makeEntry(
          SettingsSection::Launcher, "providers", tr(prefixKey + ".label"), tr(prefixKey + ".description"),
          {"shell", "launcher", "providers", std::string(provider.name), "prefix"},
          TextSetting{.value = prefix, .placeholder = std::string(provider.prefixPlaceholder)},
          std::format("launcher {} prefix trigger", provider.name)
      ));
      entries.push_back(makeEntry(
          SettingsSection::Launcher, "providers", tr(globalKey + ".label"), tr(globalKey + ".description"),
          {"shell", "launcher", "providers", std::string(provider.name), "global"}, ToggleSetting{global},
          std::format("launcher {} global search unprefixed", provider.name)
      ));
      if (provider.name == "panels") {
        ListSetting ignoredPanels;
        ignoredPanels.items = cfg.shell.launcher.panels.ignored;
        const auto knownPanelIds = panel_catalog::allKnownIds();
        ignoredPanels.suggestedOptions.reserve(knownPanelIds.size());
        for (const auto& panelId : knownPanelIds) {
          ignoredPanels.suggestedOptions.push_back(SelectOption{panelId, panel_catalog::describe(panelId).title});
        }
        entries.push_back(keyedEntry(
            SettingsSection::Launcher, "providers", "panels.launcher-ignored-panels",
            {"shell", "launcher", "panels", "ignored"}, std::move(ignoredPanels),
            "launcher panels ignore hide exclude noise polkit setup wizard test"
        ));
      }
    }
    const SettingVisibility clipboardOn = [](const Config& c) { return c.shell.clipboardEnabled; };
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "clipboard", "shell.clipboard-enabled", {"shell", "clipboard_enabled"},
        ToggleSetting{cfg.shell.clipboardEnabled}, "clipboard history paste copy"
    ));
    // Deliberately not gated on clipboardOn: this keeps the live selection
    // alive, which is part of the transport that stays active when history
    // retention is turned off.
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "clipboard", "shell.clipboard-keep-from-closed-apps",
        {"shell", "clipboard_keep_from_closed_apps"}, ToggleSetting{cfg.shell.clipboardKeepFromClosedApps},
        "clipboard paste close quit exit persist"
    ));
    {
      auto e = keyedEntry(
          SettingsSection::Panels, "clipboard", "shell.clipboard-history-max-entries",
          {"shell", "clipboard_history_max_entries"},
          StepperSetting{
              .value = cfg.shell.clipboardHistoryMaxEntries,
              .minValue = static_cast<int>(noctalia::config::schema::kClipboardHistoryMaxEntriesRange.min.value()),
              .maxValue = static_cast<int>(noctalia::config::schema::kClipboardHistoryMaxEntriesRange.max.value()),
              .step = static_cast<int>(noctalia::config::schema::kClipboardHistoryMaxEntriesRange.step.value())
          },
          "clipboard history limit entries"
      );
      e.visibleWhen = clipboardOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Panels, "clipboard", "shell.clipboard-confirm-clear-history",
          {"shell", "clipboard_confirm_clear_history"}, ToggleSetting{cfg.shell.clipboardConfirmClearHistory},
          "clipboard history clear confirm pinned"
      );
      e.visibleWhen = clipboardOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Panels, "clipboard", "shell.clipboard-auto-paste", {"shell", "clipboard_auto_paste"},
          enumSelect(kClipboardAutoPasteModes, cfg.shell.clipboardAutoPaste), "clipboard paste"
      );
      e.visibleWhen = clipboardOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Panels, "clipboard", "shell.clipboard-image-action",
          {"shell", "clipboard_image_action_command"},
          TextSetting{
              .value = cfg.shell.clipboardImageActionCommand,
              .placeholder = tr("settings.schema.shell.clipboard-image-action.placeholder"),
              .width = 320.0F,
              .browseFileExtensions = {}
          },
          "clipboard image action annotation editor external gimp satty gradia"
      );
      e.visibleWhen = clipboardOn;
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "clipboard", "panels.placement-clipboard", {"shell", "panel", "clipboard_placement"},
        asSegmented(enumSelect(kPanelPlacements, cfg.shell.panel.clipboardPlacement)),
        "attached floating bar panel position"
    ));
    entries.push_back(panelPositionEntry(
        SettingsSection::Panels, "clipboard", "clipboard", "settings.schema.panels.position-clipboard.label",
        "settings.schema.panels.position-clipboard.description", cfg.shell.panel.clipboardPosition,
        &ShellConfig::PanelConfig::clipboardPlacement
    ));
    entries.push_back(panelBarAlignmentEntry(
        SettingsSection::Panels, "clipboard", "clipboard", "settings.schema.panels.open-near-click-clipboard.label",
        "settings.schema.panels.open-near-click-clipboard.description", cfg.shell.panel.openNearClickClipboard,
        &ShellConfig::PanelConfig::clipboardPlacement, &ShellConfig::PanelConfig::clipboardPosition
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "polkit", "panels.placement-polkit", {"shell", "panel", "polkit_placement"},
        asSegmented(enumSelect(kPanelPlacements, cfg.shell.panel.polkitPlacement)),
        "attached floating bar panel polkit auth password"
    ));
    entries.push_back(panelPositionEntry(
        SettingsSection::Panels, "polkit", "polkit", "settings.schema.panels.position-polkit.label",
        "settings.schema.panels.position-polkit.description", cfg.shell.panel.polkitPosition,
        &ShellConfig::PanelConfig::polkitPlacement
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "wallpaper", "panels.wallpaper-show-names", {"shell", "panel", "wallpaper_show_names"},
        ToggleSetting{cfg.shell.panel.wallpaperShowNames}, "picker grid caption file name label thumbnail"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Panels, "wallpaper", "panels.placement-wallpaper", {"shell", "panel", "wallpaper_placement"},
        asSegmented(enumSelect(kPanelPlacements, cfg.shell.panel.wallpaperPlacement)),
        "attached floating bar panel position"
    ));
    entries.push_back(panelPositionEntry(
        SettingsSection::Panels, "wallpaper", "wallpaper", "settings.schema.panels.position-wallpaper.label",
        "settings.schema.panels.position-wallpaper.description", cfg.shell.panel.wallpaperPosition,
        &ShellConfig::PanelConfig::wallpaperPlacement
    ));
    entries.push_back(panelBarAlignmentEntry(
        SettingsSection::Panels, "wallpaper", "wallpaper", "settings.schema.panels.open-near-click-wallpaper.label",
        "settings.schema.panels.open-near-click-wallpaper.description", cfg.shell.panel.openNearClickWallpaper,
        &ShellConfig::PanelConfig::wallpaperPlacement, &ShellConfig::PanelConfig::wallpaperPosition
    ));

    // Control Center
    entries.push_back(keyedEntry(
        SettingsSection::ControlCenter, "layout", "panels.placement-control-center",
        {"shell", "panel", "control_center_placement"},
        asSegmented(enumSelect(kPanelPlacements, cfg.shell.panel.controlCenterPlacement)),
        "attached floating bar panel position"
    ));
    entries.push_back(panelPositionEntry(
        SettingsSection::ControlCenter, "layout", "control_center",
        "settings.schema.panels.position-control-center.label",
        "settings.schema.panels.position-control-center.description", cfg.shell.panel.controlCenterPosition,
        &ShellConfig::PanelConfig::controlCenterPlacement
    ));
    entries.push_back(panelBarAlignmentEntry(
        SettingsSection::ControlCenter, "layout", "control_center",
        "settings.schema.panels.open-near-click-control-center.label",
        "settings.schema.panels.open-near-click-control-center.description", cfg.shell.panel.openNearClickControlCenter,
        &ShellConfig::PanelConfig::controlCenterPlacement, &ShellConfig::PanelConfig::controlCenterPosition
    ));
    {
      SliderSetting width =
          sliderFor(cfg.controlCenter.width, noctalia::config::schema::kControlCenterWidthRange, true);
      width.valueSuffix = "px";
      entries.push_back(keyedEntry(
          SettingsSection::ControlCenter, "layout", "panels.control-center-width", {"control_center", "width"},
          std::move(width), "size dimension wide narrow"
      ));
    }
    entries.push_back(keyedEntry(
        SettingsSection::ControlCenter, "navigation", "panels.control-center-sidebar", {"control_center", "sidebar"},
        asSegmented(enumSelect(kControlCenterSidebarModes, cfg.controlCenter.sidebarMode)),
        "full compact none sidebar icons narrow hidden"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::ControlCenter, "navigation", "panels.control-center-sidebar-section",
        {"control_center", "sidebar_section"},
        asSegmented(enumSelect(kControlCenterSidebarModes, cfg.controlCenter.sidebarSectionMode)),
        "full compact none sidebar icons narrow hidden tab direct widget shortcut"
    ));
    {
      MultiSelectSetting tabs;
      const auto catalog = ControlCenterPanel::hideableTabCatalog();
      tabs.options.reserve(catalog.size());
      tabs.selectedValues.reserve(catalog.size());
      for (const auto& tab : catalog) {
        tabs.options.push_back(SelectOption{std::string(tab.key), tr(tab.titleKey)});
        if (!std::ranges::contains(cfg.controlCenter.hiddenTabs, tab.key)) {
          tabs.selectedValues.emplace_back(tab.key);
        }
      }
      tabs.persistUnselected = true;
      entries.push_back(keyedEntry(
          SettingsSection::ControlCenter, "navigation", "panels.control-center-tabs", {"control_center", "hidden_tabs"},
          std::move(tabs), "tabs sections visible hide show display brightness media audio network power"
      ));
    }
    entries.push_back(keyedEntry(
        SettingsSection::ControlCenter, "home", "panels.home-shortcuts", {"control_center", "shortcuts"},
        ShortcutListSetting{
            .items = cfg.controlCenter.shortcuts, .suggestedOptions = controlCenterShortcutOptions(cfg), .maxItems = 6
        },
        "quick settings shortcuts toggles wifi bluetooth caffeine night light dnd power media weather clipboard"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::ControlCenter, "home", "panels.home-shortcuts-show-labels",
        {"control_center", "show_shortcut_labels"}, ToggleSetting{cfg.controlCenter.showShortcutLabels},
        "shortcuts labels text hide show titles"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::ControlCenter, "home", "panels.home-session-button", {"control_center", "show_session_button"},
        ToggleSetting{cfg.controlCenter.showSessionButton}, "session button show hide"
    ));

    // Desktop
    entries.push_back(keyedEntry(
        SettingsSection::Desktop, "widgets", "desktop.widgets", {"desktop_widgets", "enabled"},
        ToggleSetting{cfg.desktopWidgets.enabled}, "desktop"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Desktop, "screen-corners", "desktop.screen-corners-enabled",
        {"shell", "screen_corners", "enabled"}, ToggleSetting{cfg.shell.screenCorners.enabled}, "screen corners rounded"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Desktop, "screen-corners", "desktop.screen-corners-size", {"shell", "screen_corners", "size"},
        sliderFor(cfg.shell.screenCorners.size, noctalia::config::schema::kScreenCornersSizeRange, true),
        "screen corners radius"
    ));

    entries.push_back(keyedEntry(
        SettingsSection::Desktop, "hot-corners", "desktop.hot-corners-enabled", {"hot_corners", "enabled"},
        ToggleSetting{cfg.hotCorners.enabled}, "hot corners trigger mouse edge screen"
    ));
    {
      auto delay = sliderFor(
          static_cast<std::int64_t>(cfg.hotCorners.delayMs), noctalia::config::schema::kHotCornersDelayMsRange, true
      );
      delay.valueSuffix = "ms";
      SettingEntry e = keyedEntry(
          SettingsSection::Desktop, "hot-corners", "desktop.hot-corners-delay-ms", {"hot_corners", "delay_ms"},
          std::move(delay), "hot corners delay hold ms timeout"
      );
      e.visibleWhen = [](const Config& conf) { return conf.hotCorners.enabled; };
      entries.push_back(std::move(e));
    }

    auto hotCornerActionSelect = [](const std::string& current) {
      return plainSelect(
          {{"none", "settings.options.hot-corners.none"},
           {"launcher", "settings.options.hot-corners.launcher"},
           {"control_center", "settings.options.hot-corners.control-center"},
           {"window_switcher", "settings.options.hot-corners.window-switcher"},
           {"command", "settings.options.hot-corners.command"}},
          current
      );
    };

    auto addCornerEntry = [&](const std::string& key, const std::string& labelKey, const std::string& currentAction,
                              const std::string& currentCommand) {
      SettingEntry e = makeEntry(
          SettingsSection::Desktop, "hot-corners", tr(labelKey + ".label"), tr(labelKey + ".description"),
          {"hot_corners", key, "action"}, hotCornerActionSelect(currentAction), "hot corners " + key
      );
      e.visibleWhen = [](const Config& conf) { return conf.hotCorners.enabled; };
      entries.push_back(std::move(e));

      SettingEntry c = makeEntry(
          SettingsSection::Desktop, "hot-corners", tr(labelKey + "-command.label"),
          tr(labelKey + "-command.description"), {"hot_corners", key, "command"},
          TextSetting{.value = currentCommand, .placeholder = "Run command..."}, "hot corners command execute " + key
      );
      c.visibleWhen = [key](const Config& conf) {
        if (!conf.hotCorners.enabled) {
          return false;
        }
        const HotCornersConfig::Corner* corner = nullptr;
        if (key == "top_left") {
          corner = &conf.hotCorners.topLeft;
        } else if (key == "top_right") {
          corner = &conf.hotCorners.topRight;
        } else if (key == "bottom_left") {
          corner = &conf.hotCorners.bottomLeft;
        } else if (key == "bottom_right") {
          corner = &conf.hotCorners.bottomRight;
        }
        return corner != nullptr && corner->action == "command";
      };
      entries.push_back(std::move(c));
    };

    addCornerEntry(
        "top_left", "settings.schema.desktop.hot-corners-top-left", cfg.hotCorners.topLeft.action,
        cfg.hotCorners.topLeft.command
    );
    addCornerEntry(
        "top_right", "settings.schema.desktop.hot-corners-top-right", cfg.hotCorners.topRight.action,
        cfg.hotCorners.topRight.command
    );
    addCornerEntry(
        "bottom_left", "settings.schema.desktop.hot-corners-bottom-left", cfg.hotCorners.bottomLeft.action,
        cfg.hotCorners.bottomLeft.command
    );
    addCornerEntry(
        "bottom_right", "settings.schema.desktop.hot-corners-bottom-right", cfg.hotCorners.bottomRight.action,
        cfg.hotCorners.bottomRight.command
    );

    // Security
    entries.push_back(keyedEntry(
        SettingsSection::Security, "network", "shell.offline-mode", {"shell", "offline_mode"},
        ToggleSetting{cfg.shell.offlineMode}, "network http fetch download"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Security, "network", "shell.external-ip", {"shell", "external_ip_enabled"},
        ToggleSetting{cfg.shell.externalIpEnabled}, "wan external ip public address network resolve"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Security, "authentication", "shell.polkit-agent", {"shell", "polkit_agent"},
        ToggleSetting{cfg.shell.polkitAgent}, "auth password"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Security, "authentication", "shell.password-style", {"shell", "password_style"},
        asSegmented(enumSelect(kPasswordMaskStyles, cfg.shell.passwordMaskStyle)), "polkit lock mask"
    ));
    const SettingVisibility lockscreenOn = [](const Config& c) { return c.lockscreen.enabled; };
    {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.enabled", {"lockscreen", "enabled"},
          ToggleSetting{cfg.lockscreen.enabled}, "lock screen session"
      );
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.lock-before-suspend",
          {"lockscreen", "lock_before_suspend"}, ToggleSetting{cfg.lockscreen.lockBeforeSuspend},
          "lock screen before suspend sleep lid close prepareforsleep"
      );
      e.visibleWhen = lockscreenOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.grace-period", {"lockscreen", "grace_period_seconds"},
          StepperSetting{
              .value = std::clamp(cfg.lockscreen.gracePeriodSeconds, 0, 60),
              .minValue = 0,
              .maxValue = 60,
              .step = 1,
              .valueSuffix = "s"
          },
          "lock screen grace period passwordless unlock keypress mouse movement"
      );
      e.visibleWhen = lockscreenOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.fingerprint", {"lockscreen", "fingerprint"},
          ToggleSetting{cfg.lockscreen.fingerprint}, "lock screen fingerprint fprintd biometric"
      );
      e.visibleWhen = lockscreenOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.allow-empty-password",
          {"lockscreen", "allow_empty_password"}, ToggleSetting{cfg.lockscreen.allowEmptyPassword},
          "lock screen empty password security key pam"
      );
      e.visibleWhen = lockscreenOn;
      entries.push_back(std::move(e));
    }
    if (env.screencopySupported) {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.blurred-desktop", {"lockscreen", "blurred_desktop"},
          ToggleSetting{cfg.lockscreen.blurredDesktop}, "lock screen desktop capture screencopy background"
      );
      e.visibleWhen = lockscreenOn;
      entries.push_back(std::move(e));

      MultiSelectSetting transitions;
      transitions.options.reserve(std::size(kLockscreenTransitions));
      for (const auto& opt : kLockscreenTransitions) {
        transitions.options.push_back(SelectOption{std::string(opt.key), tr(opt.labelKey)});
      }
      transitions.selectedValues.reserve(cfg.lockscreen.transitions.size());
      for (const auto& transition : cfg.lockscreen.transitions) {
        transitions.selectedValues.emplace_back(enumToKey(kLockscreenTransitions, transition));
      }
      auto transitionEffects = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.transitions", {"lockscreen", "transition"},
          std::move(transitions), "lock screen effects animation pool"
      );
      transitionEffects.visibleWhen = lockscreenOn;
      entries.push_back(std::move(transitionEffects));

      auto transitionDuration = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.transition-duration",
          {"lockscreen", "transition_duration"},
          sliderFor(
              cfg.lockscreen.transitionDurationMs, noctalia::config::schema::kLockscreenTransitionDurationRange, true
          ),
          "lock screen transition animation duration"
      );
      transitionDuration.visibleWhen = lockscreenOn;
      entries.push_back(std::move(transitionDuration));

      auto edgeSmoothness = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.edge-smoothness", {"lockscreen", "edge_smoothness"},
          sliderFor(cfg.lockscreen.edgeSmoothness, noctalia::config::schema::kUnitRange, false),
          "lock screen transition feathering", true
      );
      edgeSmoothness.visibleWhen = lockscreenOn;
      entries.push_back(std::move(edgeSmoothness));
    }
    {
      const SettingVisibility lockscreenWallpaperOn = [](const Config& c) {
        return c.lockscreen.enabled && !c.lockscreen.blurredDesktop;
      };
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.wallpaper", {"lockscreen", "wallpaper"},
          TextSetting{
              .value = cfg.lockscreen.wallpaper,
              .placeholder = tr("settings.schema.lockscreen.wallpaper.placeholder"),
              .browseMode = TextSettingBrowseMode::OpenFile,
              .browseFileExtensions = DirectoryScanner::imageExtensionFilter(true),
              .browseFallbackDirectory = wallpaper::resolveGlobalWallpaperDirectory(
                  cfg.wallpaper, wallpaper::effectiveThemeMode(cfg.theme.mode, cfg.theme.mode == ThemeMode::Light)
              ),
          },
          "lock screen background image custom"
      );
      e.visibleWhen = lockscreenWallpaperOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.blur-intensity", {"lockscreen", "blur_intensity"},
          sliderFor(cfg.lockscreen.blurIntensity, noctalia::config::schema::kUnitRange, false), "lock screen blur"
      );
      e.visibleWhen = lockscreenOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.tint-intensity", {"lockscreen", "tint_intensity"},
          sliderFor(cfg.lockscreen.tintIntensity, noctalia::config::schema::kUnitRange, false), "lock screen tint"
      );
      e.visibleWhen = lockscreenOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.monitors", {"lockscreen", "monitors"},
          ListSetting{.items = cfg.lockscreen.monitors, .suggestedOptions = env.availableOutputs},
          "lock screen monitor output connector"
      );
      e.visibleWhen = lockscreenOn;
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Security, "lock-screen", "lockscreen.widgets", {"lockscreen_widgets", "enabled"},
          ToggleSetting{cfg.lockscreenWidgets.enabled}, "lock screen widgets layout"
      );
      e.visibleWhen = lockscreenOn;
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Security, "privacy", "shell.privacy-mic-filter-regex",
        {"shell", "privacy", "mic_filter_regex"},
        TextSetting{.value = cfg.shell.privacy.micFilterRegex, .placeholder = "", .browseFileExtensions = {}},
        "privacy microphone mic app process regex filter ignore"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Security, "privacy", "shell.privacy-cam-filter-regex",
        {"shell", "privacy", "cam_filter_regex"},
        TextSetting{.value = cfg.shell.privacy.camFilterRegex, .placeholder = "", .browseFileExtensions = {}},
        "privacy camera webcam app process regex filter ignore"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Security, "privacy", "shell.privacy-screen-filter-regex",
        {"shell", "privacy", "screen_filter_regex"},
        TextSetting{.value = cfg.shell.privacy.screenFilterRegex, .placeholder = "", .browseFileExtensions = {}},
        "privacy screen share screenshare app process regex filter ignore"
    ));
    if (env.greeterSyncAvailable) {
      entries.push_back(keyedEntry(
          SettingsSection::Security, "greeter", "shell.greeter-sync-privilege-command",
          {"shell", "greeter_sync", "privilege_command"},
          TextSetting{
              .value = cfg.shell.greeterSync.privilegeCommand,
              .placeholder = "pkexec",
              .browseFileExtensions = {},
          },
          "greeter sync pkexec run0 ghostty terminal sudo polkit wrapper"
      ));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-capture", "shell.screenshot-freeze-screen",
        {"shell", "screenshot", "freeze_screen"}, ToggleSetting{cfg.shell.screenshot.freezeScreen},
        "screenshot capture freeze region selection"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-capture", "shell.screenshot-confirm-region",
        {"shell", "screenshot", "confirm_region"}, ToggleSetting{cfg.shell.screenshot.confirmRegion},
        "screenshot capture confirm region selection"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-capture", "shell.screenshot-remember-last-region",
        {"shell", "screenshot", "remember_last_region"}, ToggleSetting{cfg.shell.screenshot.rememberLastRegion},
        "screenshot capture remember last region selection"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-capture", "shell.screenshot-region-modifier",
        {"shell", "screenshot", "region_modifier"},
        enumSelect(kScreenshotRegionModifiers, cfg.shell.screenshot.regionModifier),
        "screenshot region modifier move resize drag selection captures"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-capture", "shell.screenshot-show-cursor",
        {"shell", "screenshot", "show_cursor"}, ToggleSetting{cfg.shell.screenshot.showCursor},
        "screenshot capture show cursor pointer mouse"
    ));

    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-annotation", "shell.screenshot-annotate",
        {"shell", "screenshot", "annotate"}, ToggleSetting{cfg.shell.screenshot.annotate},
        "screenshot annotate annotation draw edit markup"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-annotation", "shell.screenshot-skip-annotate-on-copy-save",
        {"shell", "screenshot", "skip_annotate_on_copy_save"},
        ToggleSetting{cfg.shell.screenshot.skipAnnotateOnCopySave},
        "screenshot annotate annotation skip copy save region quick"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-annotation", "shell.screenshot-close-on-copy",
        {"shell", "screenshot", "close_on_copy"}, ToggleSetting{cfg.shell.screenshot.closeOnCopy},
        "screenshot annotation close copy clipboard exit"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-annotation", "shell.screenshot-close-on-save",
        {"shell", "screenshot", "close_on_save"}, ToggleSetting{cfg.shell.screenshot.closeOnSave},
        "screenshot annotation close save exit"
    ));

    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-output", "shell.screenshot-ask-action",
        {"shell", "screenshot", "ask_action"}, ToggleSetting{cfg.shell.screenshot.askAction},
        "screenshot capture ask prompt copy save both file name"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-output", "shell.screenshot-copy-to-clipboard",
        {"shell", "screenshot", "copy_to_clipboard"}, ToggleSetting{cfg.shell.screenshot.copyToClipboard},
        "screenshot capture clipboard copy"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-output", "shell.screenshot-save-to-file",
        {"shell", "screenshot", "save_to_file"}, ToggleSetting{cfg.shell.screenshot.saveToFile},
        "screenshot capture save png file"
    ));
    {
      auto e = keyedEntry(
          SettingsSection::Screenshot, "screenshot-output", "shell.screenshot-directory",
          {"shell", "screenshot", "directory"},
          TextSetting{
              .value = cfg.shell.screenshot.directory,
              .placeholder = tr("settings.schema.shell.screenshot-directory.placeholder"),
              .browseMode = TextSettingBrowseMode::SelectFolder,
              .browseFileExtensions = {}
          },
          "screenshot capture directory folder save location"
      );
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Screenshot, "screenshot-output", "shell.screenshot-filename-pattern",
          {"shell", "screenshot", "filename_pattern"},
          TextSetting{
              .value = cfg.shell.screenshot.filenamePattern,
              .placeholder = "%B-%d-%Y-%I-%M-%S-%p",
              .browseFileExtensions = {}
          },
          "screenshot capture filename pattern strftime", true
      );
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Screenshot, "screenshot-output", "shell.screenshot-pipe-to-command",
        {"shell", "screenshot", "pipe_to_command"}, ToggleSetting{cfg.shell.screenshot.pipeToCommand},
        "screenshot capture run pipe command stdin"
    ));
    {
      auto e = keyedEntry(
          SettingsSection::Screenshot, "screenshot-output", "shell.screenshot-pipe-command",
          {"shell", "screenshot", "pipe_command"},
          TextSetting{
              .value = cfg.shell.screenshot.pipeCommand,
              .placeholder = tr("settings.schema.shell.screenshot-pipe-command.placeholder"),
              .width = 320.0F,
              .browseFileExtensions = {}
          },
          "screenshot capture run pipe command stdin png"
      );
      e.visibleWhen = [](const Config& c) { return c.shell.screenshot.pipeToCommand; };
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-enabled", {"osd", "enabled"}, ToggleSetting{cfg.osd.enabled},
        "hud overlay master enable disable all"
    ));
    const std::size_t osdGatedStart = entries.size();
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-orientation", {"osd", "orientation"},
        asSegmented(plainSelect(
            {{"horizontal", "settings.options.orientation.horizontal"},
             {"vertical", "settings.options.orientation.vertical"}},
            cfg.osd.orientation
        )),
        "hud overlay volume brightness vertical"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-position", {"osd", "position"},
        plainSelect(
            {{"top_right", "settings.options.screen-position.top-right"},
             {"top_left", "settings.options.screen-position.top-left"},
             {"top_center", "settings.options.screen-position.top-center"},
             {"bottom_right", "settings.options.screen-position.bottom-right"},
             {"bottom_left", "settings.options.screen-position.bottom-left"},
             {"bottom_center", "settings.options.screen-position.bottom-center"},
             {"center_right", "settings.options.screen-position.center-right"},
             {"center_left", "settings.options.screen-position.center-left"}},
            cfg.osd.position
        ),
        "hud overlay volume brightness horizontal text"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-position-vertical", {"osd", "position_vertical"},
        plainSelect(
            {{"top_right", "settings.options.screen-position.top-right"},
             {"top_left", "settings.options.screen-position.top-left"},
             {"top_center", "settings.options.screen-position.top-center"},
             {"bottom_right", "settings.options.screen-position.bottom-right"},
             {"bottom_left", "settings.options.screen-position.bottom-left"},
             {"bottom_center", "settings.options.screen-position.bottom-center"},
             {"center_right", "settings.options.screen-position.center-right"},
             {"center_left", "settings.options.screen-position.center-left"}},
            cfg.osd.positionVertical
        ),
        "hud overlay volume brightness vertical slider"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-hide-delay", {"osd", "hide_delay_ms"},
        StepperSetting{
            .value = static_cast<int>(cfg.osd.hideDelayMs),
            .minValue = static_cast<int>(noctalia::config::schema::kOsdHideDelayMsRange.min.value()),
            .maxValue = static_cast<int>(noctalia::config::schema::kOsdHideDelayMsRange.max.value()),
            .step = static_cast<int>(noctalia::config::schema::kOsdHideDelayMsRange.step.value()),
            .valueSuffix = "ms",
        },
        "hud overlay popup timeout duration visible"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-scale", {"osd", "scale"},
        sliderFor(cfg.osd.scale, noctalia::config::schema::kScaleRange, false),
        "hud overlay volume brightness size scale multiplier"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-offset-x", {"osd", "offset_x"},
        StepperSetting{.value = cfg.osd.offsetX, .minValue = 0, .maxValue = 200, .step = 1, .valueSuffix = "px"},
        "hud overlay horizontal margin"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-offset-y", {"osd", "offset_y"},
        StepperSetting{.value = cfg.osd.offsetY, .minValue = 0, .maxValue = 200, .step = 1, .valueSuffix = "px"},
        "hud overlay vertical margin"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-background-opacity", {"osd", "background_opacity"},
        sliderFor(cfg.osd.backgroundOpacity, noctalia::config::schema::kUnitRange, false),
        "hud overlay popup alpha transparency translucent opacity"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-border", {"osd", "border"}, ToggleSetting{cfg.osd.border},
        "outline border"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-border-color", {"osd", "border_color"},
        colorSpecPicker(cfg.osd.borderColor), "outline border color theme accent"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-border-width", {"osd", "border_width"},
        sliderFor(cfg.osd.borderWidth, noctalia::config::schema::kOsdBorderWidthRange, false),
        "outline border width thickness"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "osd", "shell.osd-follow-focused-output", {"osd", "follow_focused_output"},
        ToggleSetting{cfg.osd.followFocusedOutput}, "monitor output display focused active hud overlay"
    ));
    {
      auto e = keyedEntry(
          SettingsSection::Osd, "osd", "shell.osd-monitors", {"osd", "monitors"},
          ListSetting{.items = cfg.osd.monitors, .suggestedOptions = env.availableOutputs},
          "monitor output display screen hud overlay"
      );
      e.visibleWhen = [](const Config& c) { return !c.osd.followFocusedOutput; };
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-volume", {"osd", "kinds", "volume"},
        ToggleSetting{cfg.osd.kinds.volume}, "hud overlay audio output input microphone"
    ));
    {
      const SettingVisibility volumeOn = [](const Config& c) { return c.osd.kinds.volume; };
      SettingEntry outputEntry = keyedEntry(
          SettingsSection::Osd, "kinds", "shell.osd-kinds-volume-output", {"osd", "kinds", "volume_output"},
          ToggleSetting{cfg.osd.kinds.volumeOutput}, "hud overlay audio speaker sink output"
      );
      outputEntry.advanced = true;
      outputEntry.visibleWhen = volumeOn;
      entries.push_back(std::move(outputEntry));
      SettingEntry inputEntry = keyedEntry(
          SettingsSection::Osd, "kinds", "shell.osd-kinds-volume-input", {"osd", "kinds", "volume_input"},
          ToggleSetting{cfg.osd.kinds.volumeInput}, "hud overlay audio microphone source input"
      );
      inputEntry.advanced = true;
      inputEntry.visibleWhen = volumeOn;
      entries.push_back(std::move(inputEntry));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-brightness", {"osd", "kinds", "brightness"},
        ToggleSetting{cfg.osd.kinds.brightness}, "hud overlay display backlight"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-wifi", {"osd", "kinds", "wifi"},
        ToggleSetting{cfg.osd.kinds.wifi}, "hud overlay wireless network"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-bluetooth", {"osd", "kinds", "bluetooth"},
        ToggleSetting{cfg.osd.kinds.bluetooth}, "hud overlay bt"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-power-profile", {"osd", "kinds", "power_profile"},
        ToggleSetting{cfg.osd.kinds.powerProfile}, "hud overlay balanced performance power saver"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-caffeine", {"osd", "kinds", "caffeine"},
        ToggleSetting{cfg.osd.kinds.caffeine}, "hud overlay idle inhibitor"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-nightlight", {"osd", "kinds", "nightlight"},
        ToggleSetting{cfg.osd.kinds.nightlight}, "hud overlay night light gamma"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-dnd", {"osd", "kinds", "dnd"}, ToggleSetting{cfg.osd.kinds.dnd},
        "hud overlay do not disturb notifications"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-lock-keys", {"osd", "kinds", "lock_keys"},
        ToggleSetting{cfg.osd.kinds.lockKeys}, "hud overlay caps num scroll keyboard"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-keyboard-layout", {"osd", "kinds", "keyboard_layout"},
        ToggleSetting{cfg.osd.kinds.keyboardLayout}, "hud overlay xkb input language layout switch"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-media", {"osd", "kinds", "media"},
        ToggleSetting{cfg.osd.kinds.media}, "hud overlay mpris audio music"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-privacy", {"osd", "kinds", "privacy"},
        ToggleSetting{cfg.osd.kinds.privacy}, "hud overlay microphone camera screen share recording"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Osd, "kinds", "shell.osd-kinds-keyboard-backlight", {"osd", "kinds", "keyboard_backlight"},
        ToggleSetting{cfg.osd.kinds.keyboardBacklight}, "hud overlay keyboard backlight kbd"
    ));
    // Gate every OSD entry after the master toggle on osd.enabled, preserving any per-entry visibility.
    for (std::size_t i = osdGatedStart; i < entries.size(); ++i) {
      SettingVisibility prev = std::move(entries[i].visibleWhen);
      entries[i].visibleWhen = prev
          ? SettingVisibility{[prev = std::move(prev)](const Config& c) { return c.osd.enabled && prev(c); }}
          : SettingVisibility{[](const Config& c) { return c.osd.enabled; }};
    }

    // System
    if (env.batteryAvailable) {
      if (env.systemBatteryAvailable) {
        entries.push_back(keyedEntry(
            SettingsSection::System, "battery", "system.battery-warning-threshold", {"battery", "warning_threshold"},
            sliderFor(cfg.battery.warningThreshold, noctalia::config::schema::kBatteryWarningThresholdRange, true),
            "battery low warning threshold notification"
        ));
      }
      for (const auto& device : env.batteryDeviceOptions) {
        int value = 0;
        if (const auto it = env.batteryWarningThresholds.find(device.value); it != env.batteryWarningThresholds.end()) {
          value = it->second;
        }
        entries.push_back(makeEntry(
            SettingsSection::System, "battery",
            tr("settings.schema.system.battery-device-warning-threshold.label", "device", device.label),
            tr("settings.schema.system.battery-device-warning-threshold.description"),
            {"battery", "device", device.value, "warning_threshold"},
            SliderSetting{std::clamp(value, 0, 100), 0.0F, 100.0F, 1.0F, true},
            std::string("battery device low warning threshold notification ") + device.label + " " + device.value
        ));
      }
    }
    entries.push_back(keyedEntry(
        SettingsSection::System, "screen-time", "shell.screen-time-enabled", {"shell", "screen_time_enabled"},
        ToggleSetting{cfg.shell.screenTimeEnabled}, "screen time usage tracking control center"
    ));
    const SettingVisibility monitorOn = [](const Config& c) { return c.system.monitor.enabled; };
    entries.push_back(keyedEntry(
        SettingsSection::System, "monitor", "services.system-monitor", {"system", "monitor", "enabled"},
        ToggleSetting{cfg.system.monitor.enabled}, "system monitor cpu ram memory"
    ));
    {
      // The slider goes down to 0, which disables the metric (no polling, no dGPU wakeups).
      constexpr float kPollMin = SystemConfig::MonitorConfig::kDisabledPollSeconds;
      constexpr float kPollMax = SystemConfig::MonitorConfig::kMaxPollSeconds;
      constexpr float kPollStep = 1.0F;
      const auto& mon = cfg.system.monitor;
      auto addPoll = [&](std::string_view labelKey, std::string_view descKey, std::vector<std::string> path,
                         float value) {
        const float clampedValue = std::clamp(value, kPollMin, kPollMax);
        SliderSetting slider{clampedValue, kPollMin, kPollMax, kPollStep, true};
        slider.valueSuffix = "s";
        auto entry = makeEntry(
            SettingsSection::System, "monitor-polling", tr(labelKey), tr(descKey), std::move(path), std::move(slider),
            "system monitor", true
        );
        entry.visibleWhen = monitorOn;
        entries.push_back(std::move(entry));
      };
      addPoll(
          "settings.schema.services.system-monitor.cpu-poll.label",
          "settings.schema.services.system-monitor.cpu-poll.description", {"system", "monitor", "cpu_poll_seconds"},
          mon.cpuPollSeconds
      );
      addPoll(
          "settings.schema.services.system-monitor.gpu-poll.label",
          "settings.schema.services.system-monitor.gpu-poll.description", {"system", "monitor", "gpu_poll_seconds"},
          mon.gpuPollSeconds
      );
      addPoll(
          "settings.schema.services.system-monitor.memory-poll.label",
          "settings.schema.services.system-monitor.memory-poll.description",
          {"system", "monitor", "memory_poll_seconds"}, mon.memoryPollSeconds
      );
      addPoll(
          "settings.schema.services.system-monitor.network-poll.label",
          "settings.schema.services.system-monitor.network-poll.description",
          {"system", "monitor", "network_poll_seconds"}, mon.networkPollSeconds
      );
      addPoll(
          "settings.schema.services.system-monitor.disk-poll.label",
          "settings.schema.services.system-monitor.disk-poll.description", {"system", "monitor", "disk_poll_seconds"},
          mon.diskPollSeconds
      );

      // One dual-thumb range row per metric: low thumb = activity threshold, high thumb = critical.
      auto addThresholdPair = [&](std::string_view baseKey, std::string_view statLabelKey, double activityValue,
                                  double criticalValue, noctalia::sysmon::ThresholdProfile profile, bool integerValue,
                                  std::string valueSuffix) {
        const std::vector<std::string> activityPath = {
            "system", "monitor", std::string(baseKey) + "_activity_threshold"
        };
        const std::vector<std::string> criticalPath = {
            "system", "monitor", std::string(baseKey) + "_critical_threshold"
        };
        const std::string statLabel = tr(statLabelKey);

        RangeSliderSetting range;
        range.lowValue = activityValue;
        range.highValue = criticalValue;
        range.minValue = profile.minValue;
        range.maxValue = profile.maxValue;
        range.step = profile.step;
        range.integerValue = integerValue;
        range.valueSuffix = std::move(valueSuffix);
        range.highPath = criticalPath;

        auto entry = makeEntry(
            SettingsSection::System, "monitor-thresholds",
            tr("settings.schema.services.system-monitor.threshold.label", "stat", statLabel),
            tr("settings.schema.services.system-monitor.threshold.description"), activityPath, std::move(range),
            "system monitor threshold activity critical", true
        );
        entry.visibleWhen = monitorOn;
        entries.push_back(std::move(entry));
      };

      using noctalia::sysmon::Stat;
      addThresholdPair(
          "cpu_usage", "settings.schema.services.system-monitor.stats.cpu-usage", mon.cpuUsageActivityThreshold,
          mon.cpuUsageCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::CpuUsage), true, "%"
      );
      addThresholdPair(
          "cpu_temp", "settings.schema.services.system-monitor.stats.cpu-temp", mon.cpuTempActivityThreshold,
          mon.cpuTempCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::CpuTemp), true, "°C"
      );
      addThresholdPair(
          "cpu_freq", "settings.schema.services.system-monitor.stats.cpu-freq", mon.cpuFreqActivityThreshold,
          mon.cpuFreqCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::CpuFreq), false, "GHz"
      );
      addThresholdPair(
          "gpu_usage", "settings.schema.services.system-monitor.stats.gpu-usage", mon.gpuUsageActivityThreshold,
          mon.gpuUsageCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::GpuUsage), true, "%"
      );
      addThresholdPair(
          "gpu_temp", "settings.schema.services.system-monitor.stats.gpu-temp", mon.gpuTempActivityThreshold,
          mon.gpuTempCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::GpuTemp), true, "°C"
      );
      addThresholdPair(
          "gpu_vram", "settings.schema.services.system-monitor.stats.gpu-vram", mon.gpuVramActivityThreshold,
          mon.gpuVramCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::GpuVram), true, "%"
      );
      addThresholdPair(
          "ram_pct", "settings.schema.services.system-monitor.stats.ram-usage", mon.ramPctActivityThreshold,
          mon.ramPctCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::RamPct), true, "%"
      );
      addThresholdPair(
          "swap_pct", "settings.schema.services.system-monitor.stats.swap-usage", mon.swapPctActivityThreshold,
          mon.swapPctCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::SwapPct), true, "%"
      );
      addThresholdPair(
          "disk_used_pct", "settings.schema.services.system-monitor.stats.disk-used-pct",
          mon.diskUsedPctActivityThreshold, mon.diskUsedPctCriticalThreshold,
          noctalia::sysmon::thresholdProfile(Stat::DiskUsedPct), true, "%"
      );
      addThresholdPair(
          "disk_used", "settings.schema.services.system-monitor.stats.disk-used", mon.diskUsedActivityThreshold,
          mon.diskUsedCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::DiskUsed), true, "%"
      );
      addThresholdPair(
          "disk_free_pct", "settings.schema.services.system-monitor.stats.disk-free-pct",
          mon.diskFreePctActivityThreshold, mon.diskFreePctCriticalThreshold,
          noctalia::sysmon::thresholdProfile(Stat::DiskFreePct), true, "%"
      );
      addThresholdPair(
          "disk_free", "settings.schema.services.system-monitor.stats.disk-free", mon.diskFreeActivityThreshold,
          mon.diskFreeCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::DiskFree), true, "%"
      );
      addThresholdPair(
          "net_rx", "settings.schema.services.system-monitor.stats.network-rx", mon.netRxActivityThreshold,
          mon.netRxCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::NetRx), false, "MB/s"
      );
      addThresholdPair(
          "net_tx", "settings.schema.services.system-monitor.stats.network-tx", mon.netTxActivityThreshold,
          mon.netTxCriticalThreshold, noctalia::sysmon::thresholdProfile(Stat::NetTx), false, "MB/s"
      );
    }

    // Location: single source of "where am I"; shared by weather, night light, and theme auto mode.
    entries.push_back(keyedEntry(
        SettingsSection::Location, "location", "services.location-auto-locate", {"location", "auto_locate"},
        ToggleSetting{cfg.location.autoLocate}, "location ip geolocate gps coordinate"
    ));
    const SettingVisibility autoLocateOff = [](const Config& c) { return !c.location.autoLocate; };
    entries.push_back(keyedEntry(
                          SettingsSection::Location, "location", "services.location-address", {"location", "address"},
                          TextSetting{
                              .value = cfg.location.address,
                              .placeholder = tr("settings.schema.services.location-address.placeholder"),
                              .browseFileExtensions = {}
                          },
                          "location address city geocode"
    )
                          .shownWhen(autoLocateOff));
    // Manual coordinates: shown only when no network location is configured (auto-locate off
    // and no address). The address gate is build-time; the auto-locate gate is evaluated live.
    const SettingVisibility manualLocationHidden = [](const Config&) { return false; };
    const SettingVisibility& manualLocationControlsVisible =
        cfg.location.address.empty() ? autoLocateOff : manualLocationHidden;
    entries.push_back(keyedEntry(
                          SettingsSection::Location, "location", "services.latitude", {"location", "latitude"},
                          OptionalNumberSetting{cfg.location.latitude, -90.0, 90.0, "52.5200"},
                          "coordinate location sunrise sunset", true
    )
                          .shownWhen(manualLocationControlsVisible));
    entries.push_back(keyedEntry(
                          SettingsSection::Location, "location", "services.longitude", {"location", "longitude"},
                          OptionalNumberSetting{cfg.location.longitude, -180.0, 180.0, "13.4050"},
                          "coordinate location sunrise sunset", true
    )
                          .shownWhen(manualLocationControlsVisible));

    // Custom scheduling: explicit sunrise/sunset times for night light and theme auto mode.
    {
      auto e = keyedEntry(
          SettingsSection::Location, "location", "services.custom-schedule", {"location", "custom_schedule"},
          ToggleSetting{cfg.location.customSchedule}, "schedule custom time sunrise sunset"
      );
      entries.push_back(std::move(e));
    }
    // Keep both required times editable before activation so the schedule can be valid when enabled.
    {
      auto e = keyedEntry(
          SettingsSection::Location, "location", "services.sunset", {"location", "sunset"},
          TextSetting{.value = cfg.location.sunset, .placeholder = "20:30", .browseFileExtensions = {}},
          "time schedule sunset"
      );
      entries.push_back(std::move(e));
    }
    {
      auto e = keyedEntry(
          SettingsSection::Location, "location", "services.sunrise", {"location", "sunrise"},
          TextSetting{.value = cfg.location.sunrise, .placeholder = "07:30", .browseFileExtensions = {}},
          "time schedule sunrise"
      );
      entries.push_back(std::move(e));
    }

    // Weather: consumes the resolved location.
    entries.push_back(keyedEntry(
        SettingsSection::Location, "weather", "services.weather", {"weather", "enabled"},
        ToggleSetting{cfg.weather.enabled}, "forecast"
    ));
    const SettingVisibility weatherOn = [](const Config& c) { return c.weather.enabled; };
    {
      auto e = keyedEntry(
          SettingsSection::Location, "weather", "shell.show-location", {"shell", "show_location"},
          ToggleSetting{cfg.shell.showLocation}, "weather"
      );
      e.visibleWhen = weatherOn;
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
                          SettingsSection::Location, "weather", "services.weather-unit", {"weather", "unit"},
                          asSegmented(plainSelect(
                              {{"metric", "settings.options.weather.unit.metric"},
                               {"imperial", "settings.options.weather.unit.imperial"}},
                              cfg.weather.unit
                          )),
                          "temperature"
    )
                          .shownWhen(weatherOn));
    entries.push_back(keyedEntry(
                          SettingsSection::Location, "weather", "services.weather-effects", {"weather", "effects"},
                          ToggleSetting{cfg.weather.effects}, "forecast visuals"
    )
                          .shownWhen(weatherOn));
    entries.push_back(
        keyedEntry(
            SettingsSection::Location, "weather", "services.weather-refresh-interval", {"weather", "refresh_minutes"},
            sliderFor(cfg.weather.refreshMinutes, noctalia::config::schema::kRefreshMinutesRange, true), "forecast"
        )
            .shownWhen(weatherOn)
    );

    const SettingVisibility calendarOn = [](const Config& c) { return c.calendar.enabled; };
    entries.push_back(keyedEntry(
        SettingsSection::Calendar, "general", "services.calendar", {"calendar", "enabled"},
        ToggleSetting{cfg.calendar.enabled}, "calendar events caldav google"
    ));
    entries.push_back(
        keyedEntry(
            SettingsSection::Calendar, "general", "services.calendar-event-date-format",
            {"calendar", "event_date_format"},
            TextSetting{.value = cfg.calendar.eventDateFormat, .placeholder = "%A %e %B", .browseFileExtensions = {}},
            "calendar date format strftime chrono"
        )
            .shownWhen(calendarOn)
    );
    entries.push_back(
        keyedEntry(
            SettingsSection::Calendar, "general", "services.calendar-event-time-format",
            {"calendar", "event_time_format"},
            TextSetting{.value = cfg.calendar.eventTimeFormat, .placeholder = "%H:%M", .browseFileExtensions = {}},
            "calendar time format strftime chrono"
        )
            .shownWhen(calendarOn)
    );
    entries.push_back(keyedEntry(
                          SettingsSection::Calendar, "general", "services.calendar-dedupe-events",
                          {"calendar", "dedupe_events"}, ToggleSetting{cfg.calendar.dedupeEvents},
                          "calendar events duplicate merge"
    )
                          .shownWhen(calendarOn));
    {
      auto e = keyedEntry(
          SettingsSection::Calendar, "general", "services.calendar-dedupe-ignore-patterns",
          {"calendar", "dedupe_ignore_patterns"}, ListSetting{.items = cfg.calendar.dedupeIgnorePatterns},
          "calendar events duplicate merge regex title"
      );
      e.visibleWhen = [](const Config& c) { return c.calendar.enabled && c.calendar.dedupeEvents; };
      entries.push_back(std::move(e));
    }
    // Week numbers are a grid decoration, so they stay available when event syncing is off.
    entries.push_back(keyedEntry(
        SettingsSection::Calendar, "general", "services.calendar-week-numbers",
        {"control_center", "calendar", "show_week_numbers"},
        ToggleSetting{cfg.controlCenter.calendarTab.showWeekNumbers}, "calendar week numbers iso"
    ));
    {
      auto e = keyedEntry(
          SettingsSection::Calendar, "calendar-reminders", "services.calendar-reminders",
          {"calendar", "reminders", "enabled"}, ToggleSetting{cfg.calendar.reminders.enabled},
          "calendar reminder notification alarm upcoming"
      );
      e.visibleWhen = calendarOn;
      entries.push_back(std::move(e));
    }
    const SettingVisibility remindersOn = [](const Config& c) {
      return c.calendar.enabled && c.calendar.reminders.enabled;
    };
    entries.push_back(keyedEntry(
                          SettingsSection::Calendar, "calendar-reminders", "services.calendar-reminder-use-event",
                          {"calendar", "reminders", "use_event_reminders"},
                          ToggleSetting{cfg.calendar.reminders.useEventReminders},
                          "calendar reminder valarm override event"
    )
                          .shownWhen(remindersOn));
    entries.push_back(keyedEntry(
                          SettingsSection::Calendar, "calendar-reminders", "services.calendar-reminder-lead",
                          {"calendar", "reminders", "default_lead_minutes"},
                          sliderFor(
                              cfg.calendar.reminders.defaultLeadMinutes,
                              noctalia::config::schema::kReminderLeadMinutesRange, true
                          ),
                          "calendar reminder lead minutes before default"
    )
                          .shownWhen(remindersOn));
    entries.push_back(
        keyedEntry(
            SettingsSection::Calendar, "calendar-reminders", "services.calendar-digest-time",
            {"calendar", "reminders", "all_day_digest_time"},
            TextSetting{
                .value = cfg.calendar.reminders.allDayDigestTime, .placeholder = "09:00", .browseFileExtensions = {}
            },
            "calendar all day digest time morning"
        )
            .shownWhen(remindersOn)
    );
    // Sync cadence belongs with the accounts it drives; the account rows are injected right after it.
    entries.push_back(keyedEntry(
                          SettingsSection::Calendar, "calendar-accounts", "services.calendar-refresh-interval",
                          {"calendar", "refresh_minutes"},
                          sliderFor(cfg.calendar.refreshMinutes, noctalia::config::schema::kRefreshMinutesRange, true),
                          "calendar sync"
    )
                          .shownWhen(calendarOn));

    entries.push_back(keyedEntry(
        SettingsSection::Services, "audio", "services.audio-overdrive", {"audio", "enable_overdrive"},
        ToggleSetting{cfg.audio.enableOverdrive}, "volume"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Services, "audio", "services.shell-sounds", {"audio", "enable_sounds"},
        ToggleSetting{cfg.audio.enableSounds}, "sound"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Services, "audio", "services.volume-sounds", {"audio", "enable_volume_sounds"},
        ToggleSetting{cfg.audio.enableVolumeSounds}, "sound"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Services, "audio", "services.notification-sounds", {"audio", "enable_notification_sounds"},
        ToggleSetting{cfg.audio.enableNotificationSounds}, "sound"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Services, "audio", "services.power-sounds", {"audio", "enable_power_sounds"},
        ToggleSetting{cfg.audio.enablePowerSounds}, "sound"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Services, "audio", "services.screenshot-sounds", {"audio", "enable_screenshot_sounds"},
        ToggleSetting{cfg.audio.enableScreenshotSounds}, "sound"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Services, "audio", "services.sound-volume", {"audio", "sound_volume"},
        sliderFor(cfg.audio.soundVolume, noctalia::config::schema::kUnitRange, false), "sound"
    ));
    std::vector<SelectOption> soundThemeOptions;
    for (const auto& [value, label] : SoundPlayer::availableThemes()) {
      soundThemeOptions.push_back(SelectOption{value, label});
    }
    if (soundThemeOptions.empty()) {
      soundThemeOptions.push_back(SelectOption{"freedesktop", tr("settings.schema.services.sound-theme.default")});
    }
    entries.push_back(keyedEntry(
        SettingsSection::Services, "audio", "services.sound-theme", {"audio", "sound_theme"},
        SelectSetting{std::move(soundThemeOptions), cfg.audio.soundTheme}, "sound theme"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Services, "media", "services.mpris-blacklist", {"shell", "mpris", "blacklist"},
        ListSetting{.items = cfg.shell.mpris.blacklist}, "mpris media player dbus session blacklist"
    ));

    // Power
    entries.push_back(keyedEntry(
        SettingsSection::Power, "session-panel", "panels.placement-session", {"shell", "panel", "session_placement"},
        asSegmented(enumSelect(kPanelPlacements, cfg.shell.panel.sessionPlacement)),
        "attached floating bar panel power menu position"
    ));
    entries.push_back(panelPositionEntry(
        SettingsSection::Power, "session-panel", "session", "settings.schema.panels.position-session.label",
        "settings.schema.panels.position-session.description", cfg.shell.panel.sessionPosition,
        &ShellConfig::PanelConfig::sessionPlacement
    ));
    entries.push_back(panelBarAlignmentEntry(
        SettingsSection::Power, "session-panel", "session", "settings.schema.panels.open-near-click-session.label",
        "settings.schema.panels.open-near-click-session.description", cfg.shell.panel.openNearClickSession,
        &ShellConfig::PanelConfig::sessionPlacement, &ShellConfig::PanelConfig::sessionPosition
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Power, "session-panel", "power.session-grid", {"shell", "session", "grid"},
        ToggleSetting{.checked = cfg.shell.session.grid}, "session panel grid layout rows columns"
    ));
    {
      auto e = keyedEntry(
          SettingsSection::Power, "session-panel", "power.session-grid-columns", {"shell", "session", "grid_columns"},
          StepperSetting{
              .value = static_cast<int>(cfg.shell.session.gridColumns),
              .minValue = static_cast<int>(noctalia::config::schema::kSessionGridColumnsRange.min.value()),
              .maxValue = static_cast<int>(noctalia::config::schema::kSessionGridColumnsRange.max.value()),
              .step = static_cast<int>(noctalia::config::schema::kSessionGridColumnsRange.step.value()),
          },
          "session panel grid columns per row"
      );
      e.visibleWhen = [](const Config& c) { return c.shell.session.grid; };
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Power, "session-panel", "power.session-show-shortcuts", {"shell", "session", "show_shortcuts"},
        ToggleSetting{.checked = cfg.shell.session.showShortcuts}, "session panel show shortcuts"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Power, "session-panel", "power.session-actions", {"shell", "session", "actions"},
        SessionPanelActionsSetting{.items = cfg.shell.session.actions},
        "session panel power menu logout reboot shutdown lock command actions order"
    ));

    // Idle
    entries.push_back(keyedEntry(
        SettingsSection::Power, "idle", "idle.pre-action-fade", {"idle", "pre_action_fade_seconds"},
        StepperSetting{
            .value = static_cast<int>(std::lround(std::clamp(cfg.idle.preActionFadeSeconds, 0.0F, 30.0F))),
            .minValue = 0,
            .maxValue = 30,
            .step = 1,
            .valueSuffix = "s"
        },
        "idle fade dim seconds overlay"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Power, "idle", "idle.behaviors", {"idle", "behavior"},
        IdleBehaviorsSetting{.items = cfg.idle.behaviors},
        "idle behavior timeout command resume screen lock dpms suspend lock_and_suspend caffeine"
    ));

    // Hooks
    auto hookGroup = [](HookKind kind) -> std::string {
      switch (kind) {
      case HookKind::Started:
      case HookKind::SessionLocked:
      case HookKind::SessionUnlocked:
      case HookKind::LoggingOut:
      case HookKind::Rebooting:
      case HookKind::ShuttingDown:
        return "lifecycle";
      case HookKind::WallpaperChanged:
      case HookKind::ColorsChanged:
      case HookKind::ThemeModeChanged:
        return "theme";
      case HookKind::WifiEnabled:
      case HookKind::WifiDisabled:
      case HookKind::BluetoothEnabled:
      case HookKind::BluetoothDisabled:
        return "network";
      case HookKind::BatteryCharging:
      case HookKind::BatteryDischarging:
      case HookKind::BatteryPlugged:
      case HookKind::BatteryPercentageChanged:
      case HookKind::PowerProfileChanged:
        return "power";
      case HookKind::Count:
        break;
      }
      return "general";
    };

    auto hookTags = [](HookKind kind) -> std::string {
      std::string tags = "hook command script exec event trigger";
      if (kind == HookKind::BatteryCharging
          || kind == HookKind::BatteryDischarging
          || kind == HookKind::BatteryPlugged
          || kind == HookKind::BatteryPercentageChanged) {
        tags += " battery power";
      }
      if (kind == HookKind::PowerProfileChanged) {
        tags += " power profile performance balanced saver";
      }
      if (kind == HookKind::WallpaperChanged || kind == HookKind::ColorsChanged || kind == HookKind::ThemeModeChanged) {
        tags += " wallpaper colors theme mode light dark auto";
      }
      if (kind == HookKind::WifiEnabled
          || kind == HookKind::WifiDisabled
          || kind == HookKind::BluetoothEnabled
          || kind == HookKind::BluetoothDisabled) {
        tags += " network wifi bluetooth";
      }
      if (kind == HookKind::SessionLocked
          || kind == HookKind::SessionUnlocked
          || kind == HookKind::LoggingOut
          || kind == HookKind::Rebooting
          || kind == HookKind::ShuttingDown
          || kind == HookKind::Started) {
        tags += " session startup";
      }
      return tags;
    };

    for (const auto& kind : kHookKinds) {
      const auto index = static_cast<std::size_t>(kind.value);
      const std::string key(kind.key);
      const std::string baseKey = "settings.schema.hooks.events." + i18n::keySegment(key);
      const std::string hookCmd = cfg.hooks.commands[index].empty() ? "" : cfg.hooks.commands[index][0];
      entries.push_back(makeEntry(
          SettingsSection::Hooks, hookGroup(kind.value), tr(baseKey + ".label"), tr(baseKey + ".description"),
          {"hooks", key},
          TextSetting{
              .value = hookCmd,
              .placeholder = tr("settings.schema.hooks.command-placeholder"),
              .width = 320.0F,
              .browseFileExtensions = {}
          },
          hookTags(kind.value)
      ));
    }

    // Notifications
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "general", "notifications.daemon", {"notification", "enable_daemon"},
        ToggleSetting{cfg.notification.enableDaemon}, "dbus"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "general", "notifications.show-app-name", {"notification", "show_app_name"},
        ToggleSetting{cfg.notification.showAppName}, "application identity header"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "general", "notifications.show-actions", {"notification", "show_actions"},
        ToggleSetting{cfg.notification.showActions}, "action buttons"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "general", "notifications.collapse-on-dismiss",
        {"notification", "collapse_on_dismiss"}, ToggleSetting{cfg.notification.collapseOnDismiss},
        "reorder stack slide"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.layer", {"notification", "layer"},
        asSegmented(plainSelect(
            {{"top", "settings.options.layer.top"}, {"overlay", "settings.options.layer.overlay"}},
            cfg.notification.layer
        )),
        "toast layer shell z-order"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.position", {"notification", "position"},
        plainSelect(
            {{"top_right", "settings.options.screen-position.top-right"},
             {"top_left", "settings.options.screen-position.top-left"},
             {"top_center", "settings.options.screen-position.top-center"},
             {"bottom_right", "settings.options.screen-position.bottom-right"},
             {"bottom_left", "settings.options.screen-position.bottom-left"},
             {"bottom_center", "settings.options.screen-position.bottom-center"}},
            cfg.notification.position
        ),
        "toast popup placement anchor"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.scale", {"notification", "scale"},
        sliderFor(cfg.notification.scale, noctalia::config::schema::kScaleRange, false), "toast size scale"
    ));
    {
      SliderSetting width = sliderFor(cfg.notification.width, noctalia::config::schema::kNotificationWidthRange, true);
      width.valueSuffix = "px";
      entries.push_back(keyedEntry(
          SettingsSection::Notifications, "toasts", "notifications.width", {"notification", "width"}, std::move(width),
          "toast size dimension wide narrow"
      ));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.offset-x", {"notification", "offset_x"},
        StepperSetting{
            .value = cfg.notification.offsetX, .minValue = 0, .maxValue = 200, .step = 1, .valueSuffix = "px"
        },
        "offset margin horizontal"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.offset-y", {"notification", "offset_y"},
        StepperSetting{
            .value = cfg.notification.offsetY, .minValue = 0, .maxValue = 200, .step = 1, .valueSuffix = "px"
        },
        "offset margin vertical"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.max-visible", {"notification", "max_visible"},
        StepperSetting{
            .value = cfg.notification.maxVisible, .minValue = 0, .maxValue = 20, .step = 1, .valueSuffix = ""
        },
        "maximum on-screen toasts count limit"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.toast-opacity", {"notification", "background_opacity"},
        sliderFor(cfg.notification.backgroundOpacity, noctalia::config::schema::kUnitRange, false),
        "popup alpha transparency translucent opacity"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.border", {"notification", "border"},
        ToggleSetting{cfg.notification.border}, "outline border"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.border-color", {"notification", "border_color"},
        colorSpecPicker(cfg.notification.borderColor), "outline border color theme accent"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.border-width", {"notification", "border_width"},
        sliderFor(cfg.notification.borderWidth, noctalia::config::schema::kNotificationToastBorderWidthRange, false),
        "outline border width thickness"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "toasts", "notifications.follow-focused-output",
        {"notification", "follow_focused_output"}, ToggleSetting{cfg.notification.followFocusedOutput},
        "monitor output display focused active"
    ));
    {
      auto e = keyedEntry(
          SettingsSection::Notifications, "toasts", "notifications.monitors", {"notification", "monitors"},
          ListSetting{.items = cfg.notification.monitors, .suggestedOptions = env.availableOutputs},
          "monitor output display screen"
      );
      e.visibleWhen = [](const Config& c) { return !c.notification.followFocusedOutput; };
      entries.push_back(std::move(e));
    }
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "history", "notifications.keep-dismissed-in-history",
        {"notification", "keep_dismissed_in_history"}, ToggleSetting{cfg.notification.keepDismissedInHistory},
        "history dismissed toast remove keep"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "history", "notifications.history-retention-hours",
        {"notification", "history_retention_hours"},
        StepperSetting{
            .value = cfg.notification.historyRetentionHours,
            .minValue = 0,
            .maxValue = 8760,
            .step = 1,
            .valueSuffix = "h",
        },
        "history retention hours clear"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Notifications, "filtering", "notifications.filters", {"notification", "filter"},
        NotificationFiltersSetting{.items = cfg.notification.filters},
        "filter blacklist suppress toast history sound dnd bypass do not disturb app name desktop entry category "
        "urgency"
    ));

    // Bar: register every configured bar so global search can surface settings from all of them.
    for (const auto& bar : cfg.bars) {
      constexpr SettingsSection section = SettingsSection::Bar;
      const std::vector<std::string> root = {"bar", bar.name};
      auto path = [&](std::string key) {
        std::vector<std::string> p = root;
        p.push_back(std::move(key));
        return p;
      };
      entries.push_back(makeEntry(
          section, "general", tr("settings.schema.shared.enabled.label"), tr("settings.schema.bar.enabled.description"),
          path("enabled"), ToggleSetting{bar.enabled}, "visible"
      ));
      entries.push_back(makeEntry(
          section, "general", tr("settings.schema.shared.position.label"),
          tr("settings.schema.bar.position.description"), path("position"), positionSelect(bar.position), "edge"
      ));
      entries.push_back(makeEntry(
          section, "general", tr("settings.schema.shared.auto-hide.label"),
          tr("settings.schema.bar.auto-hide.description"), path("auto_hide"),
          autoHideModeSelect(barAutoHideMode(bar.autoHide, bar.smartAutoHide), path("smart_auto_hide")),
          "autohide smart workspace"
      ));
      {
        auto e = keyedEntry(
            section, "general", "bar.auto-hide-delay", path("auto_hide_delay_ms"),
            StepperSetting{
                .value = bar.autoHideDelayMs,
                .minValue = static_cast<int>(noctalia::config::schema::kBarAutoHideDelayMsRange.min.value()),
                .maxValue = static_cast<int>(noctalia::config::schema::kBarAutoHideDelayMsRange.max.value()),
                .step = static_cast<int>(noctalia::config::schema::kBarAutoHideDelayMsRange.step.value()),
                .valueSuffix = "ms",
            },
            "autohide delay wait"
        );
        e.visibleWhen = [barName = bar.name](const Config& c) {
          const BarConfig* b = findBar(c, barName);
          return b != nullptr && b->autoHide && !b->smartAutoHide;
        };
        entries.push_back(std::move(e));
      }
      const SettingVisibility autoHideOn = [barName = bar.name](const Config& c) {
        const BarConfig* b = findBar(c, barName);
        return b != nullptr && b->isAutoHideEnabled();
      };
      {
        auto e = keyedEntry(
            section, "general", "bar.show-on-workspace-switch", path("show_on_workspace_switch"),
            ToggleSetting{bar.showOnWorkspaceSwitch}, "workspace reveal peek autohide"
        );
        e.visibleWhen = autoHideOn;
        entries.push_back(std::move(e));
      }
      entries.push_back(makeEntry(
          section, "general", tr("settings.schema.shared.reserve-space.label"),
          tr("settings.schema.bar.reserve-space.description"), path("reserve_space"), ToggleSetting{bar.reserveSpace},
          "exclusive zone"
      ));
      entries.push_back(keyedEntry(
          section, "general", "bar.layer", path("layer"),
          asSegmented(plainSelect(
              {{"top", "settings.options.layer.top"}, {"overlay", "settings.options.layer.overlay"}}, bar.layer
          )),
          "layer shell z-order"
      ));
      entries.push_back(keyedEntry(
          section, "layout", "bar.thickness", path("thickness"),
          SliderSetting{bar.thickness, 10.0F, 120.0F, 1.0F, true}, "height width"
      ));
      entries.push_back(keyedEntry(
          section, "layout", "bar.content-scale", path("scale"), SliderSetting{bar.scale, 0.5F, 4.0F, 0.05F, false},
          "zoom size"
      ));
      entries.push_back(keyedEntry(
          section, "layout", "bar.font-scale", path("font_scale"),
          sliderFor(bar.fontScale, noctalia::config::schema::kBarFontScaleRange, false), "text zoom size"
      ));
      entries.push_back(makeEntry(
          section, "layout", tr("settings.schema.shared.ends-margin.label"),
          tr("settings.schema.bar.ends-margin.description"), path("margin_ends"), barMarginStepper(bar.marginEnds),
          "gap inset"
      ));
      entries.push_back(makeEntry(
          section, "layout", tr("settings.schema.shared.edge-margin.label"),
          tr("settings.schema.bar.edge-margin.description"), path("margin_edge"), barMarginStepper(bar.marginEdge),
          "gap inset"
      ));
      entries.push_back(makeEntry(
          section, "layout", tr("settings.schema.shared.opposite-edge-margin.label"),
          tr("settings.schema.bar.opposite-edge-margin.description"), path("margin_opposite_edge"),
          barMarginStepper(bar.marginOppositeEdge), "gap inset strut"
      ));
      entries.push_back(keyedEntry(
          section, "layout", "bar.content-padding", path("padding"),
          SliderSetting{bar.padding, 0.0F, 80.0F, 1.0F, true}, "inset"
      ));
      entries.push_back(makeEntry(
          section, "shape", tr("settings.schema.shared.corner-radius.label"),
          tr("settings.schema.bar.corner-radius.description"), path("radius"),
          barReservedSlider(bar.radius, 80.0F, 1.0F, true), "rounded"
      ));
      entries.push_back(makeEntry(
          section, "shape", tr("settings.schema.shared.corner-top-left.label"),
          tr("settings.schema.bar.corner-top-left.description"), path("radius_top_left"),
          barCornerSlider(bar.radiusTopLeft), "rounded corner", true
      ));
      entries.push_back(makeEntry(
          section, "shape", tr("settings.schema.shared.corner-top-right.label"),
          tr("settings.schema.bar.corner-top-right.description"), path("radius_top_right"),
          barCornerSlider(bar.radiusTopRight), "rounded corner", true
      ));
      entries.push_back(makeEntry(
          section, "shape", tr("settings.schema.shared.corner-bottom-left.label"),
          tr("settings.schema.bar.corner-bottom-left.description"), path("radius_bottom_left"),
          barCornerSlider(bar.radiusBottomLeft), "rounded corner", true
      ));
      entries.push_back(makeEntry(
          section, "shape", tr("settings.schema.shared.corner-bottom-right.label"),
          tr("settings.schema.bar.corner-bottom-right.description"), path("radius_bottom_right"),
          barCornerSlider(bar.radiusBottomRight), "rounded corner", true
      ));
      {
        auto e = keyedEntry(
            section, "shape", "bar.concave-edge-corners", path("concave_edge_corners"),
            ToggleSetting{bar.concaveEdgeCorners}, "rounded corner carve"
        );
        e.visibleWhen = [barName = bar.name](const Config& c) {
          const BarConfig* b = findBar(c, barName);
          return b != nullptr && b->marginEdge == 0;
        };
        entries.push_back(std::move(e));
      }
      entries.push_back(
          keyedEntry(section, "shape", "bar.border", path("border"), colorSpecPicker(bar.border), "outline color", true)
      );
      entries.push_back(keyedEntry(
          section, "shape", "bar.border-width", path("border_width"),
          barReservedSlider(bar.borderWidth, 20.0F, 0.5F, false), "outline stroke", true
      ));
      entries.push_back(makeEntry(
          section, "effects", tr("settings.schema.shared.background-opacity.label"),
          tr("settings.schema.bar.background-opacity.description"), path("background_opacity"),
          SliderSetting{bar.backgroundOpacity, 0.0F, 1.0F, 0.01F, false}, "alpha transparency translucent opacity"
      ));
      if (env.backgroundEffectBlurSupported) {
        entries.push_back(keyedEntry(
            section, "effects", "bar.compositor-blur", path("compositor_blur"), ToggleSetting{bar.compositorBlur},
            "blur frosted background effect wayland"
        ));
      }
      entries.push_back(makeEntry(
          section, "effects", tr("settings.schema.shared.shadow.label"), tr("settings.schema.bar.shadow.description"),
          path("shadow"), ToggleSetting{bar.shadow}, "shadow"
      ));
      entries.push_back(makeEntry(
          section, "effects", tr("settings.schema.shared.contact-shadow.label"),
          tr("settings.schema.bar.contact-shadow.description"), path("contact_shadow"),
          ToggleSetting{bar.contactShadow}, "shadow contact panel attached"
      ));
      entries.push_back(keyedEntry(
          section, "layout", "bar.panel-overlap", path("panel_overlap"), barPanelOverlapStepper(bar.panelOverlap),
          "seam gap overlap attached panel fractional scale", true
      ));
      const std::string barResolvedFontFamily =
          bar.fontFamily && !bar.fontFamily->empty() ? *bar.fontFamily : cfg.shell.fontFamily;
      {
        SettingControl fontFamilyControl = TextSetting{
            .value = bar.fontFamily.value_or(""), .placeholder = cfg.shell.fontFamily, .browseFileExtensions = {}
        };
        if (!env.fontFamilies.empty()) {
          fontFamilyControl = SearchPickerSetting{
              .options = env.fontFamilies,
              .selectedValue = bar.fontFamily.value_or(""),
              .placeholder = cfg.shell.fontFamily,
              .emptyText = tr("ui.controls.search-picker.empty"),
              .preferredHeight = 280.0F,
          };
        }
        entries.push_back(keyedEntry(
            section, "widgets", "bar.font-family", path("font_family"), std::move(fontFamilyControl), "typeface font"
        ));
      }
      {
        std::vector<SelectOption> fontWeightOptions;
        const auto widgetOptions =
            buildLabelFontWeightSelectOptions(barResolvedFontFamily, FontWeightSelectKind::BarDefault, bar.fontWeight);
        fontWeightOptions.reserve(widgetOptions.size());
        for (const auto& option : widgetOptions) {
          fontWeightOptions.push_back(SelectOption{option.value, tr(option.labelKey)});
        }
        SelectSetting fontWeightSelect{std::move(fontWeightOptions), std::to_string(bar.fontWeight)};
        fontWeightSelect.valueType = SelectValueType::Integer;
        entries.push_back(keyedEntry(
            section, "widgets", "bar.font-weight", path("font_weight"), std::move(fontWeightSelect), "font text weight"
        ));
      }
      entries.push_back(keyedEntry(
          section, "widgets", "bar.widget-spacing", path("widget_spacing"),
          SliderSetting{bar.widgetSpacing, 0.0F, 32.0F, 1.0F, true}, "gap"
      ));
      entries.push_back(keyedEntry(
          section, "widgets", "bar.widget-color", path("color"), colorSpecPicker(bar.widgetColor, true),
          "color foreground", true
      ));
      entries.push_back(keyedEntry(
          section, "widgets", "bar.widget-icon-color", path("icon_color"), colorSpecPicker(bar.widgetIconColor, true),
          "color icon", true
      ));
      entries.push_back(keyedEntry(
          section, "widgets", "bar.hover-highlight", path("hover_highlight"), ToggleSetting{bar.hoverHighlight},
          "hover highlight mouse pointer"
      ));
      entries.push_back(keyedEntry(
          section, "widgets", "bar.show-tooltip", path("show_tooltip"), ToggleSetting{bar.showTooltip}, "tooltip hover"
      ));
      entries.push_back(keyedEntry(
          section, "capsules", "bar.widget-capsules", path("capsule"), ToggleSetting{bar.widgetCapsuleDefault}, "pill"
      ));
      entries.push_back(keyedEntry(
          section, "capsules", "bar.capsule-thickness", path("capsule_thickness"),
          SliderSetting{bar.capsuleThickness, 0.1F, 1.0F, 0.01F, false}, "pill thickness size", true
      ));
      const SettingVisibility capsuleOn = [on = bar.widgetCapsuleDefault](const Config&) { return on; };
      {
        auto e = keyedEntry(
            section, "capsules", "bar.capsule-radius", path("capsule_radius"),
            OptionalStepperSetting{
                .value = radiusStepperValue(bar.widgetCapsuleRadius),
                .minValue = 0,
                .maxValue = 80,
                .step = 1,
                .fallbackValue = radiusStepperFallback(bar.widgetCapsuleRadius),
                .unsetLabel = tr("common.states.auto"),
                .customLabel = tr("common.states.custom")
            },
            "pill rounded radius", true
        );
        entries.push_back(std::move(e));
      }
      {
        auto e = keyedEntry(
            section, "capsules", "bar.capsule-fill", path("capsule_fill"), colorSpecPicker(bar.widgetCapsuleFill),
            "color pill", true
        );
        e.visibleWhen = capsuleOn;
        entries.push_back(std::move(e));
      }
      entries.push_back(keyedEntry(
                            section, "capsules", "bar.capsule-foreground", path("capsule_foreground"),
                            colorSpecPicker(bar.widgetCapsuleForeground, true), "color foreground pill", true
      )
                            .shownWhen(capsuleOn));
      entries.push_back(keyedEntry(
                            section, "capsules", "bar.capsule-border", path("capsule_border"),
                            colorSpecPicker(bar.widgetCapsuleBorder, true), "color pill outline", true
      )
                            .shownWhen(capsuleOn));
      entries.push_back(
          keyedEntry(
              section, "capsules", "bar.capsule-border-width", path("capsule_border_width"),
              sliderFor(bar.widgetCapsuleBorderWidth, noctalia::config::schema::kBarCapsuleBorderWidthRange, false),
              "pill outline width", true
          )
              .shownWhen(capsuleOn)
      );
      entries.push_back(keyedEntry(
                            section, "capsules", "bar.capsule-padding", path("capsule_padding"),
                            SliderSetting{bar.widgetCapsulePadding, 0.0F, 48.0F, 1.0F, false}, "pill inset", true
      )
                            .shownWhen(capsuleOn));
      entries.push_back(keyedEntry(
                            section, "capsules", "bar.capsule-opacity", path("capsule_opacity"),
                            SliderSetting{bar.widgetCapsuleOpacity, 0.0F, 1.0F, 0.01F, false}, "pill alpha", true
      )
                            .shownWhen(capsuleOn));
      entries.push_back(keyedEntry(
          section, "widget-list", "bar.start-widgets", path("start"), ListSetting{.items = bar.startWidgets}, "left"
      ));
      entries.push_back(keyedEntry(
          section, "widget-list", "bar.center-widgets", path("center"), ListSetting{.items = bar.centerWidgets},
          "middle"
      ));
      entries.push_back(keyedEntry(
          section, "widget-list", "bar.end-widgets", path("end"), ListSetting{.items = bar.endWidgets}, "right"
      ));
      // One row per gesture, from the same closed set widget actions use. An unset row shows the
      // built-in default as its placeholder, so "empty" never reads as "does nothing".
      for (const auto gesture : noctalia::bar::allGestures()) {
        const std::string key(noctalia::bar::gestureConfigKey(gesture));
        const auto configured = bar.deadZone.actions.find(key);
        entries.push_back(makeEntry(
            section, "dead-zone", tr(std::string(noctalia::bar::gestureLabelKey(gesture))),
            tr("settings.schema.bar.dead-zone-action.description"),
            std::vector<std::string>{"bar", bar.name, "dead_zone", "actions", key},
            GestureActionSetting{
                .gestureKey = key,
                .configured = configured != bar.deadZone.actions.end() ? configured->second : std::string{},
                .defaultAction = deadZoneDefault(gesture),
            },
            "bar empty margin dead zone action command gesture " + key
        ));
      }
    }

    // Bar monitor overrides (all bars).
    for (const auto& bar : cfg.bars) {
      for (const auto& ovr : bar.monitorOverrides) {
        constexpr SettingsSection section = SettingsSection::Bar;
        const std::vector<std::string> root = {"bar", bar.name, "monitor", ovr.match};
        auto monitorPath = [&](std::string key) {
          std::vector<std::string> p = root;
          p.push_back(std::move(key));
          return p;
        };

        entries.push_back(makeEntry(
            section, "general", tr("settings.schema.shared.enabled.label"),
            tr("settings.schema.bar.enabled.description"), monitorPath("enabled"),
            ToggleSetting{ovr.enabled.value_or(bar.enabled)}, "visible"
        ));
        entries.push_back(makeEntry(
            section, "general", tr("settings.schema.shared.position.label"),
            tr("settings.schema.bar.position.description"), monitorPath("position"),
            positionSelect(ovr.position.value_or(bar.position)), "edge"
        ));
        entries.push_back(makeEntry(
            section, "general", tr("settings.schema.shared.auto-hide.label"),
            tr("settings.schema.bar.auto-hide.description"), monitorPath("auto_hide"),
            autoHideModeSelect(
                barAutoHideMode(ovr.autoHide.value_or(bar.autoHide), ovr.smartAutoHide.value_or(bar.smartAutoHide)),
                monitorPath("smart_auto_hide")
            ),
            "autohide smart workspace"
        ));
        {
          auto e = keyedEntry(
              section, "general", "bar.auto-hide-delay", monitorPath("auto_hide_delay_ms"),
              StepperSetting{
                  .value = ovr.autoHideDelayMs.value_or(bar.autoHideDelayMs),
                  .minValue = static_cast<int>(noctalia::config::schema::kBarAutoHideDelayMsRange.min.value()),
                  .maxValue = static_cast<int>(noctalia::config::schema::kBarAutoHideDelayMsRange.max.value()),
                  .step = static_cast<int>(noctalia::config::schema::kBarAutoHideDelayMsRange.step.value()),
                  .valueSuffix = "ms",
              },
              "autohide delay wait"
          );
          e.visibleWhen = [barName = bar.name, match = ovr.match](const Config& c) {
            const BarConfig* b = findBar(c, barName);
            if (b == nullptr) {
              return false;
            }
            const BarMonitorOverride* o = findMonitorOverride(*b, match);
            const bool autoHide = o != nullptr ? o->autoHide.value_or(b->autoHide) : b->autoHide;
            const bool smartAutoHide = o != nullptr ? o->smartAutoHide.value_or(b->smartAutoHide) : b->smartAutoHide;
            return autoHide && !smartAutoHide;
          };
          entries.push_back(std::move(e));
        }
        const SettingVisibility monitorAutoHideOn = [barName = bar.name, match = ovr.match](const Config& c) {
          const BarConfig* b = findBar(c, barName);
          if (b == nullptr) {
            return false;
          }
          const BarMonitorOverride* o = findMonitorOverride(*b, match);
          return o != nullptr ? o->isAutoHideEnabled(b->autoHide, b->smartAutoHide) : b->isAutoHideEnabled();
        };
        {
          auto e = keyedEntry(
              section, "general", "bar.show-on-workspace-switch", monitorPath("show_on_workspace_switch"),
              ToggleSetting{ovr.showOnWorkspaceSwitch.value_or(bar.showOnWorkspaceSwitch)},
              "workspace reveal peek autohide"
          );
          e.visibleWhen = monitorAutoHideOn;
          entries.push_back(std::move(e));
        }
        entries.push_back(makeEntry(
            section, "general", tr("settings.schema.shared.reserve-space.label"),
            tr("settings.schema.bar.reserve-space.description"), monitorPath("reserve_space"),
            ToggleSetting{ovr.reserveSpace.value_or(bar.reserveSpace)}, "exclusive zone"
        ));
        entries.push_back(keyedEntry(
            section, "general", "bar.layer", monitorPath("layer"),
            asSegmented(plainSelect(
                {{"top", "settings.options.layer.top"}, {"overlay", "settings.options.layer.overlay"}},
                ovr.layer.value_or(bar.layer)
            )),
            "layer shell z-order"
        ));
        entries.push_back(keyedEntry(
            section, "layout", "bar.thickness", monitorPath("thickness"),
            SliderSetting{ovr.thickness.value_or(bar.thickness), 10.0F, 120.0F, 1.0F, true}, "height width"
        ));
        entries.push_back(keyedEntry(
            section, "layout", "bar.content-scale", monitorPath("scale"),
            SliderSetting{ovr.scale.value_or(bar.scale), 0.5F, 4.0F, 0.05F, false}, "zoom size"
        ));
        entries.push_back(keyedEntry(
            section, "layout", "bar.font-scale", monitorPath("font_scale"),
            sliderFor(ovr.fontScale.value_or(bar.fontScale), noctalia::config::schema::kBarFontScaleRange, false),
            "text zoom size"
        ));
        entries.push_back(makeEntry(
            section, "layout", tr("settings.schema.shared.ends-margin.label"),
            tr("settings.schema.bar.ends-margin.description"), monitorPath("margin_ends"),
            barMarginStepper(ovr.marginEnds.value_or(bar.marginEnds)), "gap inset"
        ));
        entries.push_back(makeEntry(
            section, "layout", tr("settings.schema.shared.edge-margin.label"),
            tr("settings.schema.bar.edge-margin.description"), monitorPath("margin_edge"),
            barMarginStepper(ovr.marginEdge.value_or(bar.marginEdge)), "gap inset"
        ));
        entries.push_back(makeEntry(
            section, "layout", tr("settings.schema.shared.opposite-edge-margin.label"),
            tr("settings.schema.bar.opposite-edge-margin.description"), monitorPath("margin_opposite_edge"),
            barMarginStepper(ovr.marginOppositeEdge.value_or(bar.marginOppositeEdge)), "gap inset strut"
        ));
        entries.push_back(keyedEntry(
            section, "layout", "bar.content-padding", monitorPath("padding"),
            SliderSetting{ovr.padding.value_or(bar.padding), 0.0F, 80.0F, 1.0F, true}, "inset"
        ));
        entries.push_back(makeEntry(
            section, "shape", tr("settings.schema.shared.corner-radius.label"),
            tr("settings.schema.bar.corner-radius.description"), monitorPath("radius"),
            barReservedSlider(ovr.radius.value_or(bar.radius), 80.0F, 1.0F, true), "rounded"
        ));
        entries.push_back(makeEntry(
            section, "shape", tr("settings.schema.shared.corner-top-left.label"),
            tr("settings.schema.bar.corner-top-left.description"), monitorPath("radius_top_left"),
            barCornerSlider(ovr.radiusTopLeft.value_or(bar.radiusTopLeft)), "rounded corner", true
        ));
        entries.push_back(makeEntry(
            section, "shape", tr("settings.schema.shared.corner-top-right.label"),
            tr("settings.schema.bar.corner-top-right.description"), monitorPath("radius_top_right"),
            barCornerSlider(ovr.radiusTopRight.value_or(bar.radiusTopRight)), "rounded corner", true
        ));
        entries.push_back(makeEntry(
            section, "shape", tr("settings.schema.shared.corner-bottom-left.label"),
            tr("settings.schema.bar.corner-bottom-left.description"), monitorPath("radius_bottom_left"),
            barCornerSlider(ovr.radiusBottomLeft.value_or(bar.radiusBottomLeft)), "rounded corner", true
        ));
        entries.push_back(makeEntry(
            section, "shape", tr("settings.schema.shared.corner-bottom-right.label"),
            tr("settings.schema.bar.corner-bottom-right.description"), monitorPath("radius_bottom_right"),
            barCornerSlider(ovr.radiusBottomRight.value_or(bar.radiusBottomRight)), "rounded corner", true
        ));
        {
          auto e = keyedEntry(
              section, "shape", "bar.concave-edge-corners", monitorPath("concave_edge_corners"),
              ToggleSetting{ovr.concaveEdgeCorners.value_or(bar.concaveEdgeCorners)}, "rounded corner carve"
          );
          e.visibleWhen = [barName = bar.name, match = ovr.match](const Config& c) {
            const BarConfig* b = findBar(c, barName);
            if (b == nullptr) {
              return false;
            }
            const BarMonitorOverride* mo = findMonitorOverride(*b, match);
            const std::int32_t marginEdge = mo && mo->marginEdge.has_value() ? *mo->marginEdge : b->marginEdge;
            return marginEdge == 0;
          };
          entries.push_back(std::move(e));
        }
        entries.push_back(keyedEntry(
            section, "shape", "bar.border", monitorPath("border"),
            colorSpecPicker(ovr.border, true, tr("common.states.inherit")), "outline color", true
        ));
        entries.push_back(keyedEntry(
            section, "shape", "bar.border-width", monitorPath("border_width"),
            barReservedSlider(ovr.borderWidth.value_or(bar.borderWidth), 20.0F, 0.5F, false), "outline stroke", true
        ));
        entries.push_back(makeEntry(
            section, "effects", tr("settings.schema.shared.background-opacity.label"),
            tr("settings.schema.bar.background-opacity.description"), monitorPath("background_opacity"),
            SliderSetting{ovr.backgroundOpacity.value_or(bar.backgroundOpacity), 0.0F, 1.0F, 0.01F, false},
            "alpha transparency translucent opacity"
        ));
        if (env.backgroundEffectBlurSupported) {
          entries.push_back(keyedEntry(
              section, "effects", "bar.compositor-blur", monitorPath("compositor_blur"),
              ToggleSetting{ovr.compositorBlur.value_or(bar.compositorBlur)}, "blur frosted background effect wayland"
          ));
        }
        entries.push_back(makeEntry(
            section, "effects", tr("settings.schema.shared.shadow.label"), tr("settings.schema.bar.shadow.description"),
            monitorPath("shadow"), ToggleSetting{ovr.shadow.value_or(bar.shadow)}, "shadow"
        ));
        entries.push_back(makeEntry(
            section, "effects", tr("settings.schema.shared.contact-shadow.label"),
            tr("settings.schema.bar.contact-shadow.description"), monitorPath("contact_shadow"),
            ToggleSetting{ovr.contactShadow.value_or(bar.contactShadow)}, "shadow contact panel attached"
        ));
        entries.push_back(keyedEntry(
            section, "layout", "bar.panel-overlap", monitorPath("panel_overlap"),
            barPanelOverlapStepper(ovr.panelOverlap.value_or(bar.panelOverlap)),
            "seam gap overlap attached panel fractional scale", true
        ));
        {
          const std::string monitorInheritedFontFamily = bar.fontFamily.value_or(cfg.shell.fontFamily);
          SettingControl fontFamilyControl = TextSetting{
              .value = ovr.fontFamily.value_or(""),
              .placeholder = monitorInheritedFontFamily,
              .browseFileExtensions = {}
          };
          if (!env.fontFamilies.empty()) {
            fontFamilyControl = SearchPickerSetting{
                .options = env.fontFamilies,
                .selectedValue = ovr.fontFamily.value_or(""),
                .placeholder = monitorInheritedFontFamily,
                .emptyText = tr("ui.controls.search-picker.empty"),
                .preferredHeight = 280.0F,
            };
          }
          entries.push_back(keyedEntry(
              section, "widgets", "bar.font-family", monitorPath("font_family"), std::move(fontFamilyControl),
              "typeface font", true
          ));
        }
        entries.push_back(keyedEntry(
            section, "widgets", "bar.widget-spacing", monitorPath("widget_spacing"),
            SliderSetting{ovr.widgetSpacing.value_or(bar.widgetSpacing), 0.0F, 32.0F, 1.0F, true}, "gap"
        ));
        entries.push_back(keyedEntry(
            section, "widgets", "bar.widget-color", monitorPath("color"),
            colorSpecPicker(ovr.widgetColor, true, tr("common.states.inherit")), "color foreground", true
        ));
        entries.push_back(keyedEntry(
            section, "widgets", "bar.widget-icon-color", monitorPath("icon_color"),
            colorSpecPicker(ovr.widgetIconColor, true, tr("common.states.inherit")), "color icon", true
        ));
        entries.push_back(keyedEntry(
            section, "capsules", "bar.widget-capsules", monitorPath("capsule"),
            ToggleSetting{ovr.widgetCapsuleDefault.value_or(bar.widgetCapsuleDefault)}, "pill"
        ));
        entries.push_back(keyedEntry(
            section, "capsules", "bar.capsule-thickness", monitorPath("capsule_thickness"),
            SliderSetting{ovr.capsuleThickness.value_or(bar.capsuleThickness), 0.1F, 1.0F, 0.01F, false},
            "pill thickness size", true
        ));
        const SettingVisibility monitorCapsuleOn =
            [on = ovr.widgetCapsuleDefault.value_or(bar.widgetCapsuleDefault)](const Config&) { return on; };
        {
          auto e = keyedEntry(
              section, "capsules", "bar.capsule-radius", monitorPath("capsule_radius"),
              OptionalStepperSetting{
                  .value = radiusStepperValue(ovr.widgetCapsuleRadius),
                  .minValue = 0,
                  .maxValue = 80,
                  .step = 1,
                  .fallbackValue = radiusStepperFallback(bar.widgetCapsuleRadius),
                  .unsetLabel = tr("common.states.inherit"),
                  .customLabel = tr("common.states.custom")
              },
              "pill rounded radius", true
          );
          entries.push_back(std::move(e));
        }
        {
          auto e = keyedEntry(
              section, "capsules", "bar.capsule-fill", monitorPath("capsule_fill"),
              colorSpecPicker(ovr.widgetCapsuleFill, true, tr("common.states.inherit")), "color pill", true
          );
          e.visibleWhen = monitorCapsuleOn;
          entries.push_back(std::move(e));
        }
        entries.push_back(keyedEntry(
                              section, "capsules", "bar.capsule-foreground", monitorPath("capsule_foreground"),
                              colorSpecPicker(ovr.widgetCapsuleForeground, true, tr("common.states.inherit")),
                              "color foreground pill", true
        )
                              .shownWhen(monitorCapsuleOn));
        entries.push_back(keyedEntry(
                              section, "capsules", "bar.capsule-border", monitorPath("capsule_border"),
                              colorSpecPicker(
                                  ovr.widgetCapsuleBorderSpecified ? ovr.widgetCapsuleBorder
                                                                   : std::optional<ColorSpec>{},
                                  true, tr("common.states.inherit")
                              ),
                              "color pill outline", true
        )
                              .shownWhen(monitorCapsuleOn));
        entries.push_back(keyedEntry(
                              section, "capsules", "bar.capsule-border-width", monitorPath("capsule_border_width"),
                              sliderFor(
                                  ovr.widgetCapsuleBorderWidth.value_or(bar.widgetCapsuleBorderWidth),
                                  noctalia::config::schema::kBarCapsuleBorderWidthRange, false
                              ),
                              "pill outline width", true
        )
                              .shownWhen(monitorCapsuleOn));
        entries.push_back(
            keyedEntry(
                section, "capsules", "bar.capsule-padding", monitorPath("capsule_padding"),
                SliderSetting{ovr.widgetCapsulePadding.value_or(bar.widgetCapsulePadding), 0.0F, 48.0F, 1.0F, false},
                "pill inset", true
            )
                .shownWhen(monitorCapsuleOn)
        );
        entries.push_back(
            keyedEntry(
                section, "capsules", "bar.capsule-opacity", monitorPath("capsule_opacity"),
                SliderSetting{ovr.widgetCapsuleOpacity.value_or(bar.widgetCapsuleOpacity), 0.0F, 1.0F, 0.01F, false},
                "pill alpha", true
            )
                .shownWhen(monitorCapsuleOn)
        );
        entries.push_back(keyedEntry(
            section, "widget-list", "bar.start-widgets", monitorPath("start"),
            ListSetting{.items = ovr.startWidgets.value_or(bar.startWidgets)}, "left"
        ));
        entries.push_back(keyedEntry(
            section, "widget-list", "bar.center-widgets", monitorPath("center"),
            ListSetting{.items = ovr.centerWidgets.value_or(bar.centerWidgets)}, "middle"
        ));
        entries.push_back(keyedEntry(
            section, "widget-list", "bar.end-widgets", monitorPath("end"),
            ListSetting{.items = ovr.endWidgets.value_or(bar.endWidgets)}, "right"
        ));
        for (const auto gesture : noctalia::bar::allGestures()) {
          const std::string key(noctalia::bar::gestureConfigKey(gesture));
          std::vector<std::string> gesturePath = root;
          gesturePath.emplace_back("dead_zone");
          gesturePath.emplace_back("actions");
          gesturePath.emplace_back(key);

          const auto& overrideActions = ovr.deadZone.actions;
          std::string configured;
          if (overrideActions.has_value()) {
            const auto overridden = overrideActions->find(key);
            if (overridden != overrideActions->end()) {
              configured = overridden->second;
            }
          }
          const auto inherited = bar.deadZone.actions.find(key);
          entries.push_back(makeEntry(
              section, "dead-zone", tr(std::string(noctalia::bar::gestureLabelKey(gesture))),
              tr("settings.schema.bar.dead-zone-action.description"), std::move(gesturePath),
              GestureActionSetting{
                  .gestureKey = key,
                  .configured = std::move(configured),
                  .defaultAction =
                      inherited != bar.deadZone.actions.end() ? inherited->second : deadZoneDefault(gesture),
              },
              "bar empty margin dead zone action command gesture " + key
          ));
        }
      }
    }

    if (env.paw2Value) {
      appendPaw2BehaviourEntries(entries, Paw2Values(env));
      appendPaw2DisplayEntries(entries, env);
      appendPaw2ShortcutEntries(entries, env);
    } else {
      appendPaw2Unavailable(entries, env);
    }
    // The window switcher is paw2 window handling; it sits on that page whether or not paw2 shares its config.
    entries.push_back(keyedEntry(
        SettingsSection::Paw2, "window-switcher", "shell.window-switcher-style", {"shell", "window_switcher", "style"},
        enumSelect(ShellConfig::kWindowSwitcherStyles, cfg.shell.windowSwitcher.style),
        "window switcher alt tab style carousel compact"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2, "window-switcher", "shell.window-switcher-mru", {"shell", "window_switcher", "mru"},
        ToggleSetting{cfg.shell.windowSwitcher.mru}, "window switcher alt tab mru most recently used"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2, "window-switcher", "shell.window-switcher-show-caption",
        {"shell", "window_switcher", "show_caption"}, ToggleSetting{cfg.shell.windowSwitcher.showCaption},
        "window switcher alt tab caption title application name"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2, "window-switcher", "shell.window-switcher-show-count",
        {"shell", "window_switcher", "show_count"}, ToggleSetting{cfg.shell.windowSwitcher.showCount},
        "window switcher alt tab count position total"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2, "window-switcher", "shell.window-switcher-show-app-icon",
        {"shell", "window_switcher", "show_app_icon"}, ToggleSetting{cfg.shell.windowSwitcher.showAppIcon},
        "window switcher alt tab application app icon"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2, "window-switcher", "shell.window-switcher-show-all-outputs",
        {"shell", "window_switcher", "show_all_outputs"}, ToggleSetting{cfg.shell.windowSwitcher.showAllOutputs},
        "window switcher alt tab monitor display output screen all current"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2, "window-switcher", "shell.window-switcher-current-workspace-only",
        {"shell", "window_switcher", "current_workspace_only"},
        ToggleSetting{cfg.shell.windowSwitcher.currentWorkspaceOnly},
        "window switcher alt tab workspace current only filter"
    ));

    // Keys inside the shell's panels and menus, on the Shortcuts page beside paw2's global ones.
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.validate", {"keybinds", "validate"},
        KeybindListSetting{
            .items = effectiveKeybindItems(cfg.keybinds.validate, KeybindAction::Validate), .maxItems = 4
        },
        "keybind shortcut hotkey enter accept submit confirm"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.cancel", {"keybinds", "cancel"},
        KeybindListSetting{.items = effectiveKeybindItems(cfg.keybinds.cancel, KeybindAction::Cancel), .maxItems = 4},
        "keybind shortcut hotkey escape close dismiss"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.left", {"keybinds", "left"},
        KeybindListSetting{.items = effectiveKeybindItems(cfg.keybinds.left, KeybindAction::Left), .maxItems = 4},
        "keybind shortcut hotkey arrow move"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.right", {"keybinds", "right"},
        KeybindListSetting{.items = effectiveKeybindItems(cfg.keybinds.right, KeybindAction::Right), .maxItems = 4},
        "keybind shortcut hotkey arrow move"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.up", {"keybinds", "up"},
        KeybindListSetting{.items = effectiveKeybindItems(cfg.keybinds.up, KeybindAction::Up), .maxItems = 4},
        "keybind shortcut hotkey arrow move"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.down", {"keybinds", "down"},
        KeybindListSetting{.items = effectiveKeybindItems(cfg.keybinds.down, KeybindAction::Down), .maxItems = 4},
        "keybind shortcut hotkey arrow move"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.tab-previous", {"keybinds", "tab_previous"},
        KeybindListSetting{
            .items = effectiveKeybindItems(cfg.keybinds.tabPrevious, KeybindAction::TabPrevious), .maxItems = 4
        },
        "keybind shortcut hotkey shift tab focus pane"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.tab-next", {"keybinds", "tab_next"},
        KeybindListSetting{.items = effectiveKeybindItems(cfg.keybinds.tabNext, KeybindAction::TabNext), .maxItems = 4},
        "keybind shortcut hotkey tab focus pane"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.copy", {"keybinds", "copy"},
        KeybindListSetting{.items = effectiveKeybindItems(cfg.keybinds.copy, KeybindAction::Copy), .maxItems = 4},
        "keybind shortcut hotkey copy clipboard"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.save", {"keybinds", "save"},
        KeybindListSetting{.items = effectiveKeybindItems(cfg.keybinds.save, KeybindAction::Save), .maxItems = 4},
        "keybind shortcut hotkey save file"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "keybinds.delete", {"keybinds", "delete"},
        KeybindListSetting{
            .items = effectiveKeybindItems(cfg.keybinds.deleteEntry, KeybindAction::Delete), .maxItems = 4
        },
        "keybind shortcut hotkey delete remove clear"
    ));

    entries.push_back(keyedEntry(
        SettingsSection::Paw2Shortcuts, "panel-keys", "shell.readline-shortcuts", {"shell", "readline_shortcuts"},
        ToggleSetting{cfg.shell.readlineShortcuts}, "readline emacs keyboard shortcuts text input line editing"
    ));

    // Night light and brightness act on the monitors, so they sit under them on Displays.
    entries.push_back(makeEntry(
        SettingsSection::Paw2Displays, "brightness", tr("settings.schema.services.ddcutil.label"),
        env.ddcutilAvailable ? tr("settings.schema.services.ddcutil.description")
                             : tr("settings.schema.services.ddcutil.requires-ddcutil"),
        {"brightness", "enable_ddcutil"},
        ToggleSetting{.checked = cfg.brightness.enableDdcutil, .enabled = env.ddcutilAvailable}, "monitor ddcutil"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Displays, "brightness", "services.minimum-brightness",
        {"brightness", "minimum_brightness"},
        sliderFor(cfg.brightness.minimumBrightness, noctalia::config::schema::kUnitRange, false), "floor clamp"
    ));
    entries.push_back(keyedEntry(
        SettingsSection::Paw2Displays, "brightness", "services.sync-monitor-brightness",
        {"brightness", "sync_all_monitors"}, ToggleSetting{.checked = cfg.brightness.syncBrightnessOfAllMonitors},
        "monitor brightness"
    ));
    if (!env.gammaControlAvailable) {
      entries.push_back(makeEntry(
          SettingsSection::Paw2Displays, "night-light", tr("settings.schema.services.night-light.label"),
          tr("settings.schema.services.night-light.requires-gamma-control"), {"nightlight", "enabled"},
          ToggleSetting{.checked = cfg.nightlight.enabled, .enabled = false}, "nightlight"
      ));
    } else {
      entries.push_back(keyedEntry(
          SettingsSection::Paw2Displays, "night-light", "services.night-light", {"nightlight", "enabled"},
          ToggleSetting{cfg.nightlight.enabled}, "nightlight"
      ));
      const SettingVisibility nightLightOn = [](const Config& c) { return c.nightlight.enabled; };
      entries.push_back(keyedEntry(
                            SettingsSection::Paw2Displays, "night-light", "services.force-night-light",
                            {"nightlight", "force"}, ToggleSetting{cfg.nightlight.force}, "nightlight"
      )
                            .shownWhen(nightLightOn));
      // Both sliders span the same range; the day > night invariant is enforced at commit time
      // via SliderSetting::linkedCommit, which pushes the other temperature when needed.
      const auto tempMin = static_cast<double>(NightLightConfig::kTemperatureMin);
      const auto tempMax = static_cast<double>(NightLightConfig::kTemperatureMax);
      const auto tempStep = static_cast<double>(NightLightConfig::kTemperatureGap);

      SliderSetting daySlider{static_cast<double>(cfg.nightlight.dayTemperature), tempMin, tempMax, tempStep, true};
      daySlider.linkedCommit = [curNight = cfg.nightlight.nightTemperature](double v) {
        std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>> overrides;
        const auto newDay = std::clamp(
            static_cast<std::int32_t>(std::lround(v)), NightLightConfig::kTemperatureMin,
            NightLightConfig::kTemperatureMax
        );
        if (newDay - NightLightConfig::kTemperatureGap < curNight) {
          std::int32_t pushedNight =
              std::max(NightLightConfig::kTemperatureMin, newDay - NightLightConfig::kTemperatureGap);
          if (pushedNight + NightLightConfig::kTemperatureGap > newDay) {
            // Day was below kTemperatureMin + kTemperatureGap; bump day up too. The slider value
            // refresh comes through the rebuilt registry on the next config reload.
            const std::int32_t bumpedDay =
                std::min(NightLightConfig::kTemperatureMax, pushedNight + NightLightConfig::kTemperatureGap);
            overrides.emplace_back(
                std::vector<std::string>{"nightlight", "temperature_day"}, static_cast<std::int64_t>(bumpedDay)
            );
          }
          overrides.emplace_back(
              std::vector<std::string>{"nightlight", "temperature_night"}, static_cast<std::int64_t>(pushedNight)
          );
        }
        return overrides;
      };
      entries.push_back(keyedEntry(
                            SettingsSection::Paw2Displays, "night-light", "services.day-temperature",
                            {"nightlight", "temperature_day"}, std::move(daySlider), "nightlight kelvin"
      )
                            .shownWhen(nightLightOn));

      SliderSetting nightSlider{static_cast<double>(cfg.nightlight.nightTemperature), tempMin, tempMax, tempStep, true};
      nightSlider.linkedCommit = [curDay = cfg.nightlight.dayTemperature](double v) {
        std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>> overrides;
        const auto newNight = std::clamp(
            static_cast<std::int32_t>(std::lround(v)), NightLightConfig::kTemperatureMin,
            NightLightConfig::kTemperatureMax
        );
        if (curDay - NightLightConfig::kTemperatureGap < newNight) {
          std::int32_t pushedDay =
              std::min(NightLightConfig::kTemperatureMax, newNight + NightLightConfig::kTemperatureGap);
          if (pushedDay - NightLightConfig::kTemperatureGap < newNight) {
            const std::int32_t bumpedNight =
                std::max(NightLightConfig::kTemperatureMin, pushedDay - NightLightConfig::kTemperatureGap);
            overrides.emplace_back(
                std::vector<std::string>{"nightlight", "temperature_night"}, static_cast<std::int64_t>(bumpedNight)
            );
          }
          overrides.emplace_back(
              std::vector<std::string>{"nightlight", "temperature_day"}, static_cast<std::int64_t>(pushedDay)
          );
        }
        return overrides;
      };
      entries.push_back(keyedEntry(
                            SettingsSection::Paw2Displays, "night-light", "services.night-temperature",
                            {"nightlight", "temperature_night"}, std::move(nightSlider), "nightlight kelvin"
      )
                            .shownWhen(nightLightOn));
    }

    // Integrity guard: every override path must resolve to a real schema key, else
    // the entry silently reads/writes a dead override. Build-determined, so checked
    // once per process; warn-only. (Visibility predicates are compiler-checked.)
    {
      static bool verified = false;
      if (!verified) {
        verified = true;
        const Logger log("settings");
        const auto verify = [&](const std::vector<std::string>& path, std::string_view what) {
          if (!path.empty() && !noctalia::config::schema::isKnownConfigPath(path)) {
            std::string dotted;
            for (const auto& seg : path) {
              dotted += (dotted.empty() ? "" : ".") + seg;
            }
            log.warn("settings registry {} path does not resolve to a schema key: {}", what, dotted);
          }
        };
        for (const auto& entry : entries) {
          // paw2's rows are paw2's settings, which the shell's schema does not describe.
          if (!std::holds_alternative<ButtonSetting>(entry.control) && !isPaw2SettingPath(entry.path)) {
            verify(entry.path, "override");
          }
        }
      }
    }

    moveBarFineTuningToAdvanced(entries);
    return entries;
  }

} // namespace settings
