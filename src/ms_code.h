#ifndef MS_CODE_H
#define MS_CODE_H

#include "miniscript.h"
#include "ms_value.h"

typedef enum {
	#define OPCODE(op) op,
	#include "ms_opcodes.h"
	#undef OPCODE
} ms_Opcode;

typedef struct {
	size_t count, cap;
	uint8_t *data;
	int *lines;
	ms_List constants;
} ms_Code;

void ms_initCode(ms_VM *vm, ms_Code *code);
void ms_freeCode(ms_VM *vm, ms_Code *code);
void ms_addByteToCode(ms_VM *vm, ms_Code *code, uint8_t byte, int line);
size_t ms_addConstToCode(ms_VM *vm, ms_Code *code, ms_Value constant);

#endif
