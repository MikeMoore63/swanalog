/*

Simple analog watch with date

 */

#include <pebble.h>
#include "yachtimermodel.h"


// #define MY_UUID { 0x24, 0xD8, 0x92, 0xC9, 0xB1, 0xCB, 0x49, 0xC1, 0xBA, 0xCD, 0x19, 0x97, 0x11, 0x25, 0x9B, 0xE0 }
// PBL_APP_INFO(MY_UUID, "Analog StopWatch", "MikeM", 0x1, 0x3, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_STANDARD_APP);


Window *window;

BitmapLayer *background_image_container;
GBitmap *background_image;
TextLayer *text_date_layer;
RotBitmapLayer *hour_hand_image_container;
GBitmap *hour_hand_image;
RotBitmapLayer *minute_hand_image_container;
GBitmap *minute_hand_image;

YachtTimer *myYachtTimer;
int startappmode=WATCHMODE;

#define BUTTON_LAP BUTTON_ID_DOWN
#define BUTTON_RUN BUTTON_ID_SELECT
#define BUTTON_RESET BUTTON_ID_UP
#define TIMER_UPDATE 1
#define MODES 4 // Number of watch types stopwatch, coutdown, yachttimer, watch
#define TICKREMOVE 5

int ticks=0;

BitmapLayer *modeImages[MODES];
GBitmap *mImages[MODES];

struct modresource {
	int mode;
	int resourceid;
} mapModeImage[MODES] = {
	   { WATCHMODE, RESOURCE_ID_IMAGE_WATCH },
           { STOPWATCH, RESOURCE_ID_IMAGE_STOPWATCH },
	   { YACHTIMER, RESOURCE_ID_IMAGE_YACHTTIMER },
	   { COUNTDOWN, RESOURCE_ID_IMAGE_COUNTDOWN} };


// The documentation claims this is defined, but it is not.
// Define it here for now.
#ifndef APP_TIMER_INVALID_HANDLE
    #define APP_TIMER_INVALID_HANDLE 0xDEADBEEF
#endif

// Actually keeping track of time
static AppTimer *update_timer = NULL;
static int ticklen=0;

void toggle_stopwatch_handler(ClickRecognizerRef recognizer, void *data);
void toggle_mode(ClickRecognizerRef recognizer, void *data);
void reset_stopwatch_handler(ClickRecognizerRef recognizer, void *data);
void stop_stopwatch();
void start_stopwatch();
void update_hand_positions();
void config_provider( void *context);
// Hook to ticks
void handle_timer(  void *data);

// Custom vibration pattern
const VibePattern start_pattern = {
  .durations = (uint32_t []) {100, 300, 300, 300, 100, 300},
  .num_segments = 6
};

/* -------------- TODO: Remove this and use Public API ! ------------------- */

// from src/core/util/misc.h

#define MAX(a,b) (((a)>(b))?(a):(b))

void start_stopwatch() {
    yachtimer_start(myYachtTimer);

    // default start mode
    startappmode = yachtimer_getMode(myYachtTimer);;

    // Up the resolution to do deciseconds
    if(update_timer != NULL) {
        app_timer_cancel( update_timer);
        update_timer = NULL;
    }
    // Slow update down to once a second to save power
    ticklen = yachtimer_getTick(myYachtTimer);
    static uint32_t cookie = TIMER_UPDATE;
    update_timer = app_timer_register( yachtimer_getTick(myYachtTimer), handle_timer,  &cookie);

}
// Toggle stopwatch timer mode
void toggle_mode(ClickRecognizerRef recognizer, void *data) {

          // Can only set to first three 
	  int mode=yachtimer_getMode(myYachtTimer)+1;
          yachtimer_setMode(myYachtTimer,mode);

	  // if beyond end mode set back to start
	  if(yachtimer_getMode(myYachtTimer) != mode) yachtimer_setMode(myYachtTimer,WATCHMODE);
	  update_hand_positions();
	    // Up the resolution to do deciseconds
	    if(update_timer != NULL) {
		app_timer_cancel( update_timer);
		update_timer = NULL;
	    }

	  for (int i=0;i<MODES;i++)
	  {
		layer_set_hidden( (Layer *)modeImages[i], ((yachtimer_getMode(myYachtTimer) == mapModeImage[i].mode)?false:true));
	  }
	  ticks = 0;
	  static uint32_t cookie = TIMER_UPDATE;
	    ticklen = yachtimer_getTick(myYachtTimer);
	    update_timer = app_timer_register(ticklen, handle_timer, &cookie);
}

