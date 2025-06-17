#ifndef TARGET_CODE_H
#define TARGET_CODE_H

#include "quads.h"
#include "expr.h"
#include "symtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef enum vmopcode {
    assign_v, add_v, sub_v, mul_v, div_v, mod_v, uminus_v,
    and_v, or_v, not_v, jeq_v, jne_v, jle_v, jge_v, jlt_v, jgt_v,
    call_v, pusharg_v, funcenter_v, funcexit_v, newtable_v,
    tablegetelem_v, tablesetelem_v, nop_v, jump_v
} vmopcode;

typedef enum vmarg_t {
    label_a = 0,
    global_a = 1,
    formal_a = 2,
    local_a = 3,
    number_a = 4,
    string_a = 5,
    bool_a = 6,
    nil_a = 7,
    userfunc_a = 8,
    libfunc_a = 9,
    retval_a = 10,
    undef_a = 11
} vmarg_t;

typedef struct vmarg {
    vmarg_t type;
    unsigned val;
} vmarg;

typedef struct instruction {
    vmopcode opcode;
    vmarg result;
    vmarg arg1;
    vmarg arg2;
    unsigned srcLine;
} instruction;

extern double* numConsts;
extern unsigned totalNumConsts;
extern char** stringConsts;
extern unsigned totalStringConsts;
extern char** namedLibfuncs;
extern unsigned totalNamedLibfuncs;

typedef struct userfunc {
    unsigned address;
    unsigned localSize;
    char* id;
} userfunc;

extern userfunc* userFuncs;
extern unsigned totalUserFuncs;

extern instruction* instructions;
extern unsigned totalInstructions;
extern unsigned currentInstruction;

struct incomplete_jump {
    unsigned int instrNo;
    unsigned int iaddress;
    struct incomplete_jump* next;
};

extern struct incomplete_jump* ij_head;
extern unsigned ij_total;

void expand_instructions(void);
void emit_instruction(vmopcode opcode, vmarg* result, vmarg* arg1, vmarg* arg2, unsigned srcLine);
void make_operand(expr* e, vmarg* arg);
void make_numberoperand(vmarg* arg, double val);
void make_booloperand(vmarg* arg, unsigned char val);
void make_retvaloperand(vmarg* arg);

unsigned consts_newstring(char* s);
unsigned consts_newnumber(double n);
unsigned userfuncs_newfunc(SymbolTableEntry* sym);
unsigned libfuncs_newused(char* s);

void generate_target_code(void);
void generate_ASSIGN(quad* q);
void generate_ADD(quad* q);
void generate_SUB(quad* q);
void generate_MUL(quad* q);
void generate_DIV(quad* q);
void generate_MOD(quad* q);
void generate_UMINUS(quad* q);
void generate_AND(quad* q);
void generate_OR(quad* q);
void generate_NOT(quad* q);
void generate_IF_EQ(quad* q);
void generate_IF_NOTEQ(quad* q);
void generate_IF_GREATER(quad* q);
void generate_IF_GREATEREQ(quad* q);
void generate_IF_LESS(quad* q);
void generate_IF_LESSEQ(quad* q);
void generate_JUMP(quad* q);
void generate_CALL(quad* q);
void generate_PARAM(quad* q);
void generate_GETRETVAL(quad* q);
void generate_FUNCSTART(quad* q);
void generate_RETURN(quad* q);
void generate_FUNCEND(quad* q);
void generate_TABLECREATE(quad* q);
void generate_TABLEGETELEM(quad* q);
void generate_TABLESETELEM(quad* q);
void generate_NOP(quad* q);

void add_incomplete_jump(unsigned instrNo, unsigned iaddress);
void patch_incomplete_jumps(void);

void print_instructions_text(const char* filename);
void write_instructions_binary(const char* filename);

#endif