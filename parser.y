%{
	#include "myStruct.h"
	#include "symtable.h"
	#include <string.h>
	#include <stdio.h>
    #include "expr.h"
    #include "quads.h"
    #include "target_code.h"
    #include "target_code.h" 

	int yyerror(const char *msg);
	extern int yylex (void);
	extern int yylineno;
	extern char* yytext;
	extern FILE *yyin;
    SymbolTableEntry *head = NULL;
    quad *quads = NULL;
    unsigned currQuad = 0;
    unsigned totalQuads = 0;        
	int scope=0;
	int function_scope_level = 0; 
	int inside_function = 0;
    int error_flag=0;
	int inside_loop = 0;
	int anonym_func = 0;
	char *current_args[128];
    int increment_start ;
    int current_arg_count = 0;
    int current_array_index = 0;
    int current_func_jump = -1;
    SymbolTableEntry *func_stack[100];
    int func_stack_ptr = 0;
    typedef struct {
        expr *key;
        expr *value;
    } table_element;
    typedef struct {
        table_element elements[100];
        int count;
    } pending_table;
    pending_table pending_stack[50]; 
    int pending_stack_ptr = 0;
    table_element pending_elements[100];
    typedef struct {
        int loop_start;
        int jump_quad;
    } loop_info;

    loop_info loop_stack[100];
    int loop_stack_ptr = 0;
    int for_condition_start = 0;
    int for_exit_jump = 0;
    int for_increment_start = 0;
    extern unsigned temp_counter;
    extern expr *global_temp;

    bool is_duplicate_arg(const char *name) {
        for (int i = 0; i < current_arg_count; i++) {
            if (strcmp(current_args[i], name) == 0) return true;
        }
        return false;
    }

    typedef struct {
        unsigned int temp_counter;
        expr *global_temp;
    } TempState;

    TempState temp_state_stack[100];
    int temp_state_stack_ptr = 0;

    void save_temp_state() {
        if (temp_state_stack_ptr < 100) {
            temp_state_stack[temp_state_stack_ptr].temp_counter = temp_counter;
            temp_state_stack[temp_state_stack_ptr].global_temp = global_temp;
            temp_state_stack_ptr++;
        }
    }

    void restore_temp_state() {
        if (temp_state_stack_ptr > 0) {
            temp_state_stack_ptr--;
            temp_counter = temp_state_stack[temp_state_stack_ptr].temp_counter;
            global_temp = temp_state_stack[temp_state_stack_ptr].global_temp;
        }
    }

    void reset_temp() {
        temp_counter = 0;
        global_temp = NULL;
    }
void assign_offsets(void) {
    
    SymbolTableEntry* curr = head;
    unsigned global_offset = 0;
    unsigned local_offset = 0;
    unsigned formal_offset = 0;
    
    while (curr) {
        if (curr->type == GLOBAL_VAR || 
            (curr->type == LOCAL_VAR && curr->value.varVal->scope == 0)) {
            curr->space = programvar;
            curr->offset = global_offset++;
        } else if (curr->type == LOCAL_VAR) {
            curr->space = functionlocal;
            curr->offset = local_offset++;
        } else if (curr->type == FORMAL_ARG) {
            curr->space = formalarg;
            curr->offset = formal_offset++;
        } else if (curr->type == USERFUNC) {
            curr->space = programvar;
            curr->offset = global_offset++;
        }
        curr = curr->next;
    }
}

typedef struct {
    int saved_array_index;
} array_index_state;

array_index_state index_stack[50];
int index_stack_ptr = 0;

void save_array_index() {
    if (index_stack_ptr < 50) {
        index_stack[index_stack_ptr].saved_array_index = current_array_index;
        index_stack_ptr++;
    }
}

void restore_array_index() {
    if (index_stack_ptr > 0) {
        index_stack_ptr--;
        current_array_index = index_stack[index_stack_ptr].saved_array_index;
    }
}

%}

%start  program

%union {
	struct alpha_token_t *token;
    expr              *expr;  
    int               intVal;
}

%token <token> ID CONST_INT CONST_FLOAT STRING
%token <token> IF ELSE WHILE FOR FUNCTION RETURN BREAK CONTINUE
%token <token> AND NOT OR LOCAL TRUE FALSE NIL
%token <token> ASSIGN PLUS MINUS MUL DIV MOD
%token <token> EQUAL NOTEQUAL PLUSPLUS MINUSMINUS
%token <token> GTHAN LTHAN GEQUAL LEQUAL
%token <token> LBRACE RBRACE LBRACKET RBRACKET
%token <token> LPARENTHESIS RPARENTHESIS SEMICOLON COMMA
%token <token> COLON DOUBLECOLON DOT DOUBLEDOT


