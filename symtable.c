#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "myStruct.h"
#include "symtable.h"

#define MAX_SCOPE 1000
extern SymbolTableEntry *head;
SymbolTableEntry *insert_variable(char *name, unsigned int line, unsigned int scope, SymbolType type)
{
    SymbolTableEntry *entry = malloc(sizeof(SymbolTableEntry));
    Variable *var = malloc(sizeof(Variable));

    var->name = strdup(name);
    var->line = line;
    var->scope = scope;
    var->arg_next = NULL;

    entry->type = type;
    entry->shown = 1;
    entry->value.varVal = var;
    entry->next = NULL;

    return entry;
}

SymbolTableEntry *insert_function(char *name, unsigned int line, unsigned int scope, SymbolType type)
{
    SymbolTableEntry *entry = malloc(sizeof(SymbolTableEntry));
    Function *func = malloc(sizeof(Function));

    func->name = strdup(name);
    func->line = line;
    func->scope = scope;
    func->arguments = NULL;

    entry->type = type;
    entry->shown = 1;
    entry->value.funcVal = func;
    entry->next = NULL;

    return entry;
}

SymbolTableEntry *insert_argument(AlphaToken *token, unsigned int line, unsigned int scope)
{
    extern SymbolTableEntry *head;
    if (lookup_library(head, token->text))
    {
        fprintf(stderr, "Error: Argument '%s' collides with library function (line %d)\n", token->text, token->lineno);
        return NULL;
    }

    SymbolTableEntry *entry = insert_variable(token->text, line, scope, FORMAL_ARG);
    return entry;
}

void add_to_head(SymbolTableEntry *entry)
{
    extern SymbolTableEntry *head;
    entry->next = head;
    head = entry;
}

void init_library_functions()
{
    const char *libs[] = {
        "print", "input", "objectmemberkeys", "objecttotalmembers", "objectcopy",
        "totalarguments", "argument", "typeof", "strtonum", "sqrt", "cos", "sin"};
    for (int i = 0; i < sizeof(libs) / sizeof(libs[0]); i++)
    {
        SymbolTableEntry *lib = insert_function(strdup(libs[i]), 0, 0, LIBFUNC);
        lib->shown = 1;
        add_to_head(lib);
    }
}

int compare_entries(const void *a, const void *b)
{
    SymbolTableEntry *entry1 = *(SymbolTableEntry **)a;
    SymbolTableEntry *entry2 = *(SymbolTableEntry **)b;

    int line1 = (entry1->type == USERFUNC || entry1->type == LIBFUNC)
                    ? entry1->value.funcVal->line
                    : entry1->value.varVal->line;

    int line2 = (entry2->type == USERFUNC || entry2->type == LIBFUNC)
                    ? entry2->value.funcVal->line
                    : entry2->value.varVal->line;

    return line1 - line2;
}

void print_table(SymbolTableEntry *head)
{

    for (int current_scope = 0; current_scope <= MAX_SCOPE; ++current_scope)
    {

        SymbolTableEntry *current = head;
        SymbolTableEntry *entries[1024];
        int count = 0;

        while (current)
        {
            int entry_scope = (current->type == USERFUNC || current->type == LIBFUNC)
                                  ? current->value.funcVal->scope
                                  : current->value.varVal->scope;

            if (entry_scope == current_scope && current->shown)
            {
                if (count < 1024)
                {
                    entries[count++] = current;
                }
                else
                {
                    fprintf(stderr, "Too many entries in scope %d — some symbols skipped\n", current_scope);
                    break;
                }
            }

            current = current->next;
        }

        if (count > 0)
        {
            qsort(entries, count, sizeof(SymbolTableEntry *), compare_entries);
            printf("\n-----------  Scope #%d  -----------\n", current_scope);

            for (int i = 0; i < count; ++i)
            {
                SymbolTableEntry *entry = entries[i];

                const char *kind =
                    (entry->type == GLOBAL_VAR) ? "global variable" : (entry->type == LOCAL_VAR)                         ? "local variable"
                                                                  : (entry->type == FORMAL || entry->type == FORMAL_ARG) ? "formal argument"
                                                                  : (entry->type == USERFUNC)                            ? "user function"
                                                                  : (entry->type == LIBFUNC)                             ? "library function"
                                                                                                                         : "unknown";

                const char *name = (entry->type == USERFUNC || entry->type == LIBFUNC)
                                       ? entry->value.funcVal->name
                                       : entry->value.varVal->name;

                int line = (entry->type == USERFUNC || entry->type == LIBFUNC)
                               ? entry->value.funcVal->line
                               : entry->value.varVal->line;

                printf("\"%s\" [%s] (1line %d) (scope %d)\n",
                       name, kind, line, current_scope);
            }
        }
    }
}


SymbolTableEntry *lookup_in_scope(SymbolTableEntry *head, const char *name, unsigned int scope)
{
    SymbolTableEntry *current = head;
    while (current)
    {
        int entry_scope = (current->type == USERFUNC || current->type == LIBFUNC)
                              ? current->value.funcVal->scope
                              : current->value.varVal->scope;

        const char *entry_name = (current->type == USERFUNC || current->type == LIBFUNC)
                                     ? current->value.funcVal->name
                                     : current->value.varVal->name;

        if (entry_scope == scope && strcmp(entry_name, name) == 0)
        {
            return current;
        }

        current = current->next;
    }
    return NULL;
}

SymbolTableEntry *lookup_visible(SymbolTableEntry *head, const char *name, unsigned int current_scope)
{
    SymbolTableEntry *current = head;
    while (current)
    {
        const char *entry_name = (current->type == USERFUNC || current->type == LIBFUNC)
                                     ? current->value.funcVal->name
                                     : current->value.varVal->name;

        int entry_scope = (current->type == USERFUNC || current->type == LIBFUNC)
                              ? current->value.funcVal->scope
                              : current->value.varVal->scope;

        if (strcmp(entry_name, name) == 0 && current->shown && entry_scope <= current_scope)
        {
            return current;
        }

        current = current->next;
    }
    return NULL;
}

SymbolTableEntry *lookup_library(SymbolTableEntry *head, const char *name)
{
    SymbolTableEntry *current = head;
    while (current)
    {
        if (current->type == LIBFUNC &&
            strcmp(current->value.funcVal->name, name) == 0)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}