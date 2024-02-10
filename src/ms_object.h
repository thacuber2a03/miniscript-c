#ifndef MS_OBJECT_H
#define MS_OBJECT_H

#include "ms_common.h"
#include "ms_code.h"
#include "ms_map.h"
#include "ms_value.h"

typedef enum {
	MS_OBJ_STRING,
} ms_ObjectType;

struct ms_Object {
	ms_ObjectType type;
	struct ms_Object *next;
};

struct ms_ObjString {
	ms_Object obj;
	char *chars;
	size_t length;
	uint32_t hash;
};


#define MS_TO_OBJ(val) val.as.object
#define MS_FROM_OBJ(val) ((ms_Value){ .type = MS_TYPE_OBJ, .as.object = (ms_Object*)val })
#define MS_IS_OBJ(val) (val.type == MS_TYPE_OBJ)

#define MS_OBJ_TYPE(val) (MS_TO_OBJ(val)->type)
#define MS_IS_STRING(val) isObjType(val, MS_OBJ_STRING)
#define MS_TO_STRING(val) ((ms_ObjString*)MS_TO_OBJ(val))
#define MS_TO_CSTRING(val) (((ms_ObjString*)MS_TO_OBJ(val))->chars)

static inline bool isObjType(ms_Value val, ms_ObjectType type)
{
	return MS_IS_OBJ(val) && MS_OBJ_TYPE(val) == type;
}

ms_ObjString *ms_newString(ms_VM* vm, char *chars, size_t length);
void ms_printObject(ms_Value val);
double ms_getBoolObj(ms_Value val);

#endif
