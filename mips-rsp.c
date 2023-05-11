// Copyright (c) Namdak Tonpa
// MIPS32, MIPS64 I-IV DASM 400 LOC

#include <stdint.h>
#include <stdio.h>
#include "../../editor.h"

// regs

char * gpr[] =  {
   "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
   "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra" };

char * cp0[] =  {
   "Context", "Random", "EntryLo0", "EntryLo1", "Context2", "PageMask", "Wired",  "HWREna",
   "BadVAddr", "Count", "EntryHi", "Compare", "SR", "Cause", "EPC", "PRId", "Config",
   "LLAddr", "WatchLo", "WatchHi", 0, 0, 0, "Debug", "DEPC", "PerfCtl", "ECC", "CacheErr",
   "TagLo", "TagHi", "ErrorEPC", "DESAVE" };

// opcodes

char * rsp[] = {
   "special", "regimm", "j", "jal", "beq", "bne", "blez", "bgtz", "addi", "addiu", "slti", "sltiu", "andi", "ori", "xori", "lui",
   "cop0", "cop1", "cop2", "cop3", "beql", "bnel", "blezl", "bgtz", "daddi", "daddiu", "ldl", "ldr", 0, 0, 0, 0,
   "lb", "lh", "lwl", "lw", "lbu", "lhu", "lwr", "lwu", "sb", "sh", "swl", "sw", "sdl", "sdr", "swr", "cache",
   "ll", "lwc1", "lwc2", "pref", "lld", "ldc1", "ldc2", "ld", "sc", "swc1", "swc2", 0, "scd", "sdc1", "sdc2", "sd" };

char *specials[] = {
   "sll", "movci", "slr", "sra", "sllv", 0, "srlv", "srav", "jr", "jalr", "movz", "movn", "syscall", "break", 0, "sync",
   "mfhi", "mthi", "mflo", "mtlo", "dsllv", 0, "dsrlv", "dsrav", "mult", "multu", "div", "divu", "dmult", "dmultu", "ddiv", "ddivu",
   "add", "addu", "sub", "subu", "and", "or", "xor", "nor", 0, 0, "slt", "sltu", "dadd", "daddu", "dsub", "dsubu",
   "tge", "tgeu", "tlt", "tltu", "teq", 0, "tne", 0, "dsll", 0, "dsrl", "dsra", "dsll32", 0, "dsrl32", "dsra32", 0 };

char *regimm[] = {
   "bltz", "bgez", "bltzl", "bgezl", 0, 0, 0, 0, "tgei", "tgeiu", "tlti", "tltiu", "teqi", 0, "tnei", 0,
   "bltzal", "bgezal", "bltzall", "bgezall", 0 };

char * rsp_vec[] = {
   "vmulf", "vmulu", "vrndp", "vmulq", "vmudl", "vmudm", "vmudn", "vmudh",
   "vmacf", "vmacu", "vrndn", "vmacq", "vmadl", "vmadm", "vmadn", "vmadh",
   "vadd", "vsub", 0, "vabs", "vaddc", "vsubc", 0, 0, 0, 0, 0, 0, 0, "vsar", 0, 0,
   "vlt", "veq", "vne", "vge", "vcl", "vch", "vcr", "vmrg",
   "vand", "vnand", "vor", "vnor", "vxor", "vnxor", 0, 0,
   "vcrp", "vrcpl", "vrcph", "vmov", "vrsq", "vrsql", "vrsqh", "vnop" };

char str_opcode[256], str_bo[256], str_beqo[256], str_lost[256], str_vec_elem[256], str_vec[256], str_vecop[256], str_cop0[256];
char str_cop2[256], str_cop[256], str_mc[256], str_reg[256], unknown[256];

char *decodeVectorElement(uint8_t v, uint8_t e)
{
    if ((e & 0x8) == 8) sprintf(str_vec_elem,"%i[%i]", v, (e & 0x7));
    else if ((e & 0xC) == 4) sprintf(str_vec_elem,"%i[%ih]", v, (e & 0x3));
    else if ((e & 0xE) == 2) sprintf(str_vec_elem,"%i[%iq]", v, (e & 0x1));
    else sprintf(str_vec_elem,"%i", v);
    return str_vec_elem;
}

char *decodeVectorElementScalar(uint8_t opcode, uint32_t operation)
{
    uint8_t e  = (uint8_t)((operation >> 21) & 0xF);
    uint8_t vt = (uint8_t)((operation >> 16) & 0x1F);
    uint8_t de = (uint8_t)((operation >> 11) & 0x1F);
    uint8_t vd = (uint8_t)((operation >> 6) & 0x1F);
    sprintf(str_vec,"%s f%s, f%s", rsp_vec[opcode], decodeVectorElement(vd, de), decodeVectorElement(vt, e));
    return str_vec;
}

