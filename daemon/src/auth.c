#include "auth.h"
#include "env.h"
#include "mongoose.h"

#include <string.h>
#include <stdio.h>

static char s_static_token[SPARK_AUTH_TOKEN_MAX_LEN] = {0};
static int  s_auth_enabled = 0;
static char s_ui_token[SPARK_AUTH_UUID_LEN + 1] = {0};

static void s_generate_uuid4(char *out)
{
    uint8_t bytes[16];
    mg_random(bytes, sizeof(bytes));

    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    snprintf(out, SPARK_AUTH_UUID_LEN + 1,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5],
        bytes[6], bytes[7],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}

/* Layer 1: Static token */

void spark_auth_init(void)
{
    const char *token = spark_env_get("SPARK_AUTH_TOKEN");
    if (token && token[0] != '\0')
    {
        snprintf(s_static_token, sizeof(s_static_token), "%s", token);
        s_auth_enabled = 1;
    }
    else
    {
        s_static_token[0] = '\0';
        s_auth_enabled = 0;
    }
}

int spark_auth_enabled(void)
{
    return s_auth_enabled;
}

int spark_auth_check_static(const char *token, size_t len)
{
    if (!s_auth_enabled) return 1;
    size_t expected_len = strlen(s_static_token);
    if (len != expected_len) return 0;
    return memcmp(token, s_static_token, len) == 0;
}

const char *spark_auth_get_static_token(void)
{
    return s_static_token;
}

/* Layer 2: Single UI token */

void spark_auth_ui_init(void)
{
    const char *env = spark_env_get("SPARK_UI_TOKEN");
    if (env && env[0] != '\0')
        snprintf(s_ui_token, sizeof(s_ui_token), "%s", env);
    else
        s_generate_uuid4(s_ui_token);
}

spark_role_t spark_auth_check_any(const char *token, size_t len)
{
    if (s_auth_enabled)
    {
        size_t static_len = strlen(s_static_token);
        if (len == static_len && memcmp(token, s_static_token, len) == 0)
            return SPARK_ROLE_ADMIN;
    }

    size_t ui_len = strlen(s_ui_token);
    if (ui_len > 0 && len == ui_len && memcmp(token, s_ui_token, len) == 0)
        return SPARK_ROLE_LIVE;

    return SPARK_ROLE_NONE;
}

const char *spark_auth_ui_token(void)
{
    return s_ui_token;
}

void spark_auth_ui_rotate(void)
{
    s_generate_uuid4(s_ui_token);
}
