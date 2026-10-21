#pragma once

#include "core/frame_rate_limiter.h"
#include "shell/control_center/tab.h"

#include <chrono>
#include <string>
#include <vector>

class Flex;
class Glyph;
class Graph;
class Label;
class SystemMonitorService;

class SystemTab : public Tab {
public:
  explicit SystemTab(SystemMonitorService* monitor);
  ~SystemTab() override;

  std::unique_ptr<Flex> create() override;
  void onClose() override;
  void setActive(bool active) override;
  void onFrameTick(float deltaMs) override;

private:
  void doLayout(Renderer& renderer, float contentWidth, float bodyHeight) override;
  void doUpdate(Renderer& renderer) override;

  void retainStats(bool retain);
  void updateGraphs(Renderer& renderer);
  void syncLabels();
  void updateGpuVisibility();
  [[nodiscard]] float scrollProgressForSample(std::chrono::steady_clock::time_point sampledAt) const;

  SystemMonitorService* m_monitor;
  bool m_active = false;
  bool m_statsRetained = false;
  bool m_graphInitialized = false;
  bool m_gpuVisible = false;
  float m_scrollProgress = 1.0F;
  std::chrono::steady_clock::time_point m_lastSampleAt;
  FrameRateLimiter m_redrawLimiter{std::chrono::milliseconds{200}};

  double m_cpuTempMin = 30.0;
  double m_cpuTempMax = 80.0;
  double m_gpuTempMin = 30.0;
  double m_gpuTempMax = 80.0;
  double m_netPeak = 0.0;

  // Scene nodes owned by root(); onClose() drops them all at once.
  struct Ui {
    Flex* root = nullptr;
    Graph* cpuGraph = nullptr;
    Graph* ramGraph = nullptr;
    Graph* gpuGraph = nullptr;
    Graph* netGraph = nullptr;
    Flex* cpuCard = nullptr;
    Flex* ramCard = nullptr;
    Flex* gpuCard = nullptr;
    Flex* netCard = nullptr;
    Flex* cpuLegend = nullptr;
    Flex* ramLegend = nullptr;
    Flex* gpuLegend = nullptr;
    Flex* netLegend = nullptr;
    Glyph* cpuPctIcon = nullptr;
    Label* cpuPctLabel = nullptr;
    Glyph* cpuTempIcon = nullptr;
    Label* cpuTempLabel = nullptr;
    Flex* gpuTempGroup = nullptr;
    Glyph* gpuTempIcon = nullptr;
    Label* gpuTempLabel = nullptr;
    Flex* gpuUsageGroup = nullptr;
    Glyph* gpuUsageIcon = nullptr;
    Label* gpuUsageLabel = nullptr;
    Flex* gpuVramGroup = nullptr;
    Glyph* gpuVramIcon = nullptr;
    Label* gpuVramLabel = nullptr;
    Glyph* ramIcon = nullptr;
    Label* ramLabel = nullptr;
    Glyph* rxIcon = nullptr;
    Label* rxLabel = nullptr;
    Glyph* txIcon = nullptr;
    Label* txLabel = nullptr;
    Flex* swapRow = nullptr;
    Label* swapLabel = nullptr;
  };
  Ui m_ui;

  // System card: cpu, gpu, distro, kernel, wm, uptime + os age
  static constexpr int kSystemLines = 6;
  Label* m_systemLines[kSystemLines] = {};

  // Resources card: load, memory, swap (hidden when no swap), then up to four discovered physical disks.
  static constexpr int kResourcesLines = 2;
  Label* m_resourcesLines[kResourcesLines] = {};
  std::vector<std::string> m_diskMountPoints;
  std::vector<Label*> m_diskLabels;
};
