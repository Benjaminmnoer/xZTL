#include <omp.h>
#include <stdint.h>
#include <stdlib.h>
#include <libzrocks.h>
#include <xztl.h>
#include "CUnit/Basic.h"

/* Number of Objects */
#define TEST_N_BUFFERS 2

/* Number of random objects to read */
#define TEST_RANDOM_ID 2

/* Object Size */
#define TEST_BUFFER_SZ (1024 * 5) /* 5 KB */

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
