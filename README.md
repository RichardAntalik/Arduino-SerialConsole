# Arduino-SerialConsole
Serial console for arduino

Disclaimer: This library is fully vibe-coded. I am not Cpp proficient enough to even understand this code fully.

## Installation
Copy SerialConsole folder to your arduino libraries folder

## Usage
```
#include "SerialConsole.h"
auto console = createConsole(
  "fn_name_in_console", function, "usage string"
  "fn_name_in_console2", function2, "usage string"
);
```

You can use different stream than serial:
```
auto console = createConsoleStream(mySerial, 
  "cmd", fn, "usage",
  ...
);
```
### Source code embedding
You can use macro `EMBED_SOURCE_CODE()` to embed source code into MCU flash memory
When used, a `print_source_code` command will become available. The command prints source code of file where `EMBED_SOURCE_CODE()` macro was used.
This burns the raw file into MCU flash, so there are obviously limitations for filesize, but usually arduino projects are small.

## Example
```
#include "SerialConsole.h"
EMBED_SOURCE_CODE()

void test(int x, float y, const char *z){
  Serial.println(x);
  Serial.println(y);
  Serial.println(z);
}

void echo (const char * msg){ Serial.println(msg); }

auto console = createConsole(
    "test",    test,   "int, float, str",
    "echo",    echo,   nullptr      // Pass nullptr or "" if no usage info
);

void setup() {
  Serial.begin(115200);
}

void loop() {
  console.handleInput();
}
```
### Sample output

```
omg
Unknown command. Enter 'help' for list of available commands.
help
test int, float, str
echo
echo 123456
123456
echo 123456 aaaa
123456
test 123 4.56 hello
123
4.56
hello
test 0.123 4.56 hello
Invalid argument '0.123'.
print_source_code
#include "SerialConsole.h"
EMBED_SOURCE_CODE()

void test(int x, float y, const char *z){
  Serial.println(x);
  Serial.println(y);
  Serial.println(z);
}

void echo (const char * msg){ Serial.println(msg); }

auto console = createConsole(
    "test",    test,   "int, float, str",
    "echo",    echo,   nullptr      // Pass nullptr or "" if no usage info
);

void setup() {
  Serial.begin(115200);
}

void loop() {
  console.handleInput();
}
```
