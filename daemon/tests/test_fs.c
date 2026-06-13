#include "test.h"
#include "fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TEMP_DIR getenv("TEMP")
#else
#define TEMP_DIR "/tmp"
#endif

static char s_tmpdir[1024];

/* ---- Path manipulation (pure string) ---- */

static void test_path_normalize(void)
{
    char p[] = "C:\\Users\\foo\\bar";
    spark_fs_path_normalize(p);
    ASSERT_STR_EQ(p, "C:/Users/foo/bar");

    char p2[] = "already/fine";
    spark_fs_path_normalize(p2);
    ASSERT_STR_EQ(p2, "already/fine");
}

static void test_path_join(void)
{
    char buf[256];

    ASSERT_EQ(spark_fs_path_join(buf, sizeof(buf), "/home/user", "projects"), 0);
    ASSERT_STR_EQ(buf, "/home/user/projects");

    ASSERT_EQ(spark_fs_path_join(buf, sizeof(buf), "/home/user/", "projects"), 0);
    ASSERT_STR_EQ(buf, "/home/user/projects");

    ASSERT_EQ(spark_fs_path_join(buf, sizeof(buf), "/home/user/", "/projects"), 0);
    ASSERT_STR_EQ(buf, "/home/user/projects");

    ASSERT_EQ(spark_fs_path_join(buf, sizeof(buf), "C:/Users", "docs"), 0);
    ASSERT_STR_EQ(buf, "C:/Users/docs");

    /* Buffer too small */
    ASSERT_EQ(spark_fs_path_join(buf, 5, "/home", "user"), -1);
}

static void test_path_parent(void)
{
    char buf[256];

    ASSERT_EQ(spark_fs_path_parent(buf, sizeof(buf), "/home/user/file.txt"), 0);
    ASSERT_STR_EQ(buf, "/home/user");

    ASSERT_EQ(spark_fs_path_parent(buf, sizeof(buf), "/home/user/"), 0);
    ASSERT_STR_EQ(buf, "/home");

    ASSERT_EQ(spark_fs_path_parent(buf, sizeof(buf), "/home"), 0);
    ASSERT_STR_EQ(buf, "/");

    ASSERT_EQ(spark_fs_path_parent(buf, sizeof(buf), "C:/Users/foo"), 0);
    ASSERT_STR_EQ(buf, "C:/Users");

    ASSERT_EQ(spark_fs_path_parent(buf, sizeof(buf), "C:/Users"), 0);
    ASSERT_STR_EQ(buf, "C:/");

    /* Root has no parent */
    ASSERT_EQ(spark_fs_path_parent(buf, sizeof(buf), "/"), -1);
    ASSERT_EQ(spark_fs_path_parent(buf, sizeof(buf), "C:/"), -1);
}

/* ---- Existence checks ---- */

static void test_dir_exists(void)
{
    ASSERT_EQ(spark_fs_dir_exists(s_tmpdir), 1);
    ASSERT_EQ(spark_fs_dir_exists("/nonexistent_xyz_12345"), 0);
    ASSERT_EQ(spark_fs_dir_exists(NULL), 0);
    ASSERT_EQ(spark_fs_dir_exists(""), 0);
}

static void test_file_exists(void)
{
    /* Create a temp file to test against */
    char path[1024];
    spark_fs_path_join(path, sizeof(path), s_tmpdir, "spark_fs_test_exist.txt");
    FILE *f = fopen(path, "w");
    ASSERT_TRUE(f != NULL);
    fprintf(f, "x");
    fclose(f);

    ASSERT_EQ(spark_fs_file_exists(path), 1);
    ASSERT_EQ(spark_fs_file_exists("/nonexistent_xyz_12345"), 0);
    ASSERT_EQ(spark_fs_file_exists(s_tmpdir), 0);

    spark_fs_remove(path);
}

/* ---- mkdir + remove ---- */

static void test_mkdir_p_and_remove(void)
{
    char dir[1024];
    spark_fs_path_join(dir, sizeof(dir), s_tmpdir, "spark_fs_test_dir/nested/deep");

    /* Clean up from possible previous run */
    char d1[1024], d2[1024], d3[1024];
    spark_fs_path_join(d1, sizeof(d1), s_tmpdir, "spark_fs_test_dir/nested/deep");
    spark_fs_path_join(d2, sizeof(d2), s_tmpdir, "spark_fs_test_dir/nested");
    spark_fs_path_join(d3, sizeof(d3), s_tmpdir, "spark_fs_test_dir");
    spark_fs_remove(d1);
    spark_fs_remove(d2);
    spark_fs_remove(d3);

    ASSERT_EQ(spark_fs_dir_exists(dir), 0);
    ASSERT_EQ(spark_fs_mkdir_p(dir), 0);
    ASSERT_EQ(spark_fs_dir_exists(dir), 1);

    remove(d1); remove(d2); remove(d3);
}

