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

#include <stdlib.h>
#include <pthread.h>
#include <xztl.h>
#include <xztl-media.h>
#include <xztl-ztl.h>
#include <xztl-mempool.h>
#include <ztl-media.h>
#include <libzrocks.h>
#include <libxnvme.h>
#include <omp.h>

#define ZROCKS_DEBUG 		1
#define ZROCKS_BUF_ENTS 	128
#define ZROCKS_MAX_READ_SZ	(128 * ZNS_ALIGMENT) /* 512 KB */

extern struct xztl_core core;

/* Remove this lock if we find a way to get a thread ID starting from 0 */
static pthread_spinlock_t zrocks_mp_spin;

void *zrocks_alloc (size_t size)
{
    uint64_t phys;
    return xztl_media_dma_alloc (size, &phys);
}

void zrocks_free (void *ptr)
{
    xztl_media_dma_free (ptr);
}

static int __zrocks_write (struct xztl_io_ucmd *ucmd,
			uint64_t id, void *buf, size_t size, uint16_t level)
{
    uint32_t misalign;
    size_t new_sz, alignment;

    alignment = ZNS_ALIGMENT * ZTL_WCA_SEC_MCMD_MIN;
    misalign = size % alignment;

    new_sz = (misalign != 0) ? size + (alignment - misalign) : size;

    if (ZROCKS_DEBUG)
	log_infoa ("zrocks (write): ID %lu, level %d, size %lu, new size %lu, "
		"aligment %lu, misalign %d\n", id, level, size, new_sz,
		 alignment, misalign);

    ucmd->prov_type = level;

    ucmd->id        = id;
    ucmd->buf       = buf;
    ucmd->size      = new_sz;
    ucmd->status    = 0;
    ucmd->completed = 0;
    ucmd->callback  = NULL;
    ucmd->prov      = NULL;
    ucmd->opcode    = XZTL_USER_WRITE;

    if (ztl()->wca->submit_fn (ucmd))
	return -1;

    /* Wait for asynchronous command */
    while (!ucmd->completed) {
	usleep (1);
    }

    xztl_stats_inc (XZTL_STATS_APPEND_BYTES_U, size);
    xztl_stats_inc (XZTL_STATS_APPEND_UCMD, 1);

    return 0;
}

int zrocks_new (uint64_t id, void *buf, size_t size, uint16_t level)
{
    struct xztl_io_ucmd ucmd;
    int ret;

    if (ZROCKS_DEBUG)
	printf ("zrocks (write_obj): ID %lu, level %d, size %lu\n",
							    id, level, size);

    ucmd.app_md = 0;
    ret = __zrocks_write (&ucmd, id, buf, size, level);

    return (!ret) ? ucmd.status : ret;
}

int zrocks_write (void *buf, size_t size, uint16_t level,
				    struct zrocks_map **map, uint16_t *pieces)
{
    struct xztl_io_ucmd ucmd;
    struct zrocks_map *list;
    int ret, off_i;

    if (ZROCKS_DEBUG)
	log_infoa ("zrocks (write): level %d, size %lu\n", level, size);

    ucmd.app_md = 1;
    ret = __zrocks_write (&ucmd, 0, buf, size, level);

    if (ret)
	return ret;

    if (ucmd.status)
	return ucmd.status;

    list = zrocks_alloc (sizeof(struct zrocks_map) * ucmd.noffs);
    if (!list)
	return -1;

    for (off_i = 0; off_i < ucmd.noffs; off_i++) {
	list[off_i].g.offset = (uint64_t) ucmd.moffset[off_i];
	list[off_i].g.nsec   = ucmd.msec[off_i];
	list[off_i].g.multi  = 1;
    }

    *map = list;
    *pieces = ucmd.noffs;

    return 0;
}