%type <expr> call funcdef expr  primary assignexpr  elist lvalue  objectdef indexed indexedelem bool_primary bool_or bool_and bool_not comparison arith_expr arith_term factor
%type <intVal> if_prefix M 
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%left EQUAL
%left OR
%left AND
%right NOT 
%nonassoc NOTEQUAL
%nonassoc GTHAN GEQUAL LTHAN LEQUAL
%left PLUS MINUS
%left MUL DIV MOD
%right UMINUS 
%right  PLUSPLUS MINUSMINUS 
%left DOT DOUBLEDOT
%left LBRACE RBRACE LBRACKET RBRACKET
%left LPARENTHESIS RPARENTHESIS

%%

program:	stmt_list 
	;

stmt: assignexpr SEMICOLON { }
    | expr SEMICOLON {
        expr *e = $1;
        if (e && e->type == boolexpr_e && 
            (e->truelist >= 0 || e->falselist >= 0)) {
            
            e = finish_bool_expr(e);
        }
       
    }
     
	| BREAK SEMICOLON {
        if (inside_loop == 0) {
            fprintf(stderr, "Error: 'break' used outside of loop (line %1$d)\n", $1->lineno);
            yyerror("Semantic error");
        } else {
            
            int break_jump = nextQuad();
            emitQuad(jump, NULL, NULL, NULL, $1->lineno);
            quads[break_jump].label = 9999;
        }
      
    }
    | CONTINUE SEMICOLON {
        if (inside_loop == 0) {
            fprintf(stderr, "Error: 'continue' used outside of loop (line %1$d)\n", $1->lineno);
            yyerror("Semantic error");
        } else {
           
            int continue_jump = nextQuad();
            emitQuad(jump, NULL, NULL, NULL, $1->lineno);
            
            quads[continue_jump].label = 8888;
        }
     
    }
	| LOCAL ID SEMICOLON {
    if (lookup_library(head, $2->text)) {
        fprintf(stderr, "Error: Local variable '%s' collides with library function (line %d)\n", $2->text, $2->lineno);
        yyerror("Semantic error");
    }else{
		SymbolTableEntry *local = insert_variable($2->text, $2->lineno, scope, LOCAL_VAR);
    	local->next = head;
    	head = local;
	}

    
}

	  | LOCAL ID ASSIGN expr SEMICOLON {
		if (lookup_library(head, $2->text)) {
            fprintf(stderr, "Error: Local variable '%s' collides with library function (line %d)\n", $2->text, $2->lineno);
            yyerror("Semantic error");
        }

          SymbolTableEntry *local = insert_variable($2->text, $2->lineno, scope, LOCAL_VAR);
          local->next = head;
          head = local;
         
      }
	|ifstmt { }
    | whilestmt {}
    | forstmt {}
    | returnstmt {  }
    | block {}
    | funcdef {}
    | SEMICOLON { }
    ;

stmt_list:	stmt stmt_list 
		| /* empty */ 
	;


expr
  : assignexpr { $$ = $1; }
  | arith_expr { $$ = $1; } 
   | bool_or   { $$ = $1; } 
  ;


bool_or
  : bool_and { $$ = $1; }
  | bool_or OR M bool_and {
      patchlist($1->falselist, $3);
      
      expr* result = newexpr(boolexpr_e);
      result->truelist = mergelist($1->truelist, $4->truelist);
      result->falselist = $4->falselist;
      $$ = result;
    }
  ;

bool_and
  : bool_not { $$ = $1; }  
  | bool_and AND M bool_not {
      patchlist($1->truelist, $3);
      
      expr* result = newexpr(boolexpr_e);
      result->truelist = $4->truelist;
      result->falselist = mergelist($1->falselist, $4->falselist);
      $$ = result;
    }
  ;

bool_not
  : NOT bool_not { 
      expr* result = newexpr(boolexpr_e);
      result->truelist = $2->falselist;
      result->falselist = $2->truelist;
      $$ = result;
    }
  | LPARENTHESIS bool_or RPARENTHESIS { 
      $$ = $2; 
    }
  | bool_primary {
      $$ = $1;
    }
  ;


bool_primary
  : comparison {
      if ($1->type == boolexpr_e) {
          $$ = $1;
      } 
      else if ($1->type == var_e || $1->type == arithexpr_e || 
               $1->type == assignexpr_e || $1->type == tableitem_e || $1->type == constbool_e) {
          $$ = bool_expr($1);
      } 
      else {
          $$ = $1;
      }
    }
  ;
M : /* empty */ { $$ = nextQuad(); }
  ;


