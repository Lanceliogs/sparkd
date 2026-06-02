#include "test.h"
#include "cres.h"
#include "ui_resources.h"

#include <string.h>

void test_cres_table_count(void)
{
    ASSERT_TRUE(cres_table_count >= 3);
}

void test_cres_find_index_html(void)
{
    CResEntry *e = cres_find(cres_table, cres_table_count, "../ui/dist/index.html");
    ASSERT_TRUE(e != NULL);
}

void test_cres_find_nonexistent(void)
{
    CResEntry *e = cres_find(cres_table, cres_table_count, "does/not/exist.txt");
    ASSERT_TRUE(e == NULL);
}

void test_cres_load_raw(void)
{
    CResEntry *e = cres_find(cres_table, cres_table_count, "../ui/dist/index.html");
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(cres_load(e), 0);
    ASSERT_TRUE(e->data != NULL);
    ASSERT_TRUE(e->size > 0);
}

void test_cres_content_starts_with_doctype(void)
{
    CResEntry *e = cres_find(cres_table, cres_table_count, "../ui/dist/index.html");
    ASSERT_TRUE(e != NULL);
    ASSERT_EQ(cres_load(e), 0);
    ASSERT_TRUE(e->size > 9);
    ASSERT_TRUE(memcmp(e->data, "<!DOCTYPE", 9) == 0);
}

void test_cres_load_compressed(void)
{
    /* Find a compressed entry (has cdata set) */
    CResEntry *found = NULL;
    for (size_t i = 0; i < cres_table_count; i++)
    {
        if (cres_table[i].cdata != NULL)
        {
            found = &cres_table[i];
            break;
        }
    }
    ASSERT_TRUE(found != NULL);
    ASSERT_TRUE(found->data == NULL);
    ASSERT_EQ(cres_load(found), 0);
    ASSERT_TRUE(found->data != NULL);
    ASSERT_TRUE(found->size > 0);
    ASSERT_TRUE(found->size > found->csize);
    cres_unload(found);
}

void test_cres_mime_html(void)
{
    CResEntry *e = cres_find(cres_table, cres_table_count, "../ui/dist/index.html");
    ASSERT_TRUE(e != NULL);
    ASSERT_STR_EQ(cres_mime(e), "text/html");
}

void test_cres_mime_css(void)
{
    CResEntry *found = NULL;
    for (size_t i = 0; i < cres_table_count; i++)
    {
        const char *name = cres_table[i].name;
        size_t len = strlen(name);
        if (len > 4 && strcmp(name + len - 4, ".css") == 0)
        {
            found = &cres_table[i];
            break;
        }
    }
    ASSERT_TRUE(found != NULL);
    ASSERT_STR_EQ(cres_mime(found), "text/css");
}

void test_cres_mime_js(void)
{
    CResEntry *found = NULL;
    for (size_t i = 0; i < cres_table_count; i++)
    {
        const char *name = cres_table[i].name;
        size_t len = strlen(name);
        if (len > 3 && strcmp(name + len - 3, ".js") == 0)
        {
            found = &cres_table[i];
            break;
        }
    }
    ASSERT_TRUE(found != NULL);
    ASSERT_STR_EQ(cres_mime(found), "application/javascript");
}

void test_cres_load_all(void)
{
    ASSERT_EQ(cres_load_all(cres_table, cres_table_count), 0);
    for (size_t i = 0; i < cres_table_count; i++)
        ASSERT_TRUE(cres_table[i].data != NULL);
    cres_free_all(cres_table, cres_table_count);
}

int main(void)
{
    TEST_BEGIN();
    RUN_TEST(test_cres_table_count);
    RUN_TEST(test_cres_find_index_html);
    RUN_TEST(test_cres_find_nonexistent);
    RUN_TEST(test_cres_load_raw);
    RUN_TEST(test_cres_content_starts_with_doctype);
    RUN_TEST(test_cres_load_compressed);
    RUN_TEST(test_cres_mime_html);
    RUN_TEST(test_cres_mime_css);
    RUN_TEST(test_cres_mime_js);
    RUN_TEST(test_cres_load_all);
    TEST_END();
}
