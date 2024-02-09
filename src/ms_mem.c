#ifdef MS_DEBUG_MEM_ALLOC
#include <stdio.h>
#endif
#include <stdlib.h>

#include "ms_common.h"
#include "ms_vm.h"
#include "ms_object.h"
#include "ms_mem.h"

void *ms_vmRealloc(ms_VM *vm, void *ptr, size_t oldSize, size_t newSize)
{
	bool isFreeing = newSize == 0;
	MS_UNUSED(isFreeing);

	int diff = newSize - oldSize;
	vm->bytesUsed += diff;
#ifdef MS_DEBUG_MEM_ALLOC
	if (diff != 0)
		fprintf(stderr,
			"mem: %s %i bytes\n",
			diff < 0 ? "freed" : "allocated",
			diff < 0 ? -diff : diff
		);
#endif
	void *res = vm->reallocFn(ptr, oldSize, newSize);

	// TODO: throw a proper error? maybe
	MS_ASSERT_REASON(res != NULL || isFreeing, "pointer is NULL but VM is not requesting a free");
	return res;
}

static void freeObject(ms_VM* vm, ms_Object *object)
{
	switch (object->type)
	{
		case MS_OBJ_STRING: {
			ms_ObjString *str = (ms_ObjString*)object;
			MS_MEM_FREE_ARR(vm, char, str->chars, str->length + 1);
			MS_MEM_FREE(vm, str, sizeof(ms_ObjString));
		} break;
	}
}

void ms_freeAllObjects(ms_VM* vm)
{
	ms_Object *obj = vm->objects;
	while (obj)
	{
		ms_Object *next = obj->next;
		freeObject(vm, obj);
		obj = next;
	}
}
