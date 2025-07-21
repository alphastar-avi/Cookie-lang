# Cookie Language Compiler - Standalone Distribution

This is a standalone distribution of the Cookie programming language compiler.

## Installation

1. Extract the package:
   ```bash
   tar -xzf cookie-compiler-1.0.0.tar.gz
   cd cookie-compiler-1.0.0
   ```

2. Add to PATH (optional):
   ```bash
   export PATH=$PATH:$(pwd)/bin
   ```

3. Or install system-wide:
   ```bash
   sudo cp bin/* /usr/local/bin/
   sudo cp lib/* /usr/local/lib/
   ```

## Usage

### Basic compilation:
```bash
./bin/cookie_compiler.sh program.cook
```

### Specify output file:
```bash
./bin/cookie_compiler.sh -o myprogram program.cook
```

### Verbose compilation:
```bash
./bin/cookie_compiler.sh -v program.cook
```

### Keep intermediate files:
```bash
./bin/cookie_compiler.sh -k program.cook
```

## Examples

Try the included examples:

```bash
./bin/cookie_compiler.sh examples/cookie.cook
./a.out

./bin/cookie_compiler.sh examples/lang.cook
echo "10" | ./a.out
```

## Requirements

- LLVM tools (llc command)
- GCC compiler
- Unix-like system (Linux, macOS)

## Language Features

Cookie supports:
- Variables (int, float, bool, char, string)
- Arrays and array operations
- Control flow (if/else, loops, switch)
- Functions with parameters and return values
- Input/output operations
- Type casting and conversions

## Documentation

For complete language documentation, visit:
https://github.com/alphastar-avi/Cookie-lang

## Version

Cookie Compiler v1.0.0 - Standalone Distribution
