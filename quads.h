#ifndef QUADS_H
#define QUADS_H

#include "myStruct.h"    
#include "expr.h"        

typedef enum iopcode {
  assign, add, sub, mul, div_, mod, uminus,  
  and_, or_, not_,  
  if_eq, if_noteq, if_less, if_lesseq, if_greater, if_greatereq, jump,  
  param, call, getretval, ret, funcstart, funcend,  
  tablecreate, tablegetelem, tablesetelem,  
  nop  
} iopcode;  

expr *make_arith(expr *e1, expr *e2, iopcode op);

typedef struct quad {
  iopcode      op;         
  expr        *arg1;       
  expr        *arg2;       
  expr        *result;    
  unsigned     label;      
  unsigned     line;  
    char        *comment;      
} quad;

extern quad   *quads;         
extern unsigned currQuad;    
extern unsigned totalQuads;   

void     expandQuads(void);  
unsigned nextQuad(void);                       
void     emitQuad(iopcode op, expr *a1, expr *a2, expr *res, unsigned line);  

int      newlist(int i);                       
int      mergelist(int l1, int l2);           
void     patchlist(int list, int label);      
void     patchlabel(unsigned quadNo, unsigned label); 

void printQuadsToFile(const char *filename);  

#endif 
