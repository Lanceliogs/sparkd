/*
 * env.h - Spark environment configuration
 *
 * Loads a .spark.env file (KEY=VALUE format) and provides a unified
 * getter that checks real environment variables first, then falls back
 * to the file values.
 *
 * Search order for .spark.env:
 *   1. Current working directory
 *   2. ~/.spark/.spark.env
 *
 * Real env vars always override file values.
 */
#ifndef SPARK_ENV_H
#define SPARK_ENV_H

int  spark_env_load(void);
const char *spark_env_get(const char *key);

#endif
