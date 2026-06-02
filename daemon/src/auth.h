#ifndef SPARK_AUTH_H
#define SPARK_AUTH_H

#include <stddef.h>

#define SPARK_AUTH_TOKEN_MAX_LEN 128
#define SPARK_AUTH_UUID_LEN 36

typedef enum {
    SPARK_ROLE_NONE = 0,
    SPARK_ROLE_LIVE,
    SPARK_ROLE_ADMIN,
} spark_role_t;

/*
 * Layer 1: Static infrastructure token (SPARK_AUTH_TOKEN).
 * Call after spark_env_load(). If env is unset, auth is disabled.
 */
void spark_auth_init(void);
int  spark_auth_enabled(void);
int  spark_auth_check_static(const char *token, size_t len);
const char *spark_auth_get_static_token(void);

/*
 * Layer 2: Single dynamic UI token (live access).
 * SPARK_AUTH_TOKEN grants admin; the UI token grants live.
 */
void spark_auth_ui_init(void);
spark_role_t spark_auth_check_any(const char *token, size_t len);
const char *spark_auth_ui_token(void);
void spark_auth_ui_rotate(void);

#endif
