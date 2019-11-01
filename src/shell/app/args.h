#pragma once

#include <cxxopts.hpp>

#include "../../common/scope_guard.h"
#include "../../common/utf.h"

#if defined(WIN32)
static inline auto parse_opts(cxxopts::Options &options)
{
  scope_guard scope_always(scope_guard::execution::always);

  int argc = 0;
  char **argv = nullptr;

  auto argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
  scope_always += [argvw]() { LocalFree(argvw); };

  if (argvw != nullptr)
  {
    argv = new char *[argc];
    scope_always += [argv]() { delete argv; };

    for (int i = 0; i < argc; ++i)
    {
      auto arg = utf16_to_utf8(argvw[i]);

      argv[i] = new char[arg.length() + 1];
      scope_always += [argv_i = argv[i]]() { delete argv_i; };

      std::copy(arg.begin(), arg.end(), argv[i]);
      argv[i][arg.length()] = '\0';
    }
  }
  else
  {
    argc = 0;
  }

#else
static inline auto parse_opts(cxxopts::Options &options, int argc, char **argv)
{
#endif
  return options.parse(argc, argv);
}

#if defined(WIN32)
#define PARSE_OPTS(options) parse_opts(options)
#else
#define PARSE_OPTS(options) parse_opts(options, argc, argv)
#endif
