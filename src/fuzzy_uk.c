#include <pebble.h>
#include <num2words.h>
  
#define BUFFER_SIZE 86
  
static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_shadow_layer;
static TextLayer *s_date_layer;
static TextLayer *s_shadate_layer;
static Layer *s_canvas_layer;
static Layer *s_line_layer;
static char buffer[BUFFER_SIZE];

static GColor s_main_colour;
static GColor s_high_colour;
static GColor s_dark_colour;

static uint8_t s_charge;

static AppSync s_sync;
static uint8_t s_sync_buffer[64];

enum WeatherKey {
  WEATHER_TEMPERATURE_KEY = 0x3
};

static void set_colour(uint8_t temp){
#ifdef PBL_COLOR
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting temp: %d", temp);
  if(temp > 100){
    //error, or apocalypse. Either way, black.
    s_main_colour = GColorDarkGray;
    s_high_colour = GColorWhite;
    s_dark_colour = GColorBlack;
  }else if(temp > 25){
    //red
    s_main_colour = GColorSunsetOrange;
    s_high_colour = GColorPastelYellow;
    s_dark_colour = GColorRed;
  }else if(temp > 20){
    //dark orange?
    s_main_colour = GColorChromeYellow;
    s_high_colour = GColorPastelYellow;
    s_dark_colour = GColorOrange;
  }else if(temp > 15){
    //orange
    s_main_colour = GColorRajah;
    s_high_colour = GColorPastelYellow;
    s_dark_colour = GColorOrange;
  }else if(temp > 10){
    //green
    s_main_colour = GColorScreaminGreen;
    s_high_colour = GColorWhite;
    s_dark_colour = GColorKellyGreen;
  }else if(temp > 5){
    //blue
    s_main_colour = GColorBlueMoon;
    s_high_colour = GColorCeleste;
    s_dark_colour = GColorOxfordBlue;
  }else if(temp > 0){
    //blue
    s_main_colour = GColorVeryLightBlue;
    s_high_colour = GColorWhite;
    s_dark_colour = GColorOxfordBlue;
  }else{
    //white
    s_main_colour = GColorPictonBlue;
    s_high_colour = GColorWhite;
    s_dark_colour = GColorOxfordBlue;
  }
#else
  s_main_colour = GColorBlack;
  s_high_colour = GColorWhite;
  s_dark_colour = GColorBlack;
#endif  
  
  window_set_background_color(s_main_window,s_main_colour);
  text_layer_set_text_color(s_time_layer, s_high_colour);
  text_layer_set_text_color(s_shadow_layer, s_dark_colour);
  text_layer_set_text_color(s_date_layer, s_high_colour);
  text_layer_set_text_color(s_shadate_layer, s_dark_colour);
}

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case WEATHER_TEMPERATURE_KEY:
      // App Sync keeps new_tuple in s_sync_buffer, so we may use it directly
      set_colour(new_tuple->value->uint8);
      break;
  }
}

static void request_weather(void) {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (!iter) {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Error creating outbound message");
    return;
  }

  int value = 1;
  dict_write_int(iter, 1, &value, sizeof(int), true);
  dict_write_end(iter);

  app_message_outbox_send();
}

static void update_graphics(Layer *lyr, GContext *ctx) {
  GRect bounds = layer_get_bounds(s_canvas_layer);
  graphics_context_set_fill_color(ctx, s_dark_colour);
  graphics_fill_rect(ctx, GRect(0,0,((bounds.size.w * s_charge) / 100) + 2,7),0,GCornerNone);
  graphics_context_set_fill_color(ctx, s_high_colour);
  
  // Draw the battery level
  graphics_fill_rect(ctx, GRect(0,0,(bounds.size.w * s_charge) / 100,5),0,GCornerNone);
  
}

static void line_layer_update_callback(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, s_high_colour);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void update_time() {
  // Get a tm structure
  static char date_text[] = "Xxxxxxxxx 00";
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  int minutes = tick_time->tm_min;
  int hours = tick_time->tm_hour % 12;
  if(minutes % 30 == 0){
    // request weather every 30 minutes
    request_weather();
  }

  fuzzy_time_to_words(hours, minutes, buffer, BUFFER_SIZE);
  text_layer_set_text(s_time_layer, buffer);
  text_layer_set_text(s_shadow_layer, buffer);
  
  strftime(date_text, sizeof(date_text), "%a %e %b", tick_time);
  text_layer_set_text(s_date_layer, date_text);
  text_layer_set_text(s_shadate_layer, date_text);
}

static void main_window_load(Window *window) {
  
  s_main_colour = GColorBlack;
  s_high_colour = GColorWhite;
  s_dark_colour = GColorBlack;
  
  window_set_background_color(window, s_main_colour);

  GFont timefont = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  GFont datefont = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  Layer *window_layer = window_get_root_layer(window);
  GRect frame = layer_get_frame(window_layer);

  s_date_layer = text_layer_create(GRect(2, 4, frame.size.w, 32));
  text_layer_set_text_color(s_date_layer, s_high_colour);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_font(s_date_layer, datefont);
  
  s_shadate_layer = text_layer_create(GRect(4, 6, frame.size.w, 32));
  text_layer_set_text_color(s_shadate_layer, s_dark_colour);
  text_layer_set_background_color(s_shadate_layer, GColorClear);
  text_layer_set_font(s_shadate_layer, datefont);

  s_time_layer = text_layer_create(GRect(2, 39, frame.size.w, frame.size.h - 20));
  text_layer_set_text_color(s_time_layer, s_high_colour);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_font(s_time_layer, timefont);
  
  s_shadow_layer = text_layer_create(GRect(4, 41, frame.size.w, frame.size.h - 20));
  text_layer_set_text_color(s_shadow_layer, s_dark_colour);
  text_layer_set_background_color(s_shadow_layer, GColorClear);
  text_layer_set_font(s_shadow_layer, timefont);

  layer_add_child(window_layer, text_layer_get_layer(s_shadate_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_shadow_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  GRect line_frame = GRect(2, 35, 140, 2);
  s_line_layer = layer_create(line_frame);
  layer_set_update_proc(s_line_layer, line_layer_update_callback);
  layer_add_child(window_layer, s_line_layer);
  
  // Create a Graphics Layer and set the update_proc
  s_canvas_layer = layer_create(GRect(0, 0, 144, 100));
  layer_set_update_proc(s_canvas_layer, update_graphics);
  
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
  
  // initialise battery display
  
  BatteryChargeState charge = battery_state_service_peek();
  s_charge = charge.charge_percent;
  
  // Make sure the time is displayed from the start
  update_time();
  
  Tuplet initial_values[] = {
    TupletCString(WEATHER_TEMPERATURE_KEY, "0")
  };

  app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer), 
      initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL
  );

  request_weather();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_shadow_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_shadate_layer);
  layer_destroy(s_canvas_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void battery_handler(BatteryChargeState charge) {
  s_charge = charge.charge_percent;
}
  
static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  app_message_open(64, 64);
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
  app_sync_deinit(&s_sync);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

