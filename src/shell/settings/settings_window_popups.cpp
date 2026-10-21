#include "calendar/calendar_discovery_state.h"
#include "calendar/calendar_service.h"
#include "calendar/vdir_reader.h"
#include "config/atomic_file.h"
#include "config/config_service.h"
#include "config/config_types.h"
#include "core/deferred_call.h"
#include "i18n/i18n.h"
#include "net/url_open.h"
#include "notification/notification_filter.h"
#include "render/render_context.h"
#include "render/scene/input_area.h"
#include "shell/settings/bar_widget_editor.h"
#include "shell/settings/color_spec_picker.h"
#include "shell/settings/settings_content.h"
#include "shell/settings/settings_content_common.h"
#include "shell/settings/settings_control_factory.h"
#include "shell/settings/settings_registry.h"
#include "shell/settings/settings_window.h"
#include "shell/settings/template_store_content.h"
#include "shell/settings/widget_settings_registry.h"
#include "theme/community_templates.h"
#include "ui/builders.h"
#include "ui/controls/button.h"
#include "ui/controls/context_menu.h"
#include "ui/controls/context_menu_popup.h"
#include "ui/controls/flex.h"
#include "ui/controls/input.h"
#include "ui/controls/segmented.h"
#include "ui/dialogs/file_dialog.h"
#include "ui/popup_parent.h"
#include "util/file_utils.h"
#include "util/string_utils.h"
#include "wayland/toplevel_surface.h"
#include "wayland/wayland_connection.h"
#include "wayland/wayland_seat.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

  constexpr std::int32_t kActionSupportReport = 1;
  constexpr std::int32_t kActionExportConfig = 2;
  constexpr std::string_view kCalendarDiscoveryOwner = "calendar_discovery";

  std::string calendarCredentialError(CalendarService::CredentialOperationResult result) {
    switch (result) {
    case CalendarService::CredentialOperationResult::Unavailable:
      return i18n::tr("settings.calendar-accounts.secret-service-unavailable");
    case CalendarService::CredentialOperationResult::Cancelled:
      return i18n::tr("settings.calendar-accounts.secret-service-cancelled");
    case CalendarService::CredentialOperationResult::DeniedOrLocked:
      return i18n::tr("settings.calendar-accounts.secret-service-locked");
    case CalendarService::CredentialOperationResult::CleanupError:
      return i18n::tr("settings.calendar-accounts.secret-cleanup-error");
    case CalendarService::CredentialOperationResult::FileError:
      return i18n::tr("settings.calendar-accounts.password-file-error");
    case CalendarService::CredentialOperationResult::ConfigError:
      return i18n::tr("settings.calendar-accounts.save-error");
    case CalendarService::CredentialOperationResult::MissingCredential:
    case CalendarService::CredentialOperationResult::BackendError:
      return i18n::tr("settings.calendar-accounts.secret-service-error");
    case CalendarService::CredentialOperationResult::Success:
      return {};
    }
    return i18n::tr("settings.calendar-accounts.secret-service-error");
  }

  XdgPopupParent popupParentFor(ToplevelSurface& surface, wl_output* output, std::uint32_t serial) {
    return XdgPopupParent{
        .xdgSurface = surface.xdgSurface(),
        .wlSurface = surface.wlSurface(),
        .output = output,
        .serial = serial,
        .width = surface.width(),
        .height = surface.height(),
    };
  }

  enum class CalendarAccountProvider : std::uint8_t {
    ICloud,
    CustomCalDav,
    IcsFileURL,
    Vdir,
  };

  struct CalendarAccountDraft {
    bool creating = true;
    CalendarAccountProvider provider = CalendarAccountProvider::ICloud;
    std::string id = "personal_icloud";
    std::string name;
    std::string username;
    std::string password;
    CalendarCredentialSource credentialSource = CalendarCredentialSource::SecretService;
    std::string passwordFile;
    std::string serverUrl;
    std::string path;
    std::string color;
    std::vector<std::string> calendars;
    std::vector<CalendarSource> discoveredCalendars;
    bool idInvalid = false;
    bool usernameInvalid = false;
    bool passwordInvalid = false;
    bool passwordFileInvalid = false;
    bool serverUrlInvalid = false;
    bool pathInvalid = false;
    bool credentialOperationInFlight = false;
  };

  bool validCalendarAccountId(std::string_view id) {
    if (id.empty()) {
      return false;
    }
    return std::ranges::all_of(id, [](char ch) {
      return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_';
    });
  }

  bool calendarAccountIdExists(const Config& cfg, std::string_view id) {
    return std::ranges::contains(cfg.calendar.accounts, id, &CalendarConfig::Account::id);
  }

  const CalendarConfig::Account* findCalendarAccount(const Config& cfg, std::string_view id) {
    const auto it = std::ranges::find(cfg.calendar.accounts, id, &CalendarConfig::Account::id);
    return it != cfg.calendar.accounts.end() ? &*it : nullptr;
  }

  std::string calendarProviderKey(CalendarAccountProvider provider) {
    switch (provider) {
    case CalendarAccountProvider::ICloud:
      return "icloud";
    case CalendarAccountProvider::CustomCalDav:
      return "custom";
    case CalendarAccountProvider::IcsFileURL:
      return "ics";
    case CalendarAccountProvider::Vdir:
      return "vdir";
    }
    return "icloud";
  }

  std::string calendarProviderTitle(CalendarAccountProvider provider) {
    switch (provider) {
    case CalendarAccountProvider::ICloud:
      return i18n::tr("settings.calendar-accounts.provider.icloud");
    case CalendarAccountProvider::CustomCalDav:
      return i18n::tr("settings.calendar-accounts.provider.custom");
    case CalendarAccountProvider::IcsFileURL:
      return i18n::tr("settings.calendar-accounts.provider.ics");
    case CalendarAccountProvider::Vdir:
      return i18n::tr("settings.calendar-accounts.provider.vdir");
    }
    return i18n::tr("settings.calendar-accounts.provider.icloud");
  }

  bool calendarSourceChecked(const CalendarAccountDraft& draft, const CalendarSource& source) {
    return draft.calendars.empty() || std::ranges::contains(draft.calendars, source.id);
  }

  std::string trimInput(Input* input) { return input != nullptr ? StringUtils::trim(input->value()) : std::string{}; }

  std::string sessionActionTitle(const SessionPanelActionConfig& row) {
    return settings::sessionActionDisplayTitle(row);
  }

  std::string idleBehaviorTitle(const IdleBehaviorConfig& row) {
    IdleBehaviorConfig norm = row;
    normalizeIdleBehaviorAction(norm);
    if (norm.action == "lock") {
      return i18n::tr("settings.idle.behavior.kind.lock");
    }
    if (norm.action == "screen_off") {
      return i18n::tr("settings.idle.behavior.kind.screen-off");
    }
    if (norm.action == "suspend") {
      return i18n::tr("settings.idle.behavior.kind.suspend");
    }
    if (norm.action == "lock_and_suspend") {
      return i18n::tr("settings.idle.behavior.kind.lock-and-suspend");
    }
    if (!StringUtils::trim(row.name).empty()) {
      return row.name;
    }
    return i18n::tr("settings.idle.behavior.unnamed");
  }

  std::string notificationFilterTitle(const NotificationFilterConfig& row) {
    if (!row.match.empty()) {
      return row.match;
    }
    if (!StringUtils::trim(row.name).empty()) {
      return row.name;
    }
    return i18n::tr("settings.notifications.filter.unnamed");
  }

  // The list row an open editor writes back to: the one still named `key`, else the one at `index`.
  template <typename Row>
  typename std::vector<Row>::iterator editedRow(std::vector<Row>& rows, const std::string& key, std::size_t index) {
    auto it = std::ranges::find(rows, key, &Row::name);
    if (it == rows.end() && index < rows.size()) {
      it = rows.begin() + static_cast<std::ptrdiff_t>(index);
    }
    return it;
  }

  // Drops rows[index]; false when that row is already gone.
  template <typename Row> bool eraseRow(std::vector<Row>& rows, std::size_t index) {
    if (index >= rows.size()) {
      return false;
    }
    rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
  }

} // namespace

void SettingsWindow::commitRowRemoval(std::vector<std::string> path, ConfigOverrideValue rows) {
  setSettingOverride(std::move(path), std::move(rows));
  if (m_editorSheetModal != nullptr) {
    m_editorSheetModal->close();
  }
  requestContentRebuild();
}