char *decodeVector(uint32_t operation)
{
    uint8_t opcode = (uint8_t)(operation & 0x3F);
    if (opcode == 0x37) { sprintf(str_vecop, "%s", "nop"); return str_vecop; }
    else if (opcode < 0x37) {
        sprintf(str_vecop, "%s", decodeVectorElementScalar(opcode, operation));
        return str_vecop;
    }

    uint8_t e  = (uint8_t)((operation >> 21) & 0xF);
    uint8_t vt = (uint8_t)((operation >> 16) & 0x1F);
    uint8_t vs = (uint8_t)((operation >> 11) & 0x1F);
    uint8_t vd = (uint8_t)((operation >> 6) & 0x1F);

    sprintf(str_vecop, "%s f%i, f%i, f%s", (rsp_vec[opcode] == 0 ? unknown : rsp_vec[opcode]),
       vd, vs, decodeVectorElement(vt, e));
    return str_vecop;
}

char *decodeMoveControlToFromCoprocessor(char *opcode, uint32_t operation)
{
    uint8_t rt = (uint8_t)((operation >> 16) & 0x1F);
    uint8_t rd = (uint8_t)((operation >> 11) & 0x1F);
    sprintf(str_mc, "%s %s, f%i", opcode, gpr[rt], rd);
    return str_mc;
}

char * decodeMoveToFromCoprocessor(char *opcode, uint32_t operation)
{
    uint8_t rt = (uint8_t)((operation >> 16) & 0x1F);
    uint8_t rd = (uint8_t)((operation >> 11) & 0x1F);
    uint8_t e  = (uint8_t)((operation >> 7) & 0xF);
    sprintf(str_mc, "%s %s, f%i[%i]", opcode, gpr[rt], rd, e);
    return str_mc;
}

char *decodeCOP0(uint32_t operation) {
     uint8_t mt = (uint8_t)((operation >> 21) & 0x1F);
     uint8_t rt = (uint8_t)((operation >> 16) & 0x1F);
     uint8_t rd = (uint8_t)((operation >> 11) & 0x1F);
     uint8_t sel = (uint8_t)(operation & 0x3);
     uint8_t sc = (uint8_t)((operation >> 5) & 1);
     sprintf(str_cop0, ".word 0x%08X", operation);
     if ((operation >> 25) & 1) switch (operation & 0x3F) {
         case 0x01: sprintf(str_cop0, "tlbr"); break;
         case 0x02: sprintf(str_cop0, "tlbwi"); break;
         case 0x03: sprintf(str_cop0, "tlbinv"); break;
         case 0x04: sprintf(str_cop0, "tlbinvf"); break;
         case 0x06: sprintf(str_cop0, "tlbwr"); break;
         case 0x08: sprintf(str_cop0, "tlbp"); break;
         case 0x18: sprintf(str_cop0, ((operation >> 6) & 1) ? "eretnc" : "eret"); break;
         case 0x1F: sprintf(str_cop0, "deret"); break;
         case 0x20: sprintf(str_cop0, "wait"); break;
         default: break;
     } else switch (mt) {
         case 0x00: sprintf(str_cop0, "mfc0 %s, %s, %i", gpr[rt], cp0[rd], sel); break;
         case 0x01: sprintf(str_cop0, "dmfc0 %s, %s", gpr[rt], cp0[rd]); break;
         case 0x02: sprintf(str_cop0, "mfhc0 %s, %s, %i", gpr[rt], cp0[rd], sel); break;
         case 0x04: sprintf(str_cop0, "mtc0 %s, %s", gpr[rt], cp0[rd]); break;
         case 0x05: sprintf(str_cop0, "dmtc0 %s, %s", gpr[rt], cp0[rd]); break;
         case 0x06: sprintf(str_cop0, "mthc0 %s, %s, %i", gpr[rt], cp0[rd], sel); break;
         case 0x0A: sprintf(str_cop0, "rdpgpr %s, %s", gpr[rd], gpr[rt]); break;
         case 0x0B: sprintf(str_cop0, sc ? "ei %s" : "di %s", gpr[rt]); break;
         case 0x0E: sprintf(str_cop0, "wrpgpr %s, %s", gpr[rd], gpr[rt]); break;
         default: break;
     }
     return str_cop0;
}

