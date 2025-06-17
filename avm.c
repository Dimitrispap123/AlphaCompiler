#include "avm.h"
#include <stdarg.h>

avm_memcell stack[AVM_STACKSIZE];
avm_memcell ax, bx, cx;
avm_memcell retval;
unsigned top = 0, topsp = 0;
unsigned char executionFinished = 0;
unsigned pc = 0;
unsigned currLine = 0;
unsigned totalActuals = 0;

extern double* numConsts;
extern unsigned totalNumConsts;
extern char** stringConsts;
extern unsigned totalStringConsts;
extern char** namedLibfuncs;
extern unsigned totalNamedLibfuncs;
extern userfunc* userFuncs;
extern unsigned totalUserFuncs;
extern instruction* instructions;
extern unsigned currentInstruction;

typedef struct {
    char* id;
    library_func_t addr;
} library_func_entry;

library_func_entry* libfunc_table = NULL;
unsigned libfunc_table_size = 0;

void avm_warning(char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("Warning: ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void avm_error(char* format, ...) {
    va_list args;
    va_start(args, format);
    printf("Error: ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
    executionFinished = 1;
}

static void avm_initstack(void) {
    for (unsigned i = 0; i < AVM_STACKSIZE; i++) {
        AVM_WIPEOUT(stack[i]);
        stack[i].type = nil_m;  
    }
    top = AVM_STACKSIZE - 101;
    topsp = 0;
}

void avm_initialize(void) {
    avm_initstack();
    
    avm_registerlibfunc("print", libfunc_print);
    avm_registerlibfunc("typeof", libfunc_typeof);
    avm_registerlibfunc("totalarguments", libfunc_totalarguments);
    avm_registerlibfunc("argument", libfunc_argument);
    
    executionFinished = 0;
    pc = 0;
    currLine = 0;
    totalActuals = 0;
}

void avm_registerlibfunc(char* id, library_func_t addr) {
    if (libfunc_table_size == 0) {
        libfunc_table = malloc(sizeof(library_func_entry));
    } else {
        libfunc_table = realloc(libfunc_table, (libfunc_table_size + 1) * sizeof(library_func_entry));
    }
    
    libfunc_table[libfunc_table_size].id = strdup(id);
    libfunc_table[libfunc_table_size].addr = addr;
    libfunc_table_size++;
}

library_func_t avm_getlibraryfunc(char* id) {
    for (unsigned i = 0; i < libfunc_table_size; i++) {
        if (strcmp(libfunc_table[i].id, id) == 0) {
            return libfunc_table[i].addr;
        }
    }
    return NULL;
}

void memclear_string(avm_memcell* m) {
    assert(m->data.strVal);
    free(m->data.strVal);
}

void memclear_table(avm_memcell* m) {
    assert(m->data.tableVal);
    avm_tabledec_refcounter(m->data.tableVal);
}

memclear_func_t memclearFuncs[] = {
    0,                
    memclear_string,  
    0,                
    memclear_table,   
    0,                
    0,                
    0,                
    0                 
};

void avm_memcellclear(avm_memcell* m) {
    if (m->type != undef_m) {
        memclear_func_t f = memclearFuncs[m->type];
        if (f)
            (*f)(m);
        m->type = undef_m;
    }
}

void avm_assign(avm_memcell* lv, avm_memcell* rv) {
    if (lv == rv)
        return;

    if (lv->type == table_m &&
        rv->type == table_m &&
        lv->data.tableVal == rv->data.tableVal)
        return;

    if (rv->type == undef_m) {
        avm_warning("assigning from 'undef' content!");
        avm_memcellclear(lv);
        lv->type = nil_m;
        return;
    }

    avm_memcellclear(lv);

    memcpy(lv, rv, sizeof(avm_memcell));
    if (lv->type == string_m)
        lv->data.strVal = strdup(rv->data.strVal);
    else if (lv->type == table_m)
        avm_tableinc_refcounter(lv->data.tableVal);
}

avm_memcell* avm_translate_operand(vmarg* arg, avm_memcell* reg) {
    switch (arg->type) {
        case global_a:
            if (arg->val >= AVM_STACKSIZE) {
                avm_error("Global variable index %d out of bounds", arg->val);
                return NULL;
            }
            return &stack[arg->val];
            
        case local_a:
            if (topsp == 0) {
                unsigned local_base = 200;  
                if (local_base + arg->val >= AVM_STACKSIZE) {
                    avm_error("Local variable index %d out of bounds", arg->val);
                    return NULL;
                }
                return &stack[local_base + arg->val];
            } else {
                if (topsp < arg->val || (topsp - arg->val) >= AVM_STACKSIZE) {
                    avm_error("Local variable access out of bounds: topsp=%d, offset=%d", 
                              topsp, arg->val);
                    return NULL;
                }
                return &stack[topsp - arg->val];
            }
            
        case formal_a:
            if (topsp == 0) {
                avm_error("Formal argument accessed outside function");
                return NULL;
            }
            unsigned formal_pos = topsp + AVM_STACKENV_SIZE + arg->val;
            if (formal_pos >= AVM_STACKSIZE) {
                avm_error("Formal argument access out of bounds");
                return NULL;
            }
            return &stack[formal_pos];
            
        case retval_a:
            return &retval;
            
        case number_a:
            if (reg && arg->val < totalNumConsts) {
                avm_memcellclear(reg);
                reg->type = number_m;
                reg->data.numVal = numConsts[arg->val];
                return reg;
            }
            break;
            
        case string_a:
            if (reg && arg->val < totalStringConsts) {
                avm_memcellclear(reg);
                reg->type = string_m;
                reg->data.strVal = strdup(stringConsts[arg->val]);
                return reg;
            }
            break;
            
        case bool_a:
            if (reg) {
                avm_memcellclear(reg);
                reg->type = bool_m;
                reg->data.boolVal = (arg->val != 0) ? 1 : 0;
                return reg;
            }
            break;
            
        case nil_a:
            if (reg) {
                avm_memcellclear(reg);
                reg->type = nil_m;
                return reg;
            }
            break;
            
        case userfunc_a:
            if (reg && arg->val < totalUserFuncs) {
                avm_memcellclear(reg);
                reg->type = userfunc_m;
                reg->data.funcVal = userFuncs[arg->val].address;
                return reg;
            }
            break;
            
        case libfunc_a:
            if (reg && arg->val < totalNamedLibfuncs) {
                avm_memcellclear(reg);
                reg->type = libfunc_m;
                reg->data.libfuncVal = namedLibfuncs[arg->val];
                return reg;
            }
            break;
            
        default:
            if (reg) {
                avm_memcellclear(reg);
                reg->type = nil_m;
                return reg;
            }
            break;
    }
    
    return NULL;
}

void avm_dec_top(void) {
    if (!top) {
        avm_error("Stack overflow");
        executionFinished = 1;
    } else
        --top;
}

void avm_push_envvalue(unsigned val) {
    stack[top].type = number_m;
    stack[top].data.numVal = val;
    avm_dec_top();
}

void avm_callsaveenvironment(void) {
    avm_push_envvalue(totalActuals);
    avm_push_envvalue(pc + 1);
    avm_push_envvalue(top + totalActuals + 2);
    avm_push_envvalue(topsp);
}

unsigned avm_get_envvalue(unsigned i) {
    assert(stack[i].type == number_m);
    unsigned val = (unsigned) stack[i].data.numVal;
    assert(stack[i].data.numVal == ((double) val));
    return val;
}

avm_table* avm_tablenew(void) {
    avm_table* t = (avm_table*) malloc(sizeof(avm_table));
    AVM_WIPEOUT(*t);
    t->refCounter = t->total = 0;
    avm_tablebucketsinit(t->numIndexed);
    avm_tablebucketsinit(t->strIndexed);
    return t;
}

void avm_tablebucketsinit(avm_table_bucket** p) {
    for (unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        p[i] = (avm_table_bucket*) 0;
    }
}

void avm_tablebucketsdestroy(avm_table_bucket** p) {
    for (unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        for (avm_table_bucket* b = p[i]; b;) {
            avm_table_bucket* del = b;
            b = b->next;
            avm_memcellclear(&del->key);
            avm_memcellclear(&del->value);
            free(del);
        }
        p[i] = (avm_table_bucket*) 0;
    }
}

void avm_tabledestroy(avm_table* t) {
    avm_tablebucketsdestroy(t->numIndexed);
    avm_tablebucketsdestroy(t->strIndexed);
    free(t);
}

void avm_tableinc_refcounter(avm_table* t) {
    ++t->refCounter;
}

void avm_tabledec_refcounter(avm_table* t) {
    assert(t->refCounter > 0);
    if (!--t->refCounter) {
        avm_tabledestroy(t);
    }
}

unsigned avm_table_hash_string(char* s) {
    unsigned h = 0;
    for (; *s; s++)
        h = h * 65599 + *s;
    return h % AVM_TABLE_HASHSIZE;
}

unsigned avm_table_hash_number(double d) {
    return ((unsigned) d) % AVM_TABLE_HASHSIZE;
}

avm_memcell* avm_tablegetelem(avm_table* table, avm_memcell* index) {
    avm_table_bucket** bucket_array;
    unsigned hash;
    
    if (index->type == string_m) {
        bucket_array = table->strIndexed;
        hash = avm_table_hash_string(index->data.strVal);
    } else if (index->type == number_m) {
        bucket_array = table->numIndexed;
        hash = avm_table_hash_number(index->data.numVal);
    } else {
        return NULL;
    }
    
    for (avm_table_bucket* b = bucket_array[hash]; b; b = b->next) {
        if (index->type == string_m && b->key.type == string_m &&
            strcmp(index->data.strVal, b->key.data.strVal) == 0) {
            return &b->value;
        } else if (index->type == number_m && b->key.type == number_m &&
                   index->data.numVal == b->key.data.numVal) {
            return &b->value;
        }
    }
    
    return NULL;
}


void avm_tablesetelem(avm_table* table, avm_memcell* index, avm_memcell* content) {
    avm_table_bucket** bucket_array;
    unsigned hash;
    
    if (index->type == string_m) {
        bucket_array = table->strIndexed;
        hash = avm_table_hash_string(index->data.strVal);
    } else if (index->type == number_m) {
        bucket_array = table->numIndexed;
        hash = avm_table_hash_number(index->data.numVal);
    } else {
        return;
    }
    
    for (avm_table_bucket* b = bucket_array[hash]; b; b = b->next) {
        if ((index->type == string_m && b->key.type == string_m &&
             strcmp(index->data.strVal, b->key.data.strVal) == 0) ||
            (index->type == number_m && b->key.type == number_m &&
             index->data.numVal == b->key.data.numVal)) {
            avm_assign(&b->value, content);
            return;
        }
    }

    avm_table_bucket* new_bucket = malloc(sizeof(avm_table_bucket));

    memset(new_bucket, 0, sizeof(avm_table_bucket));
    new_bucket->key.type = undef_m;      
    new_bucket->value.type = undef_m;    
    
    avm_assign(&new_bucket->key, index);
    avm_assign(&new_bucket->value, content);
    
    new_bucket->next = bucket_array[hash];
    bucket_array[hash] = new_bucket;
    table->total++;
}

char* number_tostring(avm_memcell* m) {
    char* s = malloc(32);
    snprintf(s, 32, "%g", m->data.numVal);
    return s;
}

char* string_tostring(avm_memcell* m) {
    return strdup(m->data.strVal);
}

char* bool_tostring(avm_memcell* m) {
    return strdup(m->data.boolVal ? "true" : "false");
}

int is_array_like(avm_table* table) {

    if (table->total == 0) {
        return 1; 
    }
    
    unsigned max_index = 0;
    unsigned count = 0;
    int found_zero = 0;

    for (unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        for (avm_table_bucket* b = table->numIndexed[i]; b; b = b->next) {
            if (b->key.type == number_m) {
                double key_val = b->key.data.numVal;
                if (key_val >= 0 && key_val == (unsigned)key_val) {
                    unsigned idx = (unsigned)key_val;
                    if (idx == 0) found_zero = 1;
                    if (idx > max_index) {
                        max_index = idx;
                    }
                    count++;
                }
            }
        }
    }
    
    for (unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        if (table->strIndexed[i] != NULL) {
            return 0; 
        }
    }
    
    if (count == 0) return 1; 
    return (found_zero && count == max_index + 1);
}

avm_memcell* get_by_index(avm_table* table, unsigned index) {
    avm_memcell index_cell;
    index_cell.type = number_m;
    index_cell.data.numVal = (double)index;
    return avm_tablegetelem(table, &index_cell);
}

char* table_tostring(avm_memcell* m) {
    avm_table* table = m->data.tableVal;
    
    if (is_array_like(table)) {
        char* result = malloc(1024);
        strcpy(result, "[ ");
        
        unsigned max_index = 0;
        for (unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
            for (avm_table_bucket* b = table->numIndexed[i]; b; b = b->next) {
                if (b->key.type == number_m) {
                    unsigned idx = (unsigned)b->key.data.numVal;
                    if (idx > max_index) max_index = idx;
                }
            }
        }
        
        for (unsigned i = 0; i <= max_index; i++) {
            avm_memcell* elem = get_by_index(table, i);
            if (elem) {
                char* elem_str = avm_tostring(elem);
                strcat(result, elem_str);
                free(elem_str);
                
                if (i < max_index) {
                    strcat(result, ", ");
                }
            }
        }
        
        strcat(result, " ]");
        return result;
    } else {
        char* result = malloc(1024);
        strcpy(result, "{ ");
        
        int first = 1;

        for (unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
            for (avm_table_bucket* b = table->strIndexed[i]; b; b = b->next) {
                if (!first) strcat(result, ", ");
                first = 0;
                
                char* key_str = avm_tostring(&b->key);
                char* val_str = avm_tostring(&b->value);
                strcat(result, key_str);
                strcat(result, ": ");
                strcat(result, val_str);
                free(key_str);
                free(val_str);
            }
        }
        
        for (unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
            for (avm_table_bucket* b = table->numIndexed[i]; b; b = b->next) {
                if (!first) strcat(result, ", ");
                first = 0;
                
                char* key_str = avm_tostring(&b->key);
                char* val_str = avm_tostring(&b->value);
                strcat(result, key_str);
                strcat(result, ": ");
                strcat(result, val_str);
                free(key_str);
                free(val_str);
            }
        }
        
        strcat(result, " }");
        return result;
    }
}

char* userfunc_tostring(avm_memcell* m) {
    char* s = malloc(64);
    snprintf(s, 64, "[userfunc@%u]", m->data.funcVal);
    return s;
}

char* libfunc_tostring(avm_memcell* m) {
    char* s = malloc(strlen(m->data.libfuncVal) + 16);
    snprintf(s, strlen(m->data.libfuncVal) + 16, "[libfunc:%s]", m->data.libfuncVal);
    return s;
}

char* nil_tostring(avm_memcell* m) {
    return strdup("nil");
}

char* undef_tostring(avm_memcell* m) {
    return strdup("undef");
}

tostring_func_t tostringFuncs[] = {
    number_tostring,
    string_tostring,
    bool_tostring,
    table_tostring,
    userfunc_tostring,
    libfunc_tostring,
    nil_tostring,
    undef_tostring
};

char* avm_tostring(avm_memcell* m) {
    assert(m->type >= 0 && m->type <= undef_m);
    return (*tostringFuncs[m->type])(m);
}

// ToBool functions
unsigned char number_toboolean(avm_memcell* m) {
    return m->data.numVal != 0;
}

unsigned char string_toboolean(avm_memcell* m) {
    return m->data.strVal[0] != 0;
}

unsigned char bool_toboolean(avm_memcell* m) {
    return m->data.boolVal;
}

unsigned char table_toboolean(avm_memcell* m) {
    return 1;
}

unsigned char userfunc_toboolean(avm_memcell* m) {
    return 1;
}

unsigned char libfunc_toboolean(avm_memcell* m) {
    return 1;
}

unsigned char nil_toboolean(avm_memcell* m) {
    return 0;
}

unsigned char undef_toboolean(avm_memcell* m) {
    assert(0);
    return 0;
}

tobool_func_t toboolFuncs[] = {
    number_toboolean,
    string_toboolean,
    bool_toboolean,
    table_toboolean,
    userfunc_toboolean,
    libfunc_toboolean,
    nil_toboolean,
    undef_toboolean
};

unsigned char avm_tobool(avm_memcell* m) {
    assert(m->type >= 0 && m->type < undef_m);
    return (*toboolFuncs[m->type])(m);
}

double add_impl(double x, double y) { return x + y; }
double sub_impl(double x, double y) { return x - y; }
double mul_impl(double x, double y) { return x * y; }
double div_impl(double x, double y) { return x / y; }
double mod_impl(double x, double y) {
    if (y == 0.0) {
        avm_error("modulo by zero!");
        return 0.0;
    }
    
    long long ix = (long long)x;
    long long iy = (long long)y;
    
    if (x != (double)ix) {
        return 0.0;
    }
    
    if (iy == 0) {
        avm_error("modulo by zero!");
        return 0.0;
    }
    
    long long result = ix % iy;
    return (double)result;
}

arithmetic_func_t arithmeticFuncs[] = {
    add_impl, sub_impl, mul_impl, div_impl, mod_impl
};

unsigned char jeq_impl(double x, double y) { return x == y; }
unsigned char jne_impl(double x, double y) { return x != y; }
unsigned char jle_impl(double x, double y) { return x <= y; }
unsigned char jge_impl(double x, double y) { return x >= y; }
unsigned char jlt_impl(double x, double y) { return x < y; }
unsigned char jgt_impl(double x, double y) { return x > y; }

unsigned avm_totalactuals(void) {
    return avm_get_envvalue(topsp + AVM_NUMACTUALS_OFFSET);
}

avm_memcell* avm_getactual(unsigned i) {
    assert(i < avm_totalactuals());
    return &stack[topsp + AVM_STACKENV_SIZE + 1 + i];
}

void avm_calllibfunc(char* funcName) {
    library_func_t f = avm_getlibraryfunc(funcName);
    if (!f) {
        avm_error("unsupported lib func '%s' called!", funcName);
        executionFinished = 1;
        return;
    }
    
    avm_callsaveenvironment();
    topsp = top;
    totalActuals = 0;
    
    (*f)();
    
    if (!executionFinished) {
        execute_funcexit((instruction*) 0);
    }
}

void libfunc_print(void) {
    unsigned n = avm_totalactuals();
    for (unsigned i = 0; i < n; ++i) {
        avm_memcell* arg = avm_getactual(i);
        
        if (arg->type == string_m) {
            char* str = arg->data.strVal;
            while (*str) {
                if (*str == '\\' && *(str + 1)) {
                    switch (*(str + 1)) {
                        case 'n':
                            printf("\n");
                            str += 2;
                            break;
                        case 't':
                            printf("\t");
                            str += 2;
                            break;
                        case 'r':
                            printf("\r");
                            str += 2;
                            break;
                        case '\\':
                            printf("\\");
                            str += 2;
                            break;
                        case '"':
                            printf("\"");
                            str += 2;
                            break;
                        default:
                            printf("%c", *str);
                            str++;
                            break;
                    }
                } else {
                    printf("%c", *str);
                    str++;
                }
            }
        } else {
            char* s = avm_tostring(arg);
            printf("%s", s);
            free(s);
        }
    }
}
char* typeStrings[] = {
    "number", "string", "bool", "table",
    "userfunc", "libfunc", "nil", "undef"
};

void libfunc_typeof(void) {
    unsigned n = avm_totalactuals();
    
    if (n != 1) {
        avm_error("one argument (not %d) expected in 'typeof'!", n);
    } else {
        avm_memcellclear(&retval);
        retval.type = string_m;
        retval.data.strVal = strdup(typeStrings[avm_getactual(0)->type]);
    }
}

void libfunc_totalarguments(void) {
    unsigned p_topsp = avm_get_envvalue(topsp + AVM_SAVEDTOPSP_OFFSET);
    avm_memcellclear(&retval);
    
    if (!p_topsp) {
        avm_error("'totalarguments' called outside a function!");
        retval.type = nil_m;
    } else {
        retval.type = number_m;
        retval.data.numVal = avm_get_envvalue(p_topsp + AVM_NUMACTUALS_OFFSET);
    }
}

void libfunc_argument(void) {
    unsigned n = avm_totalactuals();
    if (n != 1) {
        avm_error("one argument expected in 'argument'!");
        return;
    }
    
    avm_memcell* arg = avm_getactual(0);
    if (arg->type != number_m) {
        avm_error("number argument expected in 'argument'!");
        return;
    }
    
    unsigned i = (unsigned) arg->data.numVal;
    unsigned p_topsp = avm_get_envvalue(topsp + AVM_SAVEDTOPSP_OFFSET);
    
    if (!p_topsp) {
        avm_error("'argument' called outside a function!");
        retval.type = nil_m;
        return;
    }
    
    unsigned total_args = avm_get_envvalue(p_topsp + AVM_NUMACTUALS_OFFSET);
    if (i >= total_args) {
        avm_error("argument index out of range!");
        retval.type = nil_m;
        return;
    }
    
    avm_memcell* requested_arg = &stack[p_topsp + AVM_STACKENV_SIZE + 1 + i];
    avm_assign(&retval, requested_arg);
}