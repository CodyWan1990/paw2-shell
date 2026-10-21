#include "application.h"
#include "core/log.h"
#include "launcher/dmenu_provider.h"
#include "shell/launcher/launcher_panel.h"

namespace {
  constexpr Logger kLog("dmenu");
} // namespace

void Application::reloadDmenuProviders() {
  if (m_launcherPanel == nullptr) {
    return;
  }
  m_launcherPanel->clearProvidersWithIdPrefix("dmenu.");
  for (const auto& entry : m_configService.config().shell.launcher.dmenu.entries) {
    if (entry.command.empty() && !entry.freeform) {
      kLog.warn("[{}] missing command, skipping", entry.id);
      continue;
    }
    if (entry.prefix.value_or("").empty() && !entry.global) {
      kLog.warn("[{}] no prefix and global=false; unreachable until configured", entry.id);
    }
    m_launcherPanel->addProvider(std::make_unique<DmenuProvider>(entry, &m_clipboardService));
  }
}
