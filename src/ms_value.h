#ifndef MS_VALUE_H
#define MS_VALUE_H

#include "ms_common.h"
#include "miniscript.h"

typedef struct ms_Object ms_Object;
typedef struct ms_ObjString ms_ObjString;

typedef enum {
	MS_TYPE_NUM,
	MS_TYPE_NULL,
	MS_TYPE_OBJ,
} ms_ValueType;

typedef struct {
	ms_ValueType type;
	union {
		double number;
		ms_Object *object;
	} as;
} ms_Value;

#define MS_VAL_TYPE(val) val.type

#define MS_TO_NUM(val) val.as.number
#define MS_FROM_NUM(val) ((ms_Value){ .type = MS_TYPE_NUM, .as.number = val })
#define MS_IS_NUM(val) (val.type == MS_TYPE_NUM)

#define MS_NULL_VAL ((ms_Value){ .type = MS_TYPE_NULL })
#define MS_IS_NULL(val) (val.type == MS_TYPE_NULL)

#define MS_TO_OBJ(val) val.as.object
#define MS_FROM_OBJ(val) ((ms_Value){ .type = MS_TYPE_OBJ, .as.object = (ms_Object*)val })
#define MS_IS_OBJ(val) (val.type == MS_TYPE_OBJ)

#define MS_OBJ_TYPE(val) (MS_TO_OBJ(val)->type)
#define MS_TO_STRING(val) ((ms_ObjString*)MS_TO_OBJ(val))
#define MS_TO_CSTRING(val) (((ms_ObjString*)MS_TO_OBJ(val))->chars)

void ms_printValue(ms_Value val);
bool ms_valuesEqual(ms_Value a, ms_Value b);
bool ms_isValueFalsy(ms_Value val);
double ms_getBoolVal(ms_Value val);

typedef struct {
	size_t count, cap;
	ms_Value *data;
} ms_List;

void ms_initList(ms_VM *vm, ms_List *list);
void ms_freeList(ms_VM *vm, ms_List *list);
size_t ms_addValueToList(ms_VM *vm, ms_List *list, ms_Value val);

#endif
