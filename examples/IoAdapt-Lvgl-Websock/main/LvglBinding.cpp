/** LvglBinding.cpp   Generated 2023-05-11
 *
 * Generated using ESP-Object-Messaging: tools/ui_bind.py. Do not edit manually.
 * 
 */        
#include "LvglHost.h"
#include "ui/ui.h"
#include "LvglBinding.h"

void LvglBindingInit(LvglHost& host)
{
    host.AddProducer("sample", ui_btnSample, BUTTON_CT, LV_EVENT_CLICKED);
    host.AddConsumer("sample", ui_btnSample, BUTTON_CT);
    host.AddConsumer("pressure", ui_lblPressure, LABEL_CT);
    host.AddProducer("zoomx", ui_arcZoomX, ARC_CT, LV_EVENT_VALUE_CHANGED);
    host.AddConsumer("zoomx", ui_arcZoomX, ARC_CT);
    host.AddProducer("zoomy", ui_sldZoomY, SLIDER_CT, LV_EVENT_VALUE_CHANGED);
    host.AddConsumer("zoomy", ui_sldZoomY, SLIDER_CT);
    host.AddProducer("schedule", ui_calSchedule, CALENDAR_CT, LV_EVENT_VALUE_CHANGED);
    host.AddConsumer("schedule", ui_calSchedule, CALENDAR_CT);
}
