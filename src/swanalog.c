
/*

Simple analog watch with date

 */

#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "yachtimermodel.h"


#define MY_UUID { 0x24, 0xD8, 0x92, 0xC9, 0xB1, 0xCB, 0x49, 0xC1, 0xBA, 0xCD, 0x19, 0x97, 0x11, 0x25, 0x9B, 0xE0 }
PBL_APP_INFO(MY_UUID, "Analog StopWatch", "MikeM", 0x1, 0x1, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_STANDARD_APP);


Window window;
AppContextRef app;

BmpContainer background_image_container;
TextLayer text_date_layer;
RotBmpContainer hour_hand_image_container;
RotBmpContainer minute_hand_image_container;

YachtTimer myYachtTimer;
int startappmode=WATCHMODE;

#define BUTTON_LAP BUTTON_ID_DOWN
#define BUTTON_RUN BUTTON_ID_SELECT
#define BUTTON_RESET BUTTON_ID_UP
#define TIMER_UPDATE 1

// The documentation claims this is defined, but it is not.
// Define it here for now.
#ifndef APP_TIMER_INVALID_HANDLE
    #define APP_TIMER_INVALID_HANDLE 0xDEADBEEF
#endif

// Actually keeping track of time
static AppTimerHandle update_timer = APP_TIMER_INVALID_HANDLE;
static int ticklen=0;

void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void toggle_mode(ClickRecognizerRef recognizer, Window *window);
void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window);
void stop_stopwatch();
void start_stopwatch();
void update_hand_positions();
void config_provider(ClickConfig **config, Window *window);
// Hook to ticks
void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie);

// Custom vibration pattern
const VibePattern start_pattern = {
  .durations = (uint32_t []) {100, 300, 300, 300, 100, 300},
  .num_segments = 6
};

/* -------------- TODO: Remove this and use Public API ! ------------------- */

// from src/core/util/misc.h

#define MAX(a,b) (((a)>(b))?(a):(b))

void start_stopwatch() {
    yachtimer_start(&myYachtTimer);

    // default start mode
    startappmode = yachtimer_getMode(&myYachtTimer);;

    // Up the resolution to do deciseconds
    if(update_timer != APP_TIMER_INVALID_HANDLE) {
        if(app_timer_cancel_event(app, update_timer)) {
            update_timer = APP_TIMER_INVALID_HANDLE;
        }
    }
    // Slow update down to once a second to save power
    ticklen = yachtimer_getTick(&myYachtTimer);
    update_timer = app_timer_send_event(app, yachtimer_getTick(&myYachtTimer), TIMER_UPDATE);

}
// Toggle stopwatch timer mode
void toggle_mode(ClickRecognizerRef recognizer, Window *window) {

          // Can only set to first three 
	  int mode=yachtimer_getMode(&myYachtTimer)+1;
          yachtimer_setMode(&myYachtTimer,mode);

	  // if beyond end mode set back to start
	  if(yachtimer_getMode(&myYachtTimer) != mode) yachtimer_setMode(&myYachtTimer,WATCHMODE);
	  update_hand_positions();
	    // Up the resolution to do deciseconds
	    if(update_timer != APP_TIMER_INVALID_HANDLE) {
		if(app_timer_cancel_event(app, update_timer)) {
		    update_timer = APP_TIMER_INVALID_HANDLE;
		}
	    }
	    // Slow update  or speed based on mode
	    // If WATCH we have no second hand so down to a minute§
	    ticklen = (yachtimer_getMode(&myYachtTimer) == WATCHMODE) ? ASECOND * 60:yachtimer_getTick(&myYachtTimer);
	    update_timer = app_timer_send_event(app, ticklen, TIMER_UPDATE);
}

void stop_stopwatch() {

    yachtimer_stop(&myYachtTimer);
    if(update_timer != APP_TIMER_INVALID_HANDLE) {
        if(app_timer_cancel_event(app, update_timer)) {
            update_timer = APP_TIMER_INVALID_HANDLE;
        }
    }
    // Slow update down to once a second to save power
    ticklen = yachtimer_getTick(&myYachtTimer);
    update_timer = app_timer_send_event(app, ticklen, TIMER_UPDATE); 
}
void toggle_stopwatch_handler(ClickRecognizerRef recognizer, Window *window) {
    if(yachtimer_isRunning(&myYachtTimer)) {
        stop_stopwatch();
    } else {
        start_stopwatch();
    }
}
void reset_stopwatch_handler(ClickRecognizerRef recognizer, Window *window) {

    yachtimer_reset(&myYachtTimer);

    switch(yachtimer_getMode(&myYachtTimer))
    {
        case STOPWATCH:
        case YACHTIMER:
        case COUNTDOWN:
            if(yachtimer_isRunning(&myYachtTimer))
            {
                 stop_stopwatch();
                 start_stopwatch();
            }
            else
            {
                stop_stopwatch();
            }
            update_hand_positions();

            break;
        default:
	    ;
            // if not in config mode won't do anything which makes this easy
            // config_watch(watchappmode,1);
    }
}

