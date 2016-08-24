/* (C) Afonso Santos, November 2014, Portugal */

#pragma once


#include <pebble.h>


typedef struct
{ uint16_t  capacity ;
  uint16_t  samplesNum ;
  int32_t   samplesAcum ;
  int16_t  *samples ;
  uint16_t  samples_headIdx ;
} Sampler ;


Sampler*  Sampler_new( const uint16_t capacity ) ;
Sampler*  Sampler_free( Sampler *this ) ;
void      Sampler_init( Sampler *this ) ;
void      Sampler_push( Sampler *this, const int16_t sample ) ;

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
) ;