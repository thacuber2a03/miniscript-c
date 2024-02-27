#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ms_common.h"
#include "ms_vm.h"
#include "ms_map.h"
#include "ms_value.h"
#include "ms_object.h"
#include "ms_compiler.h"
#include "ms_mem.h"
#include "ms_code.h"

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

ms_VM *ms_newVM(ms_ReallocFn reallocFn)
{
	if (reallocFn == NULL) reallocFn = defaultRealloc;
	ms_VM *vm = reallocFn(NULL, 0, sizeof *vm);
	MS_ASSERT(vm != NULL);

#ifdef MS_DEBUG_MEM_ALLOC
	fprintf(stderr, "vm: hello! setting up vm...\n");
#endif

	vm->reallocFn = reallocFn;
	vm->bytesUsed = 0;
	vm->objects = NULL;
	vm->stackTop = vm->stack;
	ms_initMap(vm, &vm->strings);
	ms_initMap(vm, &vm->globals);

#ifdef MS_DEBUG_MEM_ALLOC
	fprintf(stderr, "vm: all set up and ready to go!\n");
#endif

	return vm;
}

void ms_freeVM(ms_VM *vm)
{
#ifdef MS_DEBUG_MEM_ALLOC
	fprintf(stderr, "vm: about to free itself...\nvm: freeing all objects...\n");
#endif
	ms_freeAllObjects(vm);
#ifdef MS_DEBUG_MEM_ALLOC
	fprintf(stderr, "vm: all objects freed\n");
#endif
	vm->objects = NULL;
	ms_freeMap(vm, &vm->strings);
	ms_freeMap(vm, &vm->globals);

	MS_ASSERT_REASON(vm->bytesUsed == 0, "program leaked memory!!");
#ifdef MS_DEBUG_MEM_ALLOC
	fprintf(stderr, "vm: successfully freed everything, will free itself now. goodbye!");
#endif
	vm->reallocFn(vm, sizeof *vm, 0);
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

ms_Value ms_peekIntoStack(ms_VM *vm, size_t distance)
{
	MS_ASSERT_REASON(vm->stackTop - 1 - distance >= vm->stack, "peek index exits stack");
	return vm->stackTop[-1 - distance];
}

void ms_pushNullIntoVM(ms_VM *vm) { ms_pushValueIntoVM(vm, MS_NULL_VAL); }
void ms_pushTrueIntoVM(ms_VM *vm) { ms_pushValueIntoVM(vm, MS_FROM_NUM(1)); }
void ms_pushFalseIntoVM(ms_VM *vm) { ms_pushValueIntoVM(vm, MS_FROM_NUM(0)); }

ms_InterpretResult ms_runtimeError(ms_VM *vm, const char *err)
{
	MS_UNUSED(vm);
	fprintf(stderr, "Error: %s\n", err);
	return MS_INTERPRET_RUNTIME_ERROR;
}

static ms_InterpretResult interpret(ms_VM* vm, ms_Code *code)
{
	register uint8_t *ip = code->data;
	register ms_Value temp, temp2;

#define NEXT_BYTE() (*ip++)
#define NEXT_SHORT() (ip += 2, (((uint16_t)ip[-2]) << 8 | (uint16_t)ip[-1]))
#define NEXT_CONST() (code->constants.data[NEXT_BYTE()])
#define BINARY_OP(vm, op) do {                               \
    temp2 = ms_popValueFromVM(vm);                           \
    temp = ms_popValueFromVM(vm);                            \
                                                             \
    if (MS_VAL_TYPE(temp) != MS_VAL_TYPE(temp2))             \
      return ms_runtimeError(vm, "Types must be the same."); \
                                                             \
    MS_ASSERT(MS_IS_NUM(temp)); /* TODO */                   \
                                                             \
    temp = MS_FROM_NUM(MS_TO_NUM(temp) op MS_TO_NUM(temp2)); \
    ms_pushValueIntoVM(vm, temp);                            \
  } while(0)

#define COMPARISON_OP(vm, op) do {                                              \
    temp2 = ms_popValueFromVM(vm);                                              \
    temp = ms_popValueFromVM(vm);                                               \
                                                                                \
    if (MS_VAL_TYPE(temp) != MS_VAL_TYPE(temp2))                                \
      return ms_runtimeError(vm, "Types must be equal.");                       \
                                                                                \
    if (MS_VAL_TYPE(temp) == MS_TYPE_OBJ && MS_OBJ_TYPE(temp) == MS_OBJ_STRING) \
      ms_pushValueIntoVM(vm, MS_FROM_NUM(                                       \
        (strcmp(MS_TO_CSTRING(temp), MS_TO_CSTRING(temp2)) op 0)                \
      ));                                                                       \
    else                                                                        \
      ms_pushValueIntoVM(vm, MS_FROM_NUM(MS_TO_NUM(temp) op MS_TO_NUM(temp2))); \
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
			case MS_OP_NULL:  ms_pushNullIntoVM(vm); break;
			case MS_OP_TRUE:  ms_pushTrueIntoVM(vm); break;
			case MS_OP_FALSE: ms_pushFalseIntoVM(vm); break;

			case MS_OP_ADD:      BINARY_OP(vm, +); break;
			case MS_OP_SUBTRACT: BINARY_OP(vm, -); break;
			case MS_OP_MULTIPLY: BINARY_OP(vm, *); break;
			case MS_OP_DIVIDE:   BINARY_OP(vm, /); break;
			case MS_OP_POWER:
				temp2 = ms_popValueFromVM(vm);
				temp = ms_popValueFromVM(vm);

				if (MS_VAL_TYPE(temp) != MS_VAL_TYPE(temp2))
					return ms_runtimeError(vm, "Both types must be equal.");

				if (MS_VAL_TYPE(temp) != MS_TYPE_NUM)
					return ms_runtimeError(vm, "Can't currently operate on non-numbers.");
				else
					ms_pushValueIntoVM(vm, MS_FROM_NUM(pow(MS_TO_NUM(temp), MS_TO_NUM(temp2))));

				break;

			case MS_OP_MODULO:
				temp2 = ms_popValueFromVM(vm);
				temp = ms_popValueFromVM(vm);

				if (MS_VAL_TYPE(temp) != MS_VAL_TYPE(temp2))
					return ms_runtimeError(vm, "Both types must be equal.");

				if (MS_VAL_TYPE(temp) != MS_TYPE_NUM)
					return ms_runtimeError(vm, "Can't currently operate on non-numbers.");
				else
					ms_pushValueIntoVM(vm, MS_FROM_NUM(fmod(MS_TO_NUM(temp), MS_TO_NUM(temp2))));

				break;

#define ABSCLAMP01(v) fabs((v) < 0 ? 0 : ((v) > 1 ? 1 : (v)))

			case MS_OP_NEGATE:
				temp = ms_popValueFromVM(vm);
				if (!MS_IS_NUM(temp)) return ms_runtimeError(vm, "Attempt to negate non-number");
				ms_pushValueIntoVM(vm, MS_FROM_NUM(-ABSCLAMP01(MS_TO_NUM(temp))));
				break;

			case MS_OP_AND:
				temp2 = ms_popValueFromVM(vm);
				temp = ms_popValueFromVM(vm);
				ms_pushValueIntoVM(vm,
					MS_FROM_NUM(ABSCLAMP01(ms_getBoolVal(temp) * ms_getBoolVal(temp2)))
				);
				break;

			case MS_OP_OR:
				temp2 = MS_FROM_NUM(ms_getBoolVal(ms_popValueFromVM(vm)));
				temp = MS_FROM_NUM(ms_getBoolVal(ms_popValueFromVM(vm)));
				ms_pushValueIntoVM(vm, MS_FROM_NUM(ABSCLAMP01(
					// formula taken from official C# implementation
					MS_TO_NUM(temp) + MS_TO_NUM(temp2) - MS_TO_NUM(temp) * MS_TO_NUM(temp2)
				)));
				break;

			case MS_OP_NOT:
				temp = ms_popValueFromVM(vm);
				ms_pushValueIntoVM(vm, MS_FROM_NUM(1-ABSCLAMP01(ms_getBoolVal(temp))));
				break;

#undef ABSCLAMP01

			case MS_OP_EQUAL:
				ms_pushValueIntoVM(vm, MS_FROM_NUM(
					ms_valuesEqual(ms_popValueFromVM(vm), ms_popValueFromVM(vm))
				));
				break;

			case MS_OP_NOT_EQUAL:
				ms_pushValueIntoVM(vm, MS_FROM_NUM(
					!ms_valuesEqual(ms_popValueFromVM(vm), ms_popValueFromVM(vm))
				));
				break;

			case MS_OP_GREATER:       COMPARISON_OP(vm, > ); break;
			case MS_OP_LESS:          COMPARISON_OP(vm, < ); break;
			case MS_OP_GREATER_EQUAL: COMPARISON_OP(vm, >=); break;
			case MS_OP_LESS_EQUAL:    COMPARISON_OP(vm, <=); break;
			
			case MS_OP_SET_GLOBAL:
				ms_setMapKey(vm, &vm->globals, NEXT_CONST(), ms_popValueFromVM(vm));
				break;

			case MS_OP_GET_GLOBAL: {
				ms_Value val = MS_NULL_VAL;
				ms_getMapKey(vm, &vm->globals, NEXT_CONST(), &val);
				ms_pushValueIntoVM(vm, val);
			} break;

			case MS_OP_GET_LOCAL: {
				uint8_t slot = NEXT_BYTE();
				ms_pushValueIntoVM(vm, vm->stack[slot]);
			} break;

			case MS_OP_SET_LOCAL: {
				uint8_t slot = NEXT_BYTE();
				vm->stack[slot] = ms_popValueFromVM(vm);
			} break;

			case MS_OP_INVOKE: {
				// this case is currently a bunch of do nothing
				// just to not enter the default branch
			} break;

			case MS_OP_JUMP: {
				uint16_t offset = NEXT_SHORT();
				ip += offset;
			} break;

			case MS_OP_JUMP_IF_FALSE: {
				uint16_t offset = NEXT_SHORT();
				if (!ms_getBoolVal(ms_peekIntoStack(vm, 0))) ip += offset;
			} break;

			case MS_OP_LOOP: {
				uint16_t offset = NEXT_SHORT();
				ip -= offset;
			} break;

			case MS_OP_POP: ms_popValueFromVM(vm); break;

			case MS_OP_RETURN:
#ifdef MS_DEBUG_EXECUTION
				printf("vm: sucessfully finished execution!\n");
#endif
				return MS_INTERPRET_OK;

			default: MS_UNREACHABLE("interpret"); break;
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

	ms_addByteToCode(vm, &code, MS_OP_TRUE);
	ms_addByteToCode(vm, &code, MS_OP_LOOP);
	ms_addByteToCode(vm, &code, 00);
	ms_addByteToCode(vm, &code, 01);

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

	res = interpret(vm, &code);
	ms_freeCode(vm, &code);
	return res;
}
