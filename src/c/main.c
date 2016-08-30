/*
   Project: Accelerometer (watchapp)
   File   : main.c
   Author : Afonso Santos, Portugal

   Last revision: 12h31 August 28 2016
*/

#include <pebble.h>
#include <karambola/Sampler.h>

#include "Config.h"


// Obstruction related.
GSize unobstructed_plotter ;

// UI related
static Window         *s_window ;
static Layer          *s_window_layer ;

static TextLayer      *s_label_text_layer ;
static Layer          *s_label_layer ;

static Layer          *s_plotter_layer ;
static ActionBarLayer *s_action_bar_layer ;


// World related
#define SAMPLES_CAPACITY          2500
#define SAMPLES_SHOW_MAX          145
#define SAMPLES_SHOW_MIN          10

typedef enum { PLOTTER_MODE_UNDEFINED
             , PLOTTER_MODE_X
             , PLOTTER_MODE_Y
             , PLOTTER_MODE_Z
             }
PlotterMode ;


#define PLOTTER_DEFAULT_MODE       PLOTTER_MODE_Y


AccelSamplingRate  sampler_samplingRate      = ACCEL_SAMPLING_25HZ ;
uint8_t            sampler_samplesPerUpdate  = 1 ;
Sampler           *sampler_beingPlotted      = NULL ;
Sampler           *sampler_accelX            = NULL ;                        // To be allocated at world_init( ).
Sampler           *sampler_accelY            = NULL ;                        // To be allocated at world_init( ).
Sampler           *sampler_accelZ            = NULL ;                        // To be allocated at world_init( ).
static PlotterMode s_plotter_mode   	       = PLOTTER_MODE_UNDEFINED ;
static bool        s_config_mode_isOn        = false ;
static uint16_t    s_plotter_lag             = 0 ;

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
          , s_config_mode_isOn ? (++label_update_counter & 0b100 ? label_rate : "+-" ) : label_rate
          , label_min
          , label_max
          ) ;

  text_layer_set_text( s_label_text_layer, label_text ) ;
}


void
plotterMode_set
( PlotterMode plotterMode )
{
  if (s_plotter_mode == plotterMode)
    return ;

  switch (s_plotter_mode = plotterMode)
  {
    case PLOTTER_MODE_X:
      sampler_beingPlotted = sampler_accelX ;
      label_name = "X" ;
      break ;

    case PLOTTER_MODE_Y:
      sampler_beingPlotted = sampler_accelY ;
      label_name = "Y" ;
      break ;

    case PLOTTER_MODE_Z:
      sampler_beingPlotted = sampler_accelZ ;
      label_name = "Z" ;
      break ;

    default:
      sampler_beingPlotted = NULL ;
      label_name = "?" ;
      break ;
  }

  layer_mark_dirty( s_plotter_layer ) ;
}


void
vibes_click_handler
( ClickRecognizerRef recognizer, void *context )
{ vibes_long_pulse( ) ; }


void
plotterMode_change_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  switch (s_plotter_mode)
  {
    case PLOTTER_MODE_X:
      plotterMode_set( PLOTTER_MODE_Y ) ;
      break ;

    case PLOTTER_MODE_Y:
      plotterMode_set( PLOTTER_MODE_Z ) ;
      break ;

    case PLOTTER_MODE_Z:
      plotterMode_set( PLOTTER_MODE_X ) ;
      break ;

    default:
      plotterMode_set( PLOTTER_MODE_UNDEFINED ) ;
      break ;
  }
}


void
plotter_stepLeft_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  if (s_plotter_lag < sampler_beingPlotted->samplesNum - SAMPLES_SHOW_MIN )
  {
    ++s_plotter_lag ;
    layer_mark_dirty( s_plotter_layer ) ;
  }
}


void
plotter_stepRight_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  if (s_plotter_lag > 0 ) 
  {
    --s_plotter_lag ;
    layer_mark_dirty( s_plotter_layer ) ;              // this will call the plotter_layer_update_proc( ) method.
  }
}


void
plotter_fullLeft_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  s_plotter_lag = sampler_beingPlotted->samplesNum > SAMPLES_SHOW_MAX ? sampler_beingPlotted->samplesNum - SAMPLES_SHOW_MAX : sampler_beingPlotted->samplesNum - SAMPLES_SHOW_MIN ;
  layer_mark_dirty( s_plotter_layer ) ;               // this will call the plotter_layer_update_proc( ) method.
}


void
plotter_fullRight_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  s_plotter_lag = 0 ;                                 // Resume auto-scrolling plotting.
  layer_mark_dirty( s_plotter_layer ) ;               // this will call the plotter_layer_update_proc( ) method.
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
  s_config_mode_isOn = true ;
  action_bar_layer_set_click_config_provider( s_action_bar_layer, configMode_click_config_provider ) ;
}


