#include "midi.h"
#include "midi_event.h"
#include "log.h"
#include "clock.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>

static volatile uint8_t running = 1;

static void on_signal(int sig)
{
    (void)sig;
    running = 0;
}

static const char *event_type_str(spark_midi_event_type_t type)
{
    switch (type)
    {
    case SPARK_MIDI_NOTE_ON:  return "NOTE_ON ";
    case SPARK_MIDI_NOTE_OFF: return "NOTE_OFF";
    case SPARK_MIDI_CC:       return "CC      ";
    default:                  return "UNKNOWN ";
    }
}

int main(int argc, char **argv)
{
    spark_log_init(SPARK_LOG_SILENT);

    if (argc < 2)
    {
        fprintf(stderr, "Usage: midi_monitor <device_pattern> [device_pattern ...]\n");
        fprintf(stderr, "  Use 'midi_list' to see available devices.\n");
        return 1;
    }

    int rc = spark_midi_init();
    if (rc != 0)
    {
        fprintf(stderr, "Failed to initialize MIDI (%d)\n", rc);
        return 1;
    }

    int opened = 0;
    for (int i = 1; i < argc; i++)
    {
        int id = spark_midi_find_device(argv[i]);
        if (id < 0)
        {
            fprintf(stderr, "Device not found: %s\n", argv[i]);
            continue;
        }
        rc = spark_midi_open(id);
        if (rc != 0)
        {
            fprintf(stderr, "Failed to open device %d (%s)\n", id, argv[i]);
            continue;
        }
        printf("Listening on: [%d] %s\n", id, argv[i]);
        opened++;
    }

    if (opened == 0)
    {
        fprintf(stderr, "No devices opened. Exiting.\n");
        spark_midi_destroy();
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    printf("--- Ctrl+C to stop ---\n");

    spark_midi_event_t events[64];
    while (running)
    {
        int n = spark_midi_poll(events, 64);
        for (int i = 0; i < n; i++)
        {
            spark_midi_event_t *e = &events[i];
            if (e->type == SPARK_MIDI_CC)
                printf("  %s  ch=%2d  cc=%3d  val=%3d\n",
                       event_type_str(e->type), e->channel, e->cc, e->value);
            else
                printf("  %s  ch=%2d  note=%3d  vel=%3d\n",
                       event_type_str(e->type), e->channel, e->note, e->velocity);
        }
        spark_clock_msleep(2);
    }

    printf("\nDone.\n");
    spark_midi_destroy();
    return 0;
}
