#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>

#include "map.h"
#include "logger.h"
#include "errors.h"

int strcmpi(const char *str1, const char *str2);

arc_token_map_t * arcMapNew() {
	arc_token_map_t * map = malloc(sizeof(arc_token_map_t));
	if (map == NULL) {
		logf_errno("Failed allocating token_map");
		log_line();
		return NULL;
	}

	arcMapInit(map);
	return map;
}

void arcMapInit(arc_token_map_t *map) {
	ARC_CHECK_NULL(map,);

	map->arraySize = 0;
	map->elementCount = 0;
	map->keys = NULL;
	map->values = NULL;
}

void arcMapDestroy(arc_token_map_t *map) {
	ARC_CHECK_NULL(map,);

	if (map->keys != NULL) {
		for (size_t i = 0; i < map->elementCount; i++) {
			free(map->keys[i]);
			free(map->values[i]);
		}
		free(map->keys);
		free(map->values);
		map->keys = NULL;
		map->values = NULL;
	}
}

void arcMapFree(arc_token_map_t *map) {
	ARC_CHECK_NULL(map,);

	if (map->keys != NULL) {
		for (size_t i = 0; i < map->elementCount; i++) {
			free(map->keys[i]);
			free(map->values[i]);
		}
		free(map->keys);
		free(map->values);
		map->keys = NULL;
		map->values = NULL;
	}

	free(map);
}

int arcMapPut(arc_token_map_t *map, const char *key, const char *value) {
	const int FIELD_ADDED = 0;
	const int FIELD_REPLACED = 1;
	ARC_CHECK_NULL(map, ARC_ERR_NULL);
	ARC_CHECK_NULL(key, ARC_ERR_NULL);
	ARC_CHECK_NULL(value, ARC_ERR_NULL);
	
	// Resize if full.
	if (map->elementCount == map->arraySize) {
		if (map->arraySize == 0) map->arraySize = 8;
		else map->arraySize *= 2;

		char **fields = realloc(map->keys, sizeof(char*) * map->arraySize);
		if (fields == NULL) {
			logf_errno("Failed realocating token_map fields array");
			log_line();
			return ARC_ERR_ALLOC;
		}
		map->keys = fields;

		char **values = realloc(map->values, sizeof(char*) * map->arraySize);
		if (values == NULL) {
			logf_errno("Failed realocating token_map values array");
			log_line();
			return ARC_ERR_ALLOC;
		}
		map->values = values;
	}

	// Duplicate value.
	char *dupvalue = strdup(value);
	if (dupvalue == NULL) {
		logf_errno("Failed duplicating field value for token_map");
		log_line();
		return ARC_ERR_ALLOC;
	}

	// Check if key is already in the map, if yes replace.
	ssize_t index = arcMapFind(map, key);
	if (index >= 0) {
		free(map->values[index]);
		map->values[index] = dupvalue;
		return FIELD_REPLACED;
	}

	// Duplicate name.
	char *dupkey = strdup(key);
	if (dupkey == NULL) {
		logf_errno("Failed duplicating key name for token_map");
		log_line();
		free(dupvalue);
		return ARC_ERR_ALLOC;
	}

	// Append
	map->keys[map->elementCount] = dupkey;
	map->values[map->elementCount] = dupvalue;
	map->elementCount++;
	return FIELD_ADDED;
}

ssize_t arcMapFind(const arc_token_map_t *map, const char *key) {
	ARC_CHECK_NULL(map, ARC_ERR_NULL);
	ARC_CHECK_NULL(key, ARC_ERR_NULL);
	
	for (size_t i = 0; i < map->elementCount; i++) {
		if (strcmpi(key, map->keys[i]) == 0)
			return i;
	}

	return -1;
}

const char* arcMapGetByName(const arc_token_map_t *map, const char *key) {
	ARC_CHECK_NULL(map, NULL);
	ARC_CHECK_NULL(key, NULL);
	
	for (size_t i = 0; i < map->elementCount; i++) {
		if (strcmpi(key, map->keys[i]) == 0)
			return map->values[i];
	}

	return NULL;
}

const char* arcMapGetByIndex(const arc_token_map_t *map, int index) {
	ARC_CHECK_NULL(map, NULL);
	ARC_CHECK_INDEX(index, map->elementCount, NULL);

	return map->values[index];
}

const char* arcMapGetKey(const arc_token_map_t *map, int index) {
	ARC_CHECK_NULL(map, NULL);
	ARC_CHECK_INDEX(index, map->elementCount, NULL);

	return map->keys[index];
}

ssize_t arcMapLength(const arc_token_map_t *map) {
	ARC_CHECK_NULL(map, ARC_ERR_NULL);

	return map->elementCount;
}

int strcmpi(const char *str1, const char *str2) {
	if (str1 == NULL) return 1;
	if (str2 == NULL) return -1;

	int diff = tolower(*str1) - tolower(*str2);
	while (diff == 0 && *str1 && *str2) {
		str1++;
		str2++;
		diff = tolower(*str1) - tolower(*str2);
	}

	if (diff < 0) return -1;
	else if (diff > 0) return 1;
	else return 0;
}
