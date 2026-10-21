#pragma once

class CalendarService;
class ConfigService;
class HttpClient;
class MprisService;
class PipeWireService;
class PipeWireSpectrum;
class RenderContext;
class SharedTextureCache;
class SystemMonitorService;
class WaylandConnection;
class WeatherService;

struct DesktopWidgetRuntimeServices {
  CalendarService* calendar = nullptr;
  PipeWireService* pipewire = nullptr;
  PipeWireSpectrum* pipewireSpectrum = nullptr;
  const WeatherService* weather = nullptr;
  MprisService* mpris = nullptr;
  HttpClient* httpClient = nullptr;
  SystemMonitorService* sysmon = nullptr;
  ConfigService* config = nullptr;
};

struct DesktopWidgetServices {
  WaylandConnection& wayland;
  ConfigService* config = nullptr;
  RenderContext* renderContext = nullptr;
  DesktopWidgetRuntimeServices runtime;
  SharedTextureCache* textureCache = nullptr;
};
