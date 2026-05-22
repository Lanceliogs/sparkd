/*
 * fixture_bank.h - Fixture template bank
 *
 * Loads fixture templates from YAML bank files found in directories
 * specified by SPARK_FIXTURE_BANK_PATH (colon-separated) or the
 * default ~/.spark/fixtures/.
 *
 * Bank files must declare a unique bank id:
 *   bank:
 *     id: my-bank
 *     version: 1
 *
 * Templates are referenced by "bank-id:fixture-id" in project files.
 * Duplicate bank ids are skipped with a warning.
 */
#ifndef SPARK_FIXTURE_BANK_H
#define SPARK_FIXTURE_BANK_H

#include "fixture.h"

#define SPARK_FIXTURE_BANK_MAX 32
#define SPARK_FIXTURE_BANK_TEMPLATES_MAX 512
#define SPARK_FIXTURE_BANK_CHANNEL_ARENA_SIZE 2048

int  spark_fixture_bank_load(const char *search_paths);
int  spark_fixture_bank_reload(const char *search_paths);
const spark_fixture_t *spark_fixture_bank_find(const char *qualified_id);
void spark_fixture_bank_reset(void);

#endif
