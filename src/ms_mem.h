#ifndef MS_MEM_H
#define MS_MEM_H

#include "ms_common.h"
#include "ms_vm.h"

#define MS_MEM_REALLOC(vm, ptr, oldSize, newSize) \
	ms_vmRealloc(vm, ptr, oldSize, newSize)

#define MS_MEM_REALLOC_ARR(vm, type, ptr, oldSize, newSize) \
	MS_MEM_REALLOC(vm, ptr, (oldSize) * sizeof(type), (newSize) * sizeof(type))

#define MS_MEM_MALLOC(vm, newSize) \
	MS_MEM_REALLOC(vm, NULL, 0, newSize)

#define MS_MEM_FREE(vm, ptr, oldSize) \
	MS_MEM_REALLOC(vm, ptr, oldSize, 0)

#define MS_MEM_FREE_ARR(vm, type, ptr, oldSize) \
	MS_MEM_FREE(vm, ptr, (oldSize) * sizeof(type))

#define MS_ARR_GROW_CAP(oldCap) ((oldCap) < 8 ? 8 : (oldCap * 2))

void *ms_vmRealloc(ms_VM *vm, void *ptr, size_t oldSize, size_t newSize);

#endif
