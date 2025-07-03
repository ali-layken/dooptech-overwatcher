#include "herowatcher_detector.h"
#include "herowatcher_plugin.h"

void *hero_detection_thread(void *data)
{
    struct hero_watcher_data *filter = (struct hero_watcher_data *)data;
    blog(LOG_DEBUG, "[%s] Starting hero detection thread!", __func__);

    os_sleep_ms(17000);

    blog(LOG_DEBUG, "[%s] Hero detection thread done", __func__);
    os_atomic_store_bool(&filter->hero_detection_running, false);
    return NULL;
}