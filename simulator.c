#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Machine Definitions
// maximum number of data words in memory
#define NUMMEMORY 65536 
// number of machine registers
#define NUMREGS 8       

#define ADD 0
#define NOR 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5 
#define HALT 6
#define NOOP 7

const char *opcode_to_str_map[] = {
    "add",
    "nor",
    "lw",
    "sw",
    "beq",
    "jalr",
    "halt",
    "noop"};

#define NOOPINSTR (NOOP << 22)

typedef struct IFIDStruct
{
    int pcPlus1;
    int instr;
} IFIDType;

typedef struct IDEXStruct
{
    int pcPlus1;
    int valA;
    int valB;
    int offset;
    int instr;
} IDEXType;

typedef struct EXMEMStruct
{
    int branchTarget;
    int eq;
    int aluResult;
    int valB;
    int instr;
    int branchTaken;
} EXMEMType;

typedef struct MEMWBStruct
{
    int writeData;
    int instr;
} MEMWBType;

typedef struct WBENDStruct
{
    int writeData;
    int instr;
} WBENDType;

typedef struct stateStruct
{
    int pc;
    int instrMem[NUMMEMORY];
    int dataMem[NUMMEMORY];
    int reg[NUMREGS];
    unsigned int numMemory;
    IFIDType IFID;
    IDEXType IDEX;
    EXMEMType EXMEM;
    MEMWBType MEMWB;
    WBENDType WBEND;
    unsigned int cycles; // number of cycles run so far
} stateType;

static inline int opcode(int instruction)
{
    return instruction >> 22;
}

static inline int field0(int instruction)
{
    return (instruction >> 19) & 0x7;
}

static inline int field1(int instruction)
{
    return (instruction >> 16) & 0x7;
}

static inline int field2(int instruction)
{
    return instruction & 0xFFFF;
}

// convert a 16-bit number into a 32-bit Linux integer
static inline int convertNum(int num)
{
    return num - ((num & (1 << 15)) ? 1 << 16 : 0);
}

void printState(stateType *);
void printInstruction(int);
void readMachineCode(stateType *, char *);

void IF_stage(stateType *state, stateType *newState);
void ID_stage(stateType *state, stateType *newState);
void EX_stage(stateType *state, stateType *newState);
void MEM_stage(stateType *state, stateType *newState);
void WB_stage(stateType *state, stateType *newState);

