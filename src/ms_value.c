#include <stdio.h>
#include <string.h>

#include "ms_object.h"
#include "ms_mem.h"
#include "ms_value.h"

void ms_printValue(ms_Value val)
{
	switch (MS_VAL_TYPE(val))
	{
		case MS_TYPE_NUM:  printf("%g", MS_TO_NUM(val));   break;
		case MS_TYPE_NULL: printf("null");                 break;
		case MS_TYPE_OBJ:  ms_printObject(val);            break;

		default: MS_UNREACHABLE("ms_printValue"); break;
	}
}

bool ms_valuesEqual(ms_Value a, ms_Value b)
{
	if (MS_VAL_TYPE(a) != MS_VAL_TYPE(b)) return false;
	switch (MS_VAL_TYPE(a))
	{
		case MS_TYPE_NUM:  return MS_TO_NUM(a) == MS_TO_NUM(b);
		case MS_TYPE_NULL: return true;
		case MS_TYPE_OBJ:  return MS_TO_OBJ(a) == MS_TO_OBJ(b);
		default: MS_UNREACHABLE("ms_valuesEqual"); break;
	}
}

double ms_getBoolVal(ms_Value val)
{
	switch (MS_VAL_TYPE(val))
	{
		case MS_TYPE_NUM:  return MS_TO_NUM(val);
		case MS_TYPE_NULL: return 0;
		case MS_TYPE_OBJ:  return ms_getBoolObj(val);

		default: MS_UNREACHABLE("ms_getBoolVal");
	}
}

/////////////////

void ms_initList(ms_VM *vm, ms_List *list)
{
	MS_UNUSED(vm);
	list->data = NULL;
	list->count = list->cap = 0;
}

void ms_freeList(ms_VM *vm, ms_List *list)
{
	MS_MEM_FREE_ARR(vm, ms_Value, list->data, list->cap);
	ms_initList(vm, list);
}

size_t ms_addValueToList(ms_VM *vm, ms_List *list, ms_Value val)
{
	if (list->count + 1 >= list->cap)
	{
		int oldCap = list->cap;
		list->cap = MS_ARR_GROW_CAP(oldCap);
		list->data = MS_MEM_REALLOC_ARR(
			vm, ms_Value, list->data,
			oldCap, list->cap
		);
	}

	int idx = list->count;
	list->data[idx] = val;
	list->count++;
	return idx;
}

// TODO: add insert and remove from list
