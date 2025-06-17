# Alpha Compiler

**Alpha Compiler** is a complete compiler and virtual machine implementation for the ALPHA programming language. It compiles `.alpha` source code into `.abc` bytecode and executes it using a custom-built stack-based virtual machine.

Developed for the HY340 - Compiler Construction course at the University of Crete.

---

##  Features

- Lexical analysis with **Flex**
- Syntax analysis with **Bison**
- Quad-based **intermediate representation**
- **Symbol table** with scope and argument tracking
- **Target code generation** for a custom virtual machine
- Full **Abstract Virtual Machine (AVM)** to execute compiled programs
- Support for function definitions, local/global variables, loops, conditionals, expressions, and more

---

##  Build Instructions

Make sure you have `flex`, `bison`, and `gcc` installed.

To build the compiler and the virtual machine:

```bash
make
```

This will generate two executables:

- `alpha_compiler`: The compiler that produces `.abc` files from `.alpha` source  
- `alpha_vm`: The virtual machine that runs the `.abc` bytecode

---

##  Usage

### Step 1: Compile ALPHA source code

```bash
./alpha_compiler test.alpha
```

This will produce a binary file named `program.abc`.

### Step 2: Execute the program with the VM

```bash
./alpha_vm program.abc
```

---

##  Language Features

- **Variables**: `global`, `local`, `formal arguments`
- **Functions**: User-defined and built-in library functions
- **Control flow**: `if`, `else`, `while`
- **Operators**: Arithmetic, logical, comparison
- **Data types**: Numbers, strings, booleans, nil
- **Tables**: Creation and member access

---

## Notes

- Escaped characters in strings supported: `\\`, `\n`, `\t`, `\"`
- Built-in library functions include:  
  `print`, `input`, `typeof`, `strtonum`, `sqrt`, `cos`, `sin`, etc.
- Quads and instructions are printed to stdout during compilation for debugging
- All symbol information is maintained and printed using a custom symbol table system

---


##  Author

**Dimitris Papadopoulos**  
University of Crete – Computer Science Department  
Developed as part of the HY340 Compiler Construction course

---

