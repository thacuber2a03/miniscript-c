#include "ms_object.h"
#include "ms_vm.h"
#include "ms_mem.h"

static ms_Object *newObject(ms_VM *vm, size_t size, ms_ObjectType type)
{
	ms_Object* obj = MS_MEM_MALLOC(vm, size);
	obj->next = vm->objects;
	vm->objects = obj;
	obj->type = type;
	return obj;
}

ms_ObjString *ms_newString(ms_VM *vm, char *str, size_t length)
{
	ms_ObjString *obj = (ms_ObjString*)newObject(vm, sizeof(ms_ObjString), MS_OBJ_STRING);
	obj->chars = str;
	obj->length = length;
	return obj;
}

void ms_printObject(ms_Value val)
{
	switch (MS_OBJ_TYPE(val))
	{
		case MS_OBJ_STRING:
			printf("%s", MS_TO_CSTRING(val));
			break;
	}
}
