#include "compositors/compositor_runtime.h"

#include "compositors/paw2/paw2_config_writer.h"
#include "compositors/paw2/paw2_runtime.h"

namespace compositors {

  CompositorRuntimeRegistry::CompositorRuntimeRegistry()
      : m_paw2(std::make_unique<paw2::Paw2Runtime>()),
        m_paw2Config(std::make_unique<paw2::Paw2ConfigWriter>(*m_paw2)) {}

  // The writer listens to the runtime, so it goes first.
  CompositorRuntimeRegistry::~CompositorRuntimeRegistry() { m_paw2Config.reset(); }

  paw2::Paw2Runtime& CompositorRuntimeRegistry::paw2() noexcept { return *m_paw2; }

  const paw2::Paw2Runtime& CompositorRuntimeRegistry::paw2() const noexcept { return *m_paw2; }

  paw2::Paw2ConfigWriter& CompositorRuntimeRegistry::paw2Config() noexcept { return *m_paw2Config; }
  const paw2::Paw2ConfigWriter& CompositorRuntimeRegistry::paw2Config() const noexcept { return *m_paw2Config; }

} // namespace compositors