// From src/fw/ui/rotate_bitmap_layer.c

//! newton's method for floor(sqrt(x)) -> should always converge
static int32_t integer_sqrt(int32_t x) {
  if (x < 0) {
    ////    PBL_LOG(LOG_LEVEL_ERROR, "Looking for sqrt of negative number");
    return 0;
  }

  int32_t last_res = 0;
  int32_t res = (x + 1)/2;
  while (last_res != res) {
    last_res = res;
    res = (last_res + x / last_res) / 2;
  }
  return res;
}

void rot_bitmap_set_src_ic(RotBitmapLayer *image, GPoint ic) {
  image->src_ic = ic;

  // adjust the frame so the whole image will still be visible
  const int32_t horiz = MAX(ic.x, abs(image->bitmap->bounds.size.w - ic.x));
  const int32_t vert = MAX(ic.y, abs(image->bitmap->bounds.size.h - ic.y));

  GRect r = layer_get_frame(&image->layer);
  //// const int32_t new_dist = integer_sqrt(horiz*horiz + vert*vert) * 2;
  const int32_t new_dist = (integer_sqrt(horiz*horiz + vert*vert) * 2) + 1; //// Fudge to deal with non-even dimensions--to ensure right-most and bottom-most edges aren't cut off.

  r.size.w = new_dist;
  r.size.h = new_dist;
  layer_set_frame(&image->layer, r);

  r.origin = GPoint(0, 0);
  ////layer_set_bounds(&image->layer, r);
  image->layer.bounds = r;

  image->dest_ic = GPoint(new_dist / 2, new_dist / 2);

  layer_mark_dirty(&(image->layer));
}

/* ------------------------------------------------------------------------- */


void set_hand_angle(RotBmpContainer *hand_image_container, unsigned int hand_angle) {

  signed short x_fudge = 0;
  signed short y_fudge = 0;


  hand_image_container->layer.rotation =  TRIG_MAX_ANGLE * hand_angle / 360;

  //
  // Due to rounding/centre of rotation point/other issues of fitting
  // square pixels into round holes by the time hands get to 6 and 9
  // o'clock there's off-by-one pixel errors.
  //
  // The `x_fudge` and `y_fudge` values enable us to ensure the hands
  // look centred on the minute marks at those points. (This could
  // probably be improved for intermediate marks also but they're not
  // as noticable.)
  //
  // I think ideally we'd only ever calculate the rotation between
  // 0-90 degrees and then rotate again by 90 or 180 degrees to
  // eliminate the error.
  //
  if (hand_angle == 180) {
    x_fudge = -1;
  } else if (hand_angle == 270) {
    y_fudge = -1;
  }

  // (144 = screen width, 168 = screen height)
  hand_image_container->layer.layer.frame.origin.x = (144/2) - (hand_image_container->layer.layer.frame.size.w/2) + x_fudge;
  hand_image_container->layer.layer.frame.origin.y = (73) - (hand_image_container->layer.layer.frame.size.h/2) + y_fudge;

  layer_mark_dirty(&hand_image_container->layer.layer);
}


void update_hand_positions() {

  PblTm *t;
  static char date_text[] = "00 Xxxxxxxxx";

  t = yachtimer_getPblDisplayTime(&myYachtTimer);
  theTimeEventType event = yachtimer_triggerEvent(&myYachtTimer);

  if(event == MinorTime) vibes_double_pulse();
  if(event == MajorTime) vibes_enqueue_custom_pattern(start_pattern);

  // get_time(&t);

  if(yachtimer_getMode(&myYachtTimer) != WATCHMODE)
  {
  	set_hand_angle(&hour_hand_image_container, t->tm_min * 6); // ((t->tm_hour % 12) * 30) + (t->tm_min/2)); // ((((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  	set_hand_angle(&minute_hand_image_container, t->tm_sec * 6);
  }
  else
  {
  	set_hand_angle(&hour_hand_image_container, ((t->tm_hour % 12) * 30) + (t->tm_min/2)); // ((((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  	set_hand_angle(&minute_hand_image_container, t->tm_min * 6);
  }
  t = yachtimer_getPblLastTime(&myYachtTimer);

  string_format_time(date_text, sizeof(date_text), "%e %A", t);
  text_layer_set_text(&text_date_layer, date_text);

}

