#ifndef MS_DEBUG_H
#define MS_DEBUG_H

#include "ms_code.h"

const char *ms_getOpcodeName(ms_Opcode op);
size_t ms_disassembleInstruction(ms_Code code, size_t offset);

#endif
