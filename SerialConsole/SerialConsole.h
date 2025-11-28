#ifndef SERIAL_CONSOLE_H
#define SERIAL_CONSOLE_H

#include <Arduino.h>

// =============================================================
// SECTION 1: CONFIGURATION & TYPES
// =============================================================

// Max command length (including arguments)
static const size_t INPUT_BUF_SIZE = 64;

typedef void (*VoidFuncPtr)();
// Invoker takes name/usage to print specific errors
typedef void (*InvokerFunc)(VoidFuncPtr f, const char *name, const char *usage,
                            Stream &s);

struct Command {
  const char *name;
  const char *usage;
  VoidFuncPtr func;
  InvokerFunc invoker;
};

// =============================================================
// SECTION 2: TEMPLATE ENGINE (PARSING & VALIDATION)
// =============================================================
namespace console_detail {

// --- 1. Traits: Parse String -> Type ---
template <typename T> struct ArgTraits;

template <> struct ArgTraits<int> {
  static bool parse(const char *str, int &out) {
    char *end;
    out = strtol(str, &end, 0);
    return (str != end && *end == '\0');
  }
};

template <> struct ArgTraits<long> {
  static bool parse(const char *str, long &out) {
    char *end;
    out = strtol(str, &end, 0);
    return (str != end && *end == '\0');
  }
};

template <> struct ArgTraits<float> {
  static bool parse(const char *str, float &out) {
    char *end;
    out = strtod(str, &end);
    return (str != end && *end == '\0');
  }
};

template <> struct ArgTraits<double> {
  static bool parse(const char *str, float &out) {
    char *end;
    out = strtod(str, &end);
    return (str != end && *end == '\0');
  }
};

template <> struct ArgTraits<char *> {
  static bool parse(char *str, char *&out) {
    out = str;
    return true;
  }
};

template <> struct ArgTraits<const char *> {
  static bool parse(char *str, const char *&out) {
    out = str;
    return true;
  }
};

// --- 2. Recursive Executor ---
template <typename... Args> struct Executor;

// RECURSIVE STEP: Parse Head, then recurse Tail
template <typename Head, typename... Tail> struct Executor<Head, Tail...> {
  template <typename... Collected>
  static void run(VoidFuncPtr f, const char *name, const char *usage, Stream &s,
                  Collected... collected) {

    char *token = strtok(NULL, " ");

    // Check for missing arg
    if (!token) {
      s.print(F("Err: Missing argument. Usage: "));
      s.print(name);
      s.print(" ");
      s.println(usage ? usage : "");
      return;
    }

    // Try to parse
    Head val;
    if (!ArgTraits<Head>::parse(token, val)) {
      s.print(F("Err: Invalid argument '"));
      s.print(token);
      s.print(F("'. Usage: "));
      s.print(name);
      s.print(" ");
      s.println(usage ? usage : "");
      return;
    }

    // Recurse
    Executor<Tail...>::run(f, name, usage, s, collected..., val);
  }
};

// BASE CASE: All args parsed -> Call function
template <> struct Executor<> {
  template <typename... Collected>
  static void run(VoidFuncPtr f, const char *n, const char *u, Stream &s,
                  Collected... collected) {
    // Cast generic pointer back to specific function type and call it
    auto typedFunc = reinterpret_cast<void (*)(Collected...)>(f);
    typedFunc(collected...);
  }
};
} // namespace console_detail

// =============================================================
// SECTION 3: MAIN CLASS
// =============================================================

template <size_t N_CMDS> class SerialConsole {
public:
  SerialConsole(Stream &s) : _stream(s) {}

  // --- Initialization ---
  void initArgs(size_t i) {}

  template <typename... FuncArgs, typename... Rest>
  void initArgs(size_t i, const char *name, void (*func)(FuncArgs...),
                const char *usage, Rest... rest) {
    if (i >= N_CMDS)
      return;

    _commands[i].name = name;
    _commands[i].usage = usage;
    _commands[i].func = reinterpret_cast<VoidFuncPtr>(func);

    // Bind the recursive invoker
    _commands[i].invoker = [](VoidFuncPtr f, const char *n, const char *u,
                              Stream &s) {
      console_detail::Executor<FuncArgs...>::run(f, n, u, s);
    };

    initArgs(i + 1, rest...);
  }

  // --- Runtime ---
  void handleInput() {
    if (!readInputLine())
      return;

    _stream.print(F("> "));
    _stream.println(_inputBuf);

    char *token = strtok(_inputBuf, " ");
    if (!token)
      return;

    if (strcmp(token, "help") == 0) {
      printHelp();
      return;
    }

    for (size_t i = 0; i < N_CMDS; i++) {
      if (strcmp(token, _commands[i].name) == 0) {
        _commands[i].invoker(_commands[i].func, _commands[i].name,
                             _commands[i].usage, _stream);
        return;
      }
    }
    _stream.println(F("Unknown command. Type 'help' for list of commands."));
  }

private:
  Stream &_stream;
  Command _commands[N_CMDS];
  char _inputBuf[INPUT_BUF_SIZE];

  bool readInputLine() {
    if (_stream.available() == 0)
      return false;
    int len = _stream.readBytesUntil('\n', _inputBuf, INPUT_BUF_SIZE - 1);
    _inputBuf[len] = '\0';
    while (len > 0 && isspace(_inputBuf[len - 1]))
      _inputBuf[--len] = '\0';
    return len > 0;
  }

  void printHelp() {
    for (size_t i = 0; i < N_CMDS; i++) {
      _stream.print(F("  "));
      _stream.print(_commands[i].name);
      if (_commands[i].usage) {
        _stream.print(F(" "));
        _stream.print(_commands[i].usage);
      }
      _stream.println();
    }
  }
};

// =============================================================
// SECTION 4: FACTORY FUNCTIONS
// =============================================================

// Default factory (Uses 'Serial')
template <typename... Args>
SerialConsole<sizeof...(Args) / 3> createConsole(Args... args) {
  static_assert(sizeof...(Args) % 3 == 0,
                "Args must be triplets: Name, Func, Usage");
  SerialConsole<sizeof...(Args) / 3> c(Serial);
  c.initArgs(0, args...);
  return c;
}

// Stream factory (Uses custom stream)
template <typename... Args>
SerialConsole<sizeof...(Args) / 3> createConsoleStream(Stream &s,
                                                       Args... args) {
  static_assert(sizeof...(Args) % 3 == 0,
                "Args must be triplets: Name, Func, Usage");
  SerialConsole<sizeof...(Args) / 3> c(s);
  c.initArgs(0, args...);
  return c;
}

#endif