int main(int argc, char *argv[])
{

    /* Declare state and newState.
       Note these have static lifetime so that instrMem and
       dataMem are not allocated on the stack. */

    static stateType state, newState;

    if (argc != 2)
    {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    readMachineCode(&state, argv[1]);

    // Initialize states here all to nooop, others to 0
    state.cycles = 0;
    state.pc = 0;
    state.IFID.instr = NOOPINSTR;
    state.IDEX.instr = NOOPINSTR;
    state.EXMEM.instr = NOOPINSTR;
    state.MEMWB.instr = NOOPINSTR;
    state.WBEND.instr = NOOPINSTR;

    while (opcode(state.MEMWB.instr) != HALT)
    {
        printState(&state);

        newState = state;
        newState.cycles += 1;

        /* ---------------------- IF stage --------------------- */
        IF_stage(&state, &newState);

        /* ---------------------- ID stage --------------------- */
        ID_stage(&state, &newState);

        /* ---------------------- EX stage --------------------- */
        EX_stage(&state, &newState);

        /* --------------------- MEM stage --------------------- */
        MEM_stage(&state, &newState);

        /* ---------------------- WB stage --------------------- */
        WB_stage(&state, &newState);

        /* ------------------------ END ------------------------ */
        state = newState;
    }

    printf("Machine halted\n");
    printf("Total of %d cycles executed\n", state.cycles);
    printf("Final state of machine:\n");
    printState(&state);
}

void IF_stage(stateType *state, stateType *newState)
{
    newState->IFID.instr = state->instrMem[state->pc];
    newState->IFID.pcPlus1 = state->pc + 1;
    newState->pc = state->pc + 1;
}

// Detect need to stall:
/*
CHeck if instr is reading val loaded by preceding instr
    Check if current instr in ID stage depends on dest of LW instr in EX

    If needed: insert noop, repead fetch for the stalled instr
         adjsut PC
*/
void ID_stage(stateType *state, stateType *newState)
{
    // set new idex to old ifid
    int newStateIDEX = state->IFID.instr;
    newState->IDEX.instr = newStateIDEX;
    int stateIDEX = state->IDEX.instr;
    // set new state idex pc1 to if old if pc1
    newState->IDEX.pcPlus1 = state->IFID.pcPlus1;

    // Data hazard checking: Will be a load word followed by an instruction needing its value
    // look at prev cycle oinstruction in IDEX
    bool skip = false;
    if ((opcode(stateIDEX) == LW) && (opcode(newStateIDEX) == LW))
    {
        if (field1(stateIDEX) != field0(newStateIDEX))
        {
            int stateIFIDinstr = state->IFID.instr;
            newState->IDEX.offset = convertNum(field2(stateIFIDinstr));
            newState->IDEX.valA = state->reg[field0(stateIFIDinstr)];
            newState->IDEX.valB = state->reg[field1(stateIFIDinstr)];
            skip = true;
        }
    }
    if (!skip && ((opcode(stateIDEX) == LW) &&
                  (field1(stateIDEX) == field0(newStateIDEX) || field1(stateIDEX) == field1(newStateIDEX))))
    {
        // stalling:: Insert a NOOP in the ID/EX PL
        // reset the PC and IFID
        newState->IDEX.instr = NOOPINSTR;
        newState->pc = state->pc;     // this makes it so we redo state basically decrememninting
        newState->IFID = state->IFID; // save insttr
    }
    // from hazardless check:
    else
    {
        int stateIFIDinstr = state->IFID.instr;
        newState->IDEX.offset = convertNum(field2(stateIFIDinstr));
        newState->IDEX.valA = state->reg[field0(stateIFIDinstr)];
        newState->IDEX.valB = state->reg[field1(stateIFIDinstr)];
    }
}

// need to add forwarding to EX stage:
/*
If current instr requires a value that is from instr in memory or WB:
    forward value from ex mem or mem wb PR to ex to bypass regfile
*/
// function to check wb dest for hazards. Will be based on lw use and
void set_destinations(stateType *state, int *valA, int *valB, int wbDest, int wbOpcode, int memOpcode, int memDest, int exMemOpcode, int xMemDest, int regAindex, int regBIndex)
{

    // for all, check opcode , set values to new  vaues if needed
    // TODO : REGISTER NOT REGISTER VALUE EQUAL TO DESTINATIONS!!!!!!!
    if (wbOpcode == ADD || wbOpcode == NOR || wbOpcode == LW)
    {
        *valA = (regAindex == wbDest) ? state->WBEND.writeData : *valA;
        *valB = (regBIndex == wbDest) ? state->WBEND.writeData : *valB;
    }

    if (memOpcode == ADD || memOpcode == NOR || memOpcode == LW)
    {
        *valA = (regAindex == memDest) ? state->MEMWB.writeData : *valA;
        *valB = (regBIndex == memDest) ? state->MEMWB.writeData : *valB;
    }

    if (exMemOpcode == ADD || exMemOpcode == NOR || exMemOpcode == LW)
    {
        *valA = (regAindex == xMemDest) ? state->EXMEM.aluResult : *valA;
        *valB = (regBIndex == xMemDest) ? state->EXMEM.aluResult : *valB;
    }
}

void EX_stage(stateType *state, stateType *newState)
{
    int instr = state->IDEX.instr;
    newState->EXMEM.instr = instr;
    // general data:
    // values for registers
    int regAVal = state->IDEX.valA;
    int regBVal = state->IDEX.valB;
    // offset and pc
    int offset = state->IDEX.offset;
    int pcPlus1 = state->IDEX.pcPlus1;
    // wb opcode
    int wbOpCode = opcode(state->WBEND.instr);
    // mem opcode
    int memOpcode = opcode(state->MEMWB.instr);
    // exmem opcode
    int exMemOpcode = opcode(state->EXMEM.instr);
    // indexes for register
    int regAIndex = field0(instr);
    int regBIndex = field1(instr);
    // branch data
    newState->EXMEM.branchTarget = pcPlus1 + offset;
    newState->EXMEM.branchTaken = 0;

    // hazards
    // set destinations for wb and mem
    // these will be based on prev use of lw instr
    int wbDest = field2(state->WBEND.instr);
    if (wbOpCode == LW)
    {
        wbDest = field1(state->WBEND.instr);
    }

    int memDest = field2(state->MEMWB.instr);
    if (memOpcode == LW)
    {
        memDest = field1(state->MEMWB.instr);
    }

    // setting xmem:
    int exMemDest = field2(state->EXMEM.instr);
    if (exMemOpcode == LW)
    {
        exMemDest = field1(state->EXMEM.instr);
    }

    set_destinations(state, &regAVal, &regBVal, wbDest, wbOpCode, memOpcode, memDest, exMemOpcode, exMemDest, regAIndex, regBIndex);

    newState->EXMEM.eq = 1;
    //
    // execute the opcode as needed
    bool noop = false;
    switch (opcode(instr))
    {
    case ADD:
        newState->EXMEM.aluResult = regAVal + regBVal;
        break;
    case NOR:
        newState->EXMEM.aluResult = ~(regAVal | regBVal);
        break;
    case LW:
        newState->EXMEM.aluResult = regAVal + offset;
        break;
    case SW:
        newState->EXMEM.aluResult = regAVal + offset;
        break;
    case BEQ:
        newState->EXMEM.aluResult = regAVal - regBVal;
        if (regAVal == regBVal)
        {
            newState->EXMEM.eq = 1;
            newState->EXMEM.branchTaken = 1;
        }
        else
        {
            newState->EXMEM.eq = 0;
        }
        break;
    case HALT:
        break;
    case NOOP:
        noop = true;
        break;
    }

    if (!noop)
    {
        newState->EXMEM.valB = regBVal;
    }
}

void MEM_stage(stateType *state, stateType *newState)
{
    // set instr to that in pipeline
    int instr = state->EXMEM.instr;
    int aluResult = state->EXMEM.aluResult;
    bool branchTaken = state->EXMEM.branchTaken;
    // Propagate  to the next pipeline stage
    newState->MEMWB.instr = instr;

    switch (opcode(instr))
    {
    case LW:
        // load val into address given by alu
        newState->MEMWB.writeData = state->dataMem[aluResult];
        break;

    case SW:
        // store into addr from alu
        newState->dataMem[aluResult] = state->EXMEM.valB;
        break;

    case BEQ:
        // if branch inster noops to clean pipeline. -> update pc
        if (branchTaken)
        {
            newState->EXMEM.branchTaken = true;
            newState->pc = state->EXMEM.branchTarget;
            newState->IFID.instr = NOOPINSTR;
            newState->IDEX.instr = NOOPINSTR;
            newState->EXMEM.instr = NOOPINSTR;
        }
        break;

    default:
        // forward results if not noop (or halt)
        if (opcode(instr) != NOOP && opcode(instr) != HALT)
        {
            newState->MEMWB.writeData = aluResult;
        }
        break;
    }
}

void WB_stage(stateType *state, stateType *newState)
{
    // get  instr from the MEM/WB PL reg
    int instr = state->MEMWB.instr;
    // prop. Instr forard & result/writeData -> webned pipeline register.
    newState->WBEND.instr = instr;
    newState->WBEND.writeData = state->MEMWB.writeData;

    switch (opcode(instr))
    {
    case ADD:
    case NOR:
        // write the ALU result to the destination register.
        newState->reg[field2(instr)] = state->MEMWB.writeData;
        break;
    case LW:
        //  LW:: write the loaded data to the destination register.
        newState->reg[field1(instr)] = state->MEMWB.writeData;
        break;
    }
}

/*
 * DO NOT MODIFY ANY OF THE CODE BELOW.
 */

void printInstruction(int instr)
{
    const char *instr_opcode_str;
    int instr_opcode = opcode(instr);
    if (ADD <= instr_opcode && instr_opcode <= NOOP)
    {
        instr_opcode_str = opcode_to_str_map[instr_opcode];
    }

    switch (instr_opcode)
    {
    case ADD:
    case NOR:
    case LW:
    case SW:
    case BEQ:
        printf("%s %d %d %d", instr_opcode_str, field0(instr), field1(instr), convertNum(field2(instr)));
        break;
    case JALR:
        printf("%s %d %d", instr_opcode_str, field0(instr), field1(instr));
        break;
    case HALT:
    case NOOP:
        printf("%s", instr_opcode_str);
        break;
    default:
        printf(".fill %d", instr);
        return;
    }
}

void printState(stateType *statePtr)
{
    printf("\n@@@\n");
    printf("state before cycle %d starts:\n", statePtr->cycles);
    printf("\tpc = %d\n", statePtr->pc);

    printf("\tdata memory:\n");
    for (int i = 0; i < statePtr->numMemory; ++i)
    {
        printf("\t\tdataMem[ %d ] = %d\n", i, statePtr->dataMem[i]);
    }
    printf("\tregisters:\n");
    for (int i = 0; i < NUMREGS; ++i)
    {
        printf("\t\treg[ %d ] = %d\n", i, statePtr->reg[i]);
    }

    // IF/ID
    printf("\tIF/ID pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IFID.instr);
    printInstruction(statePtr->IFID.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IFID.pcPlus1);
    if (opcode(statePtr->IFID.instr) == NOOP)
    {
        printf(" (Don't Care)");
    }
    printf("\n");

    // ID/EX
    int idexOp = opcode(statePtr->IDEX.instr);
    printf("\tID/EX pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IDEX.instr);
    printInstruction(statePtr->IDEX.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IDEX.pcPlus1);
    if (idexOp == NOOP)
    {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegA = %d", statePtr->IDEX.valA);
    if (idexOp >= HALT || idexOp < 0)
    {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->IDEX.valB);
    if (idexOp == LW || idexOp > BEQ || idexOp < 0)
    {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\toffset = %d", statePtr->IDEX.offset);
    if (idexOp != LW && idexOp != SW && idexOp != BEQ)
    {
        printf(" (Don't Care)");
    }
    printf("\n");

    // EX/MEM
    int exmemOp = opcode(statePtr->EXMEM.instr);
    printf("\tEX/MEM pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->EXMEM.instr);
    printInstruction(statePtr->EXMEM.instr);
    printf(" )\n");
    printf("\t\tbranchTarget %d", statePtr->EXMEM.branchTarget);
    if (exmemOp != BEQ)
    {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\teq ? %s", (statePtr->EXMEM.eq ? "True" : "False"));
    if (exmemOp != BEQ)
    {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\taluResult = %d", statePtr->EXMEM.aluResult);
    if (exmemOp > SW || exmemOp < 0)
    {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->EXMEM.valB);
    if (exmemOp != SW)
    {
        printf(" (Don't Care)");
    }
    printf("\n");

    // MEM/WB
    int memwbOp = opcode(statePtr->MEMWB.instr);
    printf("\tMEM/WB pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->MEMWB.instr);
    printInstruction(statePtr->MEMWB.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->MEMWB.writeData);
    if (memwbOp >= SW || memwbOp < 0)
    {
        printf(" (Don't Care)");
    }
    printf("\n");

    // WB/END
    int wbendOp = opcode(statePtr->WBEND.instr);
    printf("\tWB/END pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->WBEND.instr);
    printInstruction(statePtr->WBEND.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->WBEND.writeData);
    if (wbendOp >= SW || wbendOp < 0)
    {
        printf(" (Don't Care)");
    }
    printf("\n");

    printf("end state\n");
    fflush(stdout);
}

// File
#define MAXLINELENGTH 1000 // MAXLINELENGTH is the max number of characters we read

void readMachineCode(stateType *state, char *filename)
{
    char line[MAXLINELENGTH];
    FILE *filePtr = fopen(filename, "r");
    if (filePtr == NULL)
    {
        printf("error: can't open file %s", filename);
        exit(1);
    }

    printf("instruction memory:\n");
    for (state->numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL; ++state->numMemory)
    {
        if (sscanf(line, "%d", state->instrMem + state->numMemory) != 1)
        {
            printf("error in reading address %d\n", state->numMemory);
            exit(1);
        }
        printf("\tinstrMem[ %d ]\t= 0x%08x\t= %d\t= ", state->numMemory,
               state->instrMem[state->numMemory], state->instrMem[state->numMemory]);
        printInstruction(state->dataMem[state->numMemory] = state->instrMem[state->numMemory]);
        printf("\n");
    }
}
