#ifdef TOUCH_CONTROLS
#include "djui.h"
#include "djui_panel.h"
#include "djui_panel_menu.h"
#include "pc/configfile.h"
#include "pc/controller/controller_touchscreen.h"

// Same things as djui panel player
static struct DjuiSlider *sTouchConfigSliderR = NULL;
static struct DjuiSlider *sTouchConfigSliderG = NULL;
static struct DjuiSlider *sTouchConfigSliderB = NULL;
static struct DjuiSlider *sTouchConfigSliderA = NULL;
static struct DjuiSlider *sTouchConfigSliderS = NULL;

static struct DjuiSelectionbox* sTouchConfigSelectionboxAnchor = NULL;

static void djui_panel_touch_controls_editor_update_values(struct DjuiBase* caller) {
    struct DjuiSelectionbox* selectionbox = (struct DjuiSelectionbox*)caller;
    bool enabled = *selectionbox->value != TOUCH_MOUSE ? true : false;
    djui_base_set_enabled(&sTouchConfigSelectionboxAnchor->base, enabled);
    djui_base_set_enabled(&sTouchConfigSliderR->base, enabled);
    djui_base_set_enabled(&sTouchConfigSliderG->base, enabled);
    djui_base_set_enabled(&sTouchConfigSliderB->base, enabled);
    djui_base_set_enabled(&sTouchConfigSliderA->base, enabled);
    djui_base_set_enabled(&sTouchConfigSliderS->base, enabled);

    sTouchConfigSelectionboxAnchor->value = &configControlElements[*selectionbox->value].anchor;
    sTouchConfigSliderR->value = &configControlElements[*selectionbox->value].r;
    sTouchConfigSliderG->value = &configControlElements[*selectionbox->value].g;
    sTouchConfigSliderB->value = &configControlElements[*selectionbox->value].b;
    sTouchConfigSliderA->value = &configControlElements[*selectionbox->value].a;
    sTouchConfigSliderS->value = &configControlElements[*selectionbox->value].size;
    djui_selectionbox_update_value(&sTouchConfigSelectionboxAnchor->base);
    djui_slider_update_value(&sTouchConfigSliderR->base);
    djui_slider_update_value(&sTouchConfigSliderG->base);
    djui_slider_update_value(&sTouchConfigSliderB->base);
    djui_slider_update_value(&sTouchConfigSliderA->base);
    djui_slider_update_value(&sTouchConfigSliderS->base);
}

static void djui_panel_touch_controls_editor_update_anchor(struct DjuiBase* caller) {
    struct DjuiSelectionbox* selectionbox = (struct DjuiSelectionbox*)caller;
    bool enabled = *selectionbox->value != CONTROL_ELEMENT_HIDDEN ? true : false;
    djui_base_set_enabled(&sTouchConfigSliderR->base, enabled);
    djui_base_set_enabled(&sTouchConfigSliderG->base, enabled);
    djui_base_set_enabled(&sTouchConfigSliderB->base, enabled);
    djui_base_set_enabled(&sTouchConfigSliderA->base, enabled);
    djui_base_set_enabled(&sTouchConfigSliderS->base, enabled);
}

static void djui_panel_touch_controls_editor_move(struct DjuiBase* caller) {
    struct DjuiThreePanel* panel = djui_panel_menu_create(DLANG(CONTROLS, CONTROLS), false);
    djui_base_set_size(&panel->base, DJUI_DEFAULT_PANEL_WIDTH * 1.3f, 0.15f);
    djui_base_set_alignment(&panel->base, DJUI_HALIGN_CENTER, DJUI_VALIGN_TOP);
    djui_base_set_color(&panel->base, 0, 0, 0, 0);
    djui_base_set_border_color(&panel->base, 0, 0, 0, 0);
    djui_panel_add(caller, panel, NULL);
}

static bool djui_panel_touch_controls_editor_leave_menu(UNUSED struct DjuiBase* base) {
    gInTouchConfig = false;
    return false;
}

static void djui_panel_touch_controls_editor_set_alpha(UNUSED struct DjuiBase* base) {
    u8 alpha = configControlElements[gSelectedTouchElement].a;
    for (s32 i = 0; i < TOUCH_COUNT; i++) {
        configControlElements[i].a = alpha;
    }
}

