#pragma once

#include <memory>

namespace compositors {

  namespace paw2 {
    class Paw2Runtime;
  } // namespace paw2

  class CompositorRuntimeRegistry {
  public:
    CompositorRuntimeRegistry();
    ~CompositorRuntimeRegistry();

    CompositorRuntimeRegistry(const CompositorRuntimeRegistry&) = delete;
    CompositorRuntimeRegistry& operator=(const CompositorRuntimeRegistry&) = delete;

    [[nodiscard]] paw2::Paw2Runtime& paw2() noexcept;
    [[nodiscard]] const paw2::Paw2Runtime& paw2() const noexcept;

  private:
    std::unique_ptr<paw2::Paw2Runtime> m_paw2;
  };

} // namespace compositors