int zrocks_read_obj (uint64_t id, uint64_t offset, void *buf, size_t size)
{
    int ret;
    uint64_t objsec_off;
    struct xztl_io_ucmd ucmd;

    if (ZROCKS_DEBUG)
	log_infoa ("zrocks (read_obj): ID %lu, off %lu, size %lu\n",
							id, offset, size);

    ucmd.opcode = XZTL_USER_READ;
    ucmd.app_md = 1;
    ucmd.id = id;
    ucmd.obj_off = offset;
    ucmd.buf = buf;
    ucmd.size = size;
    ucmd.callback = NULL;

    ret = ztl()->wca->submit_fn(&ucmd);
    if (ret){
        log_erra ("zrocks: Read failure. ID %lu, off 0x%lx, sz %lu. ret %d",
                                    id, offset, size, ret);
        return ret;
    }

    /* Wait for asynchronous command */
    while (!ucmd.completed) {
	usleep (1);
    }

    return 0;
}

int zrocks_read (uint64_t offset, void *buf, uint64_t size)
{
    int ret;
    struct xztl_io_ucmd ucmd;

    if (ZROCKS_DEBUG)
	log_infoa ("zrocks (read): off %lu, size %lu\n", offset, size);

    ucmd.opcode = XZTL_USER_READ;
    ucmd.app_md = 0;
    ucmd.offset = offset;
    ucmd.size = size;
    ucmd.buf = buf;
    ucmd.callback = NULL;

    ret = ztl()->wca->submit_fn(&ucmd);
    
    if (ret){
	log_erra ("zrocks: Read failure. off %lu, sz %lu. ret %d",
						    	    offset, size, ret);
        return ret;
    }

    /* Wait for asynchronous command */
    while (!ucmd.completed) {
	usleep (1);
    }
    return 0;
}

int zrocks_delete (uint64_t id)
{
    uint64_t old;

    return ztl()->map->upsert_fn (id, 0, &old, 0);
}

int zrocks_trim (struct zrocks_map *map, uint16_t level)
{
    struct app_zmd_entry *zmd;
    struct app_group *grp;
    int ret;

    if (ZROCKS_DEBUG) log_infoa ("zrocks (trim): (0x%lu/%d)\n",
					(uint64_t) map->g.offset, map->g.nsec);

    /* We use a single group for now */
    grp = ztl()->groups.get_fn (0);

    zmd = ztl()->zmd->get_fn (grp, map->g.offset, 1);
    xztl_atomic_int32_update (&zmd->ndeletes, zmd->ndeletes + 1);

    if (zmd->npieces == zmd->ndeletes) {

	ret = ztl()->pro->finish_zn_fn (grp, zmd->addr.g.zone, level);

	if (!ret && ztl()->pro->put_zone_fn (grp, zmd->addr.g.zone)) {
	    log_erra ("zrocks-trim: Failed to return zone to provisioning. "
						"ID %d", zmd->addr.g.zone);
	}
    }

    return 0;
}

int zrocks_exit (void)
{
    pthread_spin_destroy (&zrocks_mp_spin);
    xztl_mempool_destroy (ZROCKS_MEMORY, 0);
    return xztl_exit ();
}

int zrocks_init (const char *dev_name)
{
    int ret;

    /* Add libznd media layer */
    xztl_add_media (znd_media_register);

    /* Add the ZTL modules */
    ztl_zmd_register ();
    ztl_pro_register ();
    ztl_mpe_register ();
    ztl_map_register ();
    ztl_wca_register ();

    if (pthread_spin_init (&zrocks_mp_spin, 0))
	return -1;

    ret = xztl_init (dev_name);
    if (ret) {
	pthread_spin_destroy (&zrocks_mp_spin);
	return -1;
    }

    if (xztl_mempool_create (ZROCKS_MEMORY,
			     0,
			     ZROCKS_BUF_ENTS,
			     ZROCKS_MAX_READ_SZ + ZNS_ALIGMENT,
			     zrocks_alloc,
			     zrocks_free)) {
	xztl_exit ();
	pthread_spin_destroy (&zrocks_mp_spin);
    }

    return ret;
}
