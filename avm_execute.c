#include "avm.h"

extern avm_memcell stack[AVM_STACKSIZE];
extern avm_memcell ax, bx, cx, retval;
extern unsigned top, topsp;
extern unsigned char executionFinished;
extern unsigned pc, currLine, totalActuals;
extern instruction* instructions;
extern unsigned currentInstruction;
extern arithmetic_func_t arithmeticFuncs[];
extern char* typeStrings[];

execute_func_t executeFuncs[] = {
    execute_assign,
    execute_add,
    execute_sub,
    execute_mul,
    execute_div,
    execute_mod,
    execute_uminus,
    execute_and,
    execute_or,
    execute_not,
    execute_jeq,
    execute_jne,
    execute_jle,
    execute_jge,
    execute_jlt,
    execute_jgt,
    execute_call,
    execute_pusharg,
    execute_funcenter,
    execute_funcexit,
    execute_newtable,
    execute_tablegetelem,
    execute_tablesetelem,
    execute_nop,
    execute_jump
};

void execute_cycle(void) {
    if (executionFinished)
        return;

    if (pc >= currentInstruction) {
        executionFinished = 1;
        return;
    }

    instruction* instr = instructions + pc;
    assert(instr->opcode >= 0 && instr->opcode < 25);

   

    if (instr->srcLine)
        currLine = instr->srcLine;

    unsigned oldPC = pc;
    (*executeFuncs[instr->opcode])(instr);
    
    if (pc == oldPC) {
        ++pc;
    } 
}

void execute_assign(instruction* instr) {
    avm_memcell* lv = avm_translate_operand(&instr->result, (avm_memcell*) 0);
    avm_memcell* rv = avm_translate_operand(&instr->arg1, &ax);
    
    assert(lv && ((&stack[AVM_STACKSIZE-1] >= lv && lv >= &stack[0]) || lv == &retval));
    assert(rv);
    
  
    if (rv->type == undef_m) {
        avm_memcellclear(lv);
        lv->type = nil_m;
        return;
    } 
    avm_assign(lv, rv);
    
}

void execute_arithmetic(instruction* instr) {
    avm_memcell* lv = avm_translate_operand(&instr->result, (avm_memcell*) 0);
    avm_memcell* rv1 = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* rv2 = avm_translate_operand(&instr->arg2, &bx);

    assert(lv && ((&stack[AVM_STACKSIZE-1] >= lv && lv >= &stack[0]) || lv == &retval));
    assert(rv1 && rv2);

    if (rv1->type != number_m || rv2->type != number_m) {
        avm_error("not a number in arithmetic! (left: %s, right: %s)", 
                  typeStrings[rv1->type], typeStrings[rv2->type]);
        executionFinished = 1;
    } else {
        vmopcode op = instr->opcode;
        
        if ((op == div_v || op == mod_v) && rv2->data.numVal == 0.0) {
            avm_error("%s by zero!", (op == div_v) ? "division" : "modulo");
            executionFinished = 1;
            return;
        }
        
        double result = 0.0;
        switch (op) {
            case add_v:
                result = rv1->data.numVal + rv2->data.numVal;
                break;
            case sub_v:
                result = rv1->data.numVal - rv2->data.numVal;
                break;
            case mul_v:
                result = rv1->data.numVal * rv2->data.numVal;
                break;
            case div_v:
                result = rv1->data.numVal / rv2->data.numVal;
                break;
            case mod_v:
                result = mod_impl(rv1->data.numVal, rv2->data.numVal);
                break;
            default:
                assert(0); 
        }
        
        avm_memcellclear(lv);
        lv->type = number_m;
        lv->data.numVal = result;
    }
}

void execute_add(instruction* instr) {
    execute_arithmetic(instr);
}

void execute_sub(instruction* instr) {
    execute_arithmetic(instr);
}

void execute_mul(instruction* instr) {
    execute_arithmetic(instr);
}

void execute_div(instruction* instr) {
    execute_arithmetic(instr);
}

void execute_mod(instruction* instr) {
    execute_arithmetic(instr);
}

void execute_uminus(instruction* instr) {
    avm_memcell* lv = avm_translate_operand(&instr->result, (avm_memcell*) 0);
    avm_memcell* rv = avm_translate_operand(&instr->arg1, &ax);

    assert(lv && ((&stack[AVM_STACKSIZE-1] >= lv && lv >= &stack[0]) || lv == &retval));
    assert(rv);

    if (rv->type != number_m) {
        avm_error("not a number in unary minus!");
        executionFinished = 1;
    } else {
        avm_memcellclear(lv);
        lv->type = number_m;
        lv->data.numVal = -rv->data.numVal;
    }
}