char *decodeCOP1X(uint32_t operation) {
     sprintf(str_cop, ".word 0x%08X", operation);
     return str_cop;
}

char *decodeCOP1(uint32_t operation) {
     uint8_t fmt  = (uint8_t)((operation >> 21) & 0x1F);
     uint8_t function =     (uint8_t)(operation & 0x3F);
     uint8_t rt   = (uint8_t)((operation >> 16) & 0x1F);
     uint8_t fs   = (uint8_t)((operation >> 11) & 0x1F);
     uint8_t fd   = (uint8_t)((operation >> 06) & 0x1F);
     uint8_t ndtt = rt & 3;
     uint8_t cc   = rt >> 2;
     uint16_t offset = (uint16_t)((operation & 0xFFFF));
     char *suffix[4] = { "s","d", "w", "l", 0 };
     switch (fmt) {
         case 0x00: sprintf(str_cop0,  "mfc1 %s, f%i", gpr[rt], fs); break;
         case 0x01: sprintf(str_cop0, "dmfc1 %s, f%i", gpr[rt], fs); break;
         case 0x02: sprintf(str_cop0,  "cfc1 %s, f%i", gpr[rt], fs); break;
         case 0x04: sprintf(str_cop0,  "mtc1 %s, f%i", gpr[rt], fs); break;
         case 0x05: sprintf(str_cop0, "dmtc1 %s, f%i", gpr[rt], fs); break;
         case 0x06: sprintf(str_cop0,  "ctc1 %i, f%i", gpr[rt], fs); break;
         case 0x08: // BC
                    switch (ndtt) {
                        case 0: sprintf(str_cop0,  "bc1f %s, %s", cc, offset); break;
                        case 1: sprintf(str_cop0,  "bc1t %s, %s", cc, offset); break;
                        case 2: sprintf(str_cop0, "bc1fl %s, %s", cc, offset); break;
                        case 3: sprintf(str_cop0, "bc1tl %s, %s", cc, offset); break;
                        default: break;
                    }
                    break;

         case 0x10: case 0x11: // D
         case 0x12: case 0x13: case 0x14: // W
         case 0x15: case 0x16: case 0x17: // L
                    switch (function) { // S, D, W
                        case 0x00: sprintf(str_cop0,    "add.%s f%i, f%i, f%i", suffix[fmt&3], fd, fs, rt); break;
                        case 0x01: sprintf(str_cop0,    "sub.%s f%i, f%i, f%i", suffix[fmt&3], fd, fs, rt); break;
                        case 0x02: sprintf(str_cop0,    "mul.%s f%i, f%i, f%i", suffix[fmt&3], fd, fs, rt); break;
                        case 0x03: sprintf(str_cop0,    "div.%s f%i, f%i, f%i", suffix[fmt&3], fd, fs, rt); break;
                        case 0x04: sprintf(str_cop0,   "sqrt.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x05: sprintf(str_cop0,    "abs.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x06: sprintf(str_cop0,    "mov.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x07: sprintf(str_cop0,    "neg.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x08: sprintf(str_cop0,"round.l.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x09: sprintf(str_cop0,"trunc.l.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x0A: sprintf(str_cop0, "ceil.l.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x0B: sprintf(str_cop0,"floor.l.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x0C: sprintf(str_cop0,"round.w.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x0D: sprintf(str_cop0,"trunc.w.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x0E: sprintf(str_cop0, "ceil.w.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x0F: sprintf(str_cop0,"floor.w.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x11: sprintf(str_cop0,   "movt.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x12: sprintf(str_cop0,   "movz.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x13: sprintf(str_cop0,   "movn.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x15: sprintf(str_cop0,  "recip.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x16: sprintf(str_cop0,  "rsqrt.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x20: sprintf(str_cop0,  "cvt.s.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x21: sprintf(str_cop0,  "cvt.d.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x24: sprintf(str_cop0,  "cvt.w.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x25: sprintf(str_cop0,  "cvt.l.%s f%i, f%i", suffix[fmt&3], fd, fs); break;
                        case 0x30: sprintf(str_cop0,    "c.f.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x31: sprintf(str_cop0,   "c.un.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x32: sprintf(str_cop0,   "c.eq.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x33: sprintf(str_cop0,  "c.ueq.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x34: sprintf(str_cop0,  "c.olt.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x35: sprintf(str_cop0,  "c.ult.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x36: sprintf(str_cop0,  "c.ole.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x37: sprintf(str_cop0,  "c.ule.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x38: sprintf(str_cop0,   "c.sf.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x39: sprintf(str_cop0, "c.ngle.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x3A: sprintf(str_cop0,  "c.seq.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x3B: sprintf(str_cop0,  "c.ngl.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x3C: sprintf(str_cop0,   "c.lt.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x3D: sprintf(str_cop0,  "c.nge.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x3E: sprintf(str_cop0,   "c.le.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        case 0x3F: sprintf(str_cop0,  "c.ngt.%s f%i, f%i", suffix[fmt&3], fs, rt); break;
                        default: break;
                    }
                    break;
           default: break;
     }

     return str_cop0;
}

char *decodeCOP2(uint32_t operation) {
    if ((operation & 0x7FF) != 0) return decodeVector(operation);
    uint8_t subop = (uint8_t)((operation >> 21) & 0x1F);
    switch (subop) {
        case 0x00: return decodeMoveToFromCoprocessor("mfc2", operation);
        case 0x04: return decodeMoveToFromCoprocessor("mtc2", operation);
        case 0x02: return decodeMoveControlToFromCoprocessor("cfc2", operation);
        case 0x06: return decodeMoveControlToFromCoprocessor("ctc2", operation);
        default:   sprintf(str_cop0, ".word 0x%08X", operation); 
        sprintf(str_cop2, ".word 0x%08X", operation);
                   return str_cop2;
    }
}


char *decodeTwoRegistersWithImmediate(char *opcode, uint32_t operation)
{
     uint8_t dst = (uint8_t)((operation >> 16) & 0x1F);
     uint8_t src = (uint8_t)((operation >> 21) & 0x1F);
     uint16_t imm = (uint16_t)(operation & 0xFFFF);
     if (imm < 0) sprintf(str_lost,"%s %s, %s, 0x%x", opcode, gpr[dst], gpr[src], imm);
             else sprintf(str_lost,"%s %s, %s, 0x%x", opcode, gpr[dst], gpr[src], imm);
     return str_lost;
}

char *decodeOneRegisterWithImmediate(char *opcode, uint32_t operation)
{
     uint8_t dst = (uint8_t)((operation >> 16) & 0x1F);
     sprintf(str_reg, "%s %s, 0x%x", opcode, gpr[dst], operation & 0xFFFF);
     return str_reg;
}

char *decodeThreeRegister(char* opcode, uint32_t operation, int swapRT_RS)
{
     uint8_t dest = (uint8_t)((operation >> 11) & 0x1F);
     uint8_t src1 = (uint8_t)((operation >> 21) & 0x1F);
     uint8_t src2 = (uint8_t)((operation >> 16) & 0x1F);
     if(!swapRT_RS) sprintf(str_reg,"%s %s, %s, %s", opcode, gpr[dest], gpr[src1], gpr[src2]);
     else sprintf(str_reg,"%s %s, %s, %s", opcode, gpr[dest], gpr[src2], gpr[src1]);
     return str_reg;
}
char * decodeBranch(char *opcode, uint32_t operation, unsigned long int address)
{
     uint8_t src = (uint8_t)((operation >> 21) & 0x1F);
     uint16_t imm = (uint16_t)((operation & 0xFFFF) << 2);
     uint32_t current_offset = (uint32_t)((address + 4) + imm);
     sprintf(str_bo, "%s %s, 0x%8x", opcode, gpr[src], current_offset);
     return str_bo;
}

char * decodeBranchEquals(char * opcode, uint32_t operation, unsigned long int address)
{
     uint8_t src1 = (uint8_t)((operation >> 21) & 0x1F);
     uint8_t src2 = (uint8_t)((operation >> 16) & 0x1F);
     uint16_t imm = (uint16_t)((operation & 0xFFFF) << 2);
     uint32_t current_offset = (uint32_t)((address + 4) + imm);
     sprintf(str_beqo, "%s %s, %s, 0x%08x", opcode, gpr[src1], gpr[src2], current_offset);
     return str_beqo;
}

char *decodeSpecialShift(char *opcode, uint32_t operation)
{
     uint8_t dest = (uint8_t)((operation >> 11) & 0x1F);
     uint8_t src  = (uint8_t)((operation >> 16) & 0x1F);
     int imm = (int)((operation >> 6) & 0x1F);
     sprintf(str_lost, "%s %s, %s, %i", opcode, gpr[dest], gpr[src], imm);
     return str_lost;
}

uint32_t le_to_be(uint32_t num) {
    uint8_t b[4] = {0}; *(uint32_t*)b = num; uint8_t tmp = 0; tmp = b[0];
    b[0] = b[3]; b[3] = tmp; tmp = b[1]; b[1] = b[2]; b[2] = tmp; return *(uint32_t*)b;
}

char *decodeLoadStore(char *opcode, uint32_t operation)
{
     uint8_t rt = (uint8_t)((operation >> 16) & 0x1F);
     uint8_t base = (uint8_t)((operation >> 21) & 0x1F);
     uint16_t offset = (uint16_t)(operation & 0xFFFF);
     if (offset < 0) sprintf(str_lost,"%s %s, -0x%08X(%s)", opcode, gpr[rt], offset, gpr[base]);
             else sprintf(str_lost,"%s %s, 0x%08X(%s)", opcode, gpr[rt], offset, gpr[base]);
     return str_lost;
}

char * decodeMIPS(unsigned long int address, char *outbuf, int*lendis, unsigned long int offset) {
    uint32_t operation = le_to_be((uint32_t)*((unsigned long int *)address)); *lendis = 4;
    if (operation == 0x00000000) { sprintf(outbuf, "%s", "nop"); return outbuf; }
    uint8_t reg = (uint8_t)((operation >> 21) & 0x1F);
    uint8_t opcode = (uint8_t)((operation >> 26) & 0x3F);
    sprintf(outbuf,".word 0x%08X", operation);
    if (opcode == 0x00) { // SPECIAL
        uint8_t function = (uint8_t)(operation & 0x3F);
        if (function < 0x04 && specials[function]) sprintf(outbuf, "%s", decodeSpecialShift(specials[function], operation));
        else if (function < 0x08 && specials[function]) sprintf(outbuf, "%s", decodeThreeRegister(specials[function], operation, 1));
        else if (function == 0x08 && specials[function]) sprintf(outbuf, "%s %s", specials[function], gpr[reg]);
        else if (function == 0x09) {
             uint8_t return_reg = (uint8_t)((operation >> 11) & 0x1F);
             if (return_reg == 0xF && specials[function]) sprintf(outbuf, "%s %s", specials[function], gpr[reg]);
             else sprintf(outbuf, "%s %s, %s", specials[function], gpr[return_reg], gpr[reg]);
        } else if (function < 0x20 && specials[function]) sprintf(outbuf, "%s %i", specials[function], ((operation >> 6) & 0xFFFFF));
        else if (function < 0x40 && specials[function]) sprintf(outbuf, "%s", decodeThreeRegister(specials[function], operation, 0));
    } else if (opcode == 0x01) { // REGIMM
        uint8_t rt = (uint8_t)((operation >> 16) & 0x1F);
        if (rt < 0x14 && regimm[rt]) sprintf(outbuf, "%s", decodeBranch(regimm[rt], operation, offset));
    } else if (opcode  < 0x04 && rsp[opcode]) sprintf(outbuf, "%s 0x0%x", rsp[opcode], ((operation & 0x03FFFFFF) << 2));
    else if (opcode  < 0x06 && rsp[opcode]) sprintf(outbuf, "%s", decodeBranchEquals(rsp[opcode], operation, offset));
    else if (opcode  < 0x08 && rsp[opcode]) sprintf(outbuf, "%s", decodeBranch(rsp[opcode], operation, offset));
    else if (opcode  < 0x0F && rsp[opcode]) sprintf(outbuf, "%s", decodeTwoRegistersWithImmediate(rsp[opcode], operation));
    else if (opcode  < 0x10 && rsp[opcode]) sprintf(outbuf, "%s", decodeOneRegisterWithImmediate(rsp[opcode], operation));
    else if (opcode == 0x10) sprintf(outbuf, "%s", decodeCOP0(operation));
    else if (opcode == 0x11) sprintf(outbuf, "%s", decodeCOP1(operation));
    else if (opcode == 0x12) sprintf(outbuf, "%s", decodeCOP2(operation));
    else if (opcode == 0x13) sprintf(outbuf, "%s", decodeCOP1X(operation));
    else if (opcode == 0x1D) { sprintf(outbuf, "jalx 0x%08X", operation & 0x1FFFFFF); }
    else if (opcode == 0x1C) { // SPECIAL2
    }
    else if (opcode == 0x1F) { // SPECIAL3
    }
    else if ((opcode == 0x1A) || (opcode == 0x1B) || ((opcode > 0x1F) && (opcode < 0x2F)) ||
             (opcode != 0x3B && ((opcode > 0x2F) || (opcode < 0x40))) && rsp[opcode])
             sprintf(outbuf, "%s", decodeLoadStore(rsp[opcode],operation));
    return outbuf;
}