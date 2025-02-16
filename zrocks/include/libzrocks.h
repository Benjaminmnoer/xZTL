/* libztl: User-space Zone Translation Layer Library
 *
 * Copyright 2019 Samsung Electronics
 *
 * Written by Ivan L. Picoli <i.picoli@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef LIBZROCKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#define ZNS_ALIGMENT 4096

/* 4KB aligment : 16 GB user buffers
 * 512b aligment: 2 GB user buffers */
#define ZNS_MAX_BUF  (ZNS_ALIGMENT * 65536)

struct zrocks_map {
    union {
	struct {

	    /* Media offset */
	    /* 4KB  sector: Max capacity: 4PB
             * 512b sector: Max capacity: 512TB */
            uint64_t offset : 40;

	    /* Number of sectors */
            /* 4KB  sector: Max entry size: 32GB
             * 512b sector: Max entry size: 4GB */
	    uint64_t nsec   : 23;

	    /* Multi-piece mapping bit */
	    uint64_t multi  : 1;
	} g;

	uint64_t addr;
    };
};

/**
 * Initialize zrocks library
 *
 * @param dev_name URI provided by the user
 * 	 	   e.g. PCIe:    pci:0000:03:00.0?nsid=2
 * 	 	   	Fabrics: fab:172.20.0.100:4420?nsid=2
 *
 * @return Returns zero if the calls succeed, or a negative value
 * 	   if the call fails
 */
int zrocks_init (const char *dev_name);

/**
 * Close zrocks library
 *
 * @return Returns zero if the calls succeed, or a negative value
 * 	   if the call fails
 */
int zrocks_exit (void);

/**
 * Allocate an aligned buffer for I/O
 *
 * @param size Memory size
 *
 * @return Return a pointer to the allocated memory if the call succeeds,
 * 	   or returns NULL if the call fails
 */
void *zrocks_alloc (size_t size);

/**
 * Free a buffer allocated by zrocks_alloc
 *
 * @param ptr Pointer to memory allocated by zrocks_alloc
 */
void zrocks_free (void *ptr);


/* >>> OBJECT INTERFACE FUNCTIONS
 * >>> WARNING: Recovery of objects after shutdown is still under development
 * 	    Use the BLOCK INTERFACE functions if your application provides
 * 	    recovery.
 */

/**
 * Creates a new variable-sized object belonging to a certain LSM-Tree level
 *
 * @param id Object ID
 * @param buf Pointer to a buffer containing data to be written
 * @param size Data size
 * @param level LSM-Tree level
 *
 * @return Returns zero if the calls succeed, or a negative value
 * 	   if the call fails
 */
int zrocks_new (uint64_t id, void *buf, size_t size, uint16_t level);

/**
 * Delete an object
 *
 * @param id Object ID to be deleted
 *
 * @return Returns zero if the calls succeed, or a negative value
 * 	   if the call fails
 */
int zrocks_delete (uint64_t id);

/**
 * Read an offset within an object
 *
 * @id - Unique integer identifier of the object
 * @offset - Offset in bytes within the object
 * @buf - Pointer to a buffer where data must be copied into
 * @size - Size in bytes starting from offset
 *
 * @return Returns zero if the calls succeed, or a negative value
 * 	   if the call fails
 **/
int zrocks_read_obj (uint64_t id, uint64_t offset, void *buf, size_t size);


/* >>> BLOCK INTERFACE FUNCTIONS
 * >>> Use these functions if your application provides recovery
 */

/**
 * Invalidate a piece of data represented by a mapping entry provided
 * by the 'zrocks_write' function.
 *
 * @param map Pointer to the piece of data to be invalidated
 *
 * @return Returns zero if the calls succeed, or a negative value
 * 	   if the call fails
 */
int zrocks_trim (struct zrocks_map *map, uint16_t level);
/**
 * Write to ZNS device and return the mapping multi-piece list
 *
 * @param buf Pointer to the data
 * @param size Data size
 * @param level LSM-Tree level
 * @param map Pointer to an array of zrocks_map entries. This list is
 * 	      filled by the ZTL and contains the physical addresses
 * 	      of the locations where data was written. Depending on the
 * 	      zone size and how the data striping is performed, we write
 * 	      to multiple zones. We call each location as a mapping piece.
 * @param pieces Pointer to an integer. This value is filled by the ZTL and
 * 		 contains the number of mapping pieces created by the write
 *
 * @return If the return value is zero, the user is responsible for
 * 	   calling 'zrocks_free' and free 'map' by passing its value as
 * 	   parameter. A negative value is return in case of failure.
 */
int zrocks_write (void *buf, size_t size, uint16_t level,
				struct zrocks_map **map, uint16_t *pieces);

/**
 * Read from the ZNS drive using physical offsets
 *
 * @offset - Offset in bytes within the ZNS device
 * @buf - Pointer to a buffer where data must be copied into
 * @size - Size in bytes starting from offset
 *
 * @return Returns zero if the calls succeed, or a negative value
 * 	   if the call fails
 */
int zrocks_read (uint64_t offset, void *buf, size_t size);

#ifdef __cplusplus
}; // closing brace for extern "C"
#endif

#endif // LIBZROCKS_H
