#include "target_code.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

instruction* instructions = NULL;
unsigned totalInstructions = 0;
unsigned currentInstruction = 0;

double* numConsts = NULL;
unsigned totalNumConsts = 0;
char** stringConsts = NULL;
unsigned totalStringConsts = 0;
char** namedLibfuncs = NULL;
unsigned totalNamedLibfuncs = 0;

userfunc* userFuncs = NULL;
unsigned totalUserFuncs = 0;

struct incomplete_jump* ij_head = NULL;
unsigned ij_total = 0;

extern quad* quads;
extern unsigned currQuad;

void expand_instructions(void) {
    if (totalInstructions == 0) {
        totalInstructions = 1024;
        instructions = malloc(totalInstructions * sizeof(instruction));
        assert(instructions);
        return;
    }
    
    instruction* newInstr = realloc(instructions, 2 * totalInstructions * sizeof(instruction));
    assert(newInstr);
    instructions = newInstr;
    totalInstructions *= 2;
}

void emit_instruction(vmopcode opcode, vmarg* result, vmarg* arg1, vmarg* arg2, unsigned srcLine) {
    if (currentInstruction == totalInstructions) {
        expand_instructions();
    }
    
    instruction* instr = &instructions[currentInstruction++];
    instr->opcode = opcode;
    instr->srcLine = srcLine;
    
    if (result) instr->result = *result;
    else memset(&instr->result, 0, sizeof(vmarg));
    
    if (arg1) instr->arg1 = *arg1;
    else memset(&instr->arg1, 0, sizeof(vmarg));
    
    if (arg2) instr->arg2 = *arg2;
    else memset(&instr->arg2, 0, sizeof(vmarg));
}

unsigned consts_newnumber(double n) {
    for (unsigned i = 0; i < totalNumConsts; i++) {
        if (numConsts[i] == n) return i;
    }
    
    if (totalNumConsts == 0) {
        numConsts = malloc(sizeof(double));
    } else {
        numConsts = realloc(numConsts, (totalNumConsts + 1) * sizeof(double));
    }
    assert(numConsts);
    
    numConsts[totalNumConsts] = n;
    return totalNumConsts++;
}

unsigned consts_newstring(char* s) {
    for (unsigned i = 0; i < totalStringConsts; i++) {
        if (strcmp(stringConsts[i], s) == 0) return i;
    }
    
    if (totalStringConsts == 0) {
        stringConsts = malloc(sizeof(char*));
    } else {
        stringConsts = realloc(stringConsts, (totalStringConsts + 1) * sizeof(char*));
    }
    assert(stringConsts);
    
    stringConsts[totalStringConsts] = strdup(s);
    return totalStringConsts++;
}

unsigned libfuncs_newused(char* s) {
    for (unsigned i = 0; i < totalNamedLibfuncs; i++) {
        if (strcmp(namedLibfuncs[i], s) == 0) return i;
    }
    
    if (totalNamedLibfuncs == 0) {
        namedLibfuncs = malloc(sizeof(char*));
    } else {
        namedLibfuncs = realloc(namedLibfuncs, (totalNamedLibfuncs + 1) * sizeof(char*));
    }
    assert(namedLibfuncs);
    
    namedLibfuncs[totalNamedLibfuncs] = strdup(s);
    return totalNamedLibfuncs++;
}

unsigned userfuncs_newfunc(SymbolTableEntry* sym) {
    if (totalUserFuncs == 0) {
        userFuncs = malloc(sizeof(userfunc));
    } else {
        userFuncs = realloc(userFuncs, (totalUserFuncs + 1) * sizeof(userfunc));
    }
    assert(userFuncs);
    
    userFuncs[totalUserFuncs].address = 0; 
    userFuncs[totalUserFuncs].localSize = sym->value.funcVal->totalLocals;
    userFuncs[totalUserFuncs].id = strdup(sym->value.funcVal->name);
    return totalUserFuncs++;
}

