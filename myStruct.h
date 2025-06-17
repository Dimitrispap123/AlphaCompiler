#ifndef MYSTRUCT_H
#define MYSTRUCT_H
#include <stdio.h>
#include <stdlib.h>

typedef enum
{
    NULL_TOK,
    INTEGER_TOK,
    REAL_TOK,
    IDENTIFIER_TOK,
    UNKNOWN_TOK,
    STRING_TOK,
    COMMENT_TOK,
    LINE_COMMENT_TOK,
    BLOCK_COMMENT_TOK,
    NESTED_COMMENT_TOK,
    IF_TOK,
    ELSE_TOK,
    WHILE_TOK,
    FOR_TOK,
    FUNCTION_TOK,
    RETURN_TOK,
    BREAK_TOK,
    CONTINUE_TOK,
    AND_TOK,
    NOT_TOK,
    OR_TOK,
    LOCAL_TOK,
    TRUE_TOK,
    FALSE_TOK,
    NIL_TOK,
    ASSIGN_TOK,
    PLUS_TOK,
    MINUS_TOK,
    MUL_TOK,
    DIV_TOK,
    MOD_TOK,
    EQUAL_TOK,
    NOTEQUAL_TOK,
    PLUSPLUS_TOK,
    MINUSMINUS_TOK,
    GTHAN_TOK,
    LTHAN_TOK,
    GEQUAL_TOK,
    LEQUAL_TOK,
    LBRACE_TOK,
    RBRACE_TOK,
    LBRACKET_TOK,
    RBRACKET_TOK,
    LPARENTHESIS_TOK,
    RPARENTHESIS_TOK,
    SEMICOLON_TOK,
    COMMA_TOK,
    COLON_TOK,
    DOUBLECOLON_TOK,
    DOT_TOK,
    DOUBLEDOT_TOK
} TokenType;

typedef struct alpha_token_t
{
    TokenType token_type;
    int lineno;
    int endline;
    int token_num;
    char *text;
    char *type;
    char *comment;
    char *content;
    struct alpha_token_t *alpha_yylex;
} AlphaToken;

typedef struct node_t
{
    unsigned int start_line;
    struct node_t *next;
} node_t;

typedef struct stack_t
{
    struct node_t *top;
} stack_t;

stack_t *init();

int is_empty(stack_t *stack);

int push(stack_t *stack, unsigned int line);

unsigned int pop(stack_t *stack);
#endif 