bool SettingsWindow::prepareEditorSheet() {
  if (m_wayland == nullptr
      || m_renderContext == nullptr
      || m_surface == nullptr
      || m_surface->xdgSurface() == nullptr
      || m_config == nullptr) {
    return false;
  }
  // Closing an open editor can commit its focused field through focus loss, so it goes first
  // and the new editor is built from the config that leaves.
  if (m_editorSheetModal != nullptr && m_editorSheetModal->isOpen()) {
    m_editorSheetModal->close();
  }
  if (m_widgetAddPopup != nullptr && m_widgetAddPopup->isOpen()) {
    m_widgetAddPopup->close();
  }
  if (m_searchPickerPopup != nullptr && m_searchPickerPopup->isOpen()) {
    m_searchPickerPopup->close();
  }
  if (m_editorSheetModal == nullptr) {
    m_editorSheetModal = std::make_unique<settings::SettingsSheetModal>();
    m_editorSheetModal->initialize(m_modalHost, [this]() { dismissOpenSelectDropdown(); });
  }
  return true;
}

settings::SettingsContentContext SettingsWindow::hostedEditorContext(const Config& cfg) {
  const BarConfig* selectedBar = settings::findBar(cfg, m_selectedBarName);
  const BarMonitorOverride* selectedMonitorOverride = nullptr;
  if (selectedBar != nullptr && !m_selectedMonitorOverride.empty()) {
    selectedMonitorOverride = settings::findMonitorOverride(*selectedBar, m_selectedMonitorOverride);
  }
  auto ctx = makeContentContext(cfg, selectedBar, selectedMonitorOverride);
  ctx.closeHostedEditor = [this]() {
    if (m_editorSheetModal != nullptr) {
      m_editorSheetModal->close();
    }
  };
  return ctx;
}

void SettingsWindow::openActionsMenu() {
  if (m_wayland == nullptr
      || m_renderContext == nullptr
      || m_surface == nullptr
      || m_actionsMenuButton == nullptr
      || m_surface->xdgSurface() == nullptr) {
    return;
  }

  if (m_actionsMenuPopup == nullptr) {
    m_actionsMenuPopup = std::make_unique<ContextMenuPopup>(*m_wayland, *m_renderContext);
    m_actionsMenuPopup->setOnActivate([this](const ContextMenuControlEntry& entry) {
      switch (entry.id) {
      case kActionSupportReport:
        if (m_actionsMenuPopup != nullptr) {
          m_actionsMenuPopup->close();
        }
        DeferredCall::callLater([this]() { saveSupportReport(); });
        break;
      case kActionExportConfig:
        if (m_actionsMenuPopup != nullptr) {
          m_actionsMenuPopup->close();
        }
        DeferredCall::callLater([this]() { openConfigExportDialog(); });
        break;
      default:
        break;
      }
    });
  } else if (m_actionsMenuPopup->isOpen()) {
    m_actionsMenuPopup->close();
    return;
  }

  std::vector<ContextMenuControlEntry> entries;
  entries.push_back(
      {.id = kActionSupportReport,
       .label = i18n::tr("settings.window.support-report"),
       .enabled = true,
       .separator = false,
       .hasSubmenu = false}
  );
  entries.push_back(
      {.id = kActionExportConfig,
       .label = i18n::tr("settings.window.export-config"),
       .enabled = true,
       .separator = false,
       .hasSubmenu = false}
  );

  float anchorAbsX = 0.0F;
  float anchorAbsY = 0.0F;
  Node::absolutePosition(m_actionsMenuButton, anchorAbsX, anchorAbsY);

  const float scale = uiScale();
  wl_output* output = m_wayland->lastPointerOutput();
  if (output == nullptr) {
    output = m_output;
  }

  if (m_config != nullptr) {
    m_actionsMenuPopup->setShadowConfig(m_config->config().shell.shadow);
  }
  m_actionsMenuPopup->open(
      ContextMenuPopupRequest{
          .entries = std::move(entries),
          .minMenuWidth = 220.0F * scale,
          .maxMenuWidth = Style::menuAutoMaxWidth * scale,
          .maxVisible = 8,
          .anchor =
              PopupAnchorRect{
                  .x = static_cast<std::int32_t>(anchorAbsX),
                  .y = static_cast<std::int32_t>(anchorAbsY),
                  .width = static_cast<std::int32_t>(m_actionsMenuButton->width()),
                  .height = static_cast<std::int32_t>(m_actionsMenuButton->height()),
              },
          .parent = PopupSurfaceParent{
              .xdgSurface = m_surface->xdgSurface(),
              .output = output,
          },
      }
  );
}

void SettingsWindow::openConfigExportDialog() {
  if (m_surface == nullptr || m_config == nullptr) {
    return;
  }

  if (m_configExportDialogModal == nullptr) {
    m_configExportDialogModal = std::make_unique<settings::ConfigExportDialogModal>();
    m_configExportDialogModal->initialize(m_modalHost, [this]() { dismissOpenSelectDropdown(); });
  }

  m_configExportDialogModal->open(
      settings::ConfigExportDialogRequest{
          .scale = uiScale(),
          .callback = [this](settings::ConfigExportMode mode) { saveConfigExport(mode); },
      }
  );
}

void SettingsWindow::openBarWidgetAddPopup(const std::vector<std::string>& lanePath) {
  if (m_wayland == nullptr
      || m_renderContext == nullptr
      || m_surface == nullptr
      || m_surface->xdgSurface() == nullptr
      || m_config == nullptr) {
    return;
  }

  if (m_searchPickerPopup != nullptr && m_searchPickerPopup->isOpen()) {
    m_searchPickerPopup->close();
  }
  if (m_editorSheetModal != nullptr && m_editorSheetModal->isOpen()) {
    m_editorSheetModal->close();
  }

  if (m_widgetAddPopup == nullptr) {
    m_widgetAddPopup = std::make_unique<settings::WidgetAddPopup>();
    m_widgetAddPopup->initialize(*m_wayland, *m_config, *m_renderContext);
    m_widgetAddPopup->setOnSelect([this](
                                      const std::vector<std::string>& selectedLanePath, const std::string& value,
                                      const std::string& newInstanceType, const std::string& newInstanceId,
                                      const std::vector<std::pair<std::string, std::string>>& initialSettings
                                  ) {
      if (value.empty() || m_config == nullptr) {
        return;
      }

      const Config& activeConfig = m_config->config();
      auto laneItems = settings::barWidgetItemsForPath(activeConfig, selectedLanePath);

      m_pendingDeleteWidgetName.clear();
      m_pendingDeleteWidgetSettingPath.clear();
      m_renamingWidgetName.clear();
      m_editingWidgetName.clear();

      if (!newInstanceType.empty() && !newInstanceId.empty()) {
        laneItems.push_back(newInstanceId);
        std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>> overrides = {
            {{"widget", newInstanceId, "type"}, newInstanceType},
        };
        for (const auto& [key, settingValue] : initialSettings) {
          overrides.push_back({{"widget", newInstanceId, key}, settingValue});
        }
        overrides.emplace_back(selectedLanePath, laneItems);
        setSettingOverrides(overrides);
        return;
      }

      laneItems.push_back(value);
      setSettingOverride(selectedLanePath, laneItems);
    });
  }

  wl_output* output = m_wayland->lastPointerOutput();
  if (output == nullptr) {
    output = m_output;
  }

  m_widgetAddPopup->open(
      settings::WidgetAddPopupRequest{
          .parent = popupParentFor(*m_surface, output, m_wayland->lastInputSerial()),
          .lanePath = lanePath,
          .config = m_config->config(),
          .scale = uiScale(),
      }
  );
}

