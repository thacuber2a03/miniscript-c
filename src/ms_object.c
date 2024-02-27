#include <string.h>

#include "ms_code.h"
#include "ms_mem.h"
#include "ms_object.h"
#include "ms_map.h"
#include "ms_value.h"
#include "ms_vm.h"

static ms_Object *newObject(ms_VM *vm, size_t size, ms_ObjectType type)
{
	ms_Object* obj = MS_MEM_MALLOC(vm, size);
	obj->next = vm->objects;
	vm->objects = obj;
	obj->type = type;
	return obj;
}

ms_ObjFunction *ms_newFunction(ms_VM *vm)
{
	ms_ObjFunction *function = (ms_ObjFunction*)newObject(vm, sizeof(ms_ObjFunction), MS_OBJ_FUNCTION);
	function->arity = 0;
	ms_initCode(vm, &function->code);
	return function;
}

static ms_ObjString *allocateString(ms_VM *vm, char *str, size_t length, uint32_t hash)
{
	ms_ObjString *obj = (ms_ObjString*)newObject(vm, sizeof(ms_ObjString), MS_OBJ_STRING);
	obj->chars = str;
	obj->length = length;
	obj->hash = hash;
	ms_setMapKey(vm, &vm->strings, MS_FROM_OBJ(obj), MS_FROM_NUM(1));
	return obj;
}

ms_ObjString *ms_newString(ms_VM *vm, char *str, size_t length)
{
	uint32_t hash = ms_hashMem(str, length);
	ms_ObjString *interned = ms_findStringInMap(vm, &vm->strings, str, length, hash);
	if (interned != NULL)
	{
		MS_MEM_FREE_ARR(vm, char, str, length+1);
		return interned;
	}
	return allocateString(vm, str, length, hash);
}

ms_ObjString *ms_copyString(ms_VM *vm, const char *str, size_t length)
{
	uint32_t hash = ms_hashMem(str, length);
	ms_ObjString *interned = ms_findStringInMap(vm, &vm->strings, str, length, hash);
	if (interned != NULL) return interned;

	char *heapStr = MS_MEM_MALLOC_ARR(vm, char, length+1);
	memcpy(heapStr, str, length);
	heapStr[length] = '\0';
	return allocateString(vm, heapStr, length, hash);
}

void printFunction(ms_ObjFunction *function)
{
	MS_UNUSED(function);
	// TODO: print arguments and default values
	printf("FUNCTION");
}

void ms_printObject(ms_Value val)
{
	switch (MS_OBJ_TYPE(val))
	{
		case MS_OBJ_STRING:
			printf("%s", MS_TO_CSTRING(val));
			break;

		case MS_OBJ_FUNCTION:
			printFunction(MS_TO_FUNCTION(val));
			break;

		default: MS_UNREACHABLE("ms_printObject"); break;
	}
}

double ms_getBoolObj(ms_Value val)
{
	switch (MS_OBJ_TYPE(val))
	{
		case MS_OBJ_STRING: return MS_TO_STRING(val)->length != 0;
		default: MS_UNREACHABLE("ms_getBoolObj"); break;
	}
}
