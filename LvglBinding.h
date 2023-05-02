#pragma once


enum ControlType {
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

void ui_binding_init(LvglHost& host);
