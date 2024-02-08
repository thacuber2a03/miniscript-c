#include <stdio.h>

#include "ms_debug.h"

const char *ms_getOpcodeName(ms_Opcode op)
{
	switch (op)
	{
		#define OPCODE(op) case op: return #op;
		#include "ms_opcodes.h"
		#undef OPCODE
	}
}

static inline size_t simpleInstruction(uint8_t *code, size_t offset)
{
	printf("%s", ms_getOpcodeName(*code));
	return offset + 1;
}

static size_t constantInstruction(uint8_t *code, ms_List constants, size_t offset)
{
	int index = code[offset+1];
	printf("%s", ms_getOpcodeName(*code));
	printf(" %i '", index);
	ms_printValue(constants.data[index]);
	printf("'");
	return offset + 2;
}

size_t ms_disassembleInstruction(ms_Code code, size_t offset)
{
	uint8_t *off = code.data + offset;
	switch (*off)
	{
		case MS_OP_CONST:
			return constantInstruction(off, code.constants, offset);

		case MS_OP_NULL: case MS_OP_ADD: case MS_OP_SUBTRACT: case MS_OP_MULTIPLY:
		case MS_OP_DIVIDE: case MS_OP_NEGATE: case MS_OP_NOT: case MS_OP_RETURN:
		case MS_OP_TRUE: case MS_OP_FALSE: case MS_OP_POP:
			return simpleInstruction(off, offset);

		default:
			printf("debug: unknown opcode %s", ms_getOpcodeName(*off));
			return offset + 1;
	}
}