void
configMode_exit_click_handler
( ClickRecognizerRef recognizer, void *context )
{
  s_config_mode_isOn = false ;
  action_bar_layer_set_click_config_provider( s_action_bar_layer, normalMode_click_config_provider ) ;
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
  if (s_plotter_lag != 0)
    return ;                   // Freeze feeding the FIFO XYZ samplers until the user returns to the rightmost part.

  for (uint8_t iSample = 0  ;  iSample < num_samples  ;  ++iSample )
  { 
    Sampler_push( sampler_accelX, ad->x ) ;
    Sampler_push( sampler_accelY, ad->y ) ;
    Sampler_push( sampler_accelZ, ad->z ) ;
    ++ad ;
  }

  layer_mark_dirty( s_plotter_layer ) ;              // this will call the plotter_layer_update_proc( ) method.
}


void
world_initialize
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
  LOGD("plotter_layer_update_proc:: ENTER") ;

#ifdef  PBL_COLOR
  switch (s_plotter_mode)
  {
    case PLOTTER_MODE_X:
    	graphics_context_set_stroke_color( gCtx, GColorRed ) ;
      break ;

    case PLOTTER_MODE_Y:
    	graphics_context_set_stroke_color( gCtx, GColorYellow ) ;
      break ;

    case PLOTTER_MODE_Z:
    	graphics_context_set_stroke_color( gCtx, GColorGreen ) ;
      break ;

    default:
      graphics_context_set_stroke_color( gCtx, GColorWhite ) ;
      break ;
  }
#else
	graphics_context_set_stroke_color( gCtx, GColorWhite ) ;
#endif

  Sampler_plot( gCtx, sampler_beingPlotted, unobstructed_plotter.w, unobstructed_plotter.h, s_plotter_lag, SAMPLES_SHOW_MAX, SAMPLES_SHOW_MIN, &label_max, &label_min ) ;
  label_update( ) ;

  LOGD("plotter_layer_update_proc:: LEAVE") ;
}


void
world_finalize
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
  plotterMode_set( PLOTTER_DEFAULT_MODE ) ;
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
unobstructed_area_change_handler
( AnimationProgress progress
, void             *context
)
{
  unobstructed_plotter = layer_get_unobstructed_bounds( s_plotter_layer ).size ;
}


void
window_load
( Window *window )
{
  s_window_layer = window_get_root_layer( window ) ;

  s_action_bar_layer = action_bar_layer_create( ) ;
  action_bar_layer_add_to_window( s_action_bar_layer, window ) ;
  action_bar_layer_set_click_config_provider( s_action_bar_layer, normalMode_click_config_provider ) ;

  GRect root_bounds = layer_get_frame( s_window_layer ) ;

  // Label layer.
  GRect label_bounds ;
  label_bounds.origin.x = root_bounds.origin.x ;
  label_bounds.origin.y = root_bounds.origin.y ;
  label_bounds.size.h   = 24 ;
  label_bounds.size.w   = root_bounds.size.w ;

  s_label_text_layer = text_layer_create( label_bounds ) ;
  text_layer_set_background_color( s_label_text_layer, GColorBlack ) ;
  text_layer_set_text_color( s_label_text_layer, GColorWhite ) ;
  layer_add_child( s_window_layer, s_label_layer = text_layer_get_layer( s_label_text_layer ) ) ;

  // Plotter layer
  GRect plotter_bounds ;
  plotter_bounds.origin.x = label_bounds.origin.x ;
  plotter_bounds.origin.y = label_bounds.origin.y + label_bounds.size.h ;
  plotter_bounds.size.h   = root_bounds.size.h - label_bounds.size.h ;
  plotter_bounds.size.w   = root_bounds.size.w ;

  s_plotter_layer = layer_create( plotter_bounds ) ;
  layer_set_update_proc( s_plotter_layer, plotter_layer_update_proc ) ;
  layer_add_child( s_window_layer, s_plotter_layer ) ;

  unobstructed_plotter = layer_get_unobstructed_bounds( s_plotter_layer ).size ;

  // Obstrution handling.
  UnobstructedAreaHandlers unobstructed_area_handlers = { .change = unobstructed_area_change_handler } ;
  unobstructed_area_service_subscribe( unobstructed_area_handlers, NULL ) ;

  world_start( ) ;
}


void
window_unload
( Window *window )
{
  unobstructed_area_service_unsubscribe( ) ;
  world_stop( ) ;
  text_layer_destroy( s_label_text_layer ) ;
  layer_destroy( s_plotter_layer ) ;
}


void
app_initialize
( void )
{
  world_initialize( ) ;

  s_window = window_create( ) ;
  window_set_background_color( s_window, GColorBlack ) ;
 
  window_set_window_handlers( s_window
                            , (WindowHandlers)
                              { .load   = window_load
                              , .unload = window_unload
                              }
                            ) ;

  window_stack_push( s_window, false ) ;
}


void
app_finalize
( void )
{
  window_stack_remove( s_window, false ) ;
  window_destroy( s_window ) ;
  world_finalize( ) ;
}


int
main
( void )
{
  app_initialize( ) ;
  app_event_loop( ) ;
  app_finalize( ) ;
}