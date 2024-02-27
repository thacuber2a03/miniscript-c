#ifndef MS_VM_H
#define MS_VM_H

#include "miniscript.h"
#include "ms_common.h"
#include "ms_object.h"
#include "ms_value.h"
#include "ms_map.h"

#define MS_MAX_FRAMES_AMT 64
#define MS_MAX_STACK_SIZE (MS_MAX_FRAMES_AMT * UINT8_COUNT)

typedef struct {
	ms_ObjFunction *function;
	uint8_t *ip;
	ms_Value *slots;
} CallFrame;

struct ms_VM {
	CallFrame frames[MS_MAX_FRAMES_AMT];
	int frameCount;

	ms_Value stack[MS_MAX_STACK_SIZE], *stackTop;
	size_t bytesUsed;
	ms_ReallocFn reallocFn;
	ms_Map strings, globals;
	ms_Object* objects;
};

ms_VM *ms_newVM(ms_ReallocFn reallocFn);
void ms_freeVM(ms_VM *vm);

#endif