void execute_and(instruction* instr) {
    avm_memcell* lv = avm_translate_operand(&instr->result, (avm_memcell*) 0);
    avm_memcell* rv1 = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* rv2 = avm_translate_operand(&instr->arg2, &bx);

    assert(lv && ((&stack[AVM_STACKSIZE-1] >= lv && lv >= &stack[0]) || lv == &retval));
    assert(rv1 && rv2);

    avm_memcellclear(lv);
    lv->type = bool_m;
    lv->data.boolVal = avm_tobool(rv1) && avm_tobool(rv2);
}

void execute_or(instruction* instr) {
    avm_memcell* lv = avm_translate_operand(&instr->result, (avm_memcell*) 0);
    avm_memcell* rv1 = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* rv2 = avm_translate_operand(&instr->arg2, &bx);

    assert(lv && ((&stack[AVM_STACKSIZE-1] >= lv && lv >= &stack[0]) || lv == &retval));
    assert(rv1 && rv2);

    avm_memcellclear(lv);
    lv->type = bool_m;
    lv->data.boolVal = avm_tobool(rv1) || avm_tobool(rv2);
}

void execute_not(instruction* instr) {
    avm_memcell* lv = avm_translate_operand(&instr->result, (avm_memcell*) 0);
    avm_memcell* rv = avm_translate_operand(&instr->arg1, &ax);

    assert(lv && ((&stack[AVM_STACKSIZE-1] >= lv && lv >= &stack[0]) || lv == &retval));
    assert(rv);

    avm_memcellclear(lv);
    lv->type = bool_m;
    lv->data.boolVal = !avm_tobool(rv);
}
double get_numeric_value(avm_memcell* cell) {
    switch (cell->type) {
        case number_m:
            return cell->data.numVal;
        case bool_m:
            return cell->data.boolVal ? 1.0 : 0.0;
        case nil_m:
            return 0.0;
        default:
            return 0.0;
    }
}

void execute_jeq(instruction* instr) {
    assert(instr->result.type == label_a);

    avm_memcell* rv1 = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* rv2 = avm_translate_operand(&instr->arg2, &bx);

    unsigned char result = 0;

    if (rv1->type == undef_m || rv2->type == undef_m) {
        if (rv1->type == undef_m && rv2->type == undef_m) {
            result = 1;
        } else {
            result = 0;
        }
    }
    else if (rv1->type == nil_m || rv2->type == nil_m) {
        result = (rv1->type == nil_m && rv2->type == nil_m);
    }
    else if (rv1->type == bool_m || rv2->type == bool_m) {
        result = (avm_tobool(rv1) == avm_tobool(rv2));
    }
    else if (rv1->type != rv2->type) {
        result = 0;
    }
    else {
        switch (rv1->type) {
            case number_m:
                result = (rv1->data.numVal == rv2->data.numVal);
                break;
            case string_m:
                result = (strcmp(rv1->data.strVal, rv2->data.strVal) == 0);
                break;
            case table_m:
                result = (rv1->data.tableVal == rv2->data.tableVal);
                break;
            case userfunc_m:
                result = (rv1->data.funcVal == rv2->data.funcVal);
                break;
            case libfunc_m:
                result = (strcmp(rv1->data.libfuncVal, rv2->data.libfuncVal) == 0);
                break;
            default:
                result = 0;
        }
    }

    if (!executionFinished && result) {
        unsigned target = instr->result.val;
        
        if (target >= currentInstruction) {
            executionFinished = 1;
        } else {
            pc = target;
        }
    }
}

