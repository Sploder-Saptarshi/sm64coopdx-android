#ifdef TOUCH_CONTROLS
#include "djui.h"
#include "djui_panel.h"
#include "djui_panel_menu.h"
#include "djui_panel_touch_controls_editor.h"
#include "pc/configfile.h"
#include "pc/controller/controller_touchscreen.h"

void djui_panel_touch_controls_create(struct DjuiBase* caller) {
    struct DjuiThreePanel* panel = djui_panel_menu_create(DLANG(TOUCH_CONTROLS, TOUCH_CONTROLS), false);
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    {
        djui_button_create(body, DLANG(TOUCH_CONTROLS, TOUCH_EDIT_BINDS), DJUI_BUTTON_STYLE_NORMAL, djui_panel_touch_controls_editor_create);

        djui_checkbox_create(body, DLANG(TOUCH_CONTROLS, TOUCH_AUTOHIDE), &configAutohideTouch, NULL);
        djui_checkbox_create(body, DLANG(TOUCH_CONTROLS, TOUCH_SLIDE_TOUCH), &configSlideTouch, NULL);
        djui_checkbox_create(body, DLANG(TOUCH_CONTROLS, TOUCH_IGNORE_PHANTOMS), &configPhantomTouch, NULL);

        djui_button_create(body, DLANG(MENU, BACK), DJUI_BUTTON_STYLE_BACK, djui_panel_menu_back);
    }

    djui_panel_add(caller, panel, NULL);
}
#endif