void SettingsWindow::openSearchPickerPopup(settings::SearchPickerOpenRequest request) {
  if (m_wayland == nullptr
      || m_renderContext == nullptr
      || m_surface == nullptr
      || m_surface->xdgSurface() == nullptr
      || m_config == nullptr
      || request.options.empty()) {
    return;
  }

  if (m_searchPickerPopup == nullptr) {
    m_searchPickerPopup = std::make_unique<settings::SearchPickerPopup>();
    m_searchPickerPopup->initialize(*m_wayland, *m_config, *m_renderContext);
  }

  if (m_widgetAddPopup != nullptr && m_widgetAddPopup->isOpen()) {
    m_widgetAddPopup->close();
  }

  m_searchPickerPopup->setOnSelect([this, settingPath = request.settingPath, selectedValue = request.selectedValue,
                                    onSelect = request.onSelect](const std::string& value) {
    if (value == selectedValue) {
      return;
    }
    if (onSelect) {
      onSelect(value);
      return;
    }
    if (value.empty()) {
      clearSettingOverride(settingPath);
      return;
    }
    setSettingOverride(settingPath, value);
  });

  std::vector<SearchPickerOption> pickerOptions;
  pickerOptions.reserve(request.options.size());
  for (const auto& opt : request.options) {
    pickerOptions.push_back(
        SearchPickerOption{
            .value = opt.value,
            .label = opt.label,
            .description = opt.description,
            .enabled = true,
            .icon = {},
            .preview = opt.preview,
        }
    );
  }

  wl_output* output = m_wayland->lastPointerOutput();
  if (output == nullptr) {
    output = m_output;
  }

  XdgPopupParent parent = popupParentFor(*m_surface, output, m_wayland->lastInputSerial());

  m_searchPickerPopup->open(
      settings::SearchPickerPopupRequest{
          .parent = parent,
          .title = std::move(request.title),
          .options = std::move(pickerOptions),
          .selectedValue = std::move(request.selectedValue),
          .placeholder = std::move(request.placeholder),
          .emptyText = std::move(request.emptyText),
          .scale = uiScale(),
      }
  );
}

void SettingsWindow::openMonitorOverrideCreateDialog(std::optional<std::string> barName) {
  if (!prepareEditorSheet()) {
    return;
  }

  const Config& cfg = m_config->config();
  const BarConfig* bar = barName ? settings::findBar(cfg, *barName) : nullptr;
  if (barName && bar == nullptr) {
    return;
  }

  const float scale = uiScale();
  const std::vector<settings::SelectOption> outputs = availableOutputs();

  std::vector<std::string> existingMatches;
  if (bar != nullptr) {
    existingMatches.reserve(bar->monitorOverrides.size());
    for (const auto& monitorOverride : bar->monitorOverrides) {
      existingMatches.push_back(monitorOverride.match);
    }
  } else {
    existingMatches.reserve(cfg.dock.monitorOverrides.size());
    for (const auto& monitorOverride : cfg.dock.monitorOverrides) {
      existingMatches.push_back(monitorOverride.match);
    }
  }

  // Transient value of the pending match, shared between the segmented picker, the free-text input,
  // and the create action. Held here (not on SettingsWindow) so it lives and dies with this dialog.
  // Defaults to the first detected output so the common case (pick a connected monitor) is one click;
  // with no detected outputs it stays empty and the "Custom" free-text field takes over.
  auto matchState = std::make_shared<std::string>();
  if (!outputs.empty()) {
    *matchState = outputs.front().value;
  }

  auto populate = [this, scale, outputs, existingMatches, barName, matchState](Flex& body) {
    // Derive the selected segment and whether the free-text input is shown from the current value.
    // A detected connector match selects its segment and hides the input; anything else (an empty or
    // free-text value) selects the trailing "Custom" segment and shows the input.
    std::size_t selectedOutput = outputs.size();
    for (std::size_t i = 0; i < outputs.size(); ++i) {
      if (outputs[i].value == *matchState) {
        selectedOutput = i;
        break;
      }
    }
    const bool customSelected = outputs.empty() || selectedOutput == outputs.size();

    Input* inputPtr = nullptr;
    auto input = ui::input({
        .out = &inputPtr,
        .value = *matchState,
        .placeholder = i18n::tr("settings.entities.monitor-override.match-placeholder"),
        .fontSize = Style::fontSizeBody * scale,
        .controlHeight = Style::controlHeight * scale,
        .horizontalPadding = Style::spaceSm * scale,
        .width = 280.0F * scale,
        .height = Style::controlHeight * scale,
        .visible = customSelected ? std::nullopt : std::optional<bool>{false},
        .participatesInLayout = customSelected ? std::nullopt : std::optional<bool>{false},
    });
    inputPtr->setOnChange([this, matchState, inputPtr](const std::string& value) {
      *matchState = value;
      inputPtr->setInvalid(false);
      if (m_editorSheetModal != nullptr) {
        m_editorSheetModal->clearStatusMessage();
      }
    });

    auto doCreate = [this, existingMatches, barName, matchState, inputPtr]() {
      const std::string match = StringUtils::trim(*matchState);
      if (match.empty()) {
        inputPtr->setInvalid(true);
        return;
      }
      if (std::ranges::contains(existingMatches, match)) {
        inputPtr->setInvalid(true);
        if (m_editorSheetModal != nullptr) {
          m_editorSheetModal->setStatusMessage(i18n::tr("settings.entities.monitor-override.exists"), true);
        }
        return;
      }
      if (barName) {
        createMonitorOverride(*barName, match);
      } else {
        createDockMonitorOverride(match);
      }
      if (m_editorSheetModal != nullptr) {
        m_editorSheetModal->close();
      }
    };
    inputPtr->setOnSubmit([doCreate](const std::string& /*text*/) mutable { doCreate(); });

    if (!outputs.empty()) {
      // Wrapping, fill-width pill row (same idiom as the settings group-jump pills): one pill per
      // detected output plus a trailing "Custom" pill. A fill-width wrapping row breaks onto the next
      // line for many-monitor setups; a single Segmented cannot, since it keeps every segment on one
      // line (its surface background would trail past the last segment when given room to wrap).
      Flex* pickerRow = nullptr;
      body.addChild(
          ui::row({
              .out = &pickerRow,
              .align = FlexAlign::Center,
              .wrap = true,
              .gap = Style::spaceXs * scale,
              .fillWidth = true,
          })
      );

      const auto addPill = [&](std::string text, std::string tooltip, bool selected, std::function<void()> onSelect) {
        pickerRow->addChild(
            ui::button({
                .text = std::move(text),
                .fontSize = Style::fontSizeBody * scale,
                .variant = selected ? ButtonVariant::Primary : ButtonVariant::Default,
                .tooltip = tooltip.empty() ? std::nullopt : std::optional<std::string>{std::move(tooltip)},
                .minHeight = Style::controlHeight * scale,
                .paddingV = Style::spaceXs * scale,
                .paddingH = Style::spaceMd * scale,
                .radius = Style::scaledRadiusMd(scale),
                .onClick = std::move(onSelect),
            })
        );
      };

      for (std::size_t i = 0; i < outputs.size(); ++i) {
        // Connector name only on the pill (e.g. "DP-1"); the fuller "DP-1 (description)" label is the
        // tooltip and would make the pills too wide.
        addPill(
            outputs[i].value, outputs[i].label, !customSelected && i == selectedOutput,
            [this, matchState, value = outputs[i].value]() {
              *matchState = value;
              if (m_editorSheetModal != nullptr) {
                m_editorSheetModal->clearStatusMessage();
                m_editorSheetModal->rebuildBody();
              }
            }
        );
      }
      // Trailing "Custom": clear the match so the free-text input takes over. Rebuild re-derives the
      // selection and the input's visibility.
      addPill(i18n::tr("settings.entities.monitor-override.custom"), {}, customSelected, [this, matchState]() {
        matchState->clear();
        if (m_editorSheetModal != nullptr) {
          m_editorSheetModal->clearStatusMessage();
          m_editorSheetModal->rebuildBody();
        }
      });
    }

    body.addChild(std::move(input));
    body.addChild(
        ui::row(
            {
                .align = FlexAlign::Center,
                .justify = FlexJustify::End,
                .gap = Style::spaceSm * scale,
            },
            ui::button({
                .text = i18n::tr("common.actions.cancel"),
                .fontSize = Style::fontSizeBody * scale,
                .variant = ButtonVariant::Ghost,
                .minHeight = Style::controlHeight * scale,
                .paddingV = Style::spaceXs * scale,
                .paddingH = Style::spaceMd * scale,
                .radius = Style::scaledRadiusMd(scale),
                .onClick =
                    [this]() {
                      if (m_editorSheetModal != nullptr) {
                        m_editorSheetModal->close();
                      }
                    },
            }),
            ui::button({
                .text = i18n::tr("settings.entities.monitor-override.create"),
                .fontSize = Style::fontSizeBody * scale,
                .variant = ButtonVariant::Primary,
                .minHeight = Style::controlHeight * scale,
                .paddingV = Style::spaceXs * scale,
                .paddingH = Style::spaceMd * scale,
                .radius = Style::scaledRadiusMd(scale),
                .onClick = doCreate,
            })
        )
    );
  };

  m_editorSheetModal->open(
      settings::SettingsSheetRequest{
          .sheetTitle = i18n::tr("settings.entities.monitor-override.new-title"),
          .populateSheetBody = std::move(populate),
          .scale = scale,
      }
  );
}