comparison
  : arith_expr { $$ = $1; }
  | comparison EQUAL arith_expr {
      $$ = make_relop($1, $3, if_eq);
    }
  | comparison NOTEQUAL arith_expr {
      $$ = make_relop($1, $3, if_noteq);
    }
  | comparison GTHAN arith_expr {
      $$ = make_relop($1, $3, if_greater);
    }
  | comparison LTHAN arith_expr {
      $$ = make_relop($1, $3, if_less);
    }
  | comparison GEQUAL arith_expr {
      $$ = make_relop($1, $3, if_greatereq);
    }
  | comparison LEQUAL arith_expr {
      $$ = make_relop($1, $3, if_lesseq);
    }
  ;

arith_expr
  : arith_expr PLUS  arith_term { $$ = make_arith($1,$3,add); }
  | arith_expr MINUS arith_term { $$ = make_arith($1,$3,sub); }
  | arith_term                  { $$ = $1; }
  ;

arith_term
  : arith_term MUL  factor { $$ = make_arith($1,$3,mul); }
  | arith_term DIV  factor { $$ = make_arith($1,$3,div_); }
  | arith_term MOD  factor { $$ = make_arith($1,$3,mod); }
  | factor                  { $$ = $1; }
  ;


factor
  : MINUS factor %prec UMINUS { $$ = make_uminus($2); }
  | PLUSPLUS lvalue {
        if (!$2) {
            fprintf(stderr, "Error: Invalid pre-increment operation at line %d\n", yylineno);
            $$ = NULL;
        } else {
            if ($2->type == tableitem_e) {
                expr *current_val = emit_iftableitem($2);
                expr *one = newexpr_constnum(1);
                expr *result = newexpr(arithexpr_e);
                result->sym = newtemp();
                
                emitQuad(add, current_val, one, result, yylineno);
                emitQuad(tablesetelem, $2, $2->index, result, yylineno);
                
                $$ = result;
            } else {
                expr *one = newexpr_constnum(1);
                expr *result = newexpr(arithexpr_e);
                result->sym = newtemp();
                
                emitQuad(add, $2, one, result, yylineno);
                
                emitQuad(assign, result, NULL, $2, yylineno);
                
                $$ = result; 
            }
        }
    }
  | lvalue PLUSPLUS {
    if (!$1) {
        fprintf(stderr, "Error: Invalid post-increment operation at line %d\n", yylineno);
        $$ = NULL;
    } else {
        if ($1->type == tableitem_e) {
            expr *current_val = emit_iftableitem($1);
            expr *one = newexpr_constnum(1);
            expr *old_val = newexpr(arithexpr_e);
            old_val->sym = newtemp();
            expr *new_val = newexpr(arithexpr_e);
            new_val->sym = newtemp();
            
            emitQuad(assign, current_val, NULL, old_val, yylineno);
            
            emitQuad(add, current_val, one, new_val, yylineno);
            
            emitQuad(tablesetelem, $1, $1->index, new_val, yylineno);
            
            $$ = old_val; 
        } else {
            expr *one = newexpr_constnum(1);
            expr *old_val = newexpr(arithexpr_e);
            old_val->sym = newtemp();
            expr *new_val = newexpr(arithexpr_e);
            new_val->sym = newtemp();
            
            
            emitQuad(assign, $1, NULL, old_val, yylineno);
             
            emitQuad(add, $1, one, new_val, yylineno);
            
            emitQuad(assign, new_val, NULL, $1, yylineno);
            
            $$ = old_val; 
        }
    }
}
  | MINUSMINUS lvalue {
        if (!$2) {
            fprintf(stderr, "Error: Invalid pre-decrement operation at line %d\n", yylineno);
            $$ = NULL;
        } else {
            if ($2->type == tableitem_e) {
                expr *current_val = emit_iftableitem($2);
                expr *one = newexpr_constnum(1);
                expr *result = newexpr(arithexpr_e);
                result->sym = newtemp();
                
                emitQuad(sub, current_val, one, result, yylineno);
                emitQuad(tablesetelem, $2, $2->index, result, yylineno);
                
                $$ = result;
            } else {
                expr *one = newexpr_constnum(1);
                expr *result = newexpr(arithexpr_e);
                result->sym = newtemp();
                
                emitQuad(sub, $2, one, result, yylineno);
                
                emitQuad(assign, result, NULL, $2, yylineno);
                
                $$ = result;
            }
        }
    }
  | lvalue MINUSMINUS {
        if (!$1) {
            fprintf(stderr, "Error: Invalid post-decrement operation at line %d\n", yylineno);
            $$ = NULL;
        } else {
            if ($1->type == tableitem_e) {
                expr *current_val = emit_iftableitem($1);
                expr *one = newexpr_constnum(1);
                expr *old_val = newexpr(arithexpr_e);
                old_val->sym = newtemp();
                expr *new_val = newexpr(arithexpr_e);
                new_val->sym = newtemp();
                
                
                emitQuad(assign, current_val, NULL, old_val, yylineno);
                
                emitQuad(sub, current_val, one, new_val, yylineno);
                
                emitQuad(tablesetelem, $1, $1->index, new_val, yylineno);
                
                $$ = old_val; 
            } else {
                expr *one = newexpr_constnum(1);
                expr *old_val = newexpr(arithexpr_e);
                old_val->sym = newtemp();
                expr *new_val = newexpr(arithexpr_e);
                new_val->sym = newtemp();
                
                
                emitQuad(assign, $1, NULL, old_val, yylineno);
                
                emitQuad(sub, $1, one, new_val, yylineno);
                
                emitQuad(assign, new_val, NULL, $1, yylineno);
                
                $$ = old_val; 
            }
        }
    }
  | primary { $$ = $1; }
  ;
