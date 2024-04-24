#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "cJSON_Utils.h"
#include "VLIW470.h"



void parseString(char *instr_string, InstructionEntry *entry)
{
    char *token = strtok(instr_string, " ");
    strcpy(entry->opcode, token+1);

   
    // if opcode is ld or st this is the format -> "ld x3, imm(x0)"
    if (strcmp(entry->opcode, "ld") == 0 || strcmp(entry->opcode, "st") == 0)
    {

        token = strtok(NULL, " ");
        entry->dest = atoi(token + 1);


        //"ld x3, imm(x0)"
        // if there isn't an imm , imm =0
        token = strtok(NULL, " ");
        
        if (token[0] == '(')
        {

            entry->imm = 0;
            token += 1;
        }
        else
        {
            entry->imm = atoi(token);

            token += 2;
        }

        entry->src1 = atoi(token + 1);
        entry->src2 = -1;
    }
    else if (strcmp(entry->opcode, "loop") == 0 || strcmp(entry->opcode, "loop.pip")==0)
    { // if opcode is loop
 
        token = strtok(NULL, " ");
        entry->imm = atoi(token);
        entry->dest = -1;
        entry->src1 = -1;
        entry->src2 = -1;
    }
    //if opcode == nop"
    else if (strncmp(entry->opcode,"nop",3) == 0)
    { // if opcode is nop
 
        strncpy(entry->opcode, "nop", 3);
        entry->dest = -1;
        entry->src1 = -1;
        entry->src2 = -1;
        entry->imm = -1;
    }
    else if (strcmp(entry->opcode, "mov") == 0)
    {
    token = strtok(NULL, " ");
        // if dest = "pX" then dest = -2 and imm = 0 if false and 1 if true
        if (token[0] == 'p')
        {
            entry->dest = -2;
            entry->imm = atoi(strtok(NULL, " "));
        }
        else if (token[1] == "C")
        {
            // if dest = LC then dest = -3  or dest = EC then dest = -4
            if (token[0] == 'L')
            {
                entry->dest = -3;
            }
            else
            {
                entry->dest = -4;
            }
            entry->imm = atoi(strtok(NULL, " "));
        }
        else
        {
 
            entry->dest = atoi(token + 1);
            // need to check if there is an imm or a src

            token = strtok(NULL, " ");
            if (token[0] == 'x')
            {
                entry->src1 = atoi(token + 1);
                entry->src2 = -1;
                entry->imm = -1;
            }
            else
            {
                entry->src1 = -1;
                entry->src2 = -1;
                entry->imm = atoi(token);
            }
        }
    }
    else
    {

        token = strtok(NULL, ",");
        entry->dest = atoi(token + 2);
        token = strtok(NULL, ",");
        entry->src1 = atoi(token + 2);
        token = strtok(NULL, ",");
        // need to check if last element is imm or  xX
        if (token[0] == 'x')
        {
            entry->src2 = atoi(token + 2);
            entry->imm = -1;
        }
        else
        {
            entry->src2 = -1;
            entry->imm = atoi(token);
        }
    }
}