void SettingsWindow::openSessionActionEntryEditor(std::size_t index) {
  if (!prepareEditorSheet()) {
    return;
  }

  const Config& cfg = m_config->config();
  if (index >= cfg.shell.session.actions.size()) {
    return;
  }

  const float scale = uiScale();

  auto rowState = std::make_shared<SessionPanelActionConfig>(cfg.shell.session.actions[index]);

  const auto persist = [this, rowState, index]() {
    if (m_config == nullptr) {
      return;
    }
    auto next = m_config->config().shell.session.actions;
    if (index >= next.size()) {
      return;
    }
    next[index] = *rowState;
    if (m_sessionActionsEditState != nullptr && index < m_sessionActionsEditState->size()) {
      (*m_sessionActionsEditState)[index] = *rowState;
    }
    syncSessionActionInlineSummary(index, *rowState);
    setSettingOverride({"shell", "session", "actions"}, next);
    if (m_editorSheetModal != nullptr && m_editorSheetModal->isOpen()) {
      m_editorSheetModal->setSheetTitle(sessionActionTitle(*rowState));
      m_editorSheetModal->requestLayout();
    }
  };

  const auto removeRow = [this, index]() {
    if (m_config == nullptr) {
      return;
    }
    auto next = m_config->config().shell.session.actions;
    if (eraseRow(next, index)) {
      commitRowRemoval({"shell", "session", "actions"}, next);
    }
  };

  auto ctx = hostedEditorContext(cfg);
  ctx.openSessionActionEntryEditor = {};
  ctx.openIdleBehaviorEntryEditor = {};

  const std::string sheetTitle = sessionActionTitle(*rowState);

  m_editorSheetModal->open(
      settings::SettingsSheetRequest{
          .sheetTitle = sheetTitle,
          .removeAction = removeRow,
          .populateSheetBody =
              [ctx, rowState, persist](Flex& body) mutable {
                settings::buildSessionActionEntryDetailContent(body, ctx, *rowState, persist);
              },
          .scale = scale,
      }
  );
}

void SettingsWindow::openIdleBehaviorEntryEditor(std::size_t index) {
  if (!prepareEditorSheet()) {
    return;
  }

  const Config& cfg = m_config->config();
  if (index >= cfg.idle.behaviors.size()) {
    return;
  }

  const float scale = uiScale();

  auto rowState = std::make_shared<IdleBehaviorConfig>(cfg.idle.behaviors[index]);
  auto rowKey = std::make_shared<std::string>(rowState->name);
  normalizeIdleBehaviorAction(*rowState);

  const auto persist = [this, rowState, rowKey, index]() {
    if (m_config == nullptr) {
      return;
    }
    normalizeIdleBehaviorAction(*rowState);
    auto next = m_config->config().idle.behaviors;
    const auto target = editedRow(next, *rowKey, index);
    if (target == next.end()) {
      return;
    }
    const auto targetIndex = static_cast<std::size_t>(std::distance(next.begin(), target));
    next[targetIndex] = *rowState;
    settings::normalizeIdleBehaviorNames(next);
    *rowState = next[targetIndex];
    *rowKey = rowState->name;
    setSettingOverride({"idle", "behavior"}, next);
    requestContentRebuild();
    if (m_editorSheetModal != nullptr && m_editorSheetModal->isOpen()) {
      m_editorSheetModal->requestLayout();
    }
  };

  const auto removeRow = [this, index]() {
    if (m_config == nullptr) {
      return;
    }
    auto next = m_config->config().idle.behaviors;
    if (eraseRow(next, index)) {
      settings::normalizeIdleBehaviorNames(next);
      commitRowRemoval({"idle", "behavior"}, next);
    }
  };

  auto ctx = hostedEditorContext(cfg);
  ctx.openSessionActionEntryEditor = {};
  ctx.openIdleBehaviorEntryEditor = {};

  m_editorSheetModal->open(
      settings::SettingsSheetRequest{
          .sheetTitle = idleBehaviorTitle(*rowState),
          .removeAction = removeRow,
          .populateSheetBody =
              [ctx, rowState, persist](Flex& body) mutable {
                settings::buildIdleBehaviorEntryDetailContent(body, ctx, *rowState, persist);
              },
          .scale = scale,
      }
  );
}

void SettingsWindow::openIdleBehaviorCreateEditor() {
  if (!prepareEditorSheet()) {
    return;
  }

  const Config& cfg = m_config->config();
  const float scale = uiScale();

  auto rowState = std::make_shared<IdleBehaviorConfig>(IdleBehaviorConfig{
      .name = "idle-behavior",
      .enabled = false,
      .timeoutSeconds = 600,
      .action = "command",
      .command = "",
      .resumeCommand = "",
  });

  const auto persistDraft = [this]() {
    if (m_editorSheetModal != nullptr && m_editorSheetModal->isOpen()) {
      m_editorSheetModal->requestLayout();
    }
  };

  auto ctx = hostedEditorContext(cfg);
  ctx.openSessionActionEntryEditor = {};
  ctx.openIdleBehaviorEntryEditor = {};
  ctx.afterIdleBehaviorApply = [this, rowState]() {
    if (m_config == nullptr) {
      return;
    }
    normalizeIdleBehaviorAction(*rowState);
    auto next = m_config->config().idle.behaviors;
    next.push_back(*rowState);
    settings::normalizeIdleBehaviorNames(next);
    setSettingOverride({"idle", "behavior"}, next);
    requestContentRebuild();
  };

  m_editorSheetModal->open(
      settings::SettingsSheetRequest{
          .sheetTitle = idleBehaviorTitle(*rowState),
          .removeAction = nullptr,
          .populateSheetBody =
              [ctx, rowState, persistDraft](Flex& body) mutable {
                settings::buildIdleBehaviorEntryDetailContent(body, ctx, *rowState, persistDraft);
              },
          .scale = scale,
      }
  );
}

void SettingsWindow::openNotificationFilterEntryEditor(std::size_t index) {
  if (!prepareEditorSheet()) {
    return;
  }

  const Config& cfg = m_config->config();
  if (index >= cfg.notification.filters.size()) {
    return;
  }

  const float scale = uiScale();

  auto rowState = std::make_shared<NotificationFilterConfig>(cfg.notification.filters[index]);
  auto rowKey = std::make_shared<std::string>(rowState->name);

  const auto persist = [this, rowState, rowKey, index]() {
    if (m_config == nullptr) {
      return;
    }
    auto next = m_config->config().notification.filters;
    const auto target = editedRow(next, *rowKey, index);
    if (target == next.end()) {
      return;
    }
    const auto targetIndex = static_cast<std::size_t>(std::distance(next.begin(), target));
    next[targetIndex] = *rowState;
    normalizeNotificationFilterNames(next);
    *rowState = next[targetIndex];
    *rowKey = rowState->name;
    setSettingOverride({"notification", "filter"}, next);
    requestContentRebuild();
    if (m_editorSheetModal != nullptr && m_editorSheetModal->isOpen()) {
      m_editorSheetModal->requestLayout();
    }
  };

  const auto removeRow = [this, index]() {
    if (m_config == nullptr) {
      return;
    }
    auto next = m_config->config().notification.filters;
    if (eraseRow(next, index)) {
      normalizeNotificationFilterNames(next);
      commitRowRemoval({"notification", "filter"}, next);
    }
  };

  auto ctx = hostedEditorContext(cfg);
  ctx.openNotificationFilterEntryEditor = {};
  ctx.afterNotificationFilterApply = [persist]() { persist(); };

  m_editorSheetModal->open(
      settings::SettingsSheetRequest{
          .sheetTitle = notificationFilterTitle(*rowState),
          .removeAction = removeRow,
          .populateSheetBody =
              [ctx, rowState, persist](Flex& body) mutable {
                settings::buildNotificationFilterEntryDetailContent(body, ctx, *rowState, persist);
              },
          .scale = scale,
      }
  );
}