primary
  : lvalue { $$ = emit_iftableitem($1); }
  | NIL           { $$ = newexpr_nil(); }
  | CONST_INT    { $$ = newexpr_constnum(atof($1->text)); }
  | CONST_FLOAT  { $$ = newexpr_constnum(atof($1->text)); }
  | STRING       { $$ = newexpr_conststring($1->text); }
  | TRUE         { $$ = newexpr_constbool(1); }
  | FALSE        { $$ = newexpr_constbool(0); }
    
  | LPARENTHESIS expr RPARENTHESIS { $$ = $2; }
  | call        { $$ = $1; } 
  | objectdef
  | LPARENTHESIS funcdef RPARENTHESIS { $$ = $2; }
  ;

assignexpr:
    lvalue ASSIGN expr {
        if (!$1 || !$3) {
            fprintf(stderr, "Error: Invalid assignment at line %d\n", yylineno);
            $$ = NULL;
            YYERROR;
        }
        
        expr *value = $3;
        
        if (value->type == boolexpr_e) {
            value = finish_bool_expr(value);
        }
        
         if ($1->type == tableitem_e) {
            if (value->type == tableitem_e) {
                value = emit_iftableitem(value);
            }
            
            emitQuad(tablesetelem, $1->index, value, $1, yylineno);
            $$ = emit_iftableitem($1);
        } else {
            emitQuad(assign, value, NULL, $1, yylineno);
            $$ = $1;
        }
    }
;

lvalue:
   ID {
       SymbolTableEntry *sym = lookup_visible(head, $1->text, scope);
       if (!sym) {
         sym = insert_variable(
             strdup($1->text),
             $1->lineno,
             scope,
             scope ? LOCAL_VAR : GLOBAL_VAR
         );
         sym->next = head;
         head = sym;
       }
       $$ = newexpr(var_e);
       $$->sym = sym;
     }
 
   | lvalue DOT ID {
       $$ = member_item($1, $3->text);
     }
 
   | lvalue LBRACKET expr RBRACKET {
       expr *base = $1;
       
       if (base->type == tableitem_e) {
           base = emit_iftableitem(base);
       }
       
       expr *ti = newexpr(tableitem_e);
       ti->sym = base->sym;
       ti->index = $3;
       $$ = ti;
     }
     | LOCAL ID {
        SymbolTableEntry *local = insert_variable($2->text, $2->lineno, scope, LOCAL_VAR);
        local->next = head;
        head = local;
        
        $$ = newexpr(var_e);
        $$->sym = local;
    }
     ;

