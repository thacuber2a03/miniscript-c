#ifndef MINISCRIPT_H
#define MINISCRIPT_H

// this is intended to be the public API's header file.
// it's a bit, uhm... lacking atm...

#include <stddef.h>

typedef struct ms_VM ms_VM;

typedef void *(*ms_ReallocFn)(void *ptr, size_t oldSize, size_t newSize);

typedef enum {
	MS_INTERPRET_OK,
	MS_INTERPRET_COMPILE_ERROR,
	MS_INTERPRET_RUNTIME_ERROR,
} ms_InterpretResult;

ms_VM *ms_newVM(ms_ReallocFn reallocFn);
void ms_freeVM(ms_VM *vm);

ms_InterpretResult ms_interpretString(ms_VM *vm, char *str);

void ms_runTestProgram(ms_VM *vm);

#endif