void SettingsWindow::openNotificationFilterCreateEditor() {
  if (!prepareEditorSheet()) {
    return;
  }

  const Config& cfg = m_config->config();
  const float scale = uiScale();

  auto rowState = std::make_shared<NotificationFilterConfig>(NotificationFilterConfig{
      .name = "filter",
      .enabled = true,
      .match = {},
      .showToast = true,
      .saveHistory = true,
      .playSound = true,
      .bypassDnd = false,
      .allowPermanent = true,
      .allowedUrgencies = {},
  });

  const auto persistDraft = [this]() {
    if (m_editorSheetModal != nullptr && m_editorSheetModal->isOpen()) {
      m_editorSheetModal->requestLayout();
    }
  };

  auto ctx = hostedEditorContext(cfg);
  ctx.openNotificationFilterEntryEditor = {};
  ctx.afterNotificationFilterApply = [this, rowState]() {
    if (m_config == nullptr || (rowState->match.empty() && rowState->matchContent.empty())) {
      return;
    }
    auto next = m_config->config().notification.filters;
    next.push_back(*rowState);
    normalizeNotificationFilterNames(next);
    setSettingOverride({"notification", "filter"}, next);
    requestContentRebuild();
  };

  m_editorSheetModal->open(
      settings::SettingsSheetRequest{
          .sheetTitle = i18n::tr("settings.notifications.filter.add-title"),
          .removeAction = nullptr,
          .populateSheetBody =
              [ctx, rowState, persistDraft](Flex& body) mutable {
                settings::buildNotificationFilterEntryDetailContent(body, ctx, *rowState, persistDraft);
              },
          .scale = scale,
      }
  );
}

