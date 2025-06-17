#include "quads.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "expr.h"
#define INITIAL_SIZE 1024

extern quad *quads;
extern unsigned currQuad;
extern unsigned totalQuads;

void set_label(unsigned quadNo, unsigned label) {
    if (quadNo >= currQuad) {
        fprintf(stderr, "Error: Invalid quad number %d for set_label\n", quadNo);
        return;
    }
    
    quads[quadNo].label = label;
    
}

void expandQuads(void) {
    if (totalQuads == 0) {
        totalQuads = INITIAL_SIZE;
        quads = malloc(INITIAL_SIZE * sizeof(quad));
        if (!quads) { perror("malloc"); exit(1); }
        return;
    }
    
    quad *newArr = realloc(quads, 2 * totalQuads * sizeof(quad));
    if (!newArr) {
        fprintf(stderr, "Out of memory expanding quads\n");
        exit(1);
    }
    quads = newArr;
    totalQuads *= 2;
}

unsigned nextQuad(void) {
    return currQuad;
}

void emitQuad(iopcode op, expr *a1, expr *a2, expr *res, unsigned line) {
    
    if (op == uminus && a1 == NULL) {
        fprintf(stderr, "Error: NULL operand for unary minus at line %d\n", line);
        return;
    }
    
    if ((op == add || op == sub || op == mul || op == div_ || op == mod) &&
        (a1 == NULL || a2 == NULL)) {
        fprintf(stderr, "Error: NULL operand for binary operation at line %d\n", line);
        return;
    }
    
    if (op == assign && (a1 == NULL || res == NULL)) {
        fprintf(stderr, "Error: NULL operand for assignment at line %d\n", line);
        return;
    }
    
    if (currQuad == totalQuads) {
        expandQuads();
    }
    
    quad *q = &quads[currQuad++];
    q->op     = op;
    q->arg1   = a1;
    q->arg2   = a2;
    q->result = res;
    q->label  = 0;
    q->line   = line;
    q->comment = NULL; 
}

int newlist(int quad) {
    if (quad < 0) {
        fprintf(stderr, "Error: Invalid quad %d in newlist\n", quad);
        return 0;
    }
    
    return quad;
}

int mergelist(int list1, int list2) {
    if (list1 < 0) return list2;
    if (list2 < 0) return list1;
    
    if (list1 == list2) return list1;
    
    
    int curr = list1;
    
    while (curr >= 0 && quads[curr].label != 0 && quads[curr].label != -1) {
        
        curr = quads[curr].label;
        
        if (curr == list1) {
            printf("ERROR: Cycle detected in mergelist\n");
            break;
        }
    }
    
    if (curr >= 0) {
        quads[curr].label = list2;
    }
    
    return list1;
}
void patchlabel(unsigned quadNo, unsigned label) {
    if (quadNo >= currQuad) return;
    set_label(quadNo, label);
}

void patchlist(int list, int label) {
    if (list < 0) return;  
    
    
    int curr = list;
    while (curr >= 0) {
        int next = quads[curr].label; 
          if (quads[curr].label == 0) {
            next = -1;  
        }
        
        quads[curr].label = label;
        curr = next;
    }
}

static const char *opr(expr *e) {
    static char buf[64];
    
    if (!e) {
        return "NULL";
    }
    
    if (e->type < 0 || e->type > 10) {  
        snprintf(buf, sizeof(buf), "invalid_type(%d)", e->type);
        return buf;
    }
    
    switch (e->type) {
        case constnum_e:
            snprintf(buf, sizeof(buf), "%g", e->numConst);
            return buf;
            
        case conststring_e:
            if (e->strConst) {
                snprintf(buf, sizeof(buf), "\"%s\"", e->strConst);
            } else {
                snprintf(buf, sizeof(buf), "\"\"");
            }
            return buf;
            
        case constbool_e:
            return e->boolConst ? "true" : "false";
            
        default:
            if (!e->sym) {
                snprintf(buf, sizeof(buf), "^%p", (void*)e);  
                return buf;
            }
            
            if (e->sym->type < 0 || e->sym->type > 5) {  
                snprintf(buf, sizeof(buf), "invalid_sym(%d)", e->sym->type);
                return buf;
            }
            
            if (e->sym->type == USERFUNC || e->sym->type == LIBFUNC) {
                if (e->sym->value.funcVal && e->sym->value.funcVal->name) {
                    return e->sym->value.funcVal->name;
                } else {
                    return "unnamed_func";
                }
            } else {
                if (e->sym->value.varVal && e->sym->value.varVal->name) {
                    return e->sym->value.varVal->name;
                } else {
                    return "unnamed_var";
                }
            }
            
            snprintf(buf, sizeof(buf), "^%p", (void*)e);
            return buf;
    }
}

