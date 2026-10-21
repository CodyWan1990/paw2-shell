#pragma once

namespace paw2shell::theme {

  // Entry point for `paw2-shell theme <image> [options]`. Returns a process exit
  // code. Does not touch Application / event loop / config — pure function of
  // (argv, stdout, stderr).
  int runCli(int argc, char* argv[]);

} // namespace paw2shell::theme