void SettingsWindow::openCalendarAccountEditor(std::optional<std::string> accountId) {
  if (!prepareEditorSheet()) {
    return;
  }

  const Config& cfg = m_config->config();
  auto draft = std::make_shared<CalendarAccountDraft>();
  if (accountId.has_value()) {
    const CalendarConfig::Account* account = findCalendarAccount(cfg, *accountId);
    if (account == nullptr
        || (account->type != "caldav" && account->type != "ics" && account->type != "vdir")) {
      return;
    }
    draft->creating = false;
    draft->id = account->id;
    draft->name = account->displayName;
    draft->username = account->username;
    draft->serverUrl = account->serverUrl;
    draft->credentialSource = account->credentialSource;
    draft->passwordFile = account->passwordFile;
    draft->color = account->color;
    draft->calendars = account->calendars;
    if (account->type == "ics") {
      draft->provider = CalendarAccountProvider::IcsFileURL;
    } else if (account->type == "vdir") {
      draft->provider = CalendarAccountProvider::Vdir;
      draft->path = account->path;
      const std::string rawDiscovery =
          m_config->stateString(kCalendarDiscoveryOwner, account->id + "_calendars").value_or(std::string{});
      draft->discoveredCalendars = calendar::parseCalendarSources(rawDiscovery);
    } else {
      draft->provider =
          account->provider == "custom" ? CalendarAccountProvider::CustomCalDav : CalendarAccountProvider::ICloud;
      const std::string rawDiscovery =
          m_config->stateString(kCalendarDiscoveryOwner, account->id + "_calendars").value_or(std::string{});
      draft->discoveredCalendars = calendar::parseCalendarSources(rawDiscovery);
    }
  }

  const float scale = uiScale();
  const std::string title = draft->creating ? i18n::tr("settings.calendar-accounts.add-title")
                                            : i18n::tr("settings.calendar-accounts.edit-title");

  std::function<void()> removeAccount;
  if (!draft->creating && m_config->isOverrideOnlyCalendarAccount(draft->id)) {
    removeAccount = [this, draft, accountId = draft->id]() {
      if (m_config == nullptr || m_calendarService == nullptr) {
        return;
      }
      if (draft->credentialOperationInFlight) {
        return;
      }
      draft->credentialOperationInFlight = true;
      m_calendarService->deleteAccount(
          accountId, [this, accountId]() { return m_config->deleteCalendarAccountOverride(accountId); },
          [this, draft, accountId](CalendarService::CredentialOperationResult result) {
            draft->credentialOperationInFlight = false;
            if (result != CalendarService::CredentialOperationResult::Success) {
              const std::string message = result == CalendarService::CredentialOperationResult::ConfigError
                  ? i18n::tr("settings.calendar-accounts.delete-error")
                  : calendarCredentialError(result);
              markSettingsWriteError(message);
              return;
            }
            (void)m_config->setStateString(kCalendarDiscoveryOwner, accountId + "_calendars", "");
            markSettingsWriteSuccess(true);
            if (m_editorSheetModal != nullptr) {
              m_editorSheetModal->close();
            }
          }
      );
    };
  }

  auto populateSheetBody = [this, draft, scale](Flex& body) mutable {
    if (m_config != nullptr && m_config->config().shell.offlineMode) {
      body.addChild(
          settings::makeOfflineModeNotice(scale, i18n::tr("settings.window.offline-mode-notice.calendar-account"))
      );
    }

    auto addField = [scale](Flex& parent, const std::string& label, std::unique_ptr<Node> control) {
      auto field = ui::column({
          .align = FlexAlign::Stretch,
          .gap = Style::spaceXs * scale,
      });
      field->addChild(
          ui::label({
              .text = label,
              .fontSize = Style::fontSizeCaption * scale,
              .fontWeight = FontWeight::Medium,
              .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
          })
      );
      field->addChild(std::move(control));
      parent.addChild(std::move(field));
    };

    const auto providerIndex = [](CalendarAccountProvider provider) -> std::size_t {
      switch (provider) {
      case CalendarAccountProvider::ICloud:
        return 0;
      case CalendarAccountProvider::CustomCalDav:
        return 1;
      case CalendarAccountProvider::IcsFileURL:
        return 2;
      case CalendarAccountProvider::Vdir:
        return 3;
      }
      return 0;
    };
    addField(
        body, i18n::tr("settings.calendar-accounts.provider-label"),
        ui::segmented({
            .options =
                std::vector<ui::SegmentedOption>{
                    {.label = calendarProviderTitle(CalendarAccountProvider::ICloud), .glyph = "brand-apple"},
                    {.label = calendarProviderTitle(CalendarAccountProvider::CustomCalDav), .glyph = "calendar-cog"},
                    {.label = calendarProviderTitle(CalendarAccountProvider::IcsFileURL), .glyph = "link"},
                    {.label = calendarProviderTitle(CalendarAccountProvider::Vdir), .glyph = "folder"}
                },
            .selectedIndex = providerIndex(draft->provider),
            .scale = scale,
            .enabled = draft->creating,
            .equalSegmentWidths = true,
            .onChange = [this, draft](std::size_t index) {
              CalendarAccountProvider provider = CalendarAccountProvider::ICloud;
              if (index == 1) {
                provider = CalendarAccountProvider::CustomCalDav;
              } else if (index == 2) {
                provider = CalendarAccountProvider::IcsFileURL;
              } else if (index == 3) {
                provider = CalendarAccountProvider::Vdir;
              }

              draft->provider = provider;
              if (provider == CalendarAccountProvider::Vdir) {
                draft->credentialSource = CalendarCredentialSource::SecretService;
                draft->passwordFile.clear();
              }
              bool isDefaultId = draft->id.empty()
                  || draft->id == "personal_icloud"
                  || draft->id == "home_nextcloud"
                  || draft->id == "subscription"
                  || draft->id == "local_calendar";
              if (provider == CalendarAccountProvider::CustomCalDav && isDefaultId) {
                draft->id = "home_nextcloud";
              } else if (provider == CalendarAccountProvider::ICloud && isDefaultId) {
                draft->id = "personal_icloud";
              } else if (provider == CalendarAccountProvider::IcsFileURL && isDefaultId) {
                draft->id = "subscription";
              } else if (provider == CalendarAccountProvider::Vdir && isDefaultId) {
                draft->id = "local_calendar";
              }
              if (m_editorSheetModal != nullptr) {
                m_editorSheetModal->rebuildBody();
              }
            },
        })
    );

    Input* idInput = nullptr;
    addField(
        body, i18n::tr("settings.calendar-accounts.id-label"),
        ui::input({
            .out = &idInput,
            .value = draft->id,
            .placeholder = "personal_icloud",
            .invalid = draft->idInvalid,
            .enabled = draft->creating,
            .onChange = [draft](const std::string& value) {
              if (draft->creating) {
                draft->id = value;
              }
              draft->idInvalid = false;
            },
        })
    );

    Input* nameInput = nullptr;
    addField(
        body, i18n::tr("settings.calendar-accounts.name-label"),
        ui::input({
            .out = &nameInput,
            .value = draft->name,
            .placeholder = i18n::tr("settings.calendar-accounts.name-placeholder"),
            .onChange = [draft](const std::string& value) { draft->name = value; },
        })
    );

    Input* usernameInput = nullptr;
    Input* passwordInput = nullptr;
    Input* passwordFileInput = nullptr;
    Input* serverInput = nullptr;
    Input* pathInput = nullptr;
    if (draft->provider != CalendarAccountProvider::IcsFileURL && draft->provider != CalendarAccountProvider::Vdir) {
      addField(
          body, i18n::tr("settings.calendar-accounts.credential-source-label"),
          ui::segmented({
              .options =
                  std::vector<ui::SegmentedOption>{
                      {.label = i18n::tr("settings.calendar-accounts.credential-source-keyring"), .glyph = "key"},
                      {.label = i18n::tr("settings.calendar-accounts.credential-source-file"), .glyph = "file-lock"},
                  },
              .selectedIndex = draft->credentialSource == CalendarCredentialSource::File ? 1U : 0U,
              .scale = scale,
              .enabled = draft->creating,
              .equalSegmentWidths = true,
              .onChange = [this, draft](std::size_t index) {
                draft->credentialSource =
                    index == 1 ? CalendarCredentialSource::File : CalendarCredentialSource::SecretService;
                draft->password.clear();
                draft->passwordFile.clear();
                draft->passwordInvalid = false;
                draft->passwordFileInvalid = false;
                if (m_editorSheetModal != nullptr) {
                  m_editorSheetModal->rebuildBody();
                }
              },
          })
      );
      addField(
          body, i18n::tr("settings.calendar-accounts.username-label"),
          ui::input({
              .out = &usernameInput,
              .value = draft->username,
              .placeholder = i18n::tr("settings.calendar-accounts.username-placeholder"),
              .invalid = draft->usernameInvalid,
              .onChange = [draft](const std::string& value) {
                draft->username = value;
                draft->usernameInvalid = false;
              },
          })
      );
      if (draft->credentialSource == CalendarCredentialSource::SecretService) {
        addField(
            body, i18n::tr("settings.calendar-accounts.password-label"),
            ui::input({
                .out = &passwordInput,
                .value = {},
                .placeholder = draft->creating ? i18n::tr("settings.calendar-accounts.password-placeholder")
                                               : i18n::tr("settings.calendar-accounts.password-keep-placeholder"),
                .passwordMode = true,
                .invalid = draft->passwordInvalid,
                .onChange = [draft](const std::string& value) {
                  draft->password = value;
                  draft->passwordInvalid = false;
                },
            })
        );
      } else {
        addField(
            body, i18n::tr("settings.calendar-accounts.password-file-label"),
            ui::input({
                .out = &passwordFileInput,
                .value = draft->passwordFile,
                .placeholder = "/run/agenix/noctalia-caldav",
                .invalid = draft->passwordFileInvalid,
                .enabled = draft->creating,
                .onChange = [draft](const std::string& value) {
                  draft->passwordFile = value;
                  draft->passwordFileInvalid = false;
                },
            })
        );
      }
    }
    if (draft->provider == CalendarAccountProvider::CustomCalDav) {
      addField(
          body, i18n::tr("settings.calendar-accounts.server-url-label"),
          ui::input({
              .out = &serverInput,
              .value = draft->serverUrl,
              .placeholder = "https://cloud.example.com/remote.php/dav/",
              .invalid = draft->serverUrlInvalid,
              .onChange = [draft](const std::string& value) {
                draft->serverUrl = value;
                draft->serverUrlInvalid = false;
              },
          })
      );
    }
    if (draft->provider == CalendarAccountProvider::IcsFileURL) {
      addField(
          body, i18n::tr("settings.calendar-accounts.ics-url-label"),
          ui::input({
              .out = &serverInput,
              .value = draft->serverUrl,
              .placeholder = "https://example.com/calendar.ics",
              .invalid = draft->serverUrlInvalid,
              .onChange = [draft](const std::string& value) {
                draft->serverUrl = value;
                draft->serverUrlInvalid = false;
              },
          })
      );
    }
    if (draft->provider == CalendarAccountProvider::Vdir) {
      addField(
          body, i18n::tr("settings.calendar-accounts.vdir-path-label"),
          ui::input({
              .out = &pathInput,
              .value = draft->path,
              .placeholder = "~/.local/share/calendars",
              .invalid = draft->pathInvalid,
              .onChange = [draft](const std::string& value) {
                draft->path = value;
                draft->pathInvalid = false;
              },
          })
      );
    }

    addField(
        body, i18n::tr("settings.calendar-accounts.color-label"),
        settings::makeColorSpecSelect(
            settings::ColorSpecSelectOptions{
                .roles = {},
                .selectedValue = draft->color,
                .allowNone = true,
                .allowCustomColor = true,
                .noneLabel = {},
                .fontSize = Style::fontSizeBody * scale,
                .controlHeight = Style::controlHeight * scale,
                .glyphSize = Style::fontSizeBody * scale,
                .flexGrow = true,
            },
            [draft](std::string value) { draft->color = StringUtils::trim(value); }, [draft]() { draft->color.clear(); }
        )
    );

    if (!draft->creating && !draft->discoveredCalendars.empty()) {
      auto calendars = ui::column({
          .align = FlexAlign::Stretch,
          .gap = Style::spaceXs * scale,
      });
      calendars->addChild(
          ui::label({
              .text = i18n::tr("settings.calendar-accounts.calendars-label"),
              .fontSize = Style::fontSizeCaption * scale,
              .fontWeight = FontWeight::Medium,
              .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
          })
      );

      auto list = ui::column({
          .align = FlexAlign::Stretch,
          .gap = Style::spaceXs * scale,
          .padding = Style::spaceSm * scale,
          .fill = colorSpecFromRole(ColorRole::SurfaceVariant, 0.35F),
          .radius = Style::scaledRadiusMd(scale),
      });
      for (const CalendarSource& source : draft->discoveredCalendars) {
        const bool checked = calendarSourceChecked(*draft, source);
        auto row = ui::row({
            .align = FlexAlign::Center,
            .gap = Style::spaceSm * scale,
            .fillWidth = true,
        });
        auto info = ui::column({
            .align = FlexAlign::Start,
            .gap = 2.0F * scale,
            .flexGrow = 1.0F,
        });
        info->addChild(
            ui::label({
                .text = source.name.empty() ? source.id : source.name,
                .fontSize = Style::fontSizeBody * scale,
                .fontWeight = FontWeight::Medium,
                .color = colorSpecFromRole(ColorRole::OnSurface),
                .maxLines = 1,
                .ellipsize = TextEllipsize::End,
            })
        );
        if (!source.name.empty()) {
          info->addChild(
              ui::label({
                  .text = source.id,
                  .fontSize = Style::fontSizeCaption * scale,
                  .color = colorSpecFromRole(ColorRole::OnSurfaceVariant),
                  .maxLines = 1,
                  .ellipsize = TextEllipsize::End,
              })
          );
        }
        row->addChild(std::move(info));
        row->addChild(
            ui::toggle({
                .checked = checked,
                .scale = scale,
                .onChange = [this, draft, sourceId = source.id](bool on) {
                  draft->calendars =
                      calendar::setCalendarSourceChecked(draft->discoveredCalendars, draft->calendars, sourceId, on);
                  if (m_editorSheetModal != nullptr) {
                    m_editorSheetModal->rebuildBody();
                  }
                },
            })
        );
        list->addChild(std::move(row));
      }
      calendars->addChild(std::move(list));
      body.addChild(std::move(calendars));
    }

    const auto persistAccount = [this, draft, idInput, nameInput, usernameInput, passwordInput, passwordFileInput,
                                 serverInput, pathInput](bool closeAfter) {
      if (m_config == nullptr) {
        return;
      }
      if (draft->credentialOperationInFlight) {
        return;
      }

      draft->id = draft->creating ? trimInput(idInput) : draft->id;
      draft->name = trimInput(nameInput);
      draft->color = StringUtils::trim(draft->color);
      draft->username = trimInput(usernameInput);
      draft->password = trimInput(passwordInput);
      if (passwordFileInput != nullptr) {
        draft->passwordFile = trimInput(passwordFileInput);
      }
      draft->serverUrl = trimInput(serverInput);
      if (pathInput != nullptr) {
        draft->path = trimInput(pathInput);
      }

      draft->idInvalid = false;
      draft->usernameInvalid = false;
      draft->passwordInvalid = false;
      draft->passwordFileInvalid = false;
      draft->serverUrlInvalid = false;
      draft->pathInvalid = false;

      if (!validCalendarAccountId(draft->id)) {
        draft->idInvalid = true;
      }
      if (draft->creating && calendarAccountIdExists(m_config->config(), draft->id)) {
        draft->idInvalid = true;
      }

      const bool caldav = draft->provider == CalendarAccountProvider::ICloud
          || draft->provider == CalendarAccountProvider::CustomCalDav;
      const bool ics = draft->provider == CalendarAccountProvider::IcsFileURL;
      const bool vdir = draft->provider == CalendarAccountProvider::Vdir;
      if (caldav && draft->username.empty()) {
        draft->usernameInvalid = true;
      }
      if ((draft->provider == CalendarAccountProvider::CustomCalDav || ics) && draft->serverUrl.empty()) {
        draft->serverUrlInvalid = true;
      }
      if (caldav
          && draft->credentialSource == CalendarCredentialSource::File
          && (draft->passwordFile.empty() || !std::filesystem::path(draft->passwordFile).is_absolute())) {
        draft->passwordFileInvalid = true;
      }
      if (vdir) {
        const std::filesystem::path checkPath =
            draft->path.empty() ? calendar::defaultVdirPath() : FileUtils::expandUserPath(draft->path);
        std::error_code ec;
        if (!std::filesystem::exists(checkPath, ec) || !std::filesystem::is_directory(checkPath, ec)) {
          draft->pathInvalid = true;
        }
      }
      if (draft->idInvalid
          || draft->usernameInvalid
          || draft->passwordInvalid
          || draft->passwordFileInvalid
          || draft->serverUrlInvalid
          || draft->pathInvalid) {
        showTransientStatus(i18n::tr("settings.calendar-accounts.invalid"), true);
        return;
      }

      std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>> overrides;
      if (draft->creating) {
        overrides.push_back({{"calendar", "enabled"}, true});
      }
      const std::vector<std::string> base = {"calendar", "account", draft->id};

      std::string type = "caldav";
      if (ics) {
        type = "ics";
      } else if (vdir) {
        type = "vdir";
      }

      overrides.push_back({{base[0], base[1], base[2], "type"}, type});
      overrides.push_back({{base[0], base[1], base[2], "name"}, draft->name});
      overrides.push_back({{base[0], base[1], base[2], "color"}, draft->color});
      // Manual calendar selection is currently populated by CalDAV discovery.
      overrides.push_back({{base[0], base[1], base[2], "calendars"}, draft->calendars});
      if (caldav) {
        overrides.push_back({{base[0], base[1], base[2], "provider"}, calendarProviderKey(draft->provider)});
        overrides.push_back({{base[0], base[1], base[2], "username"}, draft->username});
        if (draft->creating) {
          overrides.push_back(
              {{base[0], base[1], base[2], "credential_source"},
               draft->credentialSource == CalendarCredentialSource::File ? std::string("file")
                                                                         : std::string("secret-service")}
          );
          overrides.push_back({{base[0], base[1], base[2], "password_file"}, draft->passwordFile});
        }
      }
      if (draft->provider == CalendarAccountProvider::CustomCalDav || ics) {
        overrides.push_back({{base[0], base[1], base[2], "server_url"}, draft->serverUrl});
      }
      if (vdir) {
        overrides.push_back({{base[0], base[1], base[2], "path"}, draft->path});
      }

      // Keys the target provider does not own. They ship in the same commit as the new values: a
      // separate clear would publish an account that is neither the old shape nor the new one.
      std::vector<std::vector<std::string>> staleKeys;
      if (!draft->creating) {
        if (!caldav) {
          staleKeys.push_back({base[0], base[1], base[2], "provider"});
          staleKeys.push_back({base[0], base[1], base[2], "username"});
          staleKeys.push_back({base[0], base[1], base[2], "credential_source"});
          staleKeys.push_back({base[0], base[1], base[2], "password_file"});
        }
        if (draft->provider != CalendarAccountProvider::CustomCalDav && !ics) {
          staleKeys.push_back({base[0], base[1], base[2], "server_url"});
        }
        if (!vdir) {
          staleKeys.push_back({base[0], base[1], base[2], "path"});
        }
      }

      if (!caldav) {
        if (!m_config->mutateOverrides(overrides, staleKeys, nullptr)) {
          markSettingsWriteError(i18n::tr("settings.calendar-accounts.save-error"));
          return;
        }
        markSettingsWriteSuccess(closeAfter);
        if (closeAfter && m_editorSheetModal != nullptr) {
          m_editorSheetModal->close();
        }
        return;
      }

      if (m_calendarService == nullptr) {
        markSettingsWriteError(i18n::tr("settings.calendar-accounts.secret-service-error"));
        return;
      }
      std::string password = std::move(draft->password);
      draft->password.clear();
      if (passwordInput != nullptr) {
        passwordInput->setValue("");
      }
      draft->credentialOperationInFlight = true;
      m_calendarService->saveCalDavAccount(
          draft->id, draft->credentialSource, draft->passwordFile, std::move(password),
          [this, overrides = std::move(overrides), staleKeys = std::move(staleKeys)]() mutable {
            return m_config->mutateOverrides(overrides, staleKeys, nullptr);
          },
          [this, draft, closeAfter](CalendarService::CredentialOperationResult result) {
            draft->credentialOperationInFlight = false;
            if (result == CalendarService::CredentialOperationResult::MissingCredential) {
              draft->passwordInvalid = true;
              showTransientStatus(i18n::tr("settings.calendar-accounts.invalid"), true);
              if (m_editorSheetModal != nullptr) {
                m_editorSheetModal->rebuildBody();
              }
              return;
            }
            if (result != CalendarService::CredentialOperationResult::Success) {
              markSettingsWriteError(calendarCredentialError(result));
              return;
            }
            m_calendarService->requestRefresh();
            markSettingsWriteSuccess(closeAfter);
            if (closeAfter && m_editorSheetModal != nullptr) {
              m_editorSheetModal->close();
            }
          }
      );
    };

    auto actions = ui::row({
        .align = FlexAlign::Center,
        .justify = FlexJustify::End,
        .gap = Style::spaceSm * scale,
    });
    actions->addChild(
        ui::button({
            .text = i18n::tr("common.actions.cancel"),
            .variant = ButtonVariant::Secondary,
            .minHeight = Style::controlHeight * scale,
            .paddingH = Style::spaceMd * scale,
            .radius = Style::scaledRadiusMd(scale),
            .onClick = [this]() {
              if (m_editorSheetModal != nullptr) {
                m_editorSheetModal->close();
              }
            },
        })
    );
    actions->addChild(
        ui::button({
            .text = i18n::tr("settings.calendar-accounts.save"),
            .glyph = "device-floppy",
            .variant = ButtonVariant::Primary,
            .minHeight = Style::controlHeight * scale,
            .paddingH = Style::spaceMd * scale,
            .radius = Style::scaledRadiusMd(scale),
            .onClick = [persistAccount]() { persistAccount(true); },
        })
    );
    body.addChild(std::move(actions));
  };

  m_editorSheetModal->open(
      settings::SettingsSheetRequest{
          .sheetTitle = title,
          .removeAction = removeAccount,
          .populateSheetBody = std::move(populateSheetBody),
          .scale = scale,
      }
  );
}

