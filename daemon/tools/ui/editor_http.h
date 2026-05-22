#ifndef EDITOR_HTTP_H
#define EDITOR_HTTP_H

#include "mongoose.h"
#include <stdbool.h>

void editor_http_init(const char *bank_paths);
bool editor_http_handle(struct mg_connection *c, struct mg_http_message *hm);

#endif
