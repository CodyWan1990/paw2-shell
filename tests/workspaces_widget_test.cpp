#include "shell/bar/widgets/workspaces_widget.h"
#include "tests/test_check.h"

#include <string>
#include <utility>

class WorkspacesWidgetTestAccess {
public:
  static std::string tooltip(std::string name, const std::string& label, bool showLabel) {
    return WorkspacesWidget::workspaceTooltipText(Workspace{.name = std::move(name)}, label, showLabel);
  }
  static std::optional<std::string>
  labelOverride(const Workspace& workspace, const std::unordered_map<std::string, std::string>& overrides) {
    return WorkspacesWidget::labelOverrideFor(workspace, overrides);
  }
};

int main() {
  // A name cut short by max_label_chars is recoverable on hover.
  TEST_CHECK(WorkspacesWidgetTestAccess::tooltip("client portal", "cli", true) == "client portal");
  // A pill that already spells out the whole name has nothing to add.
  TEST_CHECK(WorkspacesWidgetTestAccess::tooltip("web", "web", true).empty());
  // label_source = "id" labels a named workspace by number, so the name is only reachable on hover.
  TEST_CHECK(WorkspacesWidgetTestAccess::tooltip("client portal", "3", true) == "client portal");
  // A hidden label shows nothing, even when its text would have matched the name.
  TEST_CHECK(WorkspacesWidgetTestAccess::tooltip("web", "web", false) == "web");
  // Unnamed workspaces have no tooltip, never an empty one.
  TEST_CHECK(WorkspacesWidgetTestAccess::tooltip("", "3", true).empty());
  TEST_CHECK(WorkspacesWidgetTestAccess::tooltip("", "", false).empty());

  // Workspace name overrides: by number first, then name, then id; empty values are ignored.
  const std::unordered_map<std::string, std::string> overrides{{"1", "web"}, {"chat", "C"}, {"3", ""}};
  const Workspace first{.id = "DP-1:1", .name = "1", .index = 1};
  const Workspace named{.id = "DP-1:2", .name = "chat", .index = 2};
  const Workspace blank{.id = "DP-1:3", .name = "3", .index = 3};
  const Workspace other{.id = "DP-1:4", .name = "4", .index = 4};
  TEST_CHECK(WorkspacesWidgetTestAccess::labelOverride(first, overrides) == std::optional<std::string>("web"));
  TEST_CHECK(WorkspacesWidgetTestAccess::labelOverride(named, overrides) == std::optional<std::string>("C"));
  TEST_CHECK(!WorkspacesWidgetTestAccess::labelOverride(blank, overrides).has_value());
  TEST_CHECK(!WorkspacesWidgetTestAccess::labelOverride(other, overrides).has_value());
  TEST_CHECK(!WorkspacesWidgetTestAccess::labelOverride(first, {}).has_value());

  return 0;
}