void make_operand(expr* e, vmarg* arg) {
    if (!e) {
        arg->type = undef_a;
        return;
    }
    
    
    switch(e->type) {
        case var_e:
        case tableitem_e:
        case arithexpr_e:
        case boolexpr_e:
        case newtable_e:     
        case assignexpr_e: {
            if (!e->sym) {
                arg->type = undef_a;
                return;
            }
            
            if (e->sym->type == LIBFUNC) {
                arg->type = libfunc_a;
                arg->val = libfuncs_newused(e->sym->value.funcVal->name);
                return;
            }
            if (e->sym->type == USERFUNC) {
                arg->type = userfunc_a;
                arg->val = userfuncs_newfunc(e->sym);
                return;
            }
            
            arg->val = e->sym->offset;
            switch (e->sym->space) {
                case programvar: 
                    arg->type = global_a; 
                    break;
                case functionlocal: 
                    arg->type = local_a; 
                    break;
                case formalarg: 
                    arg->type = formal_a; 
                    break;
                default: 
                    arg->type = undef_a;
                    break;
            }
            break;
        }
        case constbool_e:
            arg->type = bool_a;
            arg->val = e->boolConst ? 1 : 0;
            break;
        case conststring_e:
            arg->type = string_a;
            arg->val = consts_newstring(e->strConst);
            break;
        case constnum_e:
            arg->type = number_a;
            arg->val = consts_newnumber(e->numConst);
            break;
        case nil_e:
            arg->type = nil_a;
            arg->val = 0;
            break;
        case programfunc_e:
            arg->type = userfunc_a;
            arg->val = userfuncs_newfunc(e->sym);
            break;
        case libraryfunc_e:
            arg->type = libfunc_a;
            arg->val = libfuncs_newused(e->sym->value.funcVal->name);
            break;
        default: 
            arg->type = undef_a;
            break;
    }
}

void make_numberoperand(vmarg* arg, double val) {
    arg->type = number_a;
    arg->val = consts_newnumber(val);
}

void make_booloperand(vmarg* arg, unsigned char val) {
    arg->type = bool_a;
    arg->val = val;
}

void make_retvaloperand(vmarg* arg) {
    arg->type = retval_a;
    arg->val = 0;
}

void add_incomplete_jump(unsigned instrNo, unsigned iaddress) {
    struct incomplete_jump* ij = malloc(sizeof(struct incomplete_jump));
    ij->instrNo = instrNo;
    ij->iaddress = iaddress;
    ij->next = ij_head;
    ij_head = ij;
    ij_total++;
}

void patch_incomplete_jumps(void) {
    struct incomplete_jump* curr = ij_head;
    while (curr) {
        unsigned target_instruction = curr->iaddress;
        
        unsigned skipped = 0;
        for (unsigned i = 0; i < curr->iaddress && i < currQuad; i++) {
            if (quads[i].op == getretval && quads[i].result && 
                quads[i].result->sym && quads[i].result->sym->value.varVal && 
                quads[i].result->sym->value.varVal->name &&
                strncmp(quads[i].result->sym->value.varVal->name, "_t", 2) == 0) {
                skipped++;
            }
        }
        
        target_instruction -= skipped;
        
        if (target_instruction < currentInstruction) {
            instructions[curr->instrNo].result.val = target_instruction;
        } else {
            instructions[curr->instrNo].result.val = currentInstruction;
        }
        
        curr = curr->next;
    }
}

typedef void (*generator_func_t)(quad*);

void generate_ASSIGN(quad* q) {
    vmarg result, arg1;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    emit_instruction(assign_v, &result, &arg1, NULL, q->line);
}

void generate_ADD(quad* q) {
    vmarg result, arg1, arg2;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(add_v, &result, &arg1, &arg2, q->line);
}

void generate_SUB(quad* q) {
    vmarg result, arg1, arg2;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(sub_v, &result, &arg1, &arg2, q->line);
}

void generate_MUL(quad* q) {
    vmarg result, arg1, arg2;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(mul_v, &result, &arg1, &arg2, q->line);
}

void generate_DIV(quad* q) {
    vmarg result, arg1, arg2;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(div_v, &result, &arg1, &arg2, q->line);
}

void generate_MOD(quad* q) {
    vmarg result, arg1, arg2;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(mod_v, &result, &arg1, &arg2, q->line);
}

void generate_UMINUS(quad* q) {
    vmarg result, arg1;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    emit_instruction(uminus_v, &result, &arg1, NULL, q->line);
}

void generate_AND(quad* q) {
    vmarg result, arg1, arg2;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(and_v, &result, &arg1, &arg2, q->line);
}

void generate_OR(quad* q) {
    vmarg result, arg1, arg2;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(or_v, &result, &arg1, &arg2, q->line);
}

void generate_NOT(quad* q) {
    vmarg result, arg1;
    make_operand(q->result, &result);
    make_operand(q->arg1, &arg1);
    emit_instruction(not_v, &result, &arg1, NULL, q->line);
}