call:
    lvalue callsuffix {
        if (!$1) {
            fprintf(stderr, "Error: Invalid function call at line %d\n", yylineno);
            $$ = NULL;
        } else {
            expr *func = $1;
            if (func->type == tableitem_e) {
                func = emit_iftableitem(func);
            }
            expr *res = newexpr(var_e);
            res->sym = newtemp();
            emitQuad(call, func, NULL, NULL, yylineno);
            emitQuad(getretval, NULL, NULL, res, yylineno);
            $$ = res;
        }
    }
     | call LPARENTHESIS param_list RPARENTHESIS {
        if (!$1) {
            fprintf(stderr, "Error: Invalid chained function call at line %d\n", yylineno);
            $$ = NULL;
        } else {
            expr *res = newexpr(var_e);
            res->sym = newtemp();
            
            emitQuad(call, $1, NULL, NULL, yylineno);
            emitQuad(getretval, NULL, NULL, res, yylineno);
            
            $$ = res;
        }
    }
    | call LPARENTHESIS RPARENTHESIS {
        if (!$1) {
            fprintf(stderr, "Error: Invalid chained function call at line %d\n", yylineno);
            $$ = NULL;
        } else {
            expr *res = newexpr(var_e);
            res->sym = newtemp();
            
            emitQuad(call, $1, NULL, NULL, yylineno);
            emitQuad(getretval, NULL, NULL, res, yylineno);
            
            $$ = res;
        }
    }
    | lvalue DOUBLEDOT ID LPARENTHESIS {
        expr *obj = $1;
        if (obj->type == tableitem_e) {
            obj = emit_iftableitem(obj);
        }
        
        expr *method = member_item(obj, $3->text);
        expr *method_func = emit_iftableitem(method);
        
        emitQuad(param, obj, NULL, NULL, yylineno);
        
        $<expr>$ = method_func;
    } param_list RPARENTHESIS {
        expr *method_func = $<expr>5;
        
        expr *res = newexpr(var_e);
        res->sym = newtemp();
        emitQuad(call, method_func, NULL, NULL, yylineno);
        emitQuad(getretval, NULL, NULL, res, yylineno);
        $$ = res;
    }
    | lvalue DOUBLEDOT ID LPARENTHESIS RPARENTHESIS {
        expr *obj = $1;
        if (obj->type == tableitem_e) {
            obj = emit_iftableitem(obj);
        }
        
        expr *method = member_item(obj, $3->text);
        expr *method_func = emit_iftableitem(method);
        
        emitQuad(param, obj, NULL, NULL, yylineno);
        
        expr *res = newexpr(var_e);
        res->sym = newtemp();
        emitQuad(call, method_func, NULL, NULL, yylineno);
        emitQuad(getretval, NULL, NULL, res, yylineno);
        $$ = res;
    }
    | LPARENTHESIS funcdef RPARENTHESIS LPARENTHESIS param_list RPARENTHESIS {
        expr *func = $2; 
        
        expr *res = newexpr(var_e);
        res->sym = newtemp();
        
        emitQuad(call, func, NULL, NULL, yylineno);
        emitQuad(getretval, NULL, NULL, res, yylineno);
        
        $$ = res;
    }
    | LPARENTHESIS funcdef RPARENTHESIS LPARENTHESIS RPARENTHESIS {  
        expr *func = $2;
        
        expr *res = newexpr(var_e);
        res->sym = newtemp();
        
        emitQuad(call, func, NULL, NULL, yylineno);
        emitQuad(getretval, NULL, NULL, res, yylineno);
        
        $$ = res;
    }
;

callsuffix:	normcall
	
	;

normcall: LPARENTHESIS RPARENTHESIS
    | LPARENTHESIS param_list RPARENTHESIS
    ;

param_list:
    expr {
        if ($1 && $1->type == boolexpr_e)
            $1 = finish_bool_expr($1);
        emitQuad(param, $1, NULL, NULL, yylineno);
    }
     | expr COMMA param_list {
        if ($1 && $1->type == boolexpr_e)
            $1 = finish_bool_expr($1);
        emitQuad(param, $1, NULL, NULL, yylineno);
    }
    ;



objectdef: LBRACKET RBRACKET {
        expr *t = newexpr(newtable_e);
        t->sym = newtemp();
        emitQuad(tablecreate, NULL, NULL, t, yylineno);
        
        expr *table_var = newexpr(var_e);
        table_var->sym = t->sym;
        $$ = table_var;
    }
    | LBRACKET {
        save_array_index();
        current_array_index = 0;
    } elist RBRACKET {
        restore_array_index();
        $$ = $3;
    }
    | LBRACKET {
        if (pending_stack_ptr < 50) {
            pending_stack[pending_stack_ptr].count = 0;
            pending_stack_ptr++;
        }
    } indexed RBRACKET {
        if (pending_stack_ptr > 0) {
            pending_stack_ptr--;
            pending_table *current = &pending_stack[pending_stack_ptr];
            
            expr *table = newexpr(newtable_e);
            table->sym = newtemp();
            emitQuad(tablecreate, NULL, NULL, table, yylineno);
            
            expr *table_var = newexpr(var_e);
            table_var->sym = table->sym;
            
            for (int i = 0; i < current->count; i++) {
                emitQuad(tablesetelem, current->elements[i].key, current->elements[i].value, table_var, yylineno);
            }
            
            $$ = table_var;
        }
    }
    ;
elist:
    expr {
        expr *table = newexpr(newtable_e);
        table->sym = newtemp();
        emitQuad(tablecreate, NULL, NULL, table, yylineno);
        
        expr *table_var = newexpr(var_e);
        table_var->sym = table->sym;
        
        expr *index = newexpr(constnum_e);
        index->numConst = current_array_index++;
        
        emitQuad(tablesetelem, index, $1, table_var, yylineno);
        
        $$ = table_var;
    }
    | elist COMMA expr {
        expr *index = newexpr(constnum_e);
        index->numConst = current_array_index++;
        
        emitQuad(tablesetelem, index, $3, $1, yylineno);
        
        $$ = $1;
    }
    ;