void SettingsWindow::openBarWidgetEditorSheet(
    std::string title, std::function<void(Flex&)> populate, std::function<void()> removeAction
) {
  if (!prepareEditorSheet()) {
    return;
  }

  const Config& cfg = m_config->config();
  const float scale = uiScale();

  auto sctx = hostedEditorContext(cfg);
  // In the sheet, "rebuild" means re-run the body in place; "close" tears the sheet down.
  sctx.requestRebuild = [this]() {
    if (m_editorSheetModal != nullptr) {
      m_editorSheetModal->rebuildBody();
    }
  };
  sctx.closeHostedEditor = [this]() { DeferredCall::callLater([this]() { closeWidgetInspectorPopup(); }); };
  // A rename changes the edited widget's id: apply it, then retitle and rebuild the sheet.
  sctx.renameWidgetInstance =
      [this](
          std::string oldName, std::string newName,
          std::vector<std::pair<std::vector<std::string>, ConfigOverrideValue>> referenceOverrides
      ) {
        std::string updatedTitle = newName;
        renameWidgetInstance(std::move(oldName), std::move(newName), std::move(referenceOverrides));
        if (m_config != nullptr) {
          const std::string display = settings::widgetReferenceInfo(m_config->config(), updatedTitle).title;
          if (!display.empty()) {
            updatedTitle = display;
          }
        }
        if (m_editorSheetModal != nullptr) {
          m_editorSheetModal->setSheetTitle(updatedTitle);
          m_editorSheetModal->rebuildBody();
        }
      };

  m_editorSheetFactory = std::make_unique<settings::SettingsControlFactory>(sctx);

  m_editorSheetModal->open(
      settings::SettingsSheetRequest{
          .sheetTitle = std::move(title),
          .removeAction = std::move(removeAction),
          .populateSheetBody = std::move(populate),
          .scale = scale,
      }
  );
}

