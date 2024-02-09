#ifdef MS_DEBUG_MEM_ALLOC
#include <stdio.h>
#endif
#include <stdlib.h>

#include "ms_common.h"
#include "ms_vm.h"
#include "ms_mem.h"

void *ms_vmRealloc(ms_VM *vm, void *ptr, size_t oldSize, size_t newSize)
{
	bool isFreeing = newSize == 0;
	MS_UNUSED(isFreeing);

	int diff = newSize - oldSize;
	vm->bytesUsed += diff;
#ifdef MS_DEBUG_MEM_ALLOC
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
