#include <stdio.h>

#include "ms_debug.h"

const char *ms_getOpcodeName(ms_Opcode op)
{
	switch (op)
	{
		#define OPCODE(op) case op: return #op;
		#include "ms_opcodes.h"
		#undef OPCODE
		default: return NULL; // unreachable
	}
}

static inline size_t simpleInstruction(uint8_t *code, size_t offset)
{
	printf("%s", ms_getOpcodeName(*code));
	return offset + 1;
}

static size_t constantInstruction(uint8_t *code, ms_List constants, size_t offset)
{
	int index = code[1];
	printf("%s %i '", ms_getOpcodeName(*code), index);
	ms_printValue(constants.data[index]);
	printf("'");
	return offset + 2;
}

static size_t byteInstruction(uint8_t *code, size_t offset)
{
	printf("%s %4d", ms_getOpcodeName(*code), code[1]);
	return offset + 2;
}

static size_t jumpInstruction(uint8_t *code, size_t offset, int sign)
{
	uint16_t jump = (uint16_t)(code[1] << 8) | code[2];
	printf("%s %zu -> %zu", ms_getOpcodeName(*code), offset, offset + 3 + sign * jump);
	return offset + 3;
}

size_t ms_disassembleInstruction(ms_Code *code, size_t offset)
{
	printf("%zu | ", offset);
	uint8_t *off = code->data + offset;
	switch (*off)
	{
		case MS_OP_CONST:
		case MS_OP_SET_GLOBAL:
		case MS_OP_GET_GLOBAL:
			return constantInstruction(off, code->constants, offset);

		case MS_OP_SET_LOCAL:
		case MS_OP_GET_LOCAL:
		case MS_OP_INVOKE:
			return byteInstruction(off, offset);

		case MS_OP_JUMP:
		case MS_OP_JUMP_IF_FALSE:
			return jumpInstruction(off, offset, 1);

		case MS_OP_LOOP:
			return jumpInstruction(off, offset, -1);

		case MS_OP_TRUE:
		case MS_OP_FALSE:
		case MS_OP_NULL:
		case MS_OP_ADD:
		case MS_OP_SUBTRACT:
		case MS_OP_MULTIPLY:
		case MS_OP_DIVIDE:
		case MS_OP_NEGATE:
		case MS_OP_MODULO:
		case MS_OP_POWER:
		case MS_OP_EQUAL:
		case MS_OP_NOT_EQUAL:
		case MS_OP_GREATER:
		case MS_OP_GREATER_EQUAL:
		case MS_OP_LESS:
		case MS_OP_LESS_EQUAL:
		case MS_OP_AND:
		case MS_OP_OR:
		case MS_OP_NOT:
		case MS_OP_POP:
		case MS_OP_RETURN:
			return simpleInstruction(off, offset);

		default:
			if (*off < MS_OP__END)
				printf("debug: no print method for %s", ms_getOpcodeName(*off));
			else
				printf("debug: unknown opcode %i", *off);
			return offset + 1;
	}
}

void ms_disassembleCode(ms_Code *code, const char *name)
{
	printf("---- %s ----\n", name);
	for (size_t offset = 0; offset < code->count;)
	{
		offset = ms_disassembleInstruction(code, offset);
		putchar('\n');
	}
}
