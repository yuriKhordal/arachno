#ifndef __MAP_H__
#define __MAP_H__

#include <stddef.h>
#include <sys/types.h>

/**
 * Represents a dynamic Map data structure of key:value pairs.
 * The keys are of string type and represents tokens, keywords, or symbols
 * The values are strings of any kind (can even represent an array of raw
 * bytes if needed).
 */
typedef struct arc_token_map {
   /** An array of the map's keys. */
   char **keys;
	/** An array of the map's values. */
   char **values;
	/** The amount of elements in the map. */
	size_t elementCount;
	/** The size of the `keys` and `values` arrays. */
	size_t arraySize;
} arc_token_map_t;

#if defined(ARC_NO_PREFIX) || defined(ARC_MAP_NO_PREFIX)
   #define token_map arc_token_map
   typedef arc_token_map_t token_map_t;

   #define mapNew() arcMapNew()
   #define mapInit(map) arcMapInit(map)
   #define mapDestroy(map) arcMapDestroy(map)
   #define mapFree(map) arcMapFree(map)
   #define mapPut(map, key, value) arcMapPut(map, key, value)
   #define mapFind(map, key) arcMapFind(map, key)
   #define mapGetByName(map, key) arcMapGetByName(map, key)
   #define mapGetByIndex(map, index) arcMapGetByIndex(map, index)
   #define mapGetKey(map, index) arcMapGetKey(map, index)
   #define mapLength(map) arcMapLength(map)
#endif

/**
 * Allocate a new map, initialize it, and return a pointer to it.
 * @note Caller takes ownership of the new pointer.
 * @note
 * Must be freed with `arcMapFree`, not `arcMapDestroy`. Destroy function
 * only releases memory inside the struct, without the pointer to the struct
 * itself.
 * @exception Return `NULL` if failed to allocate and log exception.
 */
arc_token_map_t * arcMapNew();

/**
 * Initialize a token_map struct.
 * @note
 * Must be destroyed with `arcMapDestroy()`, not `arcMapFree()`. Free function
 * releases the memory used by the map AND the struct token_map itself, so if
 * the map was allocated on the stack, the behaviour of `arcMapFree()` would
 * be undefined.
 */
void arcMapInit(arc_token_map_t *map);

/**
 * Release the memory used by the map struct, but not the struct itself!
 * @note
 * The complementory function to `arcMapInit`. For pointers allocated by
 * `arcMapNew`, use `arcMapFree`.
 */
void arcMapDestroy(arc_token_map_t *map);

/**
 * Release the memory used by the token_map struct, and the struct itself.
 * @note
 * The complementory function to `arcMapNew`. For stack allocated maps
 * initialized by `arcMapInit`, use `arcMapDestroy`.
 */
void arcMapFree(arc_token_map_t *map);

/**
 * Adds a `key:value` pair to a token_map struct. If the key
 * is already in the map, replaces the value in the map with the parameter.
 * The key and value strings are copied as new strings, and the struct
 * owns it's own memory.
 * @note Keys are case irrelevant.
 * @returns 0 if successfully added, 1 if replaced existing value, and a
 * negative value on failure.
 * @exception returns `ARC_ERR_NULL` if one of the parameters is NULL and
 * logs the exception.
 * @exception returns `ARC_ERR_ALLOC` on a memry allocation error and logs
 * the exception.
 */
int arcMapPut(arc_token_map_t *map, const char *key, const char *value);

/**
 * Searches for a key in the map, and returns it's index or -1 if the key
 * was not found.
 * @exception returns `ARC_ERR_NULL` if one of the parameters is NULL and
 * logs the exception.
 */
ssize_t arcMapFind(const arc_token_map_t *map, const char *key);

/**
 * Gets a value in the map by key, and returns the value or NULL if the
 * key was not found.
 * @exception Returns `NULL` if one of the parameters is `NULL` and logs
 * the exception.
 */
const char* arcMapGetByName(const arc_token_map_t *map, const char *key);

/**
 * Gets a value in the map by index.
 * @exception Returns `NULL` if the map pointer is `NULL` or if
 * the index is out of range and logs the exception.
 */
const char* arcMapGetByIndex(const arc_token_map_t *map, int index);

/**
 * Gets a key in the map by index.
 * @exception Returns `NULL` if the map pointer is `NULL` or if
 * the index is out of range and logs the exception.
 */
const char* arcMapGetKey(const arc_token_map_t *map, int index);

/**
 * Returns the amount of elements in the map.
 * @exception Returns `ARC_ERR_NULL` if the pointer is `NULL`.
 */
ssize_t arcMapLength(const arc_token_map_t *map);


#endif