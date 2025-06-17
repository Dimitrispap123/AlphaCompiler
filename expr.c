#include "expr.h"
#include "symtable.h"
#include "quads.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>  
extern int yylineno;

expr *global_temp = NULL;
unsigned temp_counter = 0;

expr *newexpr(expr_t t) {

    expr *e = malloc(sizeof(expr));
    if (!e) { 
        perror("malloc failed in newexpr"); 
        exit(1); 
    }
    memset(e, 0, sizeof(expr));  
     e->type = t;
    e->truelist = -1;   
    e->falselist = -1;
    return e;
}

expr* newexpr_nil(void) {
    expr* e = newexpr(nil_e);
    e->sym = NULL;
    return e;
}

expr *newexpr_constnum(double v) {
    expr *e = newexpr(constnum_e);
    e->numConst = v;
    e->sym = NULL;
    return e;
}

expr *newexpr_conststring(const char *s) {
    expr *e = newexpr(conststring_e);
    
    if (s && s[0] == '"' && s[strlen(s)-1] == '"') {
        char *no_quotes = strdup(s + 1);
        no_quotes[strlen(no_quotes) - 1] = '\0';
        e->strConst = no_quotes;
    } else {
        e->strConst = s ? strdup(s) : NULL;
    }
    
    e->sym = NULL;  
    return e;
}

expr *newexpr_constbool(unsigned b) {
    expr *e = newexpr(constbool_e);
    e->boolConst = (b != 0) ? 1 : 0;
    e->sym = NULL;  
    return e;
}

expr* newexpr_func(SymbolTableEntry* func_sym) {
    expr* e = newexpr(programfunc_e);  
    e->sym = func_sym;
    return e;
}

SymbolTableEntry *newtemp(void) {
    extern int scope;  
    extern SymbolTableEntry *head;
    
    static unsigned temp_counter = 0; 
    char namebuf[32];
    snprintf(namebuf, sizeof(namebuf), "_t%u", temp_counter++);
    
    SymbolTableEntry *sym = insert_variable(strdup(namebuf), 0, scope, LOCAL_VAR);
    sym->next = head;
    head = sym;
    
    return sym;
}

expr* member_item(expr* obj, const char* name) {
    if (!obj) return NULL;
    
    expr* base = emit_iftableitem(obj);
    
    expr* field = newexpr_conststring(name);
    
    expr* result = newexpr(tableitem_e);
    result->sym = base->sym;
    result->index = field;
    
    return result;
}

expr* emit_iftableitem(expr* e) {
    if (!e) return NULL;
    
    if (e->type != tableitem_e) {
        return e;
    }
    
    expr* result = newexpr(var_e);
    result->sym = newtemp();
    
    emitQuad(tablegetelem, e, e->index, result, yylineno);
    
    return result;
}

expr* make_arith(expr* left, expr* right, iopcode op) {
    if (!left || !right) {
        fprintf(stderr, "Error: Invalid arithmetic operation at line %d\n", yylineno);
        return NULL;
    }
    
    expr* left_val = emit_iftableitem(left);
    expr* right_val = emit_iftableitem(right);
    
    expr* result = newexpr(arithexpr_e);
    result->sym = newtemp();
    
    emitQuad(op, left_val, right_val, result, yylineno);
    
    return result;
}


expr* make_uminus(expr* e) {
    if (!e) {
        fprintf(stderr, "Error: Invalid unary minus operation at line %d\n", yylineno);
        return NULL;
    }
    
    if (e->type == constnum_e) {
        return newexpr_constnum(-e->numConst);
    }
    
    expr* result = newexpr(arithexpr_e);
    result->sym = newtemp();
    
    emitQuad(uminus, e, NULL, result, yylineno);
    
    return result;
}

expr* make_and(expr* e1, expr* e2) {
    fprintf(stderr, "Warning: make_and called - should use grammar markers\n");
    return e1;
}

expr* make_or(expr* e1, expr* e2) {
    fprintf(stderr, "Warning: make_or called - should use grammar markers\n");
    return e1;
}

expr* make_not(expr* operand) {
    if (!operand) {
        fprintf(stderr, "Error: Invalid NOT operation at line %d\n", yylineno);
        return NULL;
    }
    
    if (operand->type == constbool_e) {
        return newexpr_constbool(!operand->boolConst);
    }
    
    if (operand->type != boolexpr_e) {
        operand = bool_expr(operand);
    }
    
    expr* result = newexpr(boolexpr_e);
    result->sym = NULL;
    result->truelist = operand->falselist;
    result->falselist = operand->truelist;
    
    return result;
}

expr* make_relop(expr* left, expr* right, iopcode op) {
    if (!left || !right) {
        fprintf(stderr, "Error: Invalid relational operation at line %d\n", yylineno);
        return NULL;
    }
    
    left = emit_iftableitem(left);
    right = emit_iftableitem(right);
    
    expr* result = newexpr(boolexpr_e);
    result->sym = NULL;
    
    result->truelist = nextQuad();
    emitQuad(op, left, right, NULL, yylineno);
    
    result->falselist = nextQuad();
    emitQuad(jump, NULL, NULL, NULL, yylineno);
    
    return result;
}

expr* bool_expr(expr* e) {
    if (!e) {
        return NULL;
    }
    
    if (e->type == boolexpr_e) {
        return e;
    }
    
    if (e->type == constbool_e) {
        expr* result = newexpr(boolexpr_e);
        result->sym = NULL;
        
        if (e->boolConst) {
            result->truelist = nextQuad();
            emitQuad(jump, NULL, NULL, NULL, yylineno);
            result->falselist = -1; 
        } else {
            result->truelist = -1;
            result->falselist = nextQuad();
            emitQuad(jump, NULL, NULL, NULL, yylineno);
        }
        return result;
    }
    
    expr* result = newexpr(boolexpr_e);
    result->sym = NULL;
    
    result->truelist = nextQuad();
    emitQuad(if_eq, e, newexpr_constbool(1), NULL, yylineno);
    
    result->falselist = nextQuad();
    emitQuad(jump, NULL, NULL, NULL, yylineno);
    
    return result;
}

expr* finish_bool_expr(expr* e) {
    if (!e || e->type != boolexpr_e) {
        return e;
    }
    
    expr* result = newexpr(var_e);
    result->sym = newtemp();
    
    if (e->truelist == -1 && e->falselist == -1) {
        emitQuad(assign, newexpr_constbool(0), NULL, result, yylineno);
        return result;
    }
    
    if (e->truelist == -1) {
        emitQuad(assign, newexpr_constbool(0), NULL, result, yylineno);
        if (e->falselist >= 0) {
            patchlist(e->falselist, nextQuad());
        }
        return result;
    }
    
    if (e->falselist == -1) {
        patchlist(e->truelist, nextQuad());
        emitQuad(assign, newexpr_constbool(1), NULL, result, yylineno);
        return result;
    }
    
    patchlist(e->truelist, nextQuad());
    emitQuad(assign, newexpr_constbool(1), NULL, result, yylineno);
    
    int jump_over_false = nextQuad();
    emitQuad(jump, NULL, NULL, NULL, yylineno);
    
    patchlist(e->falselist, nextQuad());
    emitQuad(assign, newexpr_constbool(0), NULL, result, yylineno);
    
    patchlabel(jump_over_false, nextQuad());
    
    return result;
}