void SettingsWindow::openWidgetInspectorEditor(std::vector<std::string> laneListPath, std::string widgetName) {
  DeferredCall::callLater([this, laneListPath = std::move(laneListPath), widgetName = std::move(widgetName)]() mutable {
    m_editingWidgetName = widgetName;
    m_editingCapsuleGroupId.clear();
    m_renamingWidgetName.clear();
    m_pendingDeleteWidgetName.clear();
    m_pendingDeleteWidgetSettingPath.clear();
    m_editorSheetListPath = std::move(laneListPath);
    std::string title = widgetName;
    if (m_config != nullptr) {
      const std::string display = settings::widgetReferenceInfo(m_config->config(), widgetName).title;
      if (!display.empty()) {
        title = display;
      }
    }
    openBarWidgetEditorSheet(std::move(title), [this](Flex& body) {
      if (m_editorSheetFactory == nullptr) {
        return;
      }
      auto ctx = settings::makeBarWidgetEditorContext(*m_editorSheetFactory);
      settings::buildWidgetInspectorBody(body, m_editorSheetListPath, ctx);
    });
  });
}

void SettingsWindow::openCapsuleGroupEditor(std::vector<std::string> laneListPath, std::string groupId) {
  DeferredCall::callLater([this, laneListPath = std::move(laneListPath), groupId = std::move(groupId)]() mutable {
    m_editingCapsuleGroupId = std::move(groupId);
    m_editingWidgetName.clear();
    m_renamingWidgetName.clear();
    m_pendingDeleteWidgetName.clear();
    m_pendingDeleteWidgetSettingPath.clear();
    m_editorSheetListPath = std::move(laneListPath);
    openBarWidgetEditorSheet(i18n::tr("settings.entities.widget.group.title"), [this](Flex& body) {
      if (m_editorSheetFactory == nullptr) {
        return;
      }
      auto ctx = settings::makeBarWidgetEditorContext(*m_editorSheetFactory);
      settings::buildCapsuleGroupBody(body, m_editorSheetListPath, ctx);
    });
  });
}

void SettingsWindow::openCommunityTemplateStore() {
  if (!prepareEditorSheet()) {
    return;
  }

  const float scale = uiScale();
  auto catalog = noctalia::theme::CommunityTemplateService::availableTemplates();
  std::unordered_set<std::string> selectedIds(
      m_config->config().theme.templates.communityIds.begin(), m_config->config().theme.templates.communityIds.end()
  );

  auto storeContent = std::make_shared<settings::TemplateStoreContent>(
      std::move(catalog), std::move(selectedIds), m_config,
      settings::TemplateStoreCallbacks{
          .setSelected = [this](
                             std::vector<std::string> ids
                         ) { setSettingOverride({"theme", "templates", "community_ids"}, std::move(ids)); },
          .scale = scale,
      }
  );

  storeContent->setOnRebuildNeeded([this]() {
    if (m_editorSheetModal != nullptr) {
      m_editorSheetModal->rebuildBody();
    }
  });

  m_editorSheetModal->open(
      settings::SettingsSheetRequest{
          .sheetTitle = i18n::tr("settings.templates.store.title"),
          .removeAction = nullptr,
          .createHeaderAction = nullptr,
          .populateSheetBody =
              [storeContent, this, scale](Flex& body) {
                if (m_renderContext == nullptr) {
                  return;
                }
                if (m_config != nullptr && m_config->config().shell.offlineMode) {
                  body.addChild(
                      settings::makeOfflineModeNotice(
                          scale, i18n::tr("settings.window.offline-mode-notice.template-store")
                      )
                  );
                }
                storeContent->populateBody(body, m_surface->renderTarget().renderer());
              },
          .scale = scale,
          .minWidth = 720.0F,
          .maxWidth = 1000.0F,
          .parentFraction = 0.85F,
          .fillParentHeight = true,
          .scrollableBody = false,
          .preDispatchKeyboard =
              [storeContent, this](const KeyboardEvent& event) {
                InputArea* focused = m_editorSheetModal != nullptr ? m_editorSheetModal->focusedArea() : nullptr;
                return storeContent->handleKeyEvent(event.sym, event.modifiers, event.pressed, event.preedit, focused);
              },
      }
  );
}

void SettingsWindow::closeWidgetInspectorPopup() {
  if (m_editorSheetModal != nullptr) {
    m_editorSheetModal->close();
  }
  m_editorSheetFactory.reset();
  m_editingWidgetName.clear();
  m_editingCapsuleGroupId.clear();
  requestContentRebuild();
}

void SettingsWindow::saveSupportReport() {
  if (m_config == nullptr) {
    return;
  }

  FileDialogOptions options;
  options.mode = FileDialogMode::Save;
  options.defaultFilename = "noctalia-support-report.toml";
  options.title = i18n::tr("settings.window.support-report-title");
  options.extensions = {".toml"};

  const bool opened = FileDialog::open(std::move(options), [this](std::optional<std::filesystem::path> result) {
    if (!result.has_value() || m_config == nullptr) {
      return;
    }

    auto path = *result;
    if (path.extension().empty()) {
      path += ".toml";
    }

    const std::string content = m_config->buildSupportReport();
    if (!writeTextFileAtomic(path, content)) {
      m_statusMessage = i18n::tr("settings.errors.support-report");
      m_statusIsError = true;
      requestSceneRebuild();
      return;
    }

    m_statusMessage = i18n::tr("settings.window.support-report-saved");
    m_statusIsError = false;
    requestSceneRebuild();
  });

  if (!opened) {
    m_statusMessage = i18n::tr("settings.errors.support-report");
    m_statusIsError = true;
    requestSceneRebuild();
  }
}

void SettingsWindow::saveConfigExport(settings::ConfigExportMode mode) {
  if (m_config == nullptr) {
    return;
  }

  const bool fullEffective = mode == settings::ConfigExportMode::FullEffective;

  FileDialogOptions options;
  options.mode = FileDialogMode::Save;
  options.defaultFilename = fullEffective ? "noctalia-full-config.toml" : "noctalia-config.toml";
  options.title = fullEffective ? i18n::tr("settings.export-config.full-effective-save-title")
                                : i18n::tr("settings.export-config.merged-user-save-title");
  options.extensions = {".toml"};

  const bool opened = FileDialog::open(std::move(options), [this, mode](std::optional<std::filesystem::path> result) {
    if (!result.has_value() || m_config == nullptr) {
      return;
    }

    auto path = *result;
    if (path.extension().empty()) {
      path += ".toml";
    }

    const std::string content = mode == settings::ConfigExportMode::FullEffective ? m_config->buildEffectiveConfig()
                                                                                  : m_config->buildMergedUserConfig();
    if (!writeTextFileAtomic(path, content)) {
      m_statusMessage = i18n::tr("settings.errors.export-config");
      m_statusIsError = true;
      requestSceneRebuild();
      return;
    }

    m_statusMessage = i18n::tr("settings.window.export-config-saved");
    m_statusIsError = false;
    requestSceneRebuild();
  });

  if (!opened) {
    m_statusMessage = i18n::tr("settings.errors.export-config");
    m_statusIsError = true;
    requestSceneRebuild();
  }
}
