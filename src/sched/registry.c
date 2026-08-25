#include <stddef.h>
#include <string.h>

#include "sched.h"

/* Each scheduler defines one of these in its own .c file. Adding an
   algorithm means writing the file, declaring it here, and adding one
   row to the table below. Nothing in main.c or the engine changes. */
extern scheduler_t fcfs_scheduler;
extern scheduler_t sjf_scheduler;
extern scheduler_t priority_np_scheduler;
static scheduler_t *registry[] = {
    &fcfs_scheduler,
    &sjf_scheduler,
    &priority_np_scheduler,
    NULL
};

scheduler_t *scheduler_lookup(const char *name)
{
    int i;

    for (i = 0; registry[i] != NULL; i++) {
        if (strcmp(registry[i]->name, name) == 0) {
            return registry[i];
        }
    }
    return NULL;
}

const char *scheduler_names(int index)
{
    int i;

    for (i = 0; registry[i] != NULL; i++) {
        if (i == index) {
            return registry[i]->name;
        }
    }
    return NULL;
}
