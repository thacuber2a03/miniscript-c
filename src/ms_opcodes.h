// this file enumerates all of the opcodes the virtual machine uses
// it must only included after a definition of the "OPCODE" macro

// in case the file *is* included without defining OPCODE...
#ifndef OPCODE
#define OPCODE(o)
#endif

OPCODE(MS_OP_CONST)
OPCODE(MS_OP_NULL)
// convenient
OPCODE(MS_OP_TRUE)
OPCODE(MS_OP_FALSE)

OPCODE(MS_OP_ADD)
OPCODE(MS_OP_SUBTRACT)
OPCODE(MS_OP_MULTIPLY)
OPCODE(MS_OP_DIVIDE)
OPCODE(MS_OP_POWER)
OPCODE(MS_OP_MODULO)
OPCODE(MS_OP_NEGATE)

OPCODE(MS_OP_NOT)

OPCODE(MS_OP_POP)
OPCODE(MS_OP_RETURN)

OPCODE(MS_OP__END)
