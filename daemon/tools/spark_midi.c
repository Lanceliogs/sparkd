#include "midi.h"
#include "midi_event.h"
#include "log.h"
#include "clock.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>

static volatile uint8_t s_running = 1;

static void s_on_signal(int sig)
{
    (void)sig;
    s_running = 0;
}

static void usage(void)
{
    fprintf(stderr, "spark-midi - MIDI debugging tool\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  spark-midi list                List MIDI input devices\n");
    fprintf(stderr, "  spark-midi listen <pattern>    Monitor MIDI input from device\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The pattern is a substring match against device names.\n");
    fprintf(stderr, "Multiple patterns can be given to listen on several devices.\n");
}

static int cmd_list(void)
{
    spark_midi_device_t devices[SPARK_MIDI_MAX_DEVICES];
    int count = spark_midi_list_devices(devices, SPARK_MIDI_MAX_DEVICES);

    if (count == 0)
    {
        printf("No MIDI input devices found.\n");
        return 0;
    }

    printf("MIDI input devices (%d):\n", count);
    for (int i = 0; i < count; i++)
        printf("  [%d] %s\n", devices[i].id, devices[i].name);

    return 0;
}

static const char *s_event_type_str(spark_midi_event_type_t type)
{
    switch (type)
    {
    case SPARK_MIDI_NOTE_ON:  return "NOTE_ON ";
    case SPARK_MIDI_NOTE_OFF: return "NOTE_OFF";
    case SPARK_MIDI_CC:       return "CC      ";
    default:                  return "UNKNOWN ";
    }
}

static int cmd_listen(int argc, char **argv)
{
    if (argc < 1)
    {
        fprintf(stderr, "spark-midi listen: missing device pattern\n");
        return 1;
    }

    int opened = 0;
    for (int i = 0; i < argc; i++)
    {
        int id = spark_midi_find_device(argv[i]);
        if (id < 0)
        {
            fprintf(stderr, "Device not found: %s\n", argv[i]);
            continue;
        }
        int rc = spark_midi_open(id);
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
        fprintf(stderr, "No devices opened.\n");
        return 1;
    }

    signal(SIGINT, s_on_signal);
    signal(SIGTERM, s_on_signal);

    printf("--- Ctrl+C to stop ---\n");

    spark_midi_event_t events[64];
    while (s_running)
    {
        int n = spark_midi_poll(events, 64);
        for (int i = 0; i < n; i++)
        {
            spark_midi_event_t *e = &events[i];
            if (e->type == SPARK_MIDI_CC)
                printf("  %s  ch=%2d  cc=%3d  val=%3d\n",
                       s_event_type_str(e->type), e->channel, e->cc, e->value);
            else
                printf("  %s  ch=%2d  note=%3d  vel=%3d\n",
                       s_event_type_str(e->type), e->channel, e->note, e->velocity);
        }
        spark_clock_msleep(2);
    }

    printf("\nDone.\n");
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

    int rc = spark_midi_init();
    if (rc != 0)
    {
        fprintf(stderr, "Failed to initialize MIDI (%d)\n", rc);
        return 1;
    }

    int ret;
    if (strcmp(argv[1], "list") == 0)
        ret = cmd_list();
    else if (strcmp(argv[1], "listen") == 0)
        ret = cmd_listen(argc - 2, argv + 2);
    else
    {
        fprintf(stderr, "spark-midi: unknown command '%s'\n", argv[1]);
        usage();
        ret = 1;
    }

    spark_midi_destroy();
    return ret;
}
