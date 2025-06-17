#ifndef EXPR_H
#define EXPR_H

#include "symtable.h"  

typedef enum iopcode iopcode;

typedef enum expr_t {
    var_e,
    tableitem_e,
    programfunc_e,
    libraryfunc_e,
    arithexpr_e,
    boolexpr_e,
    assignexpr_e,
    newtable_e,
    constnum_e,
    constbool_e,
    conststring_e,
    nil_e
} expr_t;

typedef struct expr {
    expr_t        type;       
    SymbolTableEntry       *sym;        
    struct expr  *index;      
    struct expr  *next;       
    double        numConst;  
    char         *strConst;   
    unsigned char boolConst;  
    int truelist;   
    int falselist; 
     unsigned char is_and : 1;  
    unsigned char is_or : 1; 
    struct expr *parent;   
    unsigned char parent_is_and : 1;  
    unsigned char parent_is_or : 1; 
} expr;

expr *newexpr(expr_t t);

expr *newexpr_constnum(double v);
expr *newexpr_conststring(const char *s);
expr *newexpr_constbool(unsigned b); 
expr* newexpr_func(SymbolTableEntry* func_sym);

expr *emit_iftableitem(expr *e);
expr* member_item(expr* obj, const char* name);
SymbolTableEntry *newtemp(void);
expr *make_relop(expr *e1, expr *e2, iopcode op);
expr *bool_expr(expr *e);
expr *finish_bool_expr(expr *e);
expr *make_and(expr *e1, expr *e2);
expr *make_or(expr *e1, expr *e2);
expr *make_not(expr *e);
expr *make_uminus(expr *e);
expr* newexpr_nil(void);

#endif 