void generate_IF_LESS(quad* q) {
    vmarg result, arg1, arg2;
    result.type = label_a;
    
    result.val = q->label;  
    
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(jlt_v, &result, &arg1, &arg2, q->line);
    
}

void generate_IF_EQ(quad* q) {
    vmarg result, arg1, arg2;
    result.type = label_a;
    result.val = q->label;
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(jeq_v, &result, &arg1, &arg2, q->line);
}

void generate_JUMP(quad* q) {
    vmarg result;
    result.type = label_a;
    result.val = q->label;
    emit_instruction(jump_v, &result, NULL, NULL, q->line);
}

void generate_IF_NOTEQ(quad* q) {
    vmarg result, arg1, arg2;
    result.type = label_a;
    result.val = q->label;
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(jne_v, &result, &arg1, &arg2, q->line);
}

void generate_IF_GREATER(quad* q) {
    vmarg result, arg1, arg2;
    result.type = label_a;
    result.val = q->label;
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(jgt_v, &result, &arg1, &arg2, q->line);
}

void generate_IF_GREATEREQ(quad* q) {
    vmarg result, arg1, arg2;
    result.type = label_a;
    result.val = q->label;
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(jge_v, &result, &arg1, &arg2, q->line);
}

void generate_IF_LESSEQ(quad* q) {
    vmarg result, arg1, arg2;
    result.type = label_a;
    result.val = q->label;
    make_operand(q->arg1, &arg1);
    make_operand(q->arg2, &arg2);
    emit_instruction(jle_v, &result, &arg1, &arg2, q->line);
}
void generate_CALL(quad* q) {
    vmarg arg1;
    make_operand(q->arg1, &arg1);
    emit_instruction(call_v, NULL, &arg1, NULL, q->line);
}

void generate_PARAM(quad* q) {
    vmarg arg1;
    make_operand(q->arg1, &arg1);
    emit_instruction(pusharg_v, NULL, &arg1, NULL, q->line);
}

void generate_GETRETVAL(quad* q) {
    if (!q->result) {
        emit_instruction(nop_v, NULL, NULL, NULL, q->line);
        return;
    }
    
    vmarg result, retval_arg;
    make_operand(q->result, &result);
    make_retvaloperand(&retval_arg);
    
    emit_instruction(assign_v, &result, &retval_arg, NULL, q->line);
}

void generate_FUNCSTART(quad* q) {
    if (!q->arg1 || !q->arg1->sym) {
        fprintf(stderr, "Invalid function start quad\n");
        return;
    }
    
    vmarg func;
    make_operand(q->arg1, &func);
    
    if (func.type == userfunc_a && func.val < totalUserFuncs) {
        userFuncs[func.val].address = currentInstruction;
    }
    
    emit_instruction(funcenter_v, &func, NULL, NULL, q->line);
}

void generate_RETURN(quad* q) {
    vmarg retval_arg;
    make_retvaloperand(&retval_arg);
    
    if (q->arg1) {
        vmarg arg1;
        make_operand(q->arg1, &arg1);
        emit_instruction(assign_v, &retval_arg, &arg1, NULL, q->line);
    }
    
    emit_instruction(funcexit_v, NULL, NULL, NULL, q->line);
}

void generate_FUNCEND(quad* q) {
    vmarg retval_arg;
    make_retvaloperand(&retval_arg);
    vmarg nil_arg;
    nil_arg.type = nil_a;
    nil_arg.val = 0;
    emit_instruction(assign_v, &retval_arg, &nil_arg, NULL, q->line);
    emit_instruction(funcexit_v, NULL, NULL, NULL, q->line);
}

void generate_TABLECREATE(quad* q) {
    vmarg result;
    make_operand(q->result, &result);
    emit_instruction(newtable_v, &result, NULL, NULL, q->line);
}

void generate_TABLEGETELEM(quad* q) {
    vmarg result, table, index;
    
    make_operand(q->result, &result);  
    make_operand(q->arg1, &table);    
    make_operand(q->arg2, &index);     
    
    emit_instruction(tablegetelem_v, &result, &table, &index, q->line);
}

void generate_TABLESETELEM(quad* q) {

    vmarg table, index, value;

    if (q->result && q->result->type == newtable_e && q->result->sym) {
        table.type = (q->result->sym->space == programvar) ? global_a : 
                     (q->result->sym->space == functionlocal) ? local_a : formal_a;
        table.val = q->result->sym->offset;
    } else {
        make_operand(q->result, &table);
    }
    
    make_operand(q->arg1, &index);    
    make_operand(q->arg2, &value);    
    
    emit_instruction(tablesetelem_v, &table, &index, &value, q->line);
}
void generate_NOP(quad* q) {
    emit_instruction(nop_v, NULL, NULL, NULL, q->line);
}

