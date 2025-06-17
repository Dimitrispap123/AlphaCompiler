#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "myStruct.h"


typedef enum space_t {
    programvar,     
    functionlocal,  
    formalarg       
} space_t;

typedef struct
{
    int scope;
    int min_line;
} ScopeInfo;

typedef enum SymbolType
{
    GLOBAL_VAR,
    LOCAL_VAR,
    FORMAL,
    USERFUNC,
    LIBFUNC,
    FORMAL_ARG
} SymbolType;

typedef struct Variable
{
    char *name;
    unsigned int scope;
    unsigned int line;
    struct Variable *arg_next;
} Variable;

typedef struct Function
{
    char *name;
    unsigned int scope;
    unsigned int line;
    struct Variable *arguments;
    struct Function *next;
    unsigned int totalLocals; 
} Function;

typedef struct SymbolTableEntry
{
    char shown;
    enum SymbolType type;
    space_t space;           
    unsigned offset;         
    union
    {
        Variable *varVal;
        Function *funcVal;
    } value;
    struct SymbolTableEntry *next;
} SymbolTableEntry;

SymbolTableEntry *insert_function(char *name, unsigned int line, unsigned int scope, enum SymbolType type);
SymbolTableEntry *insert_argument(AlphaToken *token, unsigned int line, unsigned int scope);

SymbolTableEntry *insert_variable(char *name, unsigned int line, unsigned int scope, enum SymbolType type);
SymbolTableEntry *lookup_in_scope(SymbolTableEntry *head, const char *name, unsigned int scope);
SymbolTableEntry *lookup_visible(SymbolTableEntry *head, const char *name, unsigned int current_scope);
SymbolTableEntry *lookup_library(SymbolTableEntry *head, const char *name);
void print_symbol_table(SymbolTableEntry *head);
void print_table(SymbolTableEntry *head);
int SymTable_remove(SymbolTableEntry **oSymTable, const char *pcKey);
void init_library_functions();

#endif 