void execute_jne(instruction* instr) {
    assert(instr->result.type == label_a);

    avm_memcell* rv1 = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* rv2 = avm_translate_operand(&instr->arg2, &bx);

    unsigned char result = 0;

    if (rv1->type == undef_m || rv2->type == undef_m) {
        if (rv1->type == undef_m && rv2->type == undef_m) {
            result = 0; 
        } else {
            result = 1; 
        }
    }
    else if (rv1->type == nil_m || rv2->type == nil_m) {
        result = !(rv1->type == nil_m && rv2->type == nil_m);
    }
    else if (rv1->type == bool_m || rv2->type == bool_m) {
        result = (avm_tobool(rv1) != avm_tobool(rv2));
    }
    else if (rv1->type != rv2->type) {
        result = 1; 
    }
    else {
        switch (rv1->type) {
            case number_m:
                result = (rv1->data.numVal != rv2->data.numVal);
                break;
            case string_m:
                result = (strcmp(rv1->data.strVal, rv2->data.strVal) != 0);
                break;
            case table_m:
                result = (rv1->data.tableVal != rv2->data.tableVal);
                break;
            case userfunc_m:
                result = (rv1->data.funcVal != rv2->data.funcVal);
                break;
            case libfunc_m:
                result = (strcmp(rv1->data.libfuncVal, rv2->data.libfuncVal) != 0);
                break;
            default:
                result = 1;
        }
    }

     if (!executionFinished && result) {
        unsigned target = instr->result.val;
        if (target >= currentInstruction) {
            executionFinished = 1;
        } else {
            pc = target;
        }
    }
}
void execute_jge(instruction* instr) {
    assert(instr->result.type == label_a);

    avm_memcell* rv1 = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* rv2 = avm_translate_operand(&instr->arg2, &bx);

    unsigned char result = 0;

    if (rv1->type == number_m || rv1->type == bool_m || rv1->type == nil_m) {
        if (rv2->type == number_m || rv2->type == bool_m || rv2->type == nil_m) {
            double val1 = get_numeric_value(rv1);
            double val2 = get_numeric_value(rv2);
            result = (val1 >= val2);
        } else {
            avm_error("not numbers in comparison!");
            return;
        }
    } else {
        avm_error("not numbers in comparison!");
        return;
    }

    if (!executionFinished && result)
        pc = instr->result.val;
}

void execute_jle(instruction* instr) {
    assert(instr->result.type == label_a);

    avm_memcell* rv1 = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* rv2 = avm_translate_operand(&instr->arg2, &bx);

    unsigned char result = 0;

    if (rv1->type == number_m || rv1->type == bool_m || rv1->type == nil_m) {
        if (rv2->type == number_m || rv2->type == bool_m || rv2->type == nil_m) {
            double val1 = get_numeric_value(rv1);
            double val2 = get_numeric_value(rv2);
            result = (val1 <= val2);
        } else {
            avm_error("not numbers in comparison!");
            return;
        }
    } else {
        avm_error("not numbers in comparison!");
        return;
    }

    if (!executionFinished && result)
        pc = instr->result.val;
}

void execute_jlt(instruction* instr) {
    assert(instr->result.type == label_a);

    avm_memcell* rv1 = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* rv2 = avm_translate_operand(&instr->arg2, &bx);

   

    unsigned char result = 0;

    if (rv1->type == number_m || rv1->type == bool_m || rv1->type == nil_m) {
        if (rv2->type == number_m || rv2->type == bool_m || rv2->type == nil_m) {
            double val1 = get_numeric_value(rv1);
            double val2 = get_numeric_value(rv2);
            result = (val1 < val2);
            
        } else {
            avm_error("not numbers in comparison!");
            return;
        }
    } else {
        avm_error("not numbers in comparison!");
        return;
    }

    if (!executionFinished && result) {
        pc = instr->result.val;
    } 
}

void execute_jgt(instruction* instr) {
    assert(instr->result.type == label_a);

    avm_memcell* rv1 = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* rv2 = avm_translate_operand(&instr->arg2, &bx);

    unsigned char result = 0;

    if (rv1->type == number_m || rv1->type == bool_m || rv1->type == nil_m) {
        if (rv2->type == number_m || rv2->type == bool_m || rv2->type == nil_m) {
            double val1 = get_numeric_value(rv1);
            double val2 = get_numeric_value(rv2);
            result = (val1 > val2);
        } else {
            avm_error("not numbers in comparison!");
            return;
        }
    } else {
        avm_error("not numbers in comparison!");
        return;
    }

    if (!executionFinished && result)
        pc = instr->result.val;
}

void execute_jump(instruction* instr) {
    assert(instr->result.type == label_a);
    
    unsigned target = instr->result.val;
    
    if (target >= currentInstruction) {
        pc = currentInstruction;  
        executionFinished = 1;
    } else {
        pc = target;
    }
}

void execute_call(instruction* instr) {
    avm_memcell* func = avm_translate_operand(&instr->arg1, &ax);
    assert(func);

    switch (func->type) {
        case userfunc_m: {
            avm_callsaveenvironment();
            pc = func->data.funcVal;
            assert(pc < currentInstruction);
            assert(instructions[pc].opcode == funcenter_v);
            break;
        }
        case string_m:
            avm_calllibfunc(func->data.strVal);
            break;
        case libfunc_m:
            avm_calllibfunc(func->data.libfuncVal);
            break;
        default: {
            char* s = avm_tostring(func);
            printf("run time error in line %d\n", instr->srcLine);
            free(s);
            executionFinished = 1;
        }
    }
}

