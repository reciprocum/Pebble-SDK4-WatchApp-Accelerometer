/* (C) Afonso Santos, November 2014, Portugal */

#include <pebble.h>
#include "Sampler.h"


// UI related
static Window         *window ;
static TextLayer      *label_layer ;
static Layer          *plotter_layer ;
static ActionBarLayer *action_bar ;


// World related
#define SAMPLES_CAPACITY          2500
#define SAMPLES_SHOW_MAX          145
#define SAMPLES_SHOW_MIN          10

#define MODE_PLOTTER_UNDEFINED     0
#define MODE_PLOTTER_X             1
#define MODE_PLOTTER_Y             2
#define MODE_PLOTTER_Z             3

#define DEFAULT_PLOTTER_MODE       MODE_PLOTTER_Y


AccelSamplingRate sampler_samplingRate      = ACCEL_SAMPLING_25HZ ;
uint8_t           sampler_samplesPerUpdate  = 1 ;
Sampler          *sampler_beingPlotted      = NULL ;
Sampler          *sampler_accelX            = NULL ;                        // To be allocated at world_init( ).
Sampler          *sampler_accelY            = NULL ;                        // To be allocated at world_init( ).
Sampler          *sampler_accelZ            = NULL ;                        // To be allocated at world_init( ).
static uint8_t    plotter_mode   	          = MODE_PLOTTER_UNDEFINED ;
static bool       config_mode_isOn          = false ;
static uint16_t   plotter_lag               = 0 ;

char    *label_name ;
char    *label_rate = "25" ;
int16_t  label_max, label_min ;


static int label_update_counter = 0 ;

void
label_update( )
{
  static char label_text[50] ;

  snprintf( label_text
          , sizeof(label_text)
          , "%s%-2s min:%-+5d max:%-+5d"
          , label_name
          , config_mode_isOn ? (++label_update_counter & 0b100 ? label_rate : "+-" ) : label_rate
          , label_min
          , label_max
          ) ;

  text_layer_set_text( label_layer, label_text ) ;
}


void
plotterMode_set
( uint8_t plotterMode )
{
  if (plotter_mode == plotterMode)
    return ;

  switch (plotter_mode = plotterMode)
  {
    case MODE_PLOTTER_X:
      sampler_beingPlotted = sampler_accelX ;
      label_name = "X" ;
      break ;

    case MODE_PLOTTER_Y:
      sampler_beingPlotted = sampler_accelY ;
      label_name = "Y" ;
      break ;

    case MODE_PLOTTER_Z:
      sampler_beingPlotted = sampler_accelZ ;
      label_name = "Z" ;
      break ;
  }

  layer_mark_dirty( plotter_layer ) ;
}


void
vibes_click_handler
( ClickRecognizerRef recognizer, void *context )
{ vibes_long_pulse( ) ; }


void
plotterMode_change_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  switch (plotter_mode)
  {
    case MODE_PLOTTER_X:
      plotterMode_set( MODE_PLOTTER_Y ) ;
      break ;

    case MODE_PLOTTER_Y:
      plotterMode_set( MODE_PLOTTER_Z ) ;
      break ;

    case MODE_PLOTTER_Z:
      plotterMode_set( MODE_PLOTTER_X ) ;
      break ;
  }
}


void
plotter_stepLeft_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  if (plotter_lag < sampler_beingPlotted->samplesNum - SAMPLES_SHOW_MIN )
  {
    ++plotter_lag ;
    layer_mark_dirty( plotter_layer ) ;
  }
}


void
plotter_stepRight_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  if (plotter_lag > 0 ) 
  {
    --plotter_lag ;
    layer_mark_dirty( plotter_layer ) ;              // this will call the plotter_layer_update_proc( ) method.
  }
}


void
plotter_fullLeft_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  plotter_lag = sampler_beingPlotted->samplesNum > SAMPLES_SHOW_MAX ? sampler_beingPlotted->samplesNum - SAMPLES_SHOW_MAX : sampler_beingPlotted->samplesNum - SAMPLES_SHOW_MIN ;
  layer_mark_dirty( plotter_layer ) ;               // this will call the plotter_layer_update_proc( ) method.
}


void
plotter_fullRight_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  plotter_lag = 0 ;                                 // Resume auto-scrolling plotting.
  layer_mark_dirty( plotter_layer ) ;               // this will call the plotter_layer_update_proc( ) method.
}


