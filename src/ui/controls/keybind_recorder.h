#pragma once

#include "core/input/key_chord.h"
#include "ui/controls/flex.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

class Glyph;
class InputArea;
class Label;

enum class ModifierPolicy : std::uint8_t {
  Required,  // A modifier (Ctrl/Alt/Shift) must accompany printable keys.
  Optional,  // Bare printable keys and modifier combos are both accepted.
  Forbidden, // Modifiers are rejected; only bare keys are accepted.
};

// Records a KeyChord from live keyboard input. Rejects any chord involving Super, unless it records
// a compositor shortcut (setAllowSuper).
class KeybindRecorder : public Flex {
public:
  // Told when any recorder starts or stops recording, so the window can ask the compositor to pass
  // its own shortcuts through meanwhile. One observer for the process: only one window records at a time.
  static void setRecordingObserver(std::function<void(bool recording)> observer);

  KeybindRecorder();
  ~KeybindRecorder() override;

  void setChord(std::optional<KeyChord> chord);
  [[nodiscard]] std::optional<KeyChord> chord() const noexcept { return m_chord; }
  void setScale(float scale);
  void setEnabled(bool enabled);
  void setUnsetPlaceholder(std::string_view text);
  void setRecordingPlaceholder(std::string_view text);
  void setOnCommit(std::function<void(KeyChord)> callback);
  void setModifierPolicy(ModifierPolicy policy);
  // For paw2 shortcuts: keep Super, and record keys the way paw2 matches them (Shift+1, not "!").
  void setAllowSuper(bool allow);
  void setTabFocusKey(std::string key);

  [[nodiscard]] bool isRecording() const noexcept { return m_recording; }

private:
  enum class VisualState : std::uint8_t {
    Idle,
    Focused,
    Recording,
  };

  void doLayout(Renderer& renderer) override;
  LayoutSize doMeasure(Renderer& renderer, const LayoutConstraints& constraints) override;
  void doArrange(Renderer& renderer, const LayoutRect& rect) override;
  void handleKeyDown(std::uint32_t sym, std::uint32_t shortcutSym, std::uint32_t modifiers);
  void handleKeyUp(std::uint32_t sym, std::uint32_t modifiers);
  void handleWheel(std::uint32_t wheelSym);
  void enterRecording();
  void exitRecording(bool commit);
  void setRecording(bool recording);
  void refreshLabel();
  void applyVisualState(VisualState state);
  Label* m_label = nullptr;
  Glyph* m_glyph = nullptr;
  InputArea* m_inputArea = nullptr;
  std::optional<KeyChord> m_chord;
  std::function<void(KeyChord)> m_onCommit;
  std::string m_unsetPlaceholder;
  std::string m_recordingPlaceholder;
  std::uint32_t m_pendingModifiers = 0;
  float m_scale = 1.0F;
  bool m_recording = false;
  bool m_enabled = true;
  bool m_allowSuper = false;
  ModifierPolicy m_modifierPolicy = ModifierPolicy::Required;
  VisualState m_visualState = VisualState::Idle;
};
