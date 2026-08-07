#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "lvgl.h"
#include "structs.h"

void create_ui_tabview(lv_obj_t *screen_root);
void update_ui_telemetry(const RaceCaptureTelemetry *data);

#endif /* DASHBOARD_UI_H */