void
samplingRate_decrement_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  switch (sampler_samplingRate)
  {
    case ACCEL_SAMPLING_50HZ:
      sampler_samplingRate     = ACCEL_SAMPLING_25HZ ;
      label_rate               = "25" ;
      sampler_samplesPerUpdate = 1 ;
      break ;

    case ACCEL_SAMPLING_25HZ:
      sampler_samplingRate     = ACCEL_SAMPLING_10HZ ;
      label_rate               = "10" ;
      sampler_samplesPerUpdate = 1 ;
      break ;

    case ACCEL_SAMPLING_100HZ:    // Ignored/unsupported because misbehaves at this rate.
    case ACCEL_SAMPLING_10HZ:     // Cannot go lower than 10hz.
      break ;
  }

  label_update( ) ;
  accel_service_set_sampling_rate( sampler_samplingRate ) ;
  accel_service_set_samples_per_update( sampler_samplesPerUpdate ) ;
}


void
samplingRate_increment_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  switch (sampler_samplingRate)
  {
    case ACCEL_SAMPLING_10HZ:
      sampler_samplingRate     = ACCEL_SAMPLING_25HZ ;
      label_rate               = "25" ;
      sampler_samplesPerUpdate = 1 ;
      break ;

    case ACCEL_SAMPLING_25HZ:
      sampler_samplingRate     = ACCEL_SAMPLING_50HZ ;
      label_rate               = "50" ;
      sampler_samplesPerUpdate = 2 ;
      break ;

    case ACCEL_SAMPLING_50HZ:   // Ignored/unsupported because misbehaves at 100Hz rate.
    case ACCEL_SAMPLING_100HZ:  // Cannot go higger than 100hz.
      break ;
  }

  label_update( ) ;
  accel_service_set_samples_per_update( sampler_samplesPerUpdate ) ;
  accel_service_set_sampling_rate( sampler_samplingRate ) ;
}


// Forward declarations.
void  normalMode_click_config_provider( void *context ) ;
void  configMode_click_config_provider( void *context ) ;
void  app_exit_click_handler( ClickRecognizerRef recognizer, void *context ) ;


void
configMode_enter_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  config_mode_isOn = true ;
  action_bar_layer_set_click_config_provider( action_bar, configMode_click_config_provider ) ;
}


void
configMode_exit_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  config_mode_isOn = false ;
  action_bar_layer_set_click_config_provider( action_bar, normalMode_click_config_provider ) ;
}


void
configMode_click_config_provider
( void *context )
{
  window_single_click_subscribe( BUTTON_ID_UP  , samplingRate_increment_click_handler ) ;
  window_single_click_subscribe( BUTTON_ID_DOWN, samplingRate_decrement_click_handler ) ;

  window_long_click_subscribe( BUTTON_ID_SELECT
                             , 0                                 // Use default 500ms
                             , configMode_exit_click_handler     // Down handler.
                             , NULL                              // Up handler.
                             ) ;

  window_single_click_subscribe( BUTTON_ID_BACK, app_exit_click_handler ) ;
}


void
normalMode_click_config_provider
( void *context )
{
  window_single_repeating_click_subscribe( BUTTON_ID_UP  , 30, plotter_stepLeft_click_handler  ) ;
  window_single_repeating_click_subscribe( BUTTON_ID_DOWN, 30, plotter_stepRight_click_handler ) ;

  window_multi_click_subscribe(	BUTTON_ID_UP
                              , 2                // Min clicks
                              , 0                // use min clicks
                              , 0                // Use default (300ms)
                              , false
                              , plotter_fullLeft_click_handler
                              )	 ;
  
  window_multi_click_subscribe(	BUTTON_ID_DOWN
                              , 2                // Min clicks
                              , 0                // use min clicks
                              , 0                // Use default (300ms)
                              , false
                              , plotter_fullRight_click_handler
                              )	;

  window_single_click_subscribe( BUTTON_ID_SELECT, plotterMode_change_click_handler ) ;

  window_multi_click_subscribe(	BUTTON_ID_SELECT
                              , 2                // Min clicks
                              , 0                // use min clicks
                              , 0                // Use default (300ms)
                              , false
                              , vibes_click_handler
                              )	 ;

  window_long_click_subscribe( BUTTON_ID_SELECT
                             , 0                                                // Use default 500ms
                             , (ClickHandler) configMode_enter_click_handler    // Down handler.
                             , NULL                                             // Up handler.
                             ) ;

  window_single_click_subscribe( BUTTON_ID_BACK, app_exit_click_handler ) ;
}


