#ifndef SERIAL_CONSOLE_H
#define SERIAL_CONSOLE_H

#include <Arduino.h>

// =============================================================
// SECTION 1: CONFIGURATION & TYPES
// =============================================================

static const size_t INPUT_BUF_SIZE = 64;

typedef void (*VoidFuncPtr)();
// Invoker now takes name/usage so it can print errors intelligently
typedef void (*InvokerFunc)(VoidFuncPtr f, const char *name, const char *usage,
                            Stream &s);

struct Command {
  const char *name;
  const char *usage;
  VoidFuncPtr func;
  InvokerFunc invoker;
};

// =============================================================
// SECTION 2: TEMPLATE ENGINE (VALIDATION LOGIC)
// =============================================================
namespace console_detail {

// --- 1. Traits: Parse AND Validate ---
template <typename T> struct ArgTraits;

template <> struct ArgTraits<int> {
  static bool parse(const char *str, int &out) {
    char *end;
    out = strtol(str, &end, 0);
    // Valid if: pointers differ (found digits) AND end is null (consumed whole
    // string)
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

// Arduino double is float
template <> struct ArgTraits<double> {
  static bool parse(const char *str, float &out) {
    char *end;
    out = strtod(str, &end);
    return (str != end && *end == '\0');
  }
};

template <> struct ArgTraits<char *> {
  static bool parse(char *str, char *&out) {
    out = str; // Strings are always valid tokens
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

    // 1. Check if argument exists
    if (!token) {
      s.println(F("Missing argument."));
      s.println(F("Usage: "));
      s.print(name);
      s.print(" ");
      s.println(usage ? usage : "");
      return;
    }

    // 2. Try to parse and validate
    Head val;
    if (!ArgTraits<Head>::parse(token, val)) {
      s.print(F("Invalid argument '"));
      s.print(token);
      s.println(F("'."));
      s.println("Usage: ");
      s.print(name);
      s.print(" ");
      s.println(usage ? usage : "");
      return;
    }

    // 3. Recurse
    Executor<Tail...>::run(f, name, usage, s, collected..., val);
  }
};

// BASE CASE: All args parsed -> Call function
template <> struct Executor<> {
  template <typename... Collected>
  static void run(VoidFuncPtr f, const char *n, const char *u, Stream &s,
                  Collected... collected) {
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

  void initArgs(size_t i) {}

  template <typename... FuncArgs, typename... Rest>
  void initArgs(size_t i, const char *name, void (*func)(FuncArgs...),
                const char *usage, Rest... rest) {
    if (i >= N_CMDS)
      return;

    _commands[i].name = name;
    _commands[i].usage = usage;
    _commands[i].func = reinterpret_cast<VoidFuncPtr>(func);

    // Bind the recursive validator/executor
    _commands[i].invoker = [](VoidFuncPtr f, const char *n, const char *u,
                              Stream &s) {
      console_detail::Executor<FuncArgs...>::run(f, n, u, s);
    };

    initArgs(i + 1, rest...);
  }

  void handleInput() {
    if (!readInputLine())
      return;

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
        // Pass name and usage to the invoker for error printing
        _commands[i].invoker(_commands[i].func, _commands[i].name,
                             _commands[i].usage, _stream);
        return;
      }
    }
    _stream.println(
        F("Unknown command. Enter 'help' for list of available commands."));
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
      _stream.print(_commands[i].name);
      if (_commands[i].usage) {
        _stream.print(F(" "));
        _stream.print(_commands[i].usage);
      }
      _stream.println();
    }
  }
};

template <typename... Args>
SerialConsole<sizeof...(Args) / 3> createConsole(Args... args) {
  SerialConsole<sizeof...(Args) / 3> c(Serial);
  c.initArgs(0, args...);
  return c;
}

#endif