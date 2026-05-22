/*
 * mg_helpers.h - Thin convenience wrappers around Mongoose JSON and HTTP APIs
 *
 * Header-only. Include where needed in any tool or the daemon that links
 * mongoose.o. Provides:
 *   - mg_json_get_str_buf(): extract JSON string into a caller-owned buffer
 *   - mg_json_reply(): send a JSON HTTP response with CORS headers
 *   - mg_method_is(): compare request method (case-insensitive)
 */
#ifndef SPARK_MG_HELPERS_H
#define SPARK_MG_HELPERS_H

#include "mongoose.h"

#include <stdlib.h>
#include <string.h>

static inline int mg_json_get_str_buf(struct mg_str json, const char *path,
                                      char *buf, size_t buf_size)
{
    char *s = mg_json_get_str(json, path);
    if (!s) return 0;
    size_t len = strlen(s);
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    free(s);
    return (int)len;
}

static inline void mg_json_reply(struct mg_connection *c, int status, const char *body)
{
    mg_http_reply(c, status,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%s", body);
}

static inline bool mg_method_is(struct mg_http_message *hm, const char *method)
{
    return mg_strcasecmp(hm->method, mg_str(method)) == 0;
}

static inline void mg_cors_preflight(struct mg_connection *c)
{
    mg_http_reply(c, 204,
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n",
        "");
}

#endif
