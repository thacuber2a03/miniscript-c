#ifndef MS_COMPILER_H
#define MS_COMPILER_H

#include "miniscript.h"
#include "ms_common.h"
#include "ms_code.h"

ms_InterpretResult ms_compileString(ms_VM* vm, char *str, ms_Code *code);

#endif