void execute_pusharg(instruction* instr) {
    avm_memcell* arg = avm_translate_operand(&instr->arg1, &ax);
    assert(arg);

    avm_assign(&stack[top], arg);
    ++totalActuals;
    avm_dec_top();
}

void execute_funcenter(instruction* instr) {
    avm_memcell* func = avm_translate_operand(&instr->arg1, &ax);
    assert(func);
    assert(pc == func->data.funcVal);

    userfunc* funcInfo = NULL;
    for (unsigned i = 0; i < totalUserFuncs; i++) {
        if (userFuncs[i].address == pc) {
            funcInfo = &userFuncs[i];
            break;
        }
    }
    
    if (!funcInfo) {
        avm_error("Cannot find user function info for address %u", pc);
        return;
    }

    totalActuals = 0;
    topsp = top;
    top = top - funcInfo->localSize;
}

void execute_funcexit(instruction* unused) {
    unsigned oldTop = top;
    top = avm_get_envvalue(topsp + AVM_SAVEDTOP_OFFSET);
    pc = avm_get_envvalue(topsp + AVM_SAVEDPC_OFFSET);
    topsp = avm_get_envvalue(topsp + AVM_SAVEDTOPSP_OFFSET);

    while (++oldTop <= top)
        avm_memcellclear(&stack[oldTop]);
}

void execute_newtable(instruction* instr) {
    avm_memcell* lv = avm_translate_operand(&instr->result, (avm_memcell*) 0);
    
    
    assert(lv && ((&stack[AVM_STACKSIZE-1] >= lv && lv >= &stack[0]) || lv == &retval));

    avm_memcellclear(lv);
    lv->type = table_m;
    lv->data.tableVal = avm_tablenew();
    avm_tableinc_refcounter(lv->data.tableVal);
    
}
void execute_tablegetelem(instruction* instr) {
    avm_memcell* lv = avm_translate_operand(&instr->result, (avm_memcell*) 0);
    avm_memcell* t = avm_translate_operand(&instr->arg1, (avm_memcell*) 0);
    avm_memcell* i = avm_translate_operand(&instr->arg2, &ax);

    if (!lv || (lv != &retval && (lv < &stack[0] || lv > &stack[AVM_STACKSIZE-1]))) {
        printf("ERROR: Invalid result location\n");
        executionFinished = 1;
        return;
    }

    if (!t || t < &stack[0] || t > &stack[AVM_STACKSIZE-1]) {
        printf("ERROR: Table pointer %p is outside stack bounds [%p, %p]\n", 
               (void*)t, (void*)&stack[0], (void*)&stack[AVM_STACKSIZE-1]);
        executionFinished = 1;
        return;
    }
    
    if (!i) {
        printf("ERROR: Index pointer is NULL\n");
        executionFinished = 1;
        return;
    }

    avm_memcellclear(lv);
    lv->type = nil_m;

    if (t->type != table_m) {
        avm_error("illegal use of type %s as table!", typeStrings[t->type]);
    } else {
        avm_memcell* content = avm_tablegetelem(t->data.tableVal, i);
        if (content) {
            avm_assign(lv, content);
        } else {
            char* ts = avm_tostring(t);
            char* is = avm_tostring(i);
            avm_warning("%s[%s] not found!", ts, is);
            free(ts);
            free(is);
        }
    }
}
void execute_tablesetelem(instruction* instr) {
    avm_memcell* t = avm_translate_operand(&instr->result, (avm_memcell*) 0);
    avm_memcell* i = avm_translate_operand(&instr->arg1, &ax);
    avm_memcell* c = avm_translate_operand(&instr->arg2, &bx);

    if (!t) {
        printf("ERROR: Table pointer is NULL!\n");
        executionFinished = 1;
        return;
    }

    if (t < &stack[0] || t > &stack[AVM_STACKSIZE-1]) {
        printf("ERROR: Table pointer %p is outside stack bounds [%p, %p]\n", 
               (void*)t, (void*)&stack[0], (void*)&stack[AVM_STACKSIZE-1]);
        executionFinished = 1;
        return;
    }

    if (!i) {
        printf("ERROR: Index pointer is NULL!\n");
        executionFinished = 1;
        return;
    }
    
    if (!c) {
        printf("ERROR: Content pointer is NULL!\n");
        executionFinished = 1;
        return;
    }

    if (t->type != table_m) {
        avm_error("illegal use of type %s as table!", typeStrings[t->type]);
    } else {
        avm_tablesetelem(t->data.tableVal, i, c);
    }
}


void execute_nop(instruction* instr) {}