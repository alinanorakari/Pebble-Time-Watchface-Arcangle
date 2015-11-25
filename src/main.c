#include <pebble.h>

#define KEY_COLORS         0
#define KEY_INVERSE        1
#define KEY_BT_VIBE        2

#define ANTIALIASING       true
#define INVERSE            true

#define FINAL_RADIUS       90
#define HAND_WIDTH         6
#define HAND_MARGIN_OUTER  15
#define HAND_MARGIN_MIDDLE 32
#define HAND_MARGIN_INNER  40
#define GRID_SPACING       18

#define ANIMATION_DURATION 400
#define ANIMATION_DELAY    100

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *bg_canvas_layer, *s_canvas_layer;

static GPoint s_center;
static Time s_last_time;
static int colors = 1;
static bool s_animating = false, debug = false, inverse = false, btvibe = false;

static GColor gcolorbg, gcolorh, gcolort;

static void handle_colorchange() {
  switch (colors) {
    case 1: // greens
      gcolorh = GColorMediumSpringGreen;
      gcolort = GColorMidnightGreen;
      break;
    
    case 3: // blues
      gcolorh = GColorBlueMoon;
      gcolort = GColorOxfordBlue;
      break;
    
    case 2: // reds
      gcolorh = GColorRed;
      gcolort = GColorBulgarianRose;
      break;
    
    case 4: // greys
      gcolorh = GColorWhite;
      gcolort = GColorDarkGray;
      break;
    
    default:
      break;
  }
  if (inverse) {
    GColor tempcolor = gcolorh;
    gcolorh = gcolort;
    gcolort = tempcolor;
    switch (colors) {
      case 1: // greens
        gcolorh = GColorDarkGreen;
        gcolort = GColorMediumAquamarine;
        break;
      
      case 3: // blues
        gcolort = GColorPictonBlue;
        break;
      
      case 2: // reds
        gcolort = GColorMelon;
        break;
      
      case 4: // greys
        gcolort = GColorLightGray;
        break; 
      default:
        break;
    }
  }
  if (inverse) {
    gcolorbg = GColorWhite;
  } else {
    gcolorbg = GColorBlack;
  }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *colors_t = dict_find(iter, KEY_COLORS);
  Tuple *inverse_t = dict_find(iter, KEY_INVERSE);
  Tuple *btvibe_t = dict_find(iter, KEY_BT_VIBE);
    
  if(colors_t) {
    colors = colors_t->value->uint8;
    persist_write_int(KEY_COLORS, colors);
  }
  if(inverse_t && inverse_t->value->int8 > 0) {
    persist_write_bool(KEY_INVERSE, true);
    inverse = true;
  } else {
    persist_write_bool(KEY_INVERSE, false);
    inverse = false;
  }
  if(btvibe_t && btvibe_t->value->int8 > 0) {
    persist_write_bool(KEY_BT_VIBE, true);
    btvibe = true;
  } else {
    persist_write_bool(KEY_BT_VIBE, false);
    btvibe = false;
  }
  handle_colorchange();
  if(bg_canvas_layer) {
    layer_mark_dirty(bg_canvas_layer);
  }
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
  vibes_short_pulse();
}

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
  s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
  s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation *implementation, bool handlers) {
  Animation *anim = animation_create();
  animation_set_duration(anim, duration);
  animation_set_delay(anim, delay);
  animation_set_curve(anim, AnimationCurveEaseInOut);
  animation_set_implementation(anim, implementation);
  if(handlers) {
    animation_set_handlers(anim, (AnimationHandlers) {
      .started = animation_started,
      .stopped = animation_stopped
    }, NULL);
  }
  animation_schedule(anim);
}

/************************************ UI **************************************/

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Store time
  // dummy time in emulator
  if (debug) {
    s_last_time.hours = 11;
    s_last_time.minutes = tick_time->tm_sec;
  } else {
    s_last_time.hours = tick_time->tm_hour;
    s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
    s_last_time.minutes = tick_time->tm_min;
  }

  // Redraw
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
}

static void handle_bluetooth(bool connected) {
  if (btvibe && !connected) {
    static uint32_t const segments[] = { 200, 200, 50, 150, 200 };
    VibePattern pat = {
    	.durations = segments,
    	.num_segments = ARRAY_LENGTH(segments),
    };
    vibes_enqueue_custom_pattern(pat);
  }
}

static int32_t get_angle_for_minute(int minute) {
  // Progress through 60 minutes, out of 360 degrees
  return (minute * 360) / 60;
}

static int32_t get_angle_for_hour(int hour, int minute) {
  // Progress through 12 hours, out of 360 degrees
  return ((hour * 360) / 12)+(get_angle_for_minute(minute)/12);
}

