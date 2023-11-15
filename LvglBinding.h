#pragma once


enum ControlType {
    SCREEN_CT,
    ARC_CT,
    BUTTON_CT,
    LABEL_CT,
    TEXTAREA_CT,
    CALENDAR_CT,
    CHECKBOX_CT,
    COLORWHEEL_CT,
    DROPDOWN_CT,
    ROLLER_CT,
    IMGBUTTON_CT,
    KEYBOARD_CT,
    SLIDER_CT,
    SWITCH_CT,
};

class LvglHost;

void LvglBindingInit(LvglHost& host);
void LvglGroupInit(lv_group_t* group);
