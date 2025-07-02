#ifndef HEROWATCHER_DETECTOR_H
#define HEROWATCHER_DETECTOR_H

#include <util/platform.h>

// Forward declare struct from plugin
struct hero_watcher_data;


// Function declaration
void *hero_detection_thread(void *data);

#endif