#include "avm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double* numConsts = NULL;
unsigned totalNumConsts = 0;
char** stringConsts = NULL;
unsigned totalStringConsts = 0;
char** namedLibfuncs = NULL;
unsigned totalNamedLibfuncs = 0;
userfunc* userFuncs = NULL;
unsigned totalUserFuncs = 0;
instruction* instructions = NULL;
unsigned currentInstruction = 0;

int avm_load_binary(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Could not open binary file %s\n", filename);
        return 0;
    }
    
    unsigned int magic;
    if (fread(&magic, sizeof(unsigned int), 1, file) != 1 || magic != 0x12345678) {
        printf("Error: Invalid binary file format\n");
        fclose(file);
        return 0;
    }
    
    if (fread(&currentInstruction, sizeof(unsigned), 1, file) != 1) {
        printf("Error: Could not read instruction count\n");
        fclose(file);
        return 0;
    }
    
    instructions = malloc(currentInstruction * sizeof(instruction));
    if (!instructions) {
        printf("Error: Memory allocation failed\n");
        fclose(file);
        return 0;
    }
    
    if (fread(instructions, sizeof(instruction), currentInstruction, file) != currentInstruction) {
        printf("Error: Could not read instructions\n");
        fclose(file);
        return 0;
    }
    
    if (fread(&totalNumConsts, sizeof(unsigned), 1, file) != 1) {
        printf("Error: Could not read number constants count\n");
        fclose(file);
        return 0;
    }
    
    if (totalNumConsts > 0) {
        numConsts = malloc(totalNumConsts * sizeof(double));
        if (fread(numConsts, sizeof(double), totalNumConsts, file) != totalNumConsts) {
            printf("Error: Could not read number constants\n");
            fclose(file);
            return 0;
        }
    }
    
    if (fread(&totalStringConsts, sizeof(unsigned), 1, file) != 1) {
        printf("Error: Could not read string constants count\n");
        fclose(file);
        return 0;
    }
    
    if (totalStringConsts > 0) {
        stringConsts = malloc(totalStringConsts * sizeof(char*));
        for (unsigned i = 0; i < totalStringConsts; i++) {
            unsigned len;
            if (fread(&len, sizeof(unsigned), 1, file) != 1) {
                printf("Error: Could not read string length\n");
                fclose(file);
                return 0;
            }
            stringConsts[i] = malloc(len);
            if (fread(stringConsts[i], sizeof(char), len, file) != len) {
                printf("Error: Could not read string constant\n");
                fclose(file);
                return 0;
            }
        }
    }
    
    if (fread(&totalNamedLibfuncs, sizeof(unsigned), 1, file) != 1) {
        printf("Error: Could not read library functions count\n");
        fclose(file);
        return 0;
    }
    
    if (totalNamedLibfuncs > 0) {
        namedLibfuncs = malloc(totalNamedLibfuncs * sizeof(char*));
        for (unsigned i = 0; i < totalNamedLibfuncs; i++) {
            unsigned len;
            if (fread(&len, sizeof(unsigned), 1, file) != 1) {
                printf("Error: Could not read library function name length\n");
                fclose(file);
                return 0;
            }
            namedLibfuncs[i] = malloc(len);
            if (fread(namedLibfuncs[i], sizeof(char), len, file) != len) {
                printf("Error: Could not read library function name\n");
                fclose(file);
                return 0;
            }
        }
    }
    
    if (fread(&totalUserFuncs, sizeof(unsigned), 1, file) != 1) {
        printf("Error: Could not read user functions count\n");
        fclose(file);
        return 0;
    }
    
    if (totalUserFuncs > 0) {
        userFuncs = malloc(totalUserFuncs * sizeof(userfunc));
        if (fread(userFuncs, sizeof(userfunc), totalUserFuncs, file) != totalUserFuncs) {
            printf("Error: Could not read user functions\n");
            fclose(file);
            return 0;
        }
    }
    
    fclose(file);
    return 1;
}

void print_usage(const char* program_name) {
    printf("Usage: %s <binary_file.abc>\n", program_name);
    printf("Alpha Virtual Machine - Executes compiled Alpha programs\n");
}

void setup_main_call(void) {
    extern avm_memcell stack[];
    extern unsigned top, topsp, totalActuals;
    
    totalActuals = 0;  
    
    stack[top].type = number_m;
    stack[top].data.numVal = 0;  
    top--;
    
    stack[top].type = number_m; 
    stack[top].data.numVal = 999; 
    top--;
    
    stack[top].type = number_m;
    stack[top].data.numVal = top + 2; 
    top--;
    
    stack[top].type = number_m;
    stack[top].data.numVal = 0;  
    top--;
    
    topsp = top + 4; 
}

int main(int argc, char **argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    printf("\n=== Alpha Virtual Machine ===\n");
    printf("Loading: %s\n\n", argv[1]);
    
    avm_initialize();
    
    if (!avm_load_binary(argv[1])) {
        printf("Failed to load binary file\n");
        return 1;
    }
    
    printf("=== Execution ===\n\n");
    
    while (!executionFinished) {
        execute_cycle();
    }
    
    printf("\n=== Execution Completed ===\n");
    
    return 0;
}