indexed: indexedelem {
        $$ = $1;
    }
    | indexed COMMA indexedelem {
        $$ = $1;
    }
    ;

indexedelem: LBRACE expr COLON expr RBRACE {
        
        if (pending_stack_ptr > 0) {
            pending_table *current = &pending_stack[pending_stack_ptr - 1];
            if (current->count < 100) {
                current->elements[current->count].key = $2;
                current->elements[current->count].value = $4;
                current->count++;
            }
        }
        
        $$ = NULL;  
    }
    ;

block:	LBRACE { scope++; save_temp_state();} stmt_list RBRACE { scope--; restore_temp_state(); }
	;


 funcdef:
    FUNCTION {
        char anon_name[20];
        snprintf(anon_name, sizeof(anon_name), "$%d", anonym_func++);
        
        SymbolTableEntry *f = insert_function(strdup(anon_name), yylineno, scope, USERFUNC);
        f->next = head;
        head = f;
        
        if (func_stack_ptr < 100) {
            func_stack[func_stack_ptr++] = f;
        }
        
        int jump_over = nextQuad();
        emitQuad(jump, NULL, NULL, NULL, yylineno);
        
        emitQuad(funcstart, newexpr_func(f), NULL, NULL, yylineno);
        
        $<intVal>$ = jump_over;
        
        function_scope_level++;
        inside_function++;
        scope++;
        
        for (int i = 0; i < current_arg_count; i++) {
            if (current_args[i]) {
                free(current_args[i]);
                current_args[i] = NULL;
            }
        }
        current_arg_count = 0;
        
    } LPARENTHESIS idlist { scope--; } RPARENTHESIS block {
        SymbolTableEntry *f = NULL;
        if (func_stack_ptr > 0) {
            f = func_stack[--func_stack_ptr];
        }
        
        emitQuad(funcend, newexpr_func(f), NULL, NULL, yylineno);
        int func_end = nextQuad();
for (int i = 0; i < currQuad; i++) {
    if (quads[i].op == jump && quads[i].label == 7777) {
        patchlabel(i, func_end);
        quads[i].label = 0;
    }
}

        int jump_over = $<intVal>2;

        patchlabel(jump_over, func_end);
        
        function_scope_level--;
        inside_function--;
        
        expr *func_expr = newexpr(programfunc_e); 
        func_expr->sym = f;

        $$ = func_expr;
    }
    
    | FUNCTION ID {
        SymbolTableEntry *conflict = lookup_in_scope(head, $2->text, scope);
        if (conflict) {
            fprintf(stderr, "Error: Variable '%s' already defined at line: %d\n",
                    $2->text,
                    (conflict->type == USERFUNC || conflict->type == LIBFUNC)
                        ? conflict->value.funcVal->line
                        : conflict->value.varVal->line);
            yyerror("Semantic error");
            YYERROR;
        }

        SymbolTableEntry *f = insert_function($2->text, $2->lineno, scope, USERFUNC);

        if (lookup_library(head, $2->text)) {
            fprintf(stderr, "Error: User function '%s' collides with library function (line %d)\n", $2->text, $2->lineno);
            yyerror("Semantic error");
        }

        f->next = head;
        head = f;
        
        if (func_stack_ptr < 100) {
            func_stack[func_stack_ptr++] = f;
        }
        
        current_func_jump = nextQuad();
    emitQuad(jump, NULL, NULL, NULL, yylineno);
        
        emitQuad(funcstart, newexpr_func(f), NULL, NULL, yylineno);
        
        function_scope_level++;
        inside_function++;
        scope++;
        
        for (int i = 0; i < current_arg_count; i++) {
            if (current_args[i]) {
                free(current_args[i]);
                current_args[i] = NULL;
            }
        }
        current_arg_count = 0;
    } LPARENTHESIS idlist { scope--; } RPARENTHESIS block {
        SymbolTableEntry *f = NULL;
        if (func_stack_ptr > 0) {
            f = func_stack[--func_stack_ptr];
        }
        
        emitQuad(funcend, newexpr_func(f), NULL, NULL, yylineno);
        
    int func_end = nextQuad();
    for (int i = 0; i < currQuad; i++) {
        if (quads[i].op == jump && quads[i].label == 7777) {
            patchlabel(i, func_end-1);
        }
    }
    
    patchlabel(current_func_jump, func_end);
        
        function_scope_level--;
        inside_function--;
        
        expr *func_expr = newexpr(programfunc_e);
        func_expr->sym = f;
        
        $$ = func_expr;
    }
