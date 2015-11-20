#include <pebble.h>

#define KEY_COLORS         0

#define ANTIALIASING       true

#define FINAL_RADIUS       88
#define HAND_WIDTH         6
#define TICK_RADIUS        3
#define HAND_MARGIN_OUTER  16
#define HAND_MARGIN_INNER  50
#define SHADOW_OFFSET      2
#define GRID_SPACING       18

#define ANIMATION_DURATION 400
#define ANIMATION_DELAY    100

typedef struct {
  int hours;
  int minutes;
} Time;

static Window *s_main_window;
static Layer *s_canvas_layer;

static GPoint s_center;
static Time s_last_time;
static int s_radius = 0, colors = 1;
static bool s_animating = false, debug = false;
static float anim_offset;

static GColor gcolorbg, gcolorh, gcolort;

static void handle_colorchange() {
  switch(colors) {
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
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *colors_t = dict_find(iter, KEY_COLORS);
    
  if(colors_t) {
    colors = colors_t->value->uint8;
    persist_write_int(KEY_COLORS, colors);
    handle_colorchange();
  }
  if(s_canvas_layer) {
    layer_mark_dirty(s_canvas_layer);
  }
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
    s_last_time.hours = 8;
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

static int32_t get_angle_for_minute(int minute) {
  // Progress through 60 minutes, out of 360 degrees
  return (minute * 360) / 60;
}

static int32_t get_angle_for_hour(int hour, int minute) {
  // Progress through 12 hours, out of 360 degrees
  return ((hour * 360) / 12)+(get_angle_for_minute(minute)/12);
}

static void update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, gcolorbg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_antialiased(ctx, ANTIALIASING);

  // Use current time while animating
  Time mode_time = s_last_time;

  // Adjust for minutes through the hour
  float hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
  float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
  float hour_deg = get_angle_for_hour(mode_time.hours, mode_time.minutes);
  float minute_deg = get_angle_for_minute(mode_time.minutes);
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);
  if (s_animating) {
	  hour_angle += anim_offset;
	  minute_angle -= anim_offset;
  }

  GPoint minute_hand_outer = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN_OUTER) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN_OUTER) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint minute_hand_inner = (GPoint) {
    .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * ((int32_t)HAND_MARGIN_INNER+1) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * ((int32_t)HAND_MARGIN_INNER+1) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand_outer = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - HAND_MARGIN_OUTER - (0.35 * s_radius)) / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - HAND_MARGIN_OUTER - (0.35 * s_radius)) / TRIG_MAX_RATIO) + s_center.y,
  };
  GPoint hour_hand_inner = (GPoint) {
    .x = (int16_t)(sin_lookup(hour_angle) * (int32_t)HAND_MARGIN_INNER / TRIG_MAX_RATIO) + s_center.x,
    .y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)HAND_MARGIN_INNER / TRIG_MAX_RATIO) + s_center.y,
  };
  
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


  if((s_radius - HAND_MARGIN_OUTER) > HAND_MARGIN_INNER) {
    if(s_radius > 2 * HAND_MARGIN_OUTER) {
      graphics_context_set_stroke_color(ctx, gcolorh);
      graphics_context_set_stroke_width(ctx, HAND_WIDTH);
      graphics_draw_line(ctx, hour_hand_inner, hour_hand_outer);
    }
    if(s_radius > HAND_MARGIN_OUTER) {
      graphics_context_set_stroke_color(ctx, gcolorh);
      graphics_context_set_stroke_width(ctx, HAND_WIDTH);
      graphics_draw_line(ctx, minute_hand_inner, minute_hand_outer);
    }
    GRect frame = grect_inset(bounds, GEdgeInsets(37));
    graphics_context_set_fill_color(ctx, gcolorh);
    if (minute_deg > hour_deg && minute_deg-hour_deg > 180) {
      hour_deg += 360;
    } else if (minute_deg < hour_deg && hour_deg-minute_deg > 180) {
      minute_deg += 360;
    }
    if (minute_deg < hour_deg) {
      graphics_fill_radial(ctx, frame, GOvalScaleModeFitCircle, HAND_WIDTH,
                           DEG_TO_TRIGANGLE(minute_deg-1),
                           DEG_TO_TRIGANGLE(hour_deg+1));
    } else {
      graphics_fill_radial(ctx, frame, GOvalScaleModeFitCircle, HAND_WIDTH,
                           DEG_TO_TRIGANGLE(hour_deg-1),
                           DEG_TO_TRIGANGLE(minute_deg+1));
    }
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  s_center = grect_center_point(&window_bounds);
  
  if (persist_exists(KEY_COLORS)) {
    colors = persist_read_int(KEY_COLORS);
  } else {
    colors = 1;
  }


  
  s_canvas_layer = layer_create(window_bounds);
  layer_set_update_proc(s_canvas_layer, update_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
}

/*********************************** App **************************************/

static int anim_percentage(AnimationProgress dist_normalized, int max) {
  return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation *anim, AnimationProgress dist_normalized) {
  s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);
  layer_mark_dirty(s_canvas_layer);
}

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
  
  gcolorbg = GColorBlack;
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
  
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());



  // Prepare animations
  AnimationImplementation radius_impl = {
    .update = radius_update
  };
  animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}


