#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "ms_common.h"
#include "ms_vm.h"
#include "ms_compiler.h"
#include "ms_mem.h"
#include "ms_code.h"
#include "ms_value.h"

#ifdef MS_DEBUG_EXECUTION
#include "ms_debug.h"
#endif

void *defaultRealloc(void *ptr, size_t oldSize, size_t newSize)
{
	MS_UNUSED(oldSize);

	if (newSize == 0)
	{
		free(ptr);
		return NULL;
	}

	return realloc(ptr, newSize);
}

static void initVM(ms_VM *vm)
{
	vm->stackTop = vm->stack;
}

ms_VM *ms_newVM(ms_ReallocFn reallocFn)
{
	if (reallocFn == NULL) reallocFn = defaultRealloc;
	ms_VM *vm = reallocFn(NULL, 0, sizeof *vm);
	MS_ASSERT(vm != NULL);

#ifdef MS_DEBUG_MEM_ALLOC
	fprintf(stderr, "mem: allocated %zu bytes\n", sizeof(ms_VM));
#endif

	vm->reallocFn = reallocFn;
	vm->bytesUsed = sizeof *vm;
	initVM(vm);

	return vm;
}

void ms_freeVM(ms_VM *vm)
{
	MS_ASSERT(vm->bytesUsed == sizeof *vm);
	MS_MEM_FREE(vm, vm, sizeof *vm);
}

void ms_pushValueIntoVM(ms_VM *vm, ms_Value val)
{
	MS_ASSERT_REASON(vm->stackTop - vm->stack < MS_MAX_STACK_SIZE, "stack overflow");
	*vm->stackTop++ = val;
}

ms_Value ms_popValueFromVM(ms_VM *vm)
{
	MS_ASSERT_REASON(vm->stackTop != vm->stack, "stack underflow");
	return *--vm->stackTop;
}

ms_InterpretResult ms_runtimeError(ms_VM *vm, const char *err)
{
	MS_UNUSED(vm);
	fprintf(stderr, "Error: %s\n", err);
	return MS_INTERPRET_RUNTIME_ERROR;
}

static ms_InterpretResult interpret(register ms_VM* vm, register ms_Code *code)
{
	register uint8_t *ip = code->data;
	register ms_Value temp;

#define NEXT_BYTE() (*ip++)
#define NEXT_CONST() (code->constants.data[NEXT_BYTE()])
#define BINARY_OP(vm, op) do {                                         \
    ms_Value b = ms_popValueFromVM(vm);                                \
    ms_Value a = ms_popValueFromVM(vm);                                \
                                                                       \
    if (a.type != b.type)                                              \
      return ms_runtimeError(vm, "Types must be the same.");           \
    MS_ASSERT(MS_IS_NUM(a)); /* TODO */                                \
                                                                       \
    temp = MS_FROM_NUM(MS_TO_NUM(a) op MS_TO_NUM(b));                  \
    ms_pushValueIntoVM(vm, temp);                                      \
  } while(0)

#ifdef MS_DEBUG_EXECUTION
	fprintf(stderr, "vm: will start executing code...\n");
#endif

	for (;;)
	{
#ifdef MS_DEBUG_EXECUTION
		printf("vm: stack state: ");
		for (ms_Value *i = vm->stack; i < vm->stackTop; i++)
		{
			printf("[");
			ms_printValue(*i);
			printf("]");
		}
		printf("\nvm: current instruction: ");
		ms_disassembleInstruction(code, ip-code->data);
		printf("\n");
#endif

		switch (NEXT_BYTE())
		{
			case MS_OP_CONST: ms_pushValueIntoVM(vm, NEXT_CONST()); break;
			case MS_OP_NULL:  ms_pushValueIntoVM(vm, MS_NULL_VAL); break;
			case MS_OP_TRUE:  ms_pushValueIntoVM(vm, MS_FROM_NUM(1)); break;
			case MS_OP_FALSE: ms_pushValueIntoVM(vm, MS_FROM_NUM(0)); break;

			case MS_OP_ADD:      BINARY_OP(vm, +); break;
			case MS_OP_SUBTRACT: BINARY_OP(vm, -); break;
			case MS_OP_MULTIPLY: BINARY_OP(vm, *); break;
			case MS_OP_DIVIDE:   BINARY_OP(vm, /); break;
			case MS_OP_POWER: {
				ms_Value b = ms_popValueFromVM(vm);
				ms_Value a = ms_popValueFromVM(vm);

				if (MS_VAL_TYPE(a) != MS_VAL_TYPE(b))
					return ms_runtimeError(vm, "Both types must be equal.");
				MS_ASSERT(MS_VAL_TYPE(a) == MS_TYPE_NUM); // TODO

				temp = MS_FROM_NUM(pow(MS_TO_NUM(a), MS_TO_NUM(b)));
				ms_pushValueIntoVM(vm, temp);
			} break;

			case MS_OP_MODULO: {
				ms_Value b = ms_popValueFromVM(vm);
				ms_Value a = ms_popValueFromVM(vm);

				if (MS_VAL_TYPE(a) != MS_VAL_TYPE(b))
					return ms_runtimeError(vm, "Both types must be equal.");
				MS_ASSERT(MS_VAL_TYPE(a) == MS_TYPE_NUM); // TODO

				temp = MS_FROM_NUM(fmod(MS_TO_NUM(a), MS_TO_NUM(b)));
				ms_pushValueIntoVM(vm, temp);
				break;
			}

			case MS_OP_NEGATE:
				temp = ms_popValueFromVM(vm);

				if (MS_IS_NUM(temp)) MS_TO_NUM(temp) *= -1;
				else return ms_runtimeError(vm, "Attempt to negate a non-number");

				ms_pushValueIntoVM(vm, temp);
				break;

			case MS_OP_NOT:
				temp = ms_popValueFromVM(vm);
				ms_pushValueIntoVM(vm, MS_FROM_NUM(ms_isValueFalsy(temp)));
				break;

			case MS_OP_POP: ms_popValueFromVM(vm); break;

			case MS_OP_RETURN:
#ifdef MS_DEBUG_EXECUTION
				printf("vm: sucessfully finished execution\n");
#endif
				return MS_INTERPRET_OK;

			default:
				MS_ASSERT_REASON(false, "program must never reach this branch");
		}
	}

#undef NEXT_BYTE
#undef NEXT_CONST
#undef BINARY_OP
}

void ms_runTestProgram(ms_VM *vm)
{
	ms_Code code;
	ms_initCode(vm, &code);

	ms_addByteToCode(vm, &code, MS_OP_NULL);
	ms_addByteToCode(vm, &code, MS_OP_FALSE);
	ms_addByteToCode(vm, &code, MS_OP_TRUE);
	ms_addByteToCode(vm, &code, MS_OP_POP);
	ms_addByteToCode(vm, &code, MS_OP_POP);
	ms_addByteToCode(vm, &code, MS_OP_RETURN);

	interpret(vm, &code);

	ms_freeCode(vm, &code);
}

ms_InterpretResult ms_interpretString(ms_VM *vm, char *str)
{
	ms_Code code;
	ms_initCode(vm, &code);

	ms_InterpretResult res = ms_compileString(vm, str, &code);
	if (res != MS_INTERPRET_OK)
	{
		ms_freeCode(vm, &code);
		return res;
	}

	initVM(vm);
	res = interpret(vm, &code);
	ms_freeCode(vm, &code);
	return res;
}