void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie)
{
   if(cookie == TIMER_UPDATE)
   {
  	yachtimer_tick(&myYachtTimer,ticklen);
	ticklen = yachtimer_getTick(&myYachtTimer);
	update_timer = app_timer_send_event(ctx, ticklen, TIMER_UPDATE);
  	update_hand_positions(); // TODO: Pass tick event
   }

}


void handle_init(AppContextRef ctx) {
  app = ctx;

  window_init(&window, "Simple Analog");
  window_stack_push(&window, true);
  window_set_fullscreen(&window, true);
  resource_init_current_app(&APP_RESOURCES);
  // Arrange for user input.
  window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);

  // Set up a layer for the static watch face background
  bmp_init_container(RESOURCE_ID_IMAGE_BACKGROUND, &background_image_container);
  layer_add_child(&window.layer, &background_image_container.layer.layer);


  // Set up a layer for the hour hand
  rotbmp_init_container(RESOURCE_ID_IMAGE_HOUR_HAND, &hour_hand_image_container);

  //hour_hand_image_container.layer.compositing_mode = GCompOpClear;

  rot_bitmap_set_src_ic(&hour_hand_image_container.layer, GPoint(4, 37));

  layer_add_child(&window.layer, &hour_hand_image_container.layer.layer);


  // Set up a layer for the minute hand
  rotbmp_init_container(RESOURCE_ID_IMAGE_MINUTE_HAND, &minute_hand_image_container);

 // minute_hand_image_container.layer.compositing_mode = GCompOpClear;

  rot_bitmap_set_src_ic(&minute_hand_image_container.layer, GPoint(2, 56));

  layer_add_child(&window.layer, &minute_hand_image_container.layer.layer);


  text_layer_init(&text_date_layer, window.layer.frame);
  text_layer_set_text_color(&text_date_layer, GColorWhite);
  text_layer_set_background_color(&text_date_layer, GColorClear);
  layer_set_frame(&text_date_layer.layer, GRect(0, 150, 144, 22));
  text_layer_set_font(&text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PIX_14)));
  text_layer_set_text_alignment(&text_date_layer, GTextAlignmentCenter);
  layer_add_child(&window.layer, &text_date_layer.layer);
  
  
  // Set up a layer for the second hand
 yachtimer_init(&myYachtTimer,startappmode);
 yachtimer_setConfigTime(&myYachtTimer, ASECOND * 60 * 10);


  update_hand_positions();

/* 
  // Setup the black and white circle in the centre of the watch face
  // (We use a bitmap rather than just drawing it because it means not having
  // to stuff around with working out the circle center etc.)
  rotbmp_pair_init_container(RESOURCE_ID_IMAGE_CENTER_CIRCLE_WHITE, RESOURCE_ID_IMAGE_CENTER_CIRCLE_BLACK,
			     &center_circle_image_container);

  // TODO: Do this properly with a GRect().
  // (144 = screen width, 168 = screen height)
  center_circle_image_container.layer.layer.frame.origin.x = (144/2) - (center_circle_image_container.layer.layer.frame.size.w/2);
  center_circle_image_container.layer.layer.frame.origin.y = (168/2) - (center_circle_image_container.layer.layer.frame.size.h/2);


  layer_add_child(&window.layer, &center_circle_image_container.layer.layer); */

}
void config_provider(ClickConfig **config, Window *window) {
    config[BUTTON_RUN]->click.handler = (ClickHandler)toggle_stopwatch_handler;
    config[BUTTON_LAP]->click.handler = (ClickHandler) toggle_mode;
    config[BUTTON_RESET]->click.handler = (ClickHandler)reset_stopwatch_handler;
//    config[BUTTON_LAP]->click.handler = (ClickHandler)lap_time_handler;
//    config[BUTTON_LAP]->long_click.handler = (ClickHandler)handle_display_lap_times;
//    config[BUTTON_LAP]->long_click.delay_ms = 700;
    (void)window;
}


void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  bmp_deinit_container(&background_image_container);
  rotbmp_deinit_container(&hour_hand_image_container);
  rotbmp_deinit_container(&minute_hand_image_container);
}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .timer_handler = &handle_timer

  };
  app_event_loop(params, &handlers);
}