/* ---- List directory ---- */

struct list_ctx {
    int count;
    int found_self;
};

static void s_list_cb(const char *name, int is_dir, void *ctx)
{
    (void)is_dir;
    struct list_ctx *lc = (struct list_ctx *)ctx;
    lc->count++;
    if (strcmp(name, "test_fs.c") == 0)
        lc->found_self = 1;
}

static void test_iter_dir(void)
{
    struct list_ctx ctx = {0};
    ASSERT_EQ(spark_fs_iter_dir("tests", s_list_cb, &ctx), 0);
    ASSERT_TRUE(ctx.count > 0);
    ASSERT_EQ(ctx.found_self, 1);

    /* Non-existent dir returns error */
    ASSERT_EQ(spark_fs_iter_dir("/nonexistent_xyz_12345", s_list_cb, &ctx), -1);
}

static void test_list(void)
{
    spark_fs_listing_t ls;
    ASSERT_EQ(spark_fs_list("tests", &ls), 0);
    ASSERT_TRUE(ls.count > 0);

    int found = 0;
    for (int i = 0; i < ls.count; i++)
        if (strcmp(ls.entries[i].name, "test_fs.c") == 0) found = 1;
    ASSERT_EQ(found, 1);

    spark_fs_list_free(&ls);
    ASSERT_EQ(ls.entries == NULL, 1);
    ASSERT_EQ(ls.count, 0);

    /* Non-existent dir */
    ASSERT_EQ(spark_fs_list("/nonexistent_xyz_12345", &ls), -1);
}

/* ---- exe_dir and home ---- */

static void test_exe_dir(void)
{
    char buf[1024];
    ASSERT_EQ(spark_fs_exe_dir(buf, sizeof(buf)), 0);
    ASSERT_TRUE(buf[0] != '\0');
    ASSERT_EQ(spark_fs_dir_exists(buf), 1);
}

static void test_home(void)
{
    char buf[1024];
    ASSERT_EQ(spark_fs_home(buf, sizeof(buf)), 0);
    ASSERT_TRUE(buf[0] != '\0');
    ASSERT_EQ(spark_fs_dir_exists(buf), 1);
}

/* ---- Realpath ---- */

static void test_realpath(void)
{
    char buf[1024];
    /* Resolve tmpdir — should succeed and point to an existing dir */
    ASSERT_EQ(spark_fs_realpath(buf, sizeof(buf), s_tmpdir), 0);
    ASSERT_TRUE(buf[0] != '\0');
    ASSERT_EQ(spark_fs_dir_exists(buf), 1);

    ASSERT_EQ(spark_fs_realpath(buf, sizeof(buf), "/nonexistent_xyz_99999"), -1);
}

/* ---- Copy ---- */

static void test_copy(void)
{
    char src[1024], dst[1024];
    spark_fs_path_join(src, sizeof(src), s_tmpdir, "spark_fs_copy_src.txt");
    spark_fs_path_join(dst, sizeof(dst), s_tmpdir, "spark_fs_copy_dst.txt");

    FILE *f = fopen(src, "w");
    ASSERT_TRUE(f != NULL);
    fprintf(f, "hello spark_fs");
    fclose(f);

    ASSERT_EQ(spark_fs_copy(src, dst), 0);
    ASSERT_EQ(spark_fs_file_exists(dst), 1);

    f = fopen(dst, "r");
    ASSERT_TRUE(f != NULL);
    char buf[64] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    ASSERT_STR_EQ(buf, "hello spark_fs");

    spark_fs_remove(src);
    spark_fs_remove(dst);
}

int main(void)
{
    printf("test_fs\n");

    /* Resolve a temp directory that works on both platforms */
    const char *tmp = TEMP_DIR;
    if (!tmp || tmp[0] == '\0') tmp = ".";
    strncpy(s_tmpdir, tmp, sizeof(s_tmpdir) - 1);
    spark_fs_path_normalize(s_tmpdir);

    TEST_BEGIN();

    RUN_TEST(test_path_normalize);
    RUN_TEST(test_path_join);
    RUN_TEST(test_path_parent);
    RUN_TEST(test_dir_exists);
    RUN_TEST(test_file_exists);
    RUN_TEST(test_mkdir_p_and_remove);
    RUN_TEST(test_iter_dir);
    RUN_TEST(test_list);
    RUN_TEST(test_exe_dir);
    RUN_TEST(test_home);
    RUN_TEST(test_realpath);
    RUN_TEST(test_copy);

    TEST_END();
}
