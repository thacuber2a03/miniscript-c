#include "ms_code.h"
#include "ms_mem.h"

void ms_initCode(ms_VM *vm, ms_Code *code)
{
	code->data = NULL;
	code->count = code->cap = 0;
	ms_initList(vm, &code->constants);
}

void ms_freeCode(ms_VM *vm, ms_Code *code)
{
	MS_MEM_FREE_ARR(vm, uint8_t, code->data, code->cap);
	ms_freeList(vm, &code->constants);
	ms_initCode(vm, code);
}

void ms_addByteToCode(ms_VM *vm, ms_Code *code, uint8_t byte)
{
	if (code->count + 1 >= code->cap)
	{
		int oldCap = code->cap;
		code->cap = MS_ARR_GROW_CAP(oldCap);
		code->data = MS_MEM_REALLOC_ARR(
			vm, uint8_t, code->data,
			oldCap, code->cap
		);
	}

	code->data[code->count++] = byte;
}

size_t ms_addConstToCode(ms_VM *vm, ms_Code *code, ms_Value constant)
{
	for (size_t i = 0; i < code->constants.count; i++)
		if (ms_valuesEqual(code->constants.data[i], constant))
			return i;

	return ms_addValueToList(vm, &code->constants, constant);
}

