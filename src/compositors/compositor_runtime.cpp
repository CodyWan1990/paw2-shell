#include "compositors/compositor_runtime.h"

#include "compositors/paw2/paw2_runtime.h"

namespace compositors {

  CompositorRuntimeRegistry::CompositorRuntimeRegistry() : m_paw2(std::make_unique<paw2::Paw2Runtime>()) {}

  CompositorRuntimeRegistry::~CompositorRuntimeRegistry() = default;

  paw2::Paw2Runtime& CompositorRuntimeRegistry::paw2() noexcept { return *m_paw2; }

  const paw2::Paw2Runtime& CompositorRuntimeRegistry::paw2() const noexcept { return *m_paw2; }

} // namespace compositors
