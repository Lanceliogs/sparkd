#include "midi.h"
#include "log.h"

#include <stdio.h>

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);

    int rc = spark_midi_init();
    if (rc != 0)
    {
        fprintf(stderr, "Failed to initialize MIDI (%d)\n", rc);
        return 1;
    }

    spark_midi_device_t devices[SPARK_MIDI_MAX_DEVICES];
    int count = spark_midi_list_devices(devices, SPARK_MIDI_MAX_DEVICES);

    if (count == 0)
    {
        printf("No MIDI input devices found.\n");
    }
    else
    {
        printf("MIDI input devices (%d):\n", count);
        for (int i = 0; i < count; i++)
            printf("  [%d] %s\n", devices[i].id, devices[i].name);
    }

    spark_midi_destroy();
    return 0;
}
