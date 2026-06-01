/*
 * shutdown.h - Just sharing one function to the world
 *
 * Allows to shutdown the app from anywhere, especially
 * from the http handler.
 */

#ifndef SPARK_SHUTDOWN_H
#define SPARK_SHUTDOWN_H

void spark_request_shutdown(void);
int  spark_shutdown_requested(void);

#endif
