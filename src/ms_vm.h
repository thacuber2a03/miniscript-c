#ifndef MS_VM_H
#define MS_VM_H

#include "miniscript.h"
#include "ms_common.h"
#include "ms_value.h"
#include "ms_map.h"

#define MS_MAX_STACK_SIZE 256

struct ms_VM {
	ms_Value stack[MS_MAX_STACK_SIZE], *stackTop;
	size_t bytesUsed;
	ms_ReallocFn reallocFn;
	ms_Map strings, globals;
	ms_Object* objects;
};

ms_VM *ms_newVM(ms_ReallocFn reallocFn);
void ms_freeVM(ms_VM *vm);

#endif
