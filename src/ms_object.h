#ifndef MS_OBJECT_H
#define MS_OBJECT_H

#include "ms_common.h"
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
};

ms_ObjString *ms_newString(ms_VM* vm, char *chars, size_t length);
void ms_printObject(ms_Value val);

#endif