void printQuadsToFile(const char *filename) {
    static const char *opNames[] = {
        "assign", "add", "sub", "mul", "div", "mod", "uminus",
        "and", "or", "not", "if_eq", "if_noteq", "if_less",
        "if_lesseq", "if_greater", "if_greatereq", "jump",
        "param", "call", "getretval", "ret", "funcstart", "funcend",
        "tablecreate", "tablegetelem", "tablesetelem", "nop"
    };
    
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Error: Could not open file %s for writing\n", filename);
        return;
    }
    
    fprintf(file, "quad#    opcode          result      arg1        arg2        label\n");
    fprintf(file, "------------------------------------------------------------------------\n");
    
    for (unsigned i = 0; i < currQuad; ++i) {
        quad *q = &quads[i];
        
        if (q->op < 0 || q->op >= sizeof(opNames)/sizeof(opNames[0])) {
            fprintf(file, "ERROR: Invalid opcode %d in quad %d\n", q->op, i);
            continue;
        }
        
        fprintf(file, "%-8d", i+1);
        if(q->op == ret ){
            fprintf(file, "%-16s", "return");
        }else{
            fprintf(file, "%-16s", opNames[q->op]);
        }
        
        if (q->op == tablegetelem) {
            if (q->result) fprintf(file, "%-12s", opr(q->result));
            else fprintf(file, "%-12s", "NULL");
            if (q->arg1) fprintf(file, "%-12s", opr(q->arg1));
            else fprintf(file, "%-12s", "NULL");
            if (q->arg2) fprintf(file, "%-12s", opr(q->arg2));
            else fprintf(file, "%-12s", "NULL");
        }
        else if (q->op == tablesetelem) {
            if (q->arg1) fprintf(file, "%-12s", opr(q->arg1));
            else fprintf(file, "%-12s", "NULL");
            if (q->arg2) fprintf(file, "%-12s", opr(q->arg2));
            else fprintf(file, "%-12s", "NULL");
            if (q->result) fprintf(file, "%-12s", opr(q->result));
            else fprintf(file, "%-12s", "NULL");
        }
        else if (q->op == jump) {
            fprintf(file, "%-12s", "");
            fprintf(file, "%-12s", "");
            fprintf(file, "%-12s", "");
            if (q->label >= 0 && q->label < currQuad + 1) {
                fprintf(file, "%-12d", q->label + 1);
            } else {
                fprintf(file, "%-12s", "invalid");
            }
        }
        else if (q->op == if_eq || q->op == if_noteq || q->op == if_less ||
                 q->op == if_lesseq || q->op == if_greater || q->op == if_greatereq) {
            fprintf(file, "%-12s", "");
            if (q->arg1) fprintf(file, "%-12s", opr(q->arg1));
            else fprintf(file, "%-12s", "NULL");
            if (q->arg2) fprintf(file, "%-12s", opr(q->arg2));
            else fprintf(file, "%-12s", "NULL");
            if (q->label >= 0 && q->label < currQuad + 1) {
                fprintf(file, "%-12d", q->label+1);
            } else {
                fprintf(file, "%-12s", "invalid");
            }
        }
        else {
            if (q->result) fprintf(file, "%-12s", opr(q->result));
            else fprintf(file, "%-12s", "");
            if (q->arg1) fprintf(file, "%-12s", opr(q->arg1));
            else fprintf(file, "%-12s", "");
            if (q->arg2) fprintf(file, "%-12s", opr(q->arg2));
            else fprintf(file, "%-12s", "");
        }
        fprintf(file, "\n");
    }
    fprintf(file, "------------------------------------------------------------------------\n");
    
    fclose(file);
    printf("Quads written to %s\n", filename);
}