#include "pebble.h"
#include "num2words.h"

#define BUFFER_SIZE 86

static struct CommonWordsData {
  TextLayer *text_time_layer;
  TextLayer *text_date_layer;
  TextLayer *text_shadow_layer;
  TextLayer *text_date_shadow_layer;
  Layer *line_layer;
  Window *window;
  char buffer[BUFFER_SIZE];
  
} s_data;

static GColor8 s_main_colour;
static GColor8 s_high_colour;
static GColor8 s_dark_colour;

static AppSync s_sync;
static uint8_t s_sync_buffer[64];

enum WeatherKey {
  WEATHER_TEMPERATURE_KEY = 0x3
};

static void set_colour(uint8_t temp){
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
  window_set_background_color(s_data.window,s_main_colour);
  text_layer_set_text_color(s_data.text_time_layer, s_high_colour);
  text_layer_set_text_color(s_data.text_shadow_layer, s_dark_colour);
  text_layer_set_text_color(s_data.text_date_layer, s_high_colour);
  text_layer_set_text_color(s_data.text_date_shadow_layer, s_dark_colour);

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

static void line_layer_update_callback(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, s_high_colour);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void update_time(struct tm* t) {
  static char date_text[] = "Xxxxxxxxx 00";

  if (t->tm_min % 30 == 0){
	  request_weather();
  }

  fuzzy_time_to_words(t->tm_hour, t->tm_min, s_data.buffer, BUFFER_SIZE);
  text_layer_set_text(s_data.text_time_layer, s_data.buffer);
  text_layer_set_text(s_data.text_shadow_layer, s_data.buffer);
  
  strftime(date_text, sizeof(date_text), "%a %e %b", t);
  text_layer_set_text(s_data.text_date_layer, date_text);
  text_layer_set_text(s_data.text_date_shadow_layer, date_text);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
}

static void do_init(void) {
  s_data.window = window_create();
  const bool animated = true;
  window_stack_push(s_data.window, animated);

  window_set_background_color(s_data.window, s_main_colour);
  GFont timefont = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  GFont datefont = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  Layer *window_layer = window_get_root_layer(s_data.window);
  GRect frame = layer_get_frame(window_layer);

  s_data.text_date_layer = text_layer_create(GRect(2, 0, frame.size.w, 32));
  text_layer_set_text_color(s_data.text_date_layer, s_high_colour);
  text_layer_set_background_color(s_data.text_date_layer, GColorClear);
  text_layer_set_font(s_data.text_date_layer, datefont);
  
  s_data.text_date_shadow_layer = text_layer_create(GRect(4, 2, frame.size.w, 32));
  text_layer_set_text_color(s_data.text_date_shadow_layer, s_dark_colour);
  text_layer_set_background_color(s_data.text_date_shadow_layer, GColorClear);
  text_layer_set_font(s_data.text_date_shadow_layer, datefont);

  s_data.text_time_layer = text_layer_create(GRect(2, 35, frame.size.w, frame.size.h - 20));
  text_layer_set_text_color(s_data.text_time_layer, s_high_colour);
  text_layer_set_background_color(s_data.text_time_layer, GColorClear);
  text_layer_set_font(s_data.text_time_layer, timefont);
  
  s_data.text_shadow_layer = text_layer_create(GRect(4, 37, frame.size.w, frame.size.h - 20));
  text_layer_set_text_color(s_data.text_shadow_layer, s_dark_colour);
  text_layer_set_background_color(s_data.text_shadow_layer, GColorClear);
  text_layer_set_font(s_data.text_shadow_layer, timefont);
  
  layer_add_child(window_layer, text_layer_get_layer(s_data.text_date_shadow_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_data.text_shadow_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_data.text_date_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_data.text_time_layer));

  GRect line_frame = GRect(2, 33, 140, 2);
  s_data.line_layer = layer_create(line_frame);
  layer_set_update_proc(s_data.line_layer, line_layer_update_callback);
  layer_add_child(window_layer, s_data.line_layer);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  update_time(t);
  
  Tuplet initial_values[] = {
    TupletCString(WEATHER_TEMPERATURE_KEY, "0")
  };

  app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer), 
      initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL
  );

  request_weather();

  tick_timer_service_subscribe(MINUTE_UNIT, &handle_minute_tick);
  
}

static void do_deinit(void) {
  tick_timer_service_unsubscribe();
  text_layer_destroy(s_data.text_date_layer);
  text_layer_destroy(s_data.text_time_layer);
  text_layer_destroy(s_data.text_date_shadow_layer);
  text_layer_destroy(s_data.text_shadow_layer);
  layer_destroy(s_data.line_layer);
  window_destroy(s_data.window);
}

int main(void) {
  do_init();
  app_event_loop();
  do_deinit();
}