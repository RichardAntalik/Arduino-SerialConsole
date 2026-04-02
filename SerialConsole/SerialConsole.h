#ifndef SERIAL_CONSOLE_H
#define SERIAL_CONSOLE_H

#include <Arduino.h>

// If the macro isn't used, the linker sets this to a null pointer.
extern "C" void print_embedded_source_code() __attribute__((weak));

// =============================================================
// SECTION 1: CONFIGURATION & TYPES
// =============================================================

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

// --- 0. Type Traits (Manual implementation for Arduino compatibility) ---
template <typename T> struct remove_reference {
  typedef T type;
};
template <typename T> struct remove_reference<T &> {
  typedef T type;
};
template <typename T> struct remove_reference<T &&> {
  typedef T type;
};

template <typename T> struct remove_const {
  typedef T type;
};
template <typename T> struct remove_const<const T> {
  typedef T type;
};

template <typename T>
using decay_t = typename remove_const<typename remove_reference<T>::type>::type;

// --- 1. Traits: Parse String -> Type ---
template <typename T> struct ArgTraits;

template <> struct ArgTraits<int> {
  static bool parse(const char *str, int &out) {
    char *end;
    out = (int)strtol(str, &end, 0);
    return (str != end && *end == '\0');
  }
};

template <> struct ArgTraits<bool> {
  static bool parse(const char *str, bool &out) {
    if (strcasecmp(str, "true") == 0 || strcmp(str, "1") == 0) {
      out = true;
      return true;
    }
    if (strcasecmp(str, "false") == 0 || strcmp(str, "0") == 0) {
      out = false;
      return true;
    }
    return false;
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
  static bool parse(const char *str, double &out) {
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

    if (!token) {
      s.println(F("Missing argument."));
      s.print("Usage: ");
      s.print(name);
      s.print(" ");
      s.println(usage ? usage : "");
      return;
    }

    // Prepare variable for parsing
    // We strip const/ref to declare the local variable 'val'
    using DecayHead = decay_t<Head>;
    DecayHead val;

    if (!ArgTraits<DecayHead>::parse(token, val)) {
      s.print(F("Invalid argument '"));
      s.print(token);
      s.println(F("'."));
      s.print("Usage: ");
      s.print(name);
      s.print(" ");
      s.println(usage ? usage : "");
      return;
    }

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

// --- 3. Command Binder ---
template <typename T> struct CommandBinder;

// Specialization A: Standard Function Pointers
template <typename... Args> struct CommandBinder<void (*)(Args...)> {
  static void bind(Command &cmd, void (*func)(Args...)) {
    cmd.func = reinterpret_cast<VoidFuncPtr>(func);
    cmd.invoker = [](VoidFuncPtr f, const char *n, const char *u, Stream &s) {
      Executor<Args...>::run(f, n, u, s);
    };
  }
};

// Specialization B: Lambdas / Functors
template <typename T> struct CommandBinder {
  // Helper to extract args from the operator() pointer (const)
  template <typename R, typename ClassType, typename... Args>
  static void bindInternal(Command &cmd, T lambda,
                           R (ClassType::*)(Args...) const) {
    using FuncPtrType = void (*)(Args...);
    FuncPtrType rawFunc = static_cast<FuncPtrType>(lambda);
    CommandBinder<FuncPtrType>::bind(cmd, rawFunc);
  }

  // Helper for non-const operator()
  template <typename R, typename ClassType, typename... Args>
  static void bindInternal(Command &cmd, T lambda, R (ClassType::*)(Args...)) {
    using FuncPtrType = void (*)(Args...);
    FuncPtrType rawFunc = static_cast<FuncPtrType>(lambda);
    CommandBinder<FuncPtrType>::bind(cmd, rawFunc);
  }

  static void bind(Command &cmd, T lambda) {
    bindInternal(cmd, lambda, &T::operator());
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

  template <typename TFunc, typename... Rest>
  void initArgs(size_t i, const char *name, TFunc func, const char *usage,
                Rest... rest) {
    if (i >= N_CMDS)
      return;

    _commands[i].name = name;
    _commands[i].usage = usage;

    console_detail::CommandBinder<TFunc>::bind(_commands[i], func);

    initArgs(i + 1, rest...);
  }

  void addDynamicCommand(size_t i, const char *name, void (*func)(),
                         const char *usage) {
    if (i >= N_CMDS)
      return;
    _commands[i].name = name;
    _commands[i].usage = usage;
    if (func) {
      console_detail::CommandBinder<void (*)()>::bind(_commands[i], func);
    } else {
      _commands[i].func = nullptr; // Safety nulling
    }
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
        if (!_commands[i].name)
          continue;
        _commands[i].invoker(_commands[i].func, _commands[i].name,
                             _commands[i].usage, _stream);
        return;
      }
    }
    _stream.println(F("Unknown command."));
  }

private:
  Stream &_stream;
  Command _commands[N_CMDS];
  char _inputBuf[INPUT_BUF_SIZE];

  bool readInputLine() {
    if (_stream.available() == 0)
      return false;
    static int idx = 0;
    while (_stream.available()) {
      char c = _stream.read();
      if (c == '\n' || c == '\r') {
        if (idx == 0)
          continue;
        _inputBuf[idx] = '\0';
        idx = 0;
        return true;
      }
      if (idx < INPUT_BUF_SIZE - 1) {
        _inputBuf[idx++] = c;
      }
    }
    return false;
  }

  void printHelp() {
    for (size_t i = 0; i < N_CMDS; i++) {
      if (!_commands[i].name)
        continue;
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

template <typename... Args>
SerialConsole<(sizeof...(Args) / 3) + 1> createConsole(Args... args) {
  static_assert(sizeof...(Args) % 3 == 0,
                "Args must be triplets: Name, Func, Usage");

  // Allocate space for the user commands + 1 extra for the potential print_code
  SerialConsole<(sizeof...(Args) / 3) + 1> c(Serial);
  c.initArgs(0, args...);

  // Magic detection: If the macro was used, this pointer evaluates to true
  if (print_embedded_source_code) {
    c.addDynamicCommand(sizeof...(Args) / 3, "print_source_code",
                        print_embedded_source_code, "print source code");
  } else {
    c.addDynamicCommand(sizeof...(Args) / 3, nullptr, nullptr, nullptr);
  }

  return c;
}

template <typename... Args>
SerialConsole<(sizeof...(Args) / 3) + 1> createConsoleStream(Stream &s,
                                                             Args... args) {
  static_assert(sizeof...(Args) % 3 == 0,
                "Args must be triplets: Name, Func, Usage");

  // Allocate space for the user commands + 1 extra for the potential
  // print_code
  SerialConsole<(sizeof...(Args) / 3) + 1> c(s);
  c.initArgs(0, args...);

  // Magic detection: If the macro was used, this pointer evaluates to true
  if (print_embedded_source_code) {
    c.addDynamicCommand(sizeof...(Args) / 3, "print_source_code",
                        print_embedded_source_code, "print source code");
  } else {
    c.addDynamicCommand(sizeof...(Args) / 3, nullptr, nullptr, nullptr);
  }

  return c;
}

#endif

#define EMBED_SOURCE_CODE()                                                    \
  extern "C" {                                                                 \
  __asm__(".pushsection .progmem.data, \"a\"\n"                                \
          ".global embedded_source_code\n"                                     \
          "embedded_source_code:\n"                                            \
          ".incbin \"" __FILE__ "\"\n"                                         \
          ".byte 0\n"                                                          \
          ".global embedded_source_end\n"                                      \
          "embedded_source_end:\n"                                             \
          ".popsection\n");                                                    \
  extern const char embedded_source_code[] PROGMEM;                            \
  extern const char embedded_source_end[] PROGMEM;                             \
  void print_embedded_source_code() {                                          \
    const char *ptr = embedded_source_code;                                    \
    while (ptr < embedded_source_end) {                                        \
      char c = pgm_read_byte(ptr);                                             \
      if (c == 0)                                                              \
        break;                                                                 \
      Serial.print(c);                                                         \
      ptr++;                                                                   \
    }                                                                          \
  }                                                                            \
  }
