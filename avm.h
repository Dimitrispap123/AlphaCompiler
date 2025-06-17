#ifndef AVM_H
#define AVM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "target_code.h"

#define AVM_STACKSIZE 4096
#define AVM_WIPEOUT(m) memset(&(m), 0, sizeof(m))
#define AVM_TABLE_HASHSIZE 211
#define AVM_ENDING_PC currentInstruction

#define AVM_STACKENV_SIZE 4
#define AVM_NUMACTUALS_OFFSET 4
#define AVM_SAVEDPC_OFFSET 3
#define AVM_SAVEDTOP_OFFSET 2
#define AVM_SAVEDTOPSP_OFFSET 1

typedef enum avm_memcell_t {
    number_m = 0,
    string_m = 1,
    bool_m = 2,
    table_m = 3,
    userfunc_m = 4,
    libfunc_m = 5,
    nil_m = 6,
    undef_m = 7
} avm_memcell_t;

typedef struct avm_table avm_table;

typedef struct avm_memcell {
    avm_memcell_t type;
    union {
        double numVal;
        char* strVal;
        unsigned char boolVal;
        avm_table* tableVal;
        unsigned funcVal;
        char* libfuncVal;
    } data;
} avm_memcell;

typedef struct avm_table_bucket {
    avm_memcell key;
    avm_memcell value;
    struct avm_table_bucket* next;
} avm_table_bucket;

typedef struct avm_table {
    unsigned refCounter;
    avm_table_bucket* strIndexed[AVM_TABLE_HASHSIZE];
    avm_table_bucket* numIndexed[AVM_TABLE_HASHSIZE];
    unsigned total;
} avm_table;

typedef void (*execute_func_t)(instruction*);
typedef void (*memclear_func_t)(avm_memcell*);
typedef char* (*tostring_func_t)(avm_memcell*);
typedef unsigned char (*tobool_func_t)(avm_memcell*);
typedef double (*arithmetic_func_t)(double, double);
typedef unsigned char (*cmp_func_t)(double, double);
typedef void (*library_func_t)(void);

extern avm_memcell stack[AVM_STACKSIZE];
extern avm_memcell ax, bx, cx;
extern avm_memcell retval;
extern unsigned top, topsp;
extern unsigned char executionFinished;
extern unsigned pc;
extern unsigned currLine;
extern unsigned totalActuals;

void avm_initialize(void);
void avm_warning(char* format, ...);
void avm_error(char* format, ...);

void avm_assign(avm_memcell* lv, avm_memcell* rv);
void avm_memcellclear(avm_memcell* m);
char* avm_tostring(avm_memcell* m);
unsigned char avm_tobool(avm_memcell* m);

avm_memcell* avm_translate_operand(vmarg* arg, avm_memcell* reg);

void avm_dec_top(void);
void avm_push_envvalue(unsigned val);
void avm_callsaveenvironment(void);
unsigned avm_get_envvalue(unsigned i);

avm_table* avm_tablenew(void);
void avm_tabledestroy(avm_table* t);
avm_memcell* avm_tablegetelem(avm_table* table, avm_memcell* index);
void avm_tablesetelem(avm_table* table, avm_memcell* index, avm_memcell* content);
void avm_tableinc_refcounter(avm_table* t);
void avm_tabledec_refcounter(avm_table* t);
void avm_tablebucketsinit(avm_table_bucket** p);
void avm_tablebucketsdestroy(avm_table_bucket** p);

library_func_t avm_getlibraryfunc(char* id);
void avm_registerlibfunc(char* id, library_func_t addr);
void avm_calllibfunc(char* funcName);

void execute_cycle(void);
unsigned avm_totalactuals(void);
avm_memcell* avm_getactual(unsigned i);

void execute_assign(instruction* instr);
void execute_add(instruction* instr);
void execute_sub(instruction* instr);
void execute_mul(instruction* instr);
void execute_div(instruction* instr);
void execute_mod(instruction* instr);
void execute_uminus(instruction* instr);
void execute_and(instruction* instr);
void execute_or(instruction* instr);
void execute_not(instruction* instr);
void execute_jeq(instruction* instr);
void execute_jne(instruction* instr);
void execute_jle(instruction* instr);
void execute_jge(instruction* instr);
void execute_jlt(instruction* instr);
void execute_jgt(instruction* instr);
void execute_jump(instruction* instr);
void execute_call(instruction* instr);
void execute_pusharg(instruction* instr);
void execute_funcenter(instruction* instr);
void execute_funcexit(instruction* instr);
void execute_newtable(instruction* instr);
void execute_tablegetelem(instruction* instr);
void execute_tablesetelem(instruction* instr);
void execute_nop(instruction* instr);

double add_impl(double x, double y);
double sub_impl(double x, double y);
double mul_impl(double x, double y);
double div_impl(double x, double y);
double mod_impl(double x, double y);

void libfunc_print(void);
void libfunc_typeof(void);
void libfunc_totalarguments(void);
void libfunc_argument(void);
double get_numeric_value(avm_memcell* cell);

int avm_load_binary(const char* filename);

#endif