void djui_panel_touch_controls_editor_create(struct DjuiBase* caller) {
    struct DjuiThreePanel* panel = djui_panel_menu_create(DLANG(CONTROLS, CONTROLS), false);
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    djui_base_set_size(&panel->base, (DJUI_DEFAULT_PANEL_WIDTH * 1.3f) / 1.25f, 0.921f * 0.65f * 4.f);
    djui_base_set_alignment(&panel->base, DJUI_HALIGN_CENTER, DJUI_VALIGN_CENTER);
    djui_base_set_color(&panel->base, panel->base.color.r, panel->base.color.g, panel->base.color.b, panel->base.color.a - 50);
    djui_base_set_border_color(&panel->base, panel->base.borderColor.r, panel->base.borderColor.g, panel->base.borderColor.b, panel->base.borderColor.a - 50);

    gInTouchConfig = true;
    gSelectedTouchElement = TOUCH_MOUSE;
    {
        djui_checkbox_create(body, "Snap To Grid"/*DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_SNAP)*/, &configSnapTouch, NULL);

        char* buttonChoices[21] = { "Joystick", "None", "A Button", "B Button", "X Button", "Y Button", "Start Button", "L Trigger", "R Trigger", "Z Trigger", "C-Up", "C-Down", "C-Left", "C-Right", "Chat", "Playerlist", "Dpad-Up", "Dpad-Down", "Dpad-Left", "Dpad-Right", "Console" };
        djui_selectionbox_create(body, "Selected Button"/*DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_BUTTON)*/, buttonChoices, 21, &gSelectedTouchElement, djui_panel_touch_controls_editor_update_values);
        char* anchorChoices[4] = { "Left", "Right", "Center", "Hidden" /*DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_ANCHOR_LEFT), DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_ANCHOR_RIGHT), DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_ANCHOR_CENTER), DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_ANCHOR_HIDDEN)*/ };
        sTouchConfigSelectionboxAnchor = djui_selectionbox_create(body, "Anchor"/*DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_ANCHOR)*/, anchorChoices, 4, &configControlElements[gSelectedTouchElement].anchor, djui_panel_touch_controls_editor_update_anchor);

        sTouchConfigSliderR = djui_slider_create(body, DLANG(PLAYER, RED), &configControlElements[gSelectedTouchElement].r, 0, 255, NULL);
        djui_base_set_color(&sTouchConfigSliderR->rectValue->base, 255, 0, 0, 255);
        sTouchConfigSliderR->updateRectValueColor = false;

        sTouchConfigSliderG = djui_slider_create(body, DLANG(PLAYER, GREEN), &configControlElements[gSelectedTouchElement].g, 0, 255, NULL);
        djui_base_set_color(&sTouchConfigSliderG->rectValue->base, 0, 255, 0, 255);
        sTouchConfigSliderG->updateRectValueColor = false;

        sTouchConfigSliderB = djui_slider_create(body, DLANG(PLAYER, BLUE), &configControlElements[gSelectedTouchElement].b, 0, 255, NULL);
        djui_base_set_color(&sTouchConfigSliderB->rectValue->base, 0, 0, 255, 255);
        sTouchConfigSliderB->updateRectValueColor = false;
        
        sTouchConfigSliderA = djui_slider_create(body, DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_OPACITY), &configControlElements[gSelectedTouchElement].a, 0, 255, NULL);
        
        sTouchConfigSliderS = djui_slider_create(body, DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_SCALE), &configControlElements[gSelectedTouchElement].size, 1, 2, NULL);

        if (gSelectedTouchElement == TOUCH_MOUSE) {
            djui_base_set_enabled(&sTouchConfigSelectionboxAnchor->base, false);
            djui_base_set_enabled(&sTouchConfigSliderR->base, false);
            djui_base_set_enabled(&sTouchConfigSliderG->base, false);
            djui_base_set_enabled(&sTouchConfigSliderB->base, false);
            djui_base_set_enabled(&sTouchConfigSliderA->base, false);
            djui_base_set_enabled(&sTouchConfigSliderS->base, false);
        }

        {
            struct DjuiButton* applyAButton = djui_button_create(body, "Apply Opacity to All", DJUI_BUTTON_STYLE_NORMAL, djui_panel_touch_controls_editor_set_alpha);
            djui_base_set_size(&applyAButton->base, 1.0f, 36);
            struct DjuiButton* backButton = djui_button_left_create(body, DLANG(MENU, BACK), DJUI_BUTTON_STYLE_BACK, djui_panel_menu_back);
            djui_base_set_size(&backButton->base, 0.97f / 2.0f, 48);
            djui_base_set_alignment(&backButton->base, DJUI_HALIGN_LEFT, DJUI_VALIGN_BOTTOM);
            struct DjuiButton* moveButton = djui_button_right_create(body, "Move" /*DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS_MOVE)*/, DJUI_BUTTON_STYLE_BACK, djui_panel_touch_controls_editor_move);
            djui_base_set_size(&moveButton->base, 0.97f / 2.0f, 48);
            djui_base_set_alignment(&moveButton->base, DJUI_HALIGN_RIGHT, DJUI_VALIGN_BOTTOM);
        }
    }

    panel->on_back = djui_panel_touch_controls_editor_leave_menu; 
    djui_panel_add(caller, panel, NULL);
}
#endif