;


idlist:
    ID {

        if (is_duplicate_arg($1->text)) {
            fprintf(stderr, "Error: Duplicate argument '%s' in function definition (line %d)\n", $1->text, $1->lineno);
            yyerror("Semantic error");
            YYERROR;
        }

        if (current_arg_count >= 128) {
            fprintf(stderr, "Too many arguments in function definition (line %d)\n", $1->lineno);
            yyerror("Semantic error");
            YYERROR;
        }

        current_args[current_arg_count++] = strdup($1->text);

        SymbolTableEntry *arg = insert_argument($1, $1->lineno, scope);
        if (arg) {
            arg->next = head;
            head = arg;
        }
    }

  | ID COMMA idlist {

        if (is_duplicate_arg($1->text)) {
            fprintf(stderr, "Error: Duplicate argument '%s' in function definition (line %d)\n", $1->text, $1->lineno);
            yyerror("Semantic error");
            YYERROR;
        }

        if (current_arg_count >= 128) {
            fprintf(stderr, "Too many arguments in function definition (line %d)\n", $1->lineno);
            yyerror("Semantic error");
            YYERROR;
        }

        current_args[current_arg_count++] = strdup($1->text);

        SymbolTableEntry *arg = insert_argument($1, $1->lineno, scope);
        if (arg) {
            arg->next = head;
            head = arg;
        }
    }

  |  
;

if_prefix: IF LPARENTHESIS expr RPARENTHESIS {
    expr *cond = $3;
    

    if (cond->type == boolexpr_e) {
        cond = finish_bool_expr(cond);
    }

    int condQuad = nextQuad();
    emitQuad(if_eq, cond, newexpr_constbool(1), NULL, yylineno);
    

    int jumpQuad = nextQuad();
    emitQuad(jump, NULL, NULL, NULL, yylineno);
    

    patchlabel(condQuad, jumpQuad + 1);

    $$ = jumpQuad;
}

ifstmt: if_prefix stmt %prec LOWER_THAN_ELSE {

    patchlabel($1, nextQuad());
}

| if_prefix stmt ELSE {

    int jump_end_quad = nextQuad();
    emitQuad(jump, NULL, NULL, NULL, yylineno);
    
    patchlabel($1, nextQuad());
    patchlabel($1 - 1, $1 + 1);
 
    $<intVal>$ = jump_end_quad;
} stmt {

    patchlabel($<intVal>4, nextQuad());
}

whilestmt:
    WHILE {

        int loop_start = nextQuad();
        $<intVal>$ = loop_start;
    } LPARENTHESIS expr RPARENTHESIS {
        expr *cond = $4;
        
        if (cond->type == boolexpr_e) {
            cond = finish_bool_expr(cond);
        }
        
        int condition_quad = nextQuad();
        emitQuad(if_eq, cond, newexpr_constbool(1), NULL, yylineno);
        
        int jump_quad = nextQuad();
        emitQuad(jump, NULL, NULL, NULL, yylineno);
        
        patchlabel(condition_quad, jump_quad + 1);
        
        if (loop_stack_ptr < 100) {
            loop_stack[loop_stack_ptr].loop_start = $<intVal>2;
            loop_stack[loop_stack_ptr].jump_quad = jump_quad;
            loop_stack_ptr++;
        }
        
        inside_loop++;
    } stmt {
        if (loop_stack_ptr > 0) {
            loop_stack_ptr--;
            int loop_start = loop_stack[loop_stack_ptr].loop_start;
            int jump_quad = loop_stack[loop_stack_ptr].jump_quad;
            
            int back_jump = nextQuad();
            emitQuad(jump, NULL, NULL, NULL, yylineno);
            patchlabel(back_jump, loop_start);  
            
            int loop_end = nextQuad();
            
            patchlabel(jump_quad, loop_end);
             for (int i = loop_start; i < currQuad; i++) {
            if (quads[i].op == jump) {
                if (quads[i].label == 9999) {
                   
                    patchlabel(i, loop_end);
                  
                } else if (quads[i].label == 8888) {
                   
                    patchlabel(i, loop_start);
                   
                }
            }
        }
        }
        
        inside_loop--;
    }
