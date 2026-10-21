#pragma once

namespace paw2shell::ipc {

  // Entry point for `noctalia msg <command> [args...]`. Returns a process exit
  // code. Forwards the command to the running instance over the IPC socket.
  int runCli(int argc, char* argv[]);

} // namespace paw2shell::ipc
