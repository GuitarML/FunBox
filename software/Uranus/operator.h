#pragma once
#ifndef OPERATOR_H
#define OPERATOR_H

#include <stdint.h>
#include "daisysp.h"
#ifdef __cplusplus

using namespace daisysp;


/** 
    Single FM Operator that can be either a modulator or a carrier
*/

class Operator
{
  public:
    Operator() {}
    ~Operator() {}

    /** Initializes the Operator module.
        \param samplerate - The sample rate of the audio engine being run. 
    */
    void Init(float samplerate, bool isCarrier);


    /**  Returns the next sample
    */
    float Process();

    /** Carrier freq. setter
        \param freq Carrier frequency in Hz
    */
    void SetFrequency(float freq);

    /** Set modulator freq. relative to carrier
        \param ratio New modulator freq = carrier freq. * ratio
    */
    void SetRatio(float ratio);

    /** Sets the amplitude of the oscillator
    */
    void SetLevel(float level);

    /** Sets the phase input if this is a carrier driven by a modulator
         Intended to be called by an outside function at samplerate
    */
    void setPhaseInput(float modval); 

    /** Resets oscillators */
    void Reset();

    ///////////////////////////// NEW STUFF

    /** Starts note */
    void OnNoteOn(float note, float velocity);



  private:

    Oscillator osc_;
    float      level_, llevel_;
    float      freq_, lfreq_, ratio_, lratio_;
    bool       iscarrier_;
    float      modval_;

   // Notes
   float       velocity_;
   float       velocity_amp_;

};
#endif
#endif
