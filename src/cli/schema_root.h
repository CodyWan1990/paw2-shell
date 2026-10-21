#pragma once

#include "cli/schema.h"
#include "cli/schema_config.h"
#include "cli/schema_dmenu.h"
#include "cli/schema_firefox_theme.h"
#include "cli/schema_msg.h"
#include "cli/schema_theme.h"

#include <array>

namespace noctalia::cli {

  inline constexpr std::array<std::string_view, 3> kCompletionShellChoices{"bash", "fish", "zsh"};
  inline constexpr std::array kCompletionsPositionals{
      Positional{"shell", "Shell completion format", kCompletionShellChoices, true, false, false},
  };
  inline constexpr Command kCompletionsCmd{
      "completions",
      "Generate shell completion scripts",
      "Generate a completion script from the live CLI schema.\n\n"
      "Examples:\n"
      "  source <(paw2-shell completions bash)\n"
      "  paw2-shell completions fish > ~/.config/fish/completions/paw2-shell.fish",
      {},
      {},
      kCompletionsPositionals,
      {},
      false,
  };

  inline constexpr std::array kRootFlags{
      Flag{"--version", "-v", {}, "Show version information", {}, {}, false, false},
      Flag{"--daemon", "-d", {}, "Run in background", {}, {}, false, false},
  };

  inline constexpr std::array kRootSubcommands{
      kCompletionsCmd, kConfigCmd, kDmenuCmd, kFirefoxThemeCmd, kMsgCmd, kThemeCmd,
  };

  inline constexpr Command kRootCmd{
      "paw2-shell",
      "Desktop shell for the paw2 Wayland compositor, built on Noctalia",
      {},
      "Noctalia documentation (most of it applies):\n  https://noctalia.dev",
      kRootFlags,
      {},
      kRootSubcommands,
      false,
  };

  static_assert(validateCommand(kRootCmd));

} // namespace noctalia::cli
