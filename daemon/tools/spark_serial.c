#include "serial/serial.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

static void usage(void)
{
    fprintf(stderr, "spark-serial - Serial device debugging tool\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  spark-serial list              List USB-serial devices\n");
    fprintf(stderr, "  spark-serial find [tag]        Find DMX device (auto-detect)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The 'find' command searches for known DMX USB devices.\n");
    fprintf(stderr, "Optional tag filters by manufacturer: ftdi, enttec, eurolite\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  spark-serial list\n");
    fprintf(stderr, "  spark-serial find              Find any known DMX device\n");
    fprintf(stderr, "  spark-serial find enttec       Find Enttec devices only\n");
}

static int cmd_list(void)
{
    spark_serial_device_info_t devices[SPARK_SERIAL_MAX_DEVICES];
    int count = spark_serial_enumerate(devices, SPARK_SERIAL_MAX_DEVICES);

    if (count == 0)
    {
        printf("No USB-serial devices found.\n");
        return 0;
    }

    printf("USB-serial devices (%d):\n\n", count);
    for (int i = 0; i < count; i++)
    {
        spark_serial_device_info_t *d = &devices[i];
        bool known = spark_serial_is_known_dmx(d->vid, d->pid, NULL);

        printf("  [%d] %s\n", i, d->port);
        printf("      VID:PID  %04x:%04x%s\n", d->vid, d->pid,
               known ? " (known DMX device)" : "");
        if (d->description[0])
            printf("      Desc     %s\n", d->description);
        if (d->serial_number[0])
            printf("      Serial   %s\n", d->serial_number);
        printf("\n");
    }

    return 0;
}

static int cmd_find(const char *tag)
{
    printf("Searching for DMX device");
    if (tag)
        printf(" (filter: %s)", tag);
    printf("...\n\n");

    spark_serial_device_info_t info;
    int rc = spark_serial_find_dmx(tag, &info);

    if (rc != 0)
    {
        printf("No matching DMX device found.\n");
        if (tag)
            printf("Try without a tag: spark-serial find\n");
        return 1;
    }

    printf("Found DMX device:\n");
    printf("  Port:    %s\n", info.port);
    printf("  VID:PID: %04x:%04x\n", info.vid, info.pid);
    if (info.description[0])
        printf("  Desc:    %s\n", info.description);
    if (info.serial_number[0])
        printf("  Serial:  %s\n", info.serial_number);

    printf("\nUse in project YAML:\n");
    printf("  dmx:\n");
    printf("    device: %s\n", info.port);
    printf("    backend: open   # or 'pro' for DMX USB Pro\n");

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
    {
        usage();
        return argc < 2 ? 1 : 0;
    }

    spark_log_init(SPARK_LOG_SILENT);

    if (strcmp(argv[1], "list") == 0)
        return cmd_list();
    else if (strcmp(argv[1], "find") == 0)
        return cmd_find(argc > 2 ? argv[2] : NULL);
    else
    {
        fprintf(stderr, "spark-serial: unknown command '%s'\n", argv[1]);
        usage();
        return 1;
    }
}
