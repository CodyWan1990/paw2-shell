#pragma once

#include "shell/control_center/tab.h"

#include <cstdint>
#include <string>
#include <vector>

class Flex;
class Glyph;
class Label;
class ProgressBar;
class Renderer;
class Segmented;
class Toggle;
class UPowerService;
class PowerProfilesService;
class PowerTabTestAccess;
struct UPowerChargeLimitState;

class PowerTab : public Tab {
public:
  PowerTab(UPowerService* upower, PowerProfilesService* powerProfiles);

  std::unique_ptr<Flex> create() override;
  void onClose() override;

private:
  friend class PowerTabTestAccess;

  enum class ChargeLimitMode : std::uint8_t {
    Unsupported,
    UPowerActive,
    UPowerDisabled,
    ExternallyManaged,
    FirmwareManaged,
    ReadOnly,
  };

  struct ChargeLimitControlState {
    bool visible = false;
    bool checked = false;
    bool enabled = false;

    bool operator==(const ChargeLimitControlState&) const = default;
  };

  [[nodiscard]] static ChargeLimitMode classifyChargeLimit(const UPowerChargeLimitState& state) noexcept;
  [[nodiscard]] static ChargeLimitControlState chargeLimitControlState(const UPowerChargeLimitState& state) noexcept;
  [[nodiscard]] static bool shouldShowChargeLimit(const UPowerChargeLimitState& state) noexcept;

  void doLayout(Renderer& renderer, float contentWidth, float bodyHeight) override;
  void doUpdate(Renderer& renderer) override;

  void buildStatusCard(Flex& root, float scale);
  void buildChargingCard(Flex& root, float scale);
  void buildProfilesCard(Flex& root, float scale);
  void buildHealthCard(Flex& root, float scale);
  void buildPeripheralsCard(Flex& root, float scale);

  void syncBatteryStatus();
  void rebuildChargeLimits();
  void syncPowerProfiles();
  void syncBatteryHealth();
  void rebuildPeripherals();

  UPowerService* m_upower = nullptr;
  PowerProfilesService* m_powerProfiles = nullptr;

  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    Flex* root = nullptr;
    // Battery status
    Flex* statusCard = nullptr;
    Glyph* statusGlyph = nullptr;
    Label* percentLabel = nullptr;
    Label* stateLabel = nullptr;
    ProgressBar* levelBar = nullptr;
    Flex* timeRow = nullptr;
    Label* timeLabel = nullptr;
    Flex* rateRow = nullptr;
    Label* rateLabel = nullptr;
    // Battery charging limits
    Flex* chargingCard = nullptr;
    Flex* chargingList = nullptr;
    // Power profiles
    Flex* profilesCard = nullptr;
    Segmented* profiles = nullptr;
    Flex* inhibitedRow = nullptr;
    // Battery health
    Flex* healthCard = nullptr;
    Label* healthLabel = nullptr;
    ProgressBar* healthBar = nullptr;
    // Peripheral batteries
    Flex* peripheralsCard = nullptr;
    Flex* peripheralsList = nullptr;
  };
  Ui m_ui;

  struct ChargeLimitRow {
    Flex* row = nullptr;
    Label* nameLabel = nullptr;
    Label* behaviorLabel = nullptr;
    Label* configuredLabel = nullptr;
    Label* managementLabel = nullptr;
    Label* errorLabel = nullptr;
    Flex* controlRow = nullptr;
    Label* controlLabel = nullptr;
    Toggle* toggle = nullptr;
  };
  std::vector<ChargeLimitRow> m_chargeLimitRows;
  std::string m_lastChargeLimitKey;

  std::vector<std::string> m_profileOrder;
  bool m_syncingProfiles = false;

  struct PeripheralRow {
    std::string path;
    Flex* row = nullptr;
    Label* nameLabel = nullptr;
    Label* pctLabel = nullptr;
    ProgressBar* bar = nullptr;
  };
  std::vector<PeripheralRow> m_peripheralRows;
  std::string m_lastPeripheralKey;
};
