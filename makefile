CC      := gcc
CFLAGS  := -g -Wall -D_GNU_SOURCE
LEX     := flex
YACC    := bison
YFLAGS  := -d

# Source files
PARSER_SRC = parser.y
LEXER_SRC = lexer.flex
COMPILER_SOURCES = symtable.c quads.c expr.c target_code.c
VM_SOURCES = avm.c avm_execute.c
MAIN_SOURCES = simple_vm.c

# Generated files
PARSER_C = parser.tab.c
PARSER_H = parser.tab.h
LEXER_C = lex.yy.c

# Object files
COMPILER_OBJS = $(PARSER_C:.c=.o) $(LEXER_C:.c=.o) $(COMPILER_SOURCES:.c=.o) $(VM_SOURCES:.c=.o)
VM_OBJS = $(VM_SOURCES:.c=.o) $(MAIN_SOURCES:.c=.o)

# Executables
COMPILER = alpha_compiler
VM = alpha_vm

# Default target - build both compiler and VM
all: $(COMPILER) $(VM)
	@echo ""
	@echo "Alpha Language System built successfully!"
	@echo ""
	@echo "Usage:"
	@echo "  Compile: ./$(COMPILER) < program.alpha"
	@echo "  Execute: ./$(VM) program.abc"
	@echo ""

# Build the Alpha compiler
$(COMPILER): $(COMPILER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lfl -lm

# Build the Alpha VM
$(VM): $(VM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Generate parser from bison
$(PARSER_C) $(PARSER_H): $(PARSER_SRC)
	$(YACC) $(YFLAGS) $<

# Generate lexer from flex
$(LEXER_C): $(LEXER_SRC) $(PARSER_H)
	$(LEX) $<

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build only the compiler
compiler: $(COMPILER)

# Build only the VM  
vm: $(VM)

# Clean all generated files
clean:
	rm -f $(COMPILER) $(VM)
	rm -f $(COMPILER_OBJS) $(VM_OBJS)
	rm -f $(PARSER_C) $(PARSER_H) $(LEXER_C)
	rm -f quads_output.txt target_code.txt *.abc
	@echo "All files cleaned"

# Clean and rebuild everything
rebuild: clean all

# Dependencies
$(PARSER_C:.c=.o): $(PARSER_C) $(PARSER_H) symtable.h expr.h quads.h target_code.h
$(LEXER_C:.c=.o): $(LEXER_C) $(PARSER_H)
symtable.o: symtable.c symtable.h
quads.o: quads.c quads.h expr.h
expr.o: expr.c expr.h symtable.h quads.h
target_code.o: target_code.c target_code.h quads.h expr.h symtable.h
avm.o: avm.c avm.h target_code.h
avm_execute.o: avm_execute.c avm.h
simple_vm.o: simple_vm.c avm.h target_code.h

.PHONY: all test run compiler vm install clean rebuild info