static void bg_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, gcolorbg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_antialiased(ctx, ANTIALIASING);
  graphics_context_set_stroke_color(ctx, gcolort);
  graphics_context_set_stroke_width(ctx, 1);
  for (int gx=0; gx<bounds.size.h; gx += GRID_SPACING) {
    GPoint gxtop = (GPoint) { .x = gx, .y = 0 };
    GPoint gxbot = (GPoint) { .x = gx, .y = bounds.size.h };
    graphics_draw_line(ctx, gxtop, gxbot);
  }
  for (int gy=0; gy<bounds.size.w; gy += GRID_SPACING) {
    GPoint gxtop = (GPoint) { .y = gy, .x = 0 };
    GPoint gxbot = (GPoint) { .y = gy, .x = bounds.size.w };
    graphics_draw_line(ctx, gxtop, gxbot);
  }
}

static void update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect bounds_mo = grect_inset(bounds, GEdgeInsets(HAND_MARGIN_OUTER));
  GRect bounds_mi = grect_inset(bounds, GEdgeInsets(HAND_MARGIN_MIDDLE-1));
  GRect bounds_m = grect_inset(bounds, GEdgeInsets(HAND_MARGIN_MIDDLE));
  GRect bounds_ho = grect_inset(bounds, GEdgeInsets(HAND_MARGIN_MIDDLE+1));
  GRect bounds_hi = grect_inset(bounds, GEdgeInsets(HAND_MARGIN_INNER));

  // Use current time while animating
  Time mode_time = s_last_time;

  // Adjust for minutes through the hour
  float hour_deg = get_angle_for_hour(mode_time.hours, mode_time.minutes);
  float minute_deg = get_angle_for_minute(mode_time.minutes);

  GPoint minute_hand_outer = gpoint_from_polar(bounds_mo, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(minute_deg));
  GPoint minute_hand_inner = gpoint_from_polar(bounds_mi, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(minute_deg));
  GPoint hour_hand_outer = gpoint_from_polar(bounds_ho, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(hour_deg));
  GPoint hour_hand_inner = gpoint_from_polar(bounds_hi, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(hour_deg));
  
  graphics_context_set_stroke_color(ctx, gcolorh);
  graphics_context_set_stroke_width(ctx, HAND_WIDTH);
  graphics_draw_line(ctx, hour_hand_inner, hour_hand_outer);
  graphics_draw_line(ctx, minute_hand_inner, minute_hand_outer);
  if (minute_deg > hour_deg && minute_deg-hour_deg > 180) {
    hour_deg += 360;
  } else if (minute_deg < hour_deg && hour_deg-minute_deg > 180) {
    minute_deg += 360;
  }
  graphics_context_set_stroke_width(ctx, HAND_WIDTH+1);
  if (minute_deg < hour_deg) {
    graphics_draw_arc(ctx, bounds_m, GOvalScaleModeFitCircle,
                      DEG_TO_TRIGANGLE(minute_deg),
                      DEG_TO_TRIGANGLE(hour_deg));
  } else {
    graphics_draw_arc(ctx, bounds_m, GOvalScaleModeFitCircle,
                      DEG_TO_TRIGANGLE(hour_deg),
                      DEG_TO_TRIGANGLE(minute_deg));
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);
  s_center.x -= 1;
  s_center.y -= 1;
  
  if (persist_exists(KEY_COLORS)) {
    colors = persist_read_int(KEY_COLORS);
  } else {
    colors = 1;
  }
  if (persist_exists(KEY_INVERSE)) {
    inverse = persist_read_bool(KEY_INVERSE);
  } else {
    inverse = false;
  }
  if (persist_exists(KEY_BT_VIBE)) {
    btvibe = persist_read_bool(KEY_BT_VIBE);
  } else {
    btvibe = false;
  }

  bg_canvas_layer = layer_create(window_bounds);
  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(bg_canvas_layer, bg_update_proc);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, bg_canvas_layer);
  layer_add_child(bg_canvas_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(bg_canvas_layer);
  layer_destroy(s_canvas_layer);
}

/*********************************** App **************************************/

static void init() {
  srand(time(NULL));
  
  if (watch_info_get_model()==WATCH_INFO_MODEL_UNKNOWN) {
    debug = true;
  }

  time_t t = time(NULL);
  struct tm *time_now = localtime(&t);
  if (debug) {
    tick_handler(time_now, SECOND_UNIT);
  } else {
    tick_handler(time_now, MINUTE_UNIT);
  }

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_main_window, true);
  
  if (inverse) {
    gcolorbg = GColorWhite;
  } else {
    gcolorbg = GColorBlack;
  }
  
  gcolorh = GColorMediumSpringGreen;
  gcolort = GColorMidnightGreen;

  handle_colorchange();
  
  if (debug) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  }
  
  if (debug) {
    light_enable(true);
  }
  
  handle_bluetooth(connection_service_peek_pebble_app_connection());
  
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = handle_bluetooth
  });
  
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}


