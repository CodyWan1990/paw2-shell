#include "shell/session/session_action_meta.h"

#include "config/config_types.h"
#include "i18n/i18n.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace session_action {

  namespace {

    struct ActionMeta {
      std::string_view name;
      const char* labelKey;
      const char* glyph;
    };

    constexpr std::array<ActionMeta, 6> kBuiltinActions{{
        {"lock", "session.actions.lock", "lock"},
        {"logout", "session.actions.logout", "logout"},
        {"suspend", "session.actions.suspend", "suspend"},
        {"lock_and_suspend", "session.actions.lock-and-suspend", "suspend"},
        {"reboot", "session.actions.reboot", "reboot"},
        {"shutdown", "session.actions.shutdown", "shutdown"},
    }};

    [[nodiscard]] const ActionMeta* builtin(std::string_view action) {
      const auto* it = std::ranges::find(kBuiltinActions, action, &ActionMeta::name);
      return it != kBuiltinActions.end() ? it : nullptr;
    }

  } // namespace

  bool isKnown(std::string_view action) { return action == "command" || builtin(action) != nullptr; }

  const char* labelKey(std::string_view action) {
    const auto* meta = builtin(action);
    return meta != nullptr ? meta->labelKey : "session.actions.custom";
  }

  const char* defaultGlyph(std::string_view action) {
    const auto* meta = builtin(action);
    return meta != nullptr ? meta->glyph : "terminal";
  }

  std::string displayLabel(const SessionPanelActionConfig& action) {
    return action.label.has_value() && !action.label->empty() ? *action.label : i18n::tr(labelKey(action.action));
  }

  std::string displayGlyph(const SessionPanelActionConfig& action) {
    return action.glyph.has_value() && !action.glyph->empty() ? *action.glyph : defaultGlyph(action.action);
  }

  std::optional<std::string_view> canonicalActionName(std::string_view ipcOrConfigAction) {
    if (ipcOrConfigAction == "lock-and-suspend") {
      return std::string_view{"lock_and_suspend"};
    }
    if (const auto* meta = builtin(ipcOrConfigAction); meta != nullptr) {
      return meta->name;
    }
    return std::nullopt;
  }

} // namespace session_action
