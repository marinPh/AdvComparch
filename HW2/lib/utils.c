#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "cJSON_Utils.h"
#include "VLIW470.h"
#include <stdbool.h>

void parseString(char *instr_string, InstructionEntry *entry)
{
    char *token = strtok(instr_string, " ");
    strcpy(entry->opcode, token+1);

    if (strcmp(entry->opcode, "addi") == 0)
    {
        entry->type = ADDI;
    }
    else if (strcmp(entry->opcode, "add") == 0)
    {
        entry->type = ADD;
    }
    else if (strcmp(entry->opcode, "sub") == 0)
    {
        entry->type = SUB;
    }
    else if (strcmp(entry->opcode, "mulu") == 0)
    {
        entry->type = MULU;
    }
    else if (strcmp(entry->opcode, "ld") == 0)
    {
        entry->type = LD;
    }
    else if (strcmp(entry->opcode, "st") == 0)
    {
        entry->type = ST;
    }
    else if (strncmp(entry->opcode, "nop", 3) == 0)
    {
        entry->type = NOP;
    }
    else if (strcmp(entry->opcode, "mov") == 0)
    {
        entry->type = MOV;
    }
    else if (strcmp(entry->opcode, "loop") == 0)
    {
        entry->type = LOOP;
    }
    else if (strcmp(entry->opcode, "loop_pip") == 0)
    {
        entry->type = LOOP_PIP;
    }
    else
    {
        printf("Invalid instruction\n");
        exit(1);
    }

    entry->predicate = -1; // default value, will be updated only for BR ops if needed
    entry->cycle = 0;
    entry->done = false;
   
    // if opcode is ld or st this is the format -> "ld x3, imm(x0)"
    if (entry->type == LD || entry->type == ST)
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
            // check if imm is dec or hex, hex starts with 0x and need to be converted to dec
            if (token[0] == '0' && token[1] == 'x')
            {
                entry->imm = (int)strtol(token, NULL, 16);
            }
            else
            {
                entry->imm = atoi(token);
            }

            token += 2;
        }

        entry->src1 = atoi(token + 1);
        entry->src2 = -1;
    }
    else if (entry->type == LOOP || entry->type == LOOP_PIP)
    { // if opcode is loop
        token = strtok(NULL, " ");
        entry->imm = atoi(token);
        entry->dest = -1;
        entry->src1 = -1;
        entry->src2 = -1;
    }
    //if opcode == nop"
    else if (entry->type == NOP)
    { // if opcode is nop
        entry->dest = -1;
        entry->src1 = -1;
        entry->src2 = -1;
        entry->imm = 0;
    }
    else if (entry->type == MOV)
    {
    token = strtok(NULL, " ");
        // if dest = "pX" then dest = -2 and imm = 0 if false and 1 if true
        if (token[0] == 'p')
        {
            entry->dest = atoi(token + 1);
            entry->predicate = (strncmp(strtok(NULL, " "), "true", 4) == 0) ? 1 : 0;
        }
        else if (token[1] == 'C')
        {
            //NB: if dest = LC then dest = -3  or dest = EC then dest = -4
            // if dest = LC then dest = -3  or dest = EC then dest = -4
            if (token[0] == 'L')
            {
                entry->dest = -3;
            }
            else
            {
                entry->dest = -4; // EC
            }
            entry->src1 = -1;
            entry->src2 = -1;
            //check if imm is in hex or dec format, hex starts with 0x and need to be converted to dec
            token = strtok(NULL, " ");
            
            if (token[0] == '0' && token[1] == 'x')
            {
                entry->imm = (int)strtol(token + 2, NULL, 16);
            }
            else
            {
                entry->imm = atoi(token);
            }
        }
        else
        {
            entry->dest = atoi(token + 1);
            // need to check if there is an imm or a src
            token = strtok(NULL, " ");
            printf("token for 2C: %s\n", token);
            if (token[0] == 'x')
            {
                entry->src1 = atoi(token + 1);
                entry->src2 = -1;
                entry->imm = 0;
            }
            else
            {
                entry->src1 = -1;
                entry->src2 = -1;
                //check if imm is in hex or dec format, hex starts with 0x and need to be converted to dec
                if (token[0] == '0' && token[1] == 'x')
                {
                    entry->imm = (int)strtol(token + 2, NULL, 16);
                }
                else
                {
                    entry->imm = atoi(token);
                }
            }
        }
    } else {  // if opcode is add, addi, sub, mulu
        token = strtok(NULL, ",");
        entry->dest = atoi(token + 2); // TODO if x35 works, possible error 
        token = strtok(NULL, ",");
        entry->src1 = atoi(token + 2);
        token = strtok(NULL, ",");
        // need to check if last element is imm or  xX
        if (token[0] == 'x')
        {
            entry->src2 = atoi(token + 2);
            entry->imm = 0;
        }
        else
        {
            entry->src2 = -1;
            //check if imm is dec or hex, hex starts with 0x
            if (token[0] == '0' && token[1] == 'x')
            {
                entry->imm = (int)strtol(token, NULL, 16);
            }
            else
            {
                entry->imm = atoi(token);
            }
        }
    }
}