#include "ui/signal.h"

#include <iostream>
#include <memory>
#include <optional>

namespace {

  bool check(bool cond, const char* msg) {
    if (!cond) {
      std::cerr << "FAIL: " << msg << '\n';
    }
    return cond;
  }

} // namespace

int main() {
  bool ok = true;

  {
    Signal<int> signal;
    int sum = 0;
    auto a = signal.connect([&sum](int v) { sum += v; });
    auto b = signal.connect([&sum](int v) { sum += v * 10; });
    signal.emit(1);
    ok &= check(sum == 11, "both slots run");
    b.disconnect();
    signal.emit(1);
    ok &= check(sum == 12, "disconnected slot is skipped");
  }

  {
    // A slot that drops a later slot's owner must not have that slot called afterwards.
    Signal<> signal;
    int laterCalls = 0;
    auto later = std::make_unique<Signal<>::ScopedConnection>();
    auto first = signal.connect([&later]() { later.reset(); });
    *later = signal.connect([&laterCalls]() { ++laterCalls; });
    signal.emit();
    ok &= check(laterCalls == 0, "slot disconnected during emit is not called");
  }

  {
    // A slot connected during emit waits for the next one.
    Signal<> signal;
    int added = 0;
    std::optional<Signal<>::ScopedConnection> extra;
    auto first = signal.connect([&]() {
      if (!extra) {
        extra = signal.connect([&added]() { ++added; });
      }
    });
    signal.emit();
    ok &= check(added == 0, "slot connected during emit waits");
    signal.emit();
    ok &= check(added == 1, "slot connected during emit runs next time");
  }

  {
    // A connection outliving its signal disconnects quietly.
    auto connection = std::make_unique<Signal<>::ScopedConnection>();
    {
      Signal<> signal;
      *connection = signal.connect([]() {});
    }
    connection.reset();
  }

  if (ok) {
    std::cout << "signal_test: OK\n";
  }
  return ok ? 0 : 1;
}