generator_func_t generators[] = {
    generate_ASSIGN,    
    generate_ADD,      
    generate_SUB,      
    generate_MUL,       
    generate_DIV,       
    generate_MOD,       
    generate_UMINUS,    
    generate_AND,       
    generate_OR,        
    generate_NOT,       
    generate_IF_EQ,     
    generate_IF_NOTEQ,  
    generate_IF_LESS,   
    generate_IF_LESSEQ, 
    generate_IF_GREATER,
    generate_IF_GREATEREQ, 
    generate_JUMP,      
    generate_PARAM,    
    generate_CALL,      
    generate_GETRETVAL, 
    generate_RETURN,   
    generate_FUNCSTART, 
    generate_FUNCEND,   
    generate_TABLECREATE,   
    generate_TABLEGETELEM,  
    generate_TABLESETELEM,  
    generate_NOP       
};

void generate_target_code(void) {
    printf("Starting target code generation with %d quads\n", currQuad);
    
    for (unsigned i = 0; i < currQuad; ++i) {
        quad* current_quad = &quads[i];
        
        (*generators[current_quad->op])(current_quad);
    }
}

void print_instructions_text(const char* filename) {
    static const char* opNames[] = {
        "assign", "add", "sub", "mul", "div", "mod", "uminus",
        "and", "or", "not", "jeq", "jne", "jle", "jge", "jlt", "jgt",
        "call", "pusharg", "funcenter", "funcexit", "newtable",
        "tablegetelem", "tablesetelem", "nop", "jump"
    };
    
    static const char* argTypeNames[] = {
        "label", "global", "formal", "local", "number", "string",
        "bool", "nil", "userfunc", "libfunc", "retval", "undef"
    };
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error: Could not open file %s for writing\n", filename);
        return;
    }
    
    fprintf(file, "instruction# opcode       result          arg1           arg2\n");
    fprintf(file, "----------------------------------------------------------------\n");
    
    for (unsigned i = 0; i < currentInstruction; i++) {
        instruction* instr = &instructions[i];
        
        fprintf(file, "%-12d %-12s", i, opNames[instr->opcode]);
        
        if (instr->result.type != undef_a) {
            switch (instr->result.type) {
                case label_a:
                    fprintf(file, "label:%d      ", instr->result.val);
                    break;
                case global_a:
                    fprintf(file, "global[%d]    ", instr->result.val);
                    break;
                case local_a:
                    fprintf(file, "local[%d]     ", instr->result.val);
                    break;
                case formal_a:
                    fprintf(file, "formal[%d]    ", instr->result.val);
                    break;
                case number_a:
                    fprintf(file, "num[%d]       ", instr->result.val);
                    break;
                case string_a:
                    fprintf(file, "str[%d]       ", instr->result.val);
                    break;
                case bool_a:
                    fprintf(file, "bool:%d       ", instr->result.val);
                    break;
                case nil_a:
                    fprintf(file, "nil           ");
                    break;
                case userfunc_a:
                    fprintf(file, "ufunc[%d]     ", instr->result.val);
                    break;
                case libfunc_a:
                    fprintf(file, "lfunc[%d]     ", instr->result.val);
                    break;
                case retval_a:
                    fprintf(file, "retval        ");
                    break;
                default:
                    fprintf(file, "%s[%d]       ", argTypeNames[instr->result.type], instr->result.val);
                    break;
            }
        } else {
            fprintf(file, "%-14s", "");
        }
        
        if (instr->arg1.type != undef_a) {
            switch (instr->arg1.type) {
                case label_a:
                    fprintf(file, "label:%d      ", instr->arg1.val);
                    break;
                case global_a:
                    fprintf(file, "global[%d]    ", instr->arg1.val);
                    break;
                case local_a:
                    fprintf(file, "local[%d]     ", instr->arg1.val);
                    break;
                case formal_a:
                    fprintf(file, "formal[%d]    ", instr->arg1.val);
                    break;
                case number_a:
                    fprintf(file, "num[%d]       ", instr->arg1.val);
                    break;
                case string_a:
                    fprintf(file, "str[%d]       ", instr->arg1.val);
                    break;
                case bool_a:
                    fprintf(file, "bool:%d       ", instr->arg1.val);
                    break;
                case nil_a:
                    fprintf(file, "nil           ");
                    break;
                case userfunc_a:
                    fprintf(file, "ufunc[%d]     ", instr->arg1.val);
                    break;
                case libfunc_a:
                    fprintf(file, "lfunc[%d]     ", instr->arg1.val);
                    break;
                case retval_a:
                    fprintf(file, "retval        ");
                    break;
                default:
                    fprintf(file, "%s[%d]       ", argTypeNames[instr->arg1.type], instr->arg1.val);
                    break;
            }
        } else {
            fprintf(file, "%-14s", "");
        }
        
        if (instr->arg2.type != undef_a) {
            switch (instr->arg2.type) {
                case label_a:
                    fprintf(file, "label:%d", instr->arg2.val);
                    break;
                case global_a:
                    fprintf(file, "global[%d]", instr->arg2.val);
                    break;
                case local_a:
                    fprintf(file, "local[%d]", instr->arg2.val);
                    break;
                case formal_a:
                    fprintf(file, "formal[%d]", instr->arg2.val);
                    break;
                case number_a:
                    fprintf(file, "num[%d]", instr->arg2.val);
                    break;
                case string_a:
                    fprintf(file, "str[%d]", instr->arg2.val);
                    break;
                case bool_a:
                    fprintf(file, "bool:%d", instr->arg2.val);
                    break;
                case nil_a:
                    fprintf(file, "nil");
                    break;
                case userfunc_a:
                    fprintf(file, "ufunc[%d]", instr->arg2.val);
                    break;
                case libfunc_a:
                    fprintf(file, "lfunc[%d]", instr->arg2.val);
                    break;
                case retval_a:
                    fprintf(file, "retval");
                    break;
                default:
                    fprintf(file, "%s[%d]", argTypeNames[instr->arg2.type], instr->arg2.val);
                    break;
            }
        }
        
        fprintf(file, "\n");
    }
    
    fprintf(file, "\n\nConstants Tables:\n");
    fprintf(file, "Numbers: %d entries\n", totalNumConsts);
    for (unsigned i = 0; i < totalNumConsts; i++) {
        fprintf(file, "%d: %g\n", i, numConsts[i]);
    }
    
    fprintf(file, "\nStrings: %d entries\n", totalStringConsts);
    for (unsigned i = 0; i < totalStringConsts; i++) {
        fprintf(file, "%d: \"%s\"\n", i, stringConsts[i]);
    }
    
    fprintf(file, "\nLibrary Functions: %d entries\n", totalNamedLibfuncs);
    for (unsigned i = 0; i < totalNamedLibfuncs; i++) {
        fprintf(file, "%d: %s\n", i, namedLibfuncs[i]);
    }
    
    fprintf(file, "\nUser Functions: %d entries\n", totalUserFuncs);
    for (unsigned i = 0; i < totalUserFuncs; i++) {
        fprintf(file, "%d: %s (addr: %d, locals: %d)\n", i, userFuncs[i].id, 
                userFuncs[i].address, userFuncs[i].localSize);
    }
    
    fclose(file);
    printf("Target code written to %s\n", filename);
}

