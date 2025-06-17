%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "expr.h"

#include "parser.tab.h"
#include "myStruct.h"
#define YY_NO_INPUT

int token_count = 0;

void setToken(AlphaToken *token, int lineno, int token_num, const char *text, const char *type, const char *content, const char *comment, TokenType token_type);
%}

%option noyywrap
%option yylineno

ID              [A-Za-z_][A-Za-z0-9_]*
INTEGER         [0-9]+
REAL            [0-9]+\.[0-9]+
STRING_LITERAL  \"([^\"\\]|\\.)*\"

%%

"function"       { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "function", "", FUNCTION_TOK); return FUNCTION; }
"if"             { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "if", "", IF_TOK); return IF; }
"else"           { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "else", "", ELSE_TOK); return ELSE; }
"while"          { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "while", "", WHILE_TOK); return WHILE; }
"for"            { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "for", "", FOR_TOK); return FOR; }
"return"         { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "return", "", RETURN_TOK); return RETURN; }
"break"          { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "break", "", BREAK_TOK); return BREAK; }
"continue"       { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "continue", "", CONTINUE_TOK); return CONTINUE; }
"and"            { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "and", "", AND_TOK); return AND; }
"or"             { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "or", "", OR_TOK); return OR; }
"not"            { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "not", "", NOT_TOK); return NOT; }
"local"          { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "KEYWORD", "local", "", LOCAL_TOK); return LOCAL; }
"true"           { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "BOOLEAN", "true", "", TRUE_TOK); return TRUE; }
"false"          { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "BOOLEAN", "false", "", FALSE_TOK); return FALSE; }
"nil"            { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "NIL", "nil", "", NIL_TOK); return NIL; }

"=="             { return EQUAL; }
"!="             { return NOTEQUAL; }
">="             { return GEQUAL; }
"<="             { return LEQUAL; }
">"              { return GTHAN; }
"<"              { return LTHAN; }
"="              { return ASSIGN; }
"+"              { return PLUS; }
"-"              { return MINUS; }
"*"              { return MUL; }
"/"              { return DIV; }
"%"              { return MOD; }
"++"             { return PLUSPLUS; }
"--"             { return MINUSMINUS; }
"{"              { return LBRACE; }
"}"              { return RBRACE; }
"["              { return LBRACKET; }
"]"              { return RBRACKET; }
"("              { return LPARENTHESIS; }
")"              { return RPARENTHESIS; }
";"              { return SEMICOLON; }
","              { return COMMA; }
":"              { return COLON; }
"::"             { return DOUBLECOLON; }
".."             { return DOUBLEDOT; }
"."              { return DOT; }
"&&"             { return AND; }
"||"             { return OR; }
"!"                 { return NOT; }
{ID}             { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "ID", yytext, "identifier", IDENTIFIER_TOK); return ID; }
{REAL} {
    yylval.token = malloc(sizeof(AlphaToken));
    setToken(yylval.token, yylineno, ++token_count, yytext, "CONST_FLOAT", yytext, "float", REAL_TOK);
    return CONST_FLOAT;
}

{INTEGER}         { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "CONST_INT", yytext, "int", INTEGER_TOK); return CONST_INT; }
{STRING_LITERAL}  { yylval.token = malloc(sizeof(AlphaToken)); setToken(yylval.token, yylineno, ++token_count, yytext, "STRING", yytext, "string", STRING_TOK); return STRING; }

[ \t\r\n]+        ; 
"//".*           ; 
"/*"([^*]|\*+[^*/])*\*+"/" ; 

. { fprintf(stderr, "Unknown token: %s at line %d\n", yytext, yylineno); return -1; }
%%

static void __attribute__((unused)) suppress_warning(void) { 
    if(0) yyunput(0,NULL); 
}

void setToken(AlphaToken *token, int lineno, int token_num, const char *text,
              const char *type, const char *content, const char *comment, TokenType token_type) {
    token->token_type = token_type;
    token->lineno = lineno;
    token->token_num = token_num;
    token->text = strdup(text);
    token->type = strdup(type);
    token->content = strdup(content);
    token->comment = strdup(comment);
}
