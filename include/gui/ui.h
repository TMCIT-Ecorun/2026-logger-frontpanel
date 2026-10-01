#ifndef FRONTPANEL_GUI_UI_H
#define FRONTPANEL_GUI_UI_H

#include "lvgl.h"
#include "structs.h"
#include "gui/platform.h"

void gui_init(lv_obj_t *screen_root, const GuiPlatform *platform);
void gui_update_telemetry(const RaceCaptureTelemetry *data);

#endif /* FRONTPANEL_GUI_UI_H */
