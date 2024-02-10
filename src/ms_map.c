#include <string.h>

#include "ms_common.h"
#include "ms_mem.h"
#include "ms_object.h"
#include "ms_value.h"
#include "ms_map.h"

// TODO: this is a constant from Crafting Interpreters, benchmark and tune this
#define MAP_MAX_LOAD 0.75

void ms_initMap(ms_VM* vm, ms_Map *map)
{
	MS_UNUSED(vm);
	map->count = map->cap = 0;
	map->entries = NULL;
}

void ms_freeMap(ms_VM* vm, ms_Map *map)
{
	MS_MEM_FREE_ARR(vm, ms_MapEntry, map->entries, map->cap);
	ms_initMap(vm, map);
}

static ms_MapEntry *findEntry(ms_MapEntry *entries, size_t cap, ms_Value key)
{
	uint32_t index;

	if (MS_OBJ_TYPE(key) == MS_OBJ_STRING)
		index = MS_TO_STRING(key)->hash;
	else
		// TODO: cache hash somehow
		index = ms_hashMem(key.as.object, sizeof(ms_Object*));

	index %= cap;

	ms_MapEntry *tombstone = NULL;
	for (;;)
	{
		ms_MapEntry *entry = entries + index;

		if (!entry->_isUsed)
			if (MS_IS_NULL(entry->value))
				return tombstone != NULL ? tombstone : entry;
			else
			{
				if (tombstone == NULL) tombstone = entry;
			}
		else if (ms_valuesEqual(entry->key, key))
			return entry;

		index = (index + 1) % cap;
	}
}

static void adjustCapacity(ms_VM *vm, ms_Map *map, size_t cap)
{
	ms_MapEntry *entries = MS_MEM_MALLOC_ARR(vm, ms_MapEntry, cap);

	for (size_t i = 0; i < cap; i++)
	{
		entries[i].key = MS_NULL_VAL;
		entries[i].value = MS_NULL_VAL;
		entries[i]._isUsed = false;
	}

	map->count = 0;
	for (size_t i = 0; i < map->cap; i++)
	{
		ms_MapEntry* entry = map->entries + i;
		if (!entry->_isUsed) continue;

		ms_MapEntry *dest = findEntry(entries, cap, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		dest->_isUsed = true;
		map->count++;
	}

	MS_MEM_FREE_ARR(vm, ms_MapEntry, map->entries, map->cap);
	map->entries = entries;
	map->cap = cap;
}

bool ms_setMapKey(ms_VM* vm, ms_Map *map, ms_Value key, ms_Value value)
{
	if (map->count + 1 > map->cap * MAP_MAX_LOAD)
	{
		size_t cap = MS_ARR_GROW_CAP(map->cap);
		adjustCapacity(vm, map, cap);
	}

	ms_MapEntry *entry = findEntry(map->entries, map->cap, key);
	bool newKey = !entry->_isUsed;
	if (newKey && MS_IS_NULL(entry->value)) map->count++;

	entry->key = key;
	entry->value = value;
	entry->_isUsed = true;
	return newKey;
}

bool ms_getMapKey(ms_VM *vm, ms_Map *map, ms_Value key, ms_Value *value)
{
	MS_UNUSED(vm);
	if (map->count == 0) return false;

	ms_MapEntry *entry = findEntry(map->entries, map->cap, key);
	if (!entry->_isUsed) return false;

	*value = entry->value;
	return true;
}

bool ms_deleteFromMap(ms_VM *vm, ms_Map *map, ms_Value key)
{
	MS_UNUSED(vm);
	if (map->count == 0) return false;

	ms_MapEntry *entry = findEntry(map->entries, map->cap, key);
	if (!entry->_isUsed) return false;

	entry->_isUsed = false;
	entry->value = MS_FROM_NUM(1);
	return true;
}

ms_ObjString *ms_findStringInMap(ms_VM *vm, ms_Map *map, char* str, size_t length, uint32_t hash)
{
	MS_UNUSED(vm);
	if (map->count == 0) return NULL;

	uint32_t index = hash % map->cap;
	for (;;)
	{
		ms_MapEntry *entry = map->entries + index;

		if (!entry->_isUsed)
		{
			if (MS_IS_NULL(entry->value)) return NULL;
		}
		else if (MS_IS_STRING(entry->key))
		{
			ms_ObjString *strObj = MS_TO_STRING(entry->key);
			if (strObj->length == length
				&& strObj->hash == hash
				&& !memcmp(strObj->chars, str, length))
				return strObj;
		}

		index = (index + 1) % map->cap;
	}
}