void stop_stopwatch() {

    yachtimer_stop(myYachtTimer);
    if(update_timer != NULL) {
        app_timer_cancel( update_timer);
        update_timer = NULL;
    }
    // Slow update down to once a second to save power
    ticklen = yachtimer_getTick(myYachtTimer);
    static uint32_t cookie = TIMER_UPDATE;
    update_timer = app_timer_register( ticklen, handle_timer, &cookie); 
}
void toggle_stopwatch_handler(ClickRecognizerRef recognizer, void *data) {
    if(yachtimer_isRunning(myYachtTimer)) {
        stop_stopwatch();
    } else {
        start_stopwatch();
    }
}
void reset_stopwatch_handler(ClickRecognizerRef recognizer, void *data) {

    yachtimer_reset(myYachtTimer);

    switch(yachtimer_getMode(myYachtTimer))
    {
        case STOPWATCH:
        case YACHTIMER:
        case COUNTDOWN:
            if(yachtimer_isRunning(myYachtTimer))
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




void set_hand_angle(RotBitmapLayer *hand_image_container, unsigned int hand_angle) {

  signed short x_fudge = 0;
  signed short y_fudge = 0;


  rot_bitmap_layer_set_angle(hand_image_container, TRIG_MAX_ANGLE * hand_angle / 360);	
  // hand_image_container->layer.rotation = TRIG_MAX_ANGLE * hand_angle / 360;

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
  GRect frame = layer_get_frame((Layer *)hand_image_container);
  frame.origin.x = (144/2) - (frame.size.w/2) + x_fudge;
  frame.origin.y = (73) - (frame.size.h/2) + y_fudge;
  layer_set_frame((Layer *)hand_image_container,frame);

  layer_mark_dirty((Layer *)hand_image_container);
}


void update_hand_positions() {

  struct tm  *t;
  static char date_text[] = "00 Xxxxxxxxx";

  t = yachtimer_getPblDisplayTime(myYachtTimer);
  theTimeEventType event = yachtimer_triggerEvent(myYachtTimer);

  if(event == MinorTime) vibes_double_pulse();
  if(event == MajorTime) vibes_enqueue_custom_pattern(start_pattern);

  // get_time(&t);

  if(yachtimer_getMode(myYachtTimer) != WATCHMODE)
  {
  	set_hand_angle(hour_hand_image_container, t->tm_min * 6); // ((t->tm_hour % 12) * 30) + (t->tm_min/2)); // ((((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  	set_hand_angle(minute_hand_image_container, t->tm_sec * 6);
  }
  else
  {
  	set_hand_angle(hour_hand_image_container, ((t->tm_hour % 12) * 30) + (t->tm_min/2)); // ((((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  	set_hand_angle(minute_hand_image_container, t->tm_min * 6);
  }
  t = yachtimer_getPblLastTime(myYachtTimer);

  strftime(date_text, sizeof(date_text), "%e %A", t);
  text_layer_set_text(text_date_layer, date_text);

}

void handle_timer( void *data)
{
   uint32_t cookie = *(uint32_t *) data;
   if(cookie == TIMER_UPDATE)
   {
  	yachtimer_tick(myYachtTimer,ticklen);

	// If watch only showing minute had so if WATCh 60 second
	ticklen = (yachtimer_getMode(myYachtTimer) == WATCHMODE) ? 1000  * 60:yachtimer_getTick(myYachtTimer);

	// Set ext wake up for tick < TICKREMOVE wake up every second otherwise do what is asked
	// we on;y have second disply 
	update_timer = app_timer_register( (ticks <= TICKREMOVE)?1000:ticklen, handle_timer, data);
  	update_hand_positions(); // TODO: Pass tick event
	ticks++;
	if(ticks >= TICKREMOVE) 
	{
		for(int i=0;i<MODES;i++)
		{
			layer_set_hidden( (Layer *)modeImages[i], true);
		}
	}
   }

}


void handle_init() {

  window = window_create();
  // window_init(&window, "Simple Analog");
  window_set_fullscreen(window, true);
  window_stack_push(window, true);
  // resource_init_current_app(&APP_RESOURCES);
  // Arrange for user input.
  window_set_click_config_provider(window,  config_provider);

  // Set up a layer for the static watch face background
  background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  background_image_container = bitmap_layer_create(gbitmap_get_bounds(background_image));
  bitmap_layer_set_bitmap(background_image_container,background_image);
  layer_add_child((Layer *)window, (Layer *)background_image_container);



  for (int i=0;i<MODES;i++)
  {
	mImages[i] = gbitmap_create_with_resource(mapModeImage[i].resourceid);
        modeImages[i] = bitmap_layer_create(gbitmap_get_bounds(mImages[i]));
        bitmap_layer_set_bitmap(modeImages[i],mImages[i]);
	layer_set_frame((Layer *)modeImages[i], GRect((144 - 12)/2,((144 - 16)/2)+ 25,12,16));
	layer_set_hidden( (Layer *)modeImages[i], true);
	layer_add_child((Layer *)window,(Layer *)modeImages[i]);
  } 
  ticks = 0;

  // Set up a layer for the hour hand
  hour_hand_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HOUR_HAND);
  hour_hand_image_container = rot_bitmap_layer_create(hour_hand_image);

  //hour_hand_image_container.layer.compositing_mode = GCompOpClear;

  rot_bitmap_set_src_ic(hour_hand_image_container, GPoint(4, 37));

  layer_add_child((Layer *)window, (Layer *)hour_hand_image_container);


  // Set up a layer for the minute hand
  minute_hand_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MINUTE_HAND);
  minute_hand_image_container = rot_bitmap_layer_create(minute_hand_image);

 // minute_hand_image_container.layer.compositing_mode = GCompOpClear;

  rot_bitmap_set_src_ic(minute_hand_image_container, GPoint(2, 56));

  layer_add_child((Layer *)window, (Layer *)minute_hand_image_container);
  text_date_layer = text_layer_create( layer_get_bounds((Layer*) window));
  text_layer_set_text_color(text_date_layer, GColorWhite);
  text_layer_set_background_color(text_date_layer, GColorClear);
  layer_set_frame((Layer *)text_date_layer, GRect(0, 150, 144, 22));
  text_layer_set_font(text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PIX_14)));
  text_layer_set_text_alignment(text_date_layer, GTextAlignmentCenter);
  layer_add_child((Layer *)window, (Layer *)text_date_layer);
  
  
  // Set up a layer for the second hand
 myYachtTimer = yachtimer_create(startappmode);
 startappmode = yachtimer_getMode(myYachtTimer);;
 yachtimer_tick(myYachtTimer,0);


  update_hand_positions();
  // Slow update down to once a second to save power
  ticklen = yachtimer_getTick(myYachtTimer);
  static uint32_t cookie = TIMER_UPDATE;
  update_timer = app_timer_register( ticklen, handle_timer, &cookie); 

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


  layer_add_child((Layer *)window, &center_circle_image_container.layer.layer); */
 

}
void config_provider( void *data) {
    window_single_click_subscribe(BUTTON_RUN, toggle_stopwatch_handler);
    // config[BUTTON_RUN]->click.handler = (ClickHandler)toggle_stopwatch_handler;
    window_single_click_subscribe(BUTTON_LAP, toggle_mode);
    // config[BUTTON_LAP]->click.handler = (ClickHandler) toggle_mode;
    window_single_click_subscribe(BUTTON_RESET, reset_stopwatch_handler);
    // config[BUTTON_RESET]->click.handler = (ClickHandler)reset_stopwatch_handler;
}


void handle_deinit() {

  bitmap_layer_destroy(background_image_container);
  gbitmap_destroy(background_image);

  for(int i=0;i<MODES;i++)
  {
	bitmap_layer_destroy(modeImages[i]);
	gbitmap_destroy(mImages[i]);
  }

  text_layer_destroy(text_date_layer);
  rot_bitmap_layer_destroy(hour_hand_image_container);
  gbitmap_destroy(hour_hand_image);
  rot_bitmap_layer_destroy(minute_hand_image_container);
  gbitmap_destroy(minute_hand_image);
  yachtimer_destroy(myYachtTimer);
}

int main(void) {
	handle_init();
	app_event_loop();
	handle_deinit();
}
/*
void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .timer_handler = &handle_timer

  };
  app_event_loop(params, &handlers);
}
*/
