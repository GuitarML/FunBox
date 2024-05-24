#pragma once
#ifndef EXPRESSION_HANDLER_H
#define EXPRESSION_HANDLER_H
#include <stdlib.h>
#include <stdint.h>

/*
ExpressionHandler class 
author: Keith Bloemer
date  : 5/23/2024

This class was written to handle an expression pedal on the Funbox v3 guitar pedal hardware.
It was written with a specific expression operation in mind, but is independant of a specific 
control scheme. It requires a way to engage a "Expression Set Mode", and a "Clear Expression" action, 
as well as an arbitrary number of knobs able to be controlled by the expression pedal.

Intended operating sequence:

1. Hold both footswitches (more than .5 seconds) to engage Expression Set Mode. Both LEDs dim to indicate this mode.

2. Move the expression pedal into the fully up position, and set one boundary of any knobs to apply expression to. 
   Right LED brightens to indicate it's ready to set the UP position.

3. Move the expression pedal into the fully down position, and set the other boundary of any knobs to apply expression to. 
   Left LED brightens to indicate it's ready to set the DOWN position.

   Note: The Up/Down knob settings must be different to be set as active for Expression. If they are the same, it's assumed inactive.

4. Redo step 2 or 3 as desired, in either order.

5. Hold both footswitches (more than .5 seconds) to exit Expression Set Mode and Engage Expression. The chosen knobs
   are now controlled by the Expression pedal within the chosen ranges. LEDs return to normal operation.

6. Re-Enter Expression Set mode to end the current expression settings and choose new ones.

7. Hold both footswitches longer than 2 seconds to clear all expression settings and exit any expression pedal operation.

*/

class ExpressionHandler

{
  public:
    ExpressionHandler() {}
    ~ExpressionHandler() {}
    // Initialize
 
    void Init(int num_of_params)
    {
        numParams = num_of_params; // Max is 6?
        Reset();
    }


    void Reset() 
    // Resets the Expression Handler, clears all settings and gives control back to the knobs
    {
        std::fill(std::begin(expression_up), std::end(expression_up), 0.0);
        std::fill(std::begin(expression_down), std::end(expression_down), 0.0);
        std::fill(std::begin(expression_knob_active), std::end(expression_knob_active), false); 

        expressionSet = false;
        expressionEngaged = false;
        led1_brightness = 0.0f;
        led2_brightness = 0.0f;
    }

    void ToggleExpressionSetMode() 
    // Toggles Expression Set Mode on/off. When toggling out of Expression Set Mode, the Expression is engaged
    {
        expressionSet = !expressionSet;
        expressionEngaged = !expressionSet;

        if (expressionSet) {
            //led1_brightness = 0.5f;  // Dim LEDs in expression set mode // Redundant
            //led2_brightness = 0.5f;  // Dim LEDs in expression set mode

            // Reset expression_knob_active to all false
            std::fill(std::begin(expression_knob_active), std::end(expression_knob_active), false); 

        }

        if (expressionEngaged) { // If expression mode has been engaged, check for moved knobs
            for (int i = 0; i < numParams; i++) {
                if (knobMoved(expression_up[i], expression_down[i])) {
                    expression_knob_active[i] = true;
                } 
            }
        }
    }

    bool isExpressionSetMode() 
    {
        return expressionSet;
    }

    float returnLed1Brightness() 
    {
        return led1_brightness;
    }


    float returnLed2Brightness() 
    {
        return led2_brightness;
    }

    void Process(float vexpression, float* knobValues, float* newExpressionValues)
    /* vexpression : value of the expression potentiometer
       knobValues  : array of the current knob values, 0 to 1
       newExpressionValues  : array of the new expression values, 0 to 1
    */
    {

        if (expressionSet) {
            if (vexpression < 0.05) {      
                for (int i = 0; i < numParams; i++) { 
                    expression_up[i] = knobValues[i];
                }

                led1_brightness = 1.0f;  // Dim/Brighten leds to show it detects expression up (heel)
                led2_brightness = 0.5f;

            } else if (vexpression > 0.95) {
                for (int i = 0; i < numParams; i++) { 
                    expression_down[i] = knobValues[i];
                }
                led1_brightness = 0.5f;  // Dim/Brighten leds to show it detects expression down (toe)
                led2_brightness = 1.0f;

            } else {
                led1_brightness = 0.5f;  // Set both leds at half brightness to indicate Expression Set mode
                led2_brightness = 0.5f;
            }

        }
    
        // If expression is active, remap controls to current expression setting for active parameters
        if (expressionEngaged) {
            for (int i = 0; i < numParams; i++) { 
                if (expression_knob_active[i]) {

                    // Check if up position or down position is higher, and calculate new expression value accordingly
                    if (expression_down[i] - expression_up[i] > 0.0) {
                        newExpressionValues[i] = vexpression * (expression_down[i] - expression_up[i])  + expression_up[i]; // Normal operation

                    } else {
                        newExpressionValues[i] = expression_up[i] - (vexpression * (expression_up[i] - expression_down[i])); // Inverse operation
            
                        //newExpressionValues[i] = vexpression * (std::max(expression_down[i], expression_up[i]) - std::min(expression_down[i], expression_up[i]))  + std::min(expression_down[i], expression_up[i]);
                        // I'm not superstitious. But I am a little stitious.
                    }

                } else {
                    newExpressionValues[i] = knobValues[i]; // If knob is not active for expression, pass the original knob value out 
                }
            } 
        } else { 
            for (int i = 0; i < numParams; i++) { 

                newExpressionValues[i] = knobValues[i]; // If expression is not engaged simply pass the original knob values out
            }       
        }
    }



  private:
   
    // Used to detect movement of a knob within a tolerance (currently set to 1% knob movement)
    bool knobMoved(float old_value, float new_value)
    {
        float tolerance = 0.01;
        if (new_value > (old_value + tolerance) || new_value < (old_value - tolerance)) {
            return true;
        } else {
            return false;
        }
    }


    // Expression 
    int numParams;                  // Number of parameters able to be controlled by expression at one time
    float  expression_up[6];        // Holds the up (heel) position of knob values
    float  expression_down[6];      // Holds the down (toe) position of knob values
    bool expression_knob_active[6]; // Stores active controls applied to expresssion, based on if a knob was moved during ExpressionSet mode

    //  The "[6]" in the above three variables sets the maximum number of knobs. Change and set accordingly when initializing the ExpressionHandler if a different number is desired

    bool expressionSet;             // Is it in Expression Set Mode
    bool expressionEngaged;         // Is it in Expression Engaged Mode

    float led1_brightness;          // Used to control the brightess of LEDs based on current expression mode
    float led2_brightness;

};


#endif
