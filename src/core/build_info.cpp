#include "core/build_info.h"

#include "paw2_shell_git_revision.h"

namespace paw2shell::build_info {

  std::string_view version() noexcept { return PAW2_SHELL_VERSION; }

  std::string_view revision() noexcept { return PAW2_SHELL_GIT_REVISION; }

  std::string displayVersion() {
    std::string label = "v";
    label += version();

    constexpr std::string_view unknownRevision = "unknown";
    const std::string_view rev = revision();
    if (!rev.empty() && rev != unknownRevision) {
      label += " (";
      label += rev;
      label += ')';
    }
    return label;
  }

} // namespace paw2shell::build_info
