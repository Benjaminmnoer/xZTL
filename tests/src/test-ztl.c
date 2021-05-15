#include <ztl-media.h>
#include <xztl.h>
#include <xztl-ztl.h>
#include <ztl.h>
#include "CUnit/Basic.h"

static const char **devname;

static void cunit_ztl_assert_ptr (char *fn, void *ptr)
{
    CU_ASSERT ((uint64_t) ptr != 0);
    if (!ptr)
	printf ("\n %s: ptr %p\n", fn, ptr);
}

static void cunit_ztl_assert_int (char *fn, uint64_t status)
{
    CU_ASSERT (status == 0);
    if (status)
	printf ("\n %s: %lx\n", fn, status);
}

static void cunit_ztl_assert_int_equal (char *fn, int value, int expected)
{
    CU_ASSERT_EQUAL (value, expected);
    if (value != expected)
	printf ("\n %s: value %d != expected %d\n", fn, value, expected);
}

static void test_ztl_init (void)
{
    int ret;

    ret = znd_media_register (*devname);
    cunit_ztl_assert_int ("znd_media_register", ret);
    if (ret)
	return;

    ret = xztl_media_init ();
    cunit_ztl_assert_int ("xztl_media_init", ret);
    if (ret)
	return;

    /* Register ZTL modules */
    ztl_zmd_register ();
    ztl_pro_register ();
    ztl_mpe_register ();
    ztl_map_register ();
    ztl_wca_register ();

    ret = ztl_init ();
    cunit_ztl_assert_int ("ztl_init", ret);
    if (ret)
	return;
}

static void test_ztl_exit (void)
{
    ztl_exit();
    xztl_media_exit();
}


static void test_ztl_pro_new_free (void)
{
    struct app_pro_addr *proe[2];
    struct app_zmd_entry *zmde;
    uint32_t nsec = 128;
    int i;

    proe[0] = ztl()->pro->new_fn (nsec, ZTL_PRO_TUSER, 0);
    cunit_ztl_assert_ptr ("ztl()->pro->new_fn", proe[0]);
    if (!proe[0])
	return;

    for (i = 0; i < proe[0]->naddr; i++) {
	zmde = ztl()->zmd->get_fn (proe[0]->grp, proe[0]->addr[i].g.zone, 0);
	cunit_ztl_assert_int_equal ("ztl()->pro->new_fn:zmd:wptr_inflight",
		    zmde->wptr_inflight, zmde->wptr + proe[0]->nsec[i]);
    }

    ztl()->pro->free_fn (proe[0]);

    proe[0] = NULL;

    for (int i = 0; i < 2; i++) {
	proe[i] = ztl()->pro->new_fn (nsec, ZTL_PRO_TUSER, 0);
	cunit_ztl_assert_ptr ("ztl()->pro->new_fn", proe[i]);
    }

    ztl()->pro->free_fn (proe[1]);
    proe[1] = NULL;

    proe[1] = ztl()->pro->new_fn (nsec, ZTL_PRO_TUSER, 0);
    cunit_ztl_assert_ptr ("ztl()->pro->new_fn", proe[1]);

    ztl()->pro->free_fn (proe[0]);
    ztl()->pro->free_fn (proe[1]);
    proe[0] = NULL;
    proe[1] = NULL;

    proe[0] = ztl()->pro->new_fn (nsec, ZTL_PRO_TUSER, 0);
    cunit_ztl_assert_ptr ("ztl()->pro->new_fn", proe[0]);

    ztl()->pro->free_fn (proe[0]);
}

static void test_ztl_map_upsert_read (void)
{
    struct app_map_entry entry;
    uint64_t id, val, count, interval, old;
    int ret;

    count    = 256 * 4096 - 1;
    interval = 1;

    for (id = 1; id <= count; id++) {
        entry.g.zone_id = id * interval / 4096;
        entry.g.zone_offset = id * interval % 4096;
	ret = ztl()->map->upsert_fn (id * interval, entry.addr, &old, 0);
	cunit_ztl_assert_int ("ztl()->map->upsert_fn", ret);
    }

    for (id = 1; id <= count; id++) {
	val = ztl()->map->read_fn (id * interval)->addr;
	cunit_ztl_assert_int_equal ("ztl()->map->read", val, id * interval);
    }

    id = 456789;
    val = 1234;
    old = 0;

    ret = ztl()->map->upsert_fn (id, val, &old, 0);
    cunit_ztl_assert_int ("ztl()->map->upsert_fn", ret);
    cunit_ztl_assert_int_equal ("ztl()->map->upsert_fn:old", old, id);

    old = ztl()->map->read_fn (id)->addr;
    cunit_ztl_assert_int_equal ("ztl()->map->read", old, val);
}

static int cunit_ztl_init (void)
{
    return 0;
}

static int cunit_ztl_exit (void)
{
    return 0;
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

    pSuite = CU_add_suite("Suite_ztl", cunit_ztl_init, cunit_ztl_exit);
    if (pSuite == NULL) {
	CU_cleanup_registry();
	return CU_get_error();
    }

    if ((CU_add_test (pSuite, "Initialize ZTL",
		      test_ztl_init) == NULL) ||
	(CU_add_test (pSuite, "New/Free prov offset",
		      test_ztl_pro_new_free ) == NULL) ||
	(CU_add_test (pSuite, "Upsert/Read mapping",
		      test_ztl_map_upsert_read ) == NULL) ||
        (CU_add_test (pSuite, "Close ZTL",
		      test_ztl_exit) == NULL)) {
	failed = 1;
	CU_cleanup_registry();
	return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    failed = CU_get_number_of_tests_failed();
    CU_cleanup_registry();

    return failed;
}
