/* (C) Afonso Santos, November 2014, Portugal */

#include <pebble.h>
#include "Sampler.h"


Sampler*
Sampler_free
( Sampler *this )
{ 
  if (this != NULL)
  {
    free( this->samples ) ;
    this->samples = NULL ;
    this->capacity = this->samplesNum = 0 ;
  }

  return this ;
}


void
Sampler_init
( Sampler *this )
{
  this->samplesNum = this->samples_headIdx = this->samplesAcum = 0 ;

  for ( uint8_t sampleIdx = 0  ;  sampleIdx < this->capacity  ;  ++sampleIdx )
    this->samples[sampleIdx] = 0 ;
}


Sampler*
Sampler_new
( const uint16_t capacity )
{
  Sampler *this = malloc(sizeof(Sampler)) ;

  if (this == NULL)
    return NULL ;

  this->samples = malloc((this->capacity = capacity) * sizeof(int16_t)) ;

  if (this->samples == NULL)
    return Sampler_free( this ) ;

  Sampler_init( this ) ;

  return this ;
}


void
Sampler_push
( Sampler *this
, int16_t  sample
)
{
  if (this == NULL)
    return ;
 
  if (this->samplesNum < this->capacity )
    this->samples_headIdx = this->samplesNum++ ;
  else
  { // Capacity full! ...

    // ... so we must move the headIdx forward...
    this->samples_headIdx++ ;

    if (this->samples_headIdx == this->capacity)
      this->samples_headIdx = 0 ;                       // and wrap-around the FIFO stack if necessary.
  }

  this->samplesAcum += sample - this->samples[this->samples_headIdx] ;  // Refresh acum stats. Discard outgoing sample.
  this->samples[this->samples_headIdx] = sample ;
}


void
Sampler_plot
( Sampler        *this
, Layer          *me
, GContext       *gCtx
, const uint16_t  pHeadLag
, const uint16_t  pShowMax          // Maximun number of samples to be plotted at once.
, const uint8_t   pShowMin          // Minimum number of samples to be plotted at once.
, int16_t        *ptrSampleMax
, int16_t        *ptrSampleMin
)
{
  if (this == NULL)
    return ;
  
  const GRect layerBounds = layer_get_bounds( me ) ;
  const uint8_t screen_w = layerBounds.size.w ;
  const uint8_t screen_h = layerBounds.size.h ;

	graphics_context_set_stroke_color( gCtx, GColorWhite ) ;

  // MIN( pHeadLag, this->samplesNum - pShowMin )
  const uint16_t headLag   = pHeadLag < this->samplesNum - pShowMin ? pHeadLag : this->samplesNum - pShowMin ;

  // MIN( pShowMax, this->samplesNum     )
  uint16_t       showNum   = pShowMax < this->samplesNum            ? pShowMax : this->samplesNum            ;

  // Near the samples buffer tail cannot show as much samples as asked.
  if (showNum > (this->samplesNum - headLag) )
    showNum = this->samplesNum - headLag ;

  // Normalized world coordinates are a square with both x & y spanning from -1.0 to +1.0.
  const float    wPlotStep = showNum < 3 ? 2.0 : 2.0 / (showNum - 1) ;

  const float   screen_Kx = (screen_w - 1) / 2.0 ;
  const float   screen_Ky =-(screen_h - 1) / 2.0 ;
  const float   screen_Cy = (screen_h - 1) ;

  int16_t       plotter ;                              // Sample coords.
  GPoint        sPlotter, sSample ;                    // Screen coords.

  // First iteration of the samples to determine max/min for auto-scaling purpouses.
  int16_t sampleMax = INT16_MIN ;
  int16_t sampleMin = INT16_MAX ;
  int16_t sampleIdx = this->samples_headIdx - headLag ;

  for ( uint8_t iSample = 0  ;  iSample < showNum  ;  ++iSample )
  {
    const int16_t sample = this->samples[sampleIdx] ;

    if (sample > sampleMax)
      sampleMax = sample ;

    if (sample < sampleMin)
      sampleMin = sample ;

    if (--sampleIdx < 0)
      sampleIdx = this->samplesNum - 1 ;  // Wrap-around the FIFO stack.
  }

  if (ptrSampleMax != NULL)
    *ptrSampleMax = sampleMax ;

  if (ptrSampleMin != NULL)
    *ptrSampleMin = sampleMin ;

  // calculate auto-scale coeficients.
  float plotter_K, plotter_C ;

  if (sampleMax == sampleMin)  // Rare case but would cause div/0 if not properly handled.
    plotter = plotter_K = plotter_C = 0.0 ;
  else
  {
    plotter = sampleMax * sampleMin < 0 ? 0 : (sampleMax < 0 ? sampleMax : sampleMin) ;
    plotter_K = 2.0 / (sampleMax - sampleMin) ;
    plotter_C = -1.0 ;
  }

  const float wPlotter_y = plotter_K * (plotter - sampleMin)  +  plotter_C ;
  sPlotter.y = screen_Ky * (wPlotter_y + 1.0)  +  screen_Cy ;

  // Second iteration of the samples to plot/draw them.
  sampleIdx = this->samples_headIdx - headLag ;

  for ( uint8_t iSample = 0  ;  iSample < showNum  ;  ++iSample )
  {
    const float   wPlotter_x = 1.0 - iSample * wPlotStep ;
    const int16_t sample     = this->samples[sampleIdx] ;
    const float   wSample_y  = plotter_K * (sample - sampleMin)  +  plotter_C ;

    // Transform normalized world coordinates into screen coordinates.
    sPlotter.x = sSample.x = screen_Kx * (wPlotter_x + 1.0) ;
                 sSample.y = screen_Ky * (wSample_y + 1.0)  +  screen_Cy ;

    // Draw Plotter -> Sample 2D line.
    graphics_draw_line( gCtx, sPlotter, sSample ) ;

    if (--sampleIdx < 0)
      sampleIdx = this->samplesNum - 1 ;  // Wrap-around the FIFO stack.
  }
}