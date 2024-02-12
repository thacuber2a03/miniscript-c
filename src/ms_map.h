#ifndef MS_MAP_H
#define MS_MAP_H

#include "ms_common.h"
#include "ms_value.h"

typedef struct {
	ms_Value key, value;
	bool _isUsed;
} ms_MapEntry;

typedef struct {
	size_t cap, count;
	ms_MapEntry *entries;
} ms_Map;

void ms_initMap(ms_VM* vm, ms_Map *map);
void ms_freeMap(ms_VM* vm, ms_Map *map);
bool ms_setMapKey(ms_VM* vm, ms_Map *map, ms_Value key, ms_Value value);
bool ms_getMapKey(ms_VM *vm, ms_Map *map, ms_Value key, ms_Value *value);
ms_ObjString *ms_findStringInMap(ms_VM *vm, ms_Map *map, const char* str, size_t length, uint32_t hash);

#endif
