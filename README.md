# Arduino-SerialConsole
Serial console for arduino
Disclaimer: This library is fully vibe-coded. I am not Cpp proficient enough to even understand this code fully.

## Installation
Copy SerialConsole folder to your arduino libraries folder

## Usage
```
auto console = createConsole(
  "fn_name_in_console", function, "usage string"
  "fn_name_in_console2", function2, "usage string"
);
```

## Example

```
#include "SerialConsole.h"

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
```

Binary details on Leonardo platform:
```
Sketch uses 8314 bytes (28%) of program storage space. Maximum is 28672 bytes.
Global variables use 289 bytes (11%) of dynamic memory, leaving 2271 bytes for local variables. Maximum is 2560 bytes.
```