void write_instructions_binary(const char* filename) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("Error: Could not open file %s for binary writing\n", filename);
        return;
    }
    
    unsigned int magic = 0x12345678;
    fwrite(&magic, sizeof(unsigned int), 1, file);
    
    fwrite(&currentInstruction, sizeof(unsigned), 1, file);
    fwrite(instructions, sizeof(instruction), currentInstruction, file);

    fwrite(&totalNumConsts, sizeof(unsigned), 1, file);
    fwrite(numConsts, sizeof(double), totalNumConsts, file);
    
    fwrite(&totalStringConsts, sizeof(unsigned), 1, file);
    for (unsigned i = 0; i < totalStringConsts; i++) {
        unsigned len = strlen(stringConsts[i]) + 1;
        fwrite(&len, sizeof(unsigned), 1, file);
        fwrite(stringConsts[i], sizeof(char), len, file);
    }
    
    fwrite(&totalNamedLibfuncs, sizeof(unsigned), 1, file);
    for (unsigned i = 0; i < totalNamedLibfuncs; i++) {
        unsigned len = strlen(namedLibfuncs[i]) + 1;
        fwrite(&len, sizeof(unsigned), 1, file);
        fwrite(namedLibfuncs[i], sizeof(char), len, file);
    }
    
    fwrite(&totalUserFuncs, sizeof(unsigned), 1, file);
    fwrite(userFuncs, sizeof(userfunc), totalUserFuncs, file);
    
    fclose(file);
    printf("Binary target code written to %s\n", filename);
}