// Acellerometer handlers.
void
accel_data_service_handler
( AccelData *ad
, uint32_t   num_samples
)
{
  if (plotter_lag != 0)
    return ;                   // Freeze feeding the FIFO XYZ samplers until the user returns to the rightmost part.

  for (uint8_t iSample = 0  ;  iSample < num_samples  ;  ++iSample )
  { 
    Sampler_push( sampler_accelX, ad->x ) ;
    Sampler_push( sampler_accelY, ad->y ) ;
    Sampler_push( sampler_accelZ, ad->z ) ;
    ++ad ;
  }

  layer_mark_dirty( plotter_layer ) ;              // this will call the plotter_layer_update_proc( ) method.
}


void
world_init
( )
{
  sampler_accelX = Sampler_new( SAMPLES_CAPACITY ) ;
  sampler_accelY = Sampler_new( SAMPLES_CAPACITY ) ;
  sampler_accelZ = Sampler_new( SAMPLES_CAPACITY ) ;
}


void
plotter_layer_update_proc
( Layer    *me
, GContext *gCtx
)
{
  Sampler_plot( sampler_beingPlotted, me, gCtx, plotter_lag, SAMPLES_SHOW_MAX, SAMPLES_SHOW_MIN, &label_max, &label_min ) ;
  label_update( ) ;
}


void
world_deinit
( )
{
  sampler_beingPlotted = NULL ;
  free( Sampler_free( sampler_accelX ) ) ; sampler_accelX = NULL ;
  free( Sampler_free( sampler_accelY ) ) ; sampler_accelY = NULL ;
  free( Sampler_free( sampler_accelZ ) ) ; sampler_accelZ = NULL ;
}


void
world_start
( )
{
  plotterMode_set( DEFAULT_PLOTTER_MODE ) ;
  accel_data_service_subscribe( sampler_samplesPerUpdate, accel_data_service_handler ) ;
}


void
world_stop
( )
{ accel_data_service_unsubscribe( ) ; }


void
app_exit_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  world_stop( ) ;
  window_stack_pop_all(	false )	;
}


void
window_load
( Window *window )
{
  Layer *window_root_layer = window_get_root_layer( window ) ;

  action_bar = action_bar_layer_create( ) ;
  action_bar_layer_add_to_window( action_bar, window ) ;
  action_bar_layer_set_click_config_provider( action_bar, normalMode_click_config_provider ) ;

  GRect root_bounds = layer_get_frame( window_root_layer ) ;

  // Label layer.
  GRect label_bounds ;
  label_bounds.origin.x = root_bounds.origin.x ;
  label_bounds.origin.y = root_bounds.origin.y ;
  label_bounds.size.h   = 24 ;
  label_bounds.size.w   = root_bounds.size.w ;

  label_layer = text_layer_create( label_bounds ) ;
  text_layer_set_background_color( label_layer, GColorBlack ) ;
  text_layer_set_text_color( label_layer, GColorWhite ) ;
  layer_add_child( window_root_layer, text_layer_get_layer( label_layer ) ) ;

  // Plotter layer
  GRect plotter_bounds ;
  plotter_bounds.origin.x = label_bounds.origin.x ;
  plotter_bounds.origin.y = label_bounds.origin.y + label_bounds.size.h ;
  plotter_bounds.size.h   = root_bounds.size.h - label_bounds.size.h ;
  plotter_bounds.size.w   = label_bounds.size.w ;

  plotter_layer = layer_create( plotter_bounds ) ;
  layer_set_update_proc( plotter_layer, plotter_layer_update_proc ) ;
  layer_add_child( window_root_layer, plotter_layer ) ;

  world_start( ) ;
}


void
window_unload
( Window *window )
{
  world_stop( ) ;
  text_layer_destroy( label_layer ) ;
  layer_destroy( plotter_layer ) ;
}


void
app_init
( void )
{
  world_init( ) ;

  window = window_create( ) ;
  window_set_background_color( window, GColorBlack ) ;
 
  window_set_window_handlers( window
                            , (WindowHandlers)
                              { .load   = window_load
                              , .unload = window_unload
                              }
                            ) ;

#ifdef PBL_SDK_2
  window_set_fullscreen( window, true ) ;
#endif
 
  window_stack_push( window, false ) ;
}


void
app_deinit
( void )
{
  window_stack_remove( window, false ) ;
  window_destroy( window ) ;
  world_deinit( ) ;
}


int
main
( void )
{
  app_init( ) ;
  app_event_loop( ) ;
  app_deinit( ) ;
}