;
forstmt:
    FOR LPARENTHESIS expr SEMICOLON {
        for_condition_start = nextQuad();
    } expr SEMICOLON {
        expr *cond = $6;
        if (cond->type == boolexpr_e) {
            cond = finish_bool_expr(cond);
        }
        
        int cond_true_jump = nextQuad();
   
        emitQuad(if_eq, cond, newexpr_constbool(1), NULL, yylineno);


        for_exit_jump = nextQuad();
        emitQuad(jump, NULL, NULL, NULL, yylineno);

   
        $<intVal>$ = cond_true_jump;
        increment_start = nextQuad();

        inside_loop++;
    } expr RPARENTHESIS {
        

        for_increment_start = increment_start; 
        

        int back_to_condition = nextQuad();
        emitQuad(jump, NULL, NULL, NULL, yylineno);
        patchlabel(back_to_condition, for_condition_start);
       
        int cond_true_jump = $<intVal>8;
        patchlabel(cond_true_jump, nextQuad());
  
    } stmt {
      
        int to_increment = nextQuad();
        emitQuad(jump, NULL, NULL, NULL, yylineno);
        patchlabel(to_increment, for_increment_start);  
        
       
        patchlabel(for_exit_jump, nextQuad());
           for (int i = for_condition_start; i < currQuad; i++) {
            if (quads[i].op == jump) {
                if (quads[i].label == 9999) {
                   
                  
                    patchlabel(i, to_increment+1);
                   
                } else if (quads[i].label == 8888) {
                  
                    patchlabel(i, for_increment_start);
                   
                }
            }
        }
        inside_loop--;
    }
;

returnstmt:
    RETURN expr SEMICOLON {
        if (inside_function == 0) {
            error_flag=1;
            fprintf(stderr, "Error: 'return' used outside of function (line %d)\n", $1->lineno);
            yyerror("Semantic error");
        } else {
            
            expr *retval = $2;
            if (retval->type == boolexpr_e) {
                retval = finish_bool_expr(retval);
            }
            
           
            emitQuad(ret, retval, NULL, NULL, yylineno);
            
            
            int jump_to_end = nextQuad();
            emitQuad(jump, NULL, NULL, NULL, yylineno);
            quads[jump_to_end].label = 7777;
        }
    }
    | RETURN SEMICOLON {
        if (inside_function == 0) {
            error_flag=1;
            fprintf(stderr, "Error: 'return' used outside of function (line %d)\n", $1->lineno);
            yyerror("Semantic error");
        } else {
            emitQuad(ret, NULL, NULL, NULL, yylineno);
            
            int jump_to_end = nextQuad();
            emitQuad(jump, NULL, NULL, NULL, yylineno);
            quads[jump_to_end].label = 7777;
        }
    };

%%

int yyerror(const char *msg){
	fprintf(stderr, "%s: at line %d, before token: %s\n", msg, yylineno, yytext);
	fprintf(stderr, "INPUT NOT VALID\n");
	return 1   ;
}

void print_usage(const char* program_name) {
    printf("Alpha Language Compiler\n");
    printf("Usage: %s [input_file] [output_file]\n", program_name);
    printf("\n");
    printf("Arguments:\n");
    printf("  input_file   Alpha source file (.alpha) [default: stdin]\n");
    printf("  output_file  Binary output file (.abc) [default: program.abc]\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s test.alpha                # Compile test.alpha to program.abc\n", program_name);
    printf("  %s test.alpha test.abc       # Compile test.alpha to test.abc\n", program_name);
    printf("  echo 'print(42);' | %s       # Compile from stdin\n", program_name);
}

int main(int argc, char **argv) {
    char* input_file = NULL;
    char* output_file = "program.abc";
    
    if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        input_file = argv[1];
    } else if (argc == 3) {
        input_file = argv[1];
        output_file = argv[2];
    } else if (argc > 3) {
        printf("Error: Too many arguments\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    yylineno = 1;
    head = NULL;
    init_library_functions();
    quads = NULL;
    currQuad = 0;
    totalQuads = 0;
    error_flag = 0;
    
    if (input_file) {
        yyin = fopen(input_file, "r");
        if (!yyin) {
            printf("Error: Could not open input file '%s'\n", input_file);
            return 1;
        }
        printf("Compiling: %s\n", input_file);
    } else {
        printf("\nReading from file...\n");
    }
    
    if (!yyparse() && !error_flag) {
        printf("Parsing completed successfully\n");
        
        assign_offsets();
        
        printQuadsToFile("quads_output.txt");
        
        printf("Generating target code...\n");
        generate_target_code();
        
        print_instructions_text("target_code.txt");
        
        write_instructions_binary(output_file);
        
        printf("\nCompilation completed successfully!\n");
        printf("\nGenerated files:\n");
        printf("   quads_output.txt  - Intermediate code\n");
        printf("   target_code.txt   - Target instructions (phase 4)\n");
        printf("   %-16s - Binary executable\n", output_file);
        printf("\nRun with: alpha_vm %s\n", output_file);
        
    } else {
        printf("Compilation failed\n");
        if (input_file) fclose(yyin);
        return 1;
    }
    
    if (input_file) fclose(yyin);
    return 0;
}