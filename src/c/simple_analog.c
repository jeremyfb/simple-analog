#include "simple_analog.h"

#include "pebble.h"

static Window *s_window;
static Layer *s_simple_bg_layer, *s_ww_layer, *s_hands_layer;
static TextLayer *s_ww_label;

static GPath *s_tick_paths[NUM_CLOCK_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;
static char s_ww_buffer[9];

static void draw_tick(Layer *layer, GContext *ctx, int8_t num) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  
  const int16_t face_radius = PBL_IF_ROUND_ELSE((bounds.size.w / 2)-5, bounds.size.w / 2);
  int16_t tick_length = 10;
  int32_t tick_angle = TRIG_MAX_ANGLE * num /12;
  
  if (num % 3 == 0) {
    tick_length = 15;
  }
  GPoint tick_inner = {
    .x = (int16_t)(sin_lookup(tick_angle) * ((int32_t)face_radius - tick_length) / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(tick_angle) * ((int32_t)face_radius - tick_length) / TRIG_MAX_RATIO) + center.y,
  };
  GPoint tick_outer = {
    .x = (int16_t)(sin_lookup(tick_angle) * (int32_t)face_radius / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(tick_angle) * (int32_t)face_radius / TRIG_MAX_RATIO) + center.y,
  };
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 5);
  graphics_draw_line(ctx, tick_inner, tick_outer);

}
static void bg_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlue);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorWhite);
  /*
  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    const int x_offset = PBL_IF_ROUND_ELSE(18, 0);
    const int y_offset = PBL_IF_ROUND_ELSE(6, 0);
    gpath_move_to(s_tick_paths[i], GPoint(x_offset, y_offset));
    gpath_draw_filled(ctx, s_tick_paths[i]);
  }
  */
  for (int i = 0; i < 12; ++i) {
    draw_tick(layer, ctx, i);
  }
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  /*
  //const int16_t second_hand_length = PBL_IF_ROUND_ELSE((bounds.size.w / 2) - 19, bounds.size.w / 2);
  //int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;

  GPoint second_hand = {
    .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
  };
  

  // second hand
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, second_hand, center);
  */
  
  // minute/hour hand
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_draw_filled(ctx, s_minute_arrow);
  gpath_draw_outline(ctx, s_minute_arrow);

  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_context_set_stroke_color(ctx, GColorRed);
  gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_arrow);
  gpath_draw_outline(ctx, s_hour_arrow);

  // dot in the middle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 2, bounds.size.h / 2 - 2, 5, 5), 0, GCornerNone);
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  struct tm jan1;
  struct tm *ptrJan1 = NULL;
  time_t then = 0;

  int doy = t->tm_yday + 1;  // Jan 1 = 1, Jan 2 = 2, etc...
  int dow = t->tm_wday;     // Sun = 0, Mon = 1, etc...
  int dowJan1; // find out first of year's day
  
  memset(&jan1, '\0', sizeof(jan1));
  jan1.tm_year = t->tm_year;
  then = mktime(&jan1);
  ptrJan1 = localtime(&then);
  
  dowJan1 = ptrJan1->tm_wday;
  
  int weekNum = ((doy + 6) / 7);   
  if (dow <= dowJan1)                 // adjust for being after Saturday of week #1
    ++weekNum;
  snprintf(s_ww_buffer, sizeof(s_ww_buffer), "WW-%d", weekNum );
  //strftime(s_ww_buffer, sizeof(s_ww_buffer), "WW-%V", t);
  text_layer_set_text(s_ww_label, s_ww_buffer);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(s_window));
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_simple_bg_layer = layer_create(bounds);
  layer_set_update_proc(s_simple_bg_layer, bg_update_proc);
  layer_add_child(window_layer, s_simple_bg_layer);

  s_ww_layer = layer_create(bounds);
  layer_set_update_proc(s_ww_layer, date_update_proc);
  layer_add_child(window_layer, s_ww_layer);

  s_ww_label = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(0, 114, bounds.size.w, 30),
    GRect(0, 114, bounds.size.w, 30)));
  text_layer_set_text_alignment(s_ww_label, GTextAlignmentCenter);
  text_layer_set_text(s_ww_label, s_ww_buffer);
  text_layer_set_background_color(s_ww_label, GColorClear);
  text_layer_set_text_color(s_ww_label, GColorWhite);
  text_layer_set_font(s_ww_label, fonts_get_system_font(FONT_KEY_GOTHIC_24));

  layer_add_child(s_ww_layer, text_layer_get_layer(s_ww_label));

  /*
  s_num_label = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(90, 114, 18, 20),
    GRect(73, 114, 18, 20)));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorBlack);
  text_layer_set_text_color(s_num_label, GColorWhite);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));

  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));
  */

  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_simple_bg_layer);
  layer_destroy(s_ww_layer);

  text_layer_destroy(s_ww_label);
  //text_layer_destroy(s_num_label);

  layer_destroy(s_hands_layer);
}

static void init() {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  s_ww_buffer[0] = '\0';
  //s_num_buffer[0] = '\0';

  // init hand paths
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);

  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    s_tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
  }

  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
}

static void deinit() {
  gpath_destroy(s_minute_arrow);
  gpath_destroy(s_hour_arrow);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_destroy(s_tick_paths[i]);
  }

  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
