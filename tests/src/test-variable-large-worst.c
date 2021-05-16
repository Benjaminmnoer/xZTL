/* This test case is to test variable size writes in xZTL, using ZRocks as an interface.
 * The test is designed for large object writes, which only surpass the sector boundary by a small amount, 
 * e.g., the worst case scenario for variable writes.
 */
#include <omp.h>
#include <stdint.h>
#include <stdlib.h>
#include <libzrocks.h>
#include <xztl.h>
#include "CUnit/Basic.h"

/* Number of Objects */
#define TEST_N_BUFFERS 1024

/* Object Size */
/* 12 MB - 4032 bytes, meaning the last sector is mainly padding.  */
#define TEST_BUFFER_SZ (1024*1024*12-4032) 

static uint8_t *wbuf[TEST_N_BUFFERS];
static uint8_t *rbuf[TEST_N_BUFFERS];

static const char **devname;

static void cunit_zrocks_assert_ptr (char *fn, void *ptr)
{
    CU_ASSERT ((uint64_t) ptr != 0);
    if (!ptr)
	printf ("\n %s: ptr %p\n", fn, ptr);
}

static void cunit_zrocks_assert_int (char *fn, uint64_t status)
{
    CU_ASSERT (status == 0);
    if (status)
	printf ("\n %s: %lx\n", fn, status);
}

static int cunit_zrocks_init (void)
{
    return 0;
}

static int cunit_zrocks_exit (void)
{
    return 0;
}

static void test_zrocks_init (void)
{
    int ret;

    ret = zrocks_init (*devname);
    cunit_zrocks_assert_int ("zrocks_init", ret);
}

static void test_zrocks_exit (void)
{
    zrocks_exit ();
}

static void test_zrocks_fill_buffer (uint32_t id)
{
    uint32_t byte;
    uint8_t value = 0x1;

    for (byte = 0; byte < TEST_BUFFER_SZ; byte += 16) {
	value += 0x1;
	memset (&wbuf[id][byte], value, 16);
    }
}

static int test_zrocks_check_buffer (uint32_t id, uint32_t off, uint32_t size)
{
    /*printf (" \nMem check:\n");
    for (int i = off; i < off + size; i++) {
        if (i % 16 == 0 && i)
	    printf("\n %d-%d ", i - (i%16), (i - (i%16)) + 16);
	printf (" %x/%x", wbuf[id][i], rbuf[id][i]);
    }
    printf("\n");
*/
    return memcmp (wbuf[id], rbuf[id], size);
}

static void test_zrocks_new (void)
{
    uint32_t ids;
    uint64_t id;
    uint32_t size;
    uint8_t level;
    int ret[TEST_N_BUFFERS];

    ids   = TEST_N_BUFFERS;
    size  = TEST_BUFFER_SZ;
    level = 0;

    #pragma omp parallel for
    for (id = 0; id < ids; id++) {

	/* Allocate DMA memory */
	wbuf[id] = xztl_media_dma_alloc (size);
	cunit_zrocks_assert_ptr ("xztl_media_dma_alloc", wbuf[id]);

	if (!wbuf[id])
	    continue;

	test_zrocks_fill_buffer (id);

	ret[id] = zrocks_new (id + 1, wbuf[id], size, level);
	cunit_zrocks_assert_int ("zrocks_new", ret[id]);
    }
}

static void test_zrocks_read (void)
{
    uint32_t ids, offset;
    uint64_t id;
    int ret[TEST_N_BUFFERS];
    size_t read_sz, size;

    ids = TEST_N_BUFFERS;
    read_sz = 1024 * 64; /* 64 KB */
    size = TEST_BUFFER_SZ;

    for (id = 0; id < ids; id++) {

	/* Allocate DMA memory */
	rbuf[id] = xztl_media_dma_alloc (size);
	cunit_zrocks_assert_ptr ("xztl_media_dma_alloc", rbuf[id]);
	if (!rbuf[id])
	    continue;

	memset (rbuf[id], 0x0, size);

	offset = 0;
	while (offset < size) {
	    ret[id] = zrocks_read_obj (id + 1, offset, rbuf[id] + offset, (size - offset < read_sz) ? size - offset : read_sz);
	    cunit_zrocks_assert_int ("zrocks_read_obj", ret[id]);
	    if (ret[id])
		printf ("Read error: ID %lu, offset %d, status: %x\n",
							id + 1, offset, ret[id]);
	    offset += read_sz;
	}

	ret[id] = test_zrocks_check_buffer (id, 0, TEST_BUFFER_SZ);
	cunit_zrocks_assert_int ("zrocks_read_obj:check", ret[id]);
	if (ret[id])
	    printf ("Corruption: ID %lu, corrupted: %d bytes\n", id + 1, ret[id]);

	xztl_media_dma_free (rbuf[id]);
    }
}

int main (int argc, const char **argv)
{
    int failed;

    if (argc < 2) {
	printf ("Please provide the device path. e.g. liou:/dev/nvme0n2\n");
	return -1;
    }

    devname = &argv[1];
    printf ("Device: %s\n", *devname);

    CU_pSuite pSuite = NULL;

    if (CUE_SUCCESS != CU_initialize_registry())
	return CU_get_error();

    pSuite = CU_add_suite("Suite_zrocks", cunit_zrocks_init, cunit_zrocks_exit);
    if (pSuite == NULL) {
	CU_cleanup_registry();
	return CU_get_error();
    }

    if ((CU_add_test (pSuite, "Initialize ZRocks",
		      test_zrocks_init) == NULL) ||
	(CU_add_test (pSuite, "ZRocks New",
		      test_zrocks_new) == NULL) ||
	(CU_add_test (pSuite, "ZRocks Read",
		      test_zrocks_read) == NULL) ||
        (CU_add_test (pSuite, "Close ZRocks",
		      test_zrocks_exit) == NULL)) {
	CU_cleanup_registry();
	return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    failed = CU_get_number_of_tests_failed();
    CU_cleanup_registry();

    return failed;
}
