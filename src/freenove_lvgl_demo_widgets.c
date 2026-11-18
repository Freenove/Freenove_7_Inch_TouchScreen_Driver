/**
 * @file freenove_lvgl_demo_widgets.c
 * @brief Compile LVGL widgets demo from upstream lvgl/demos without copying into lvgl/src.
 *
 * Must be a .c translation unit: upstream demos use C99 void* conversions and
 * nested designated initializers that g++ rejects when pulled into a .cpp.
 *
 * Enabled only when the sketch passes -DFREENOVE_LVGL_DEMO_WIDGETS=1
 * (see examples/06.LvglArduino/build_opt.h). Other Freenove examples leave
 * this translation unit empty.
 *
 * Requires sibling install: Arduino/libraries/lvgl next to this library.
 */

#if defined(FREENOVE_LVGL_DEMO_WIDGETS) && (FREENOVE_LVGL_DEMO_WIDGETS)

/* Shared demo helpers (lv_demo_args_init, etc.) — required by lv_demo_widgets(). */
#include "../../lvgl/demos/lv_demos.c"

#include "../../lvgl/demos/widgets/lv_demo_widgets.c"
#include "../../lvgl/demos/widgets/lv_demo_widgets_components.c"
#include "../../lvgl/demos/widgets/lv_demo_widgets_profile.c"
#include "../../lvgl/demos/widgets/lv_demo_widgets_analytics.c"
#include "../../lvgl/demos/widgets/lv_demo_widgets_shop.c"
#include "../../lvgl/demos/widgets/assets/img_lvgl_logo.c"
#include "../../lvgl/demos/widgets/assets/img_clothes.c"
#include "../../lvgl/demos/widgets/assets/img_demo_widgets_avatar.c"
#include "../../lvgl/demos/widgets/assets/img_demo_widgets_needle.c"

#endif /* FREENOVE_LVGL_DEMO_WIDGETS */
