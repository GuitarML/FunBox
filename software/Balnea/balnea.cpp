// Balnea Digital Delay / Pitch Shifter Pedal

#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"
//#include "expressionHandler.h" // Not using normal funbox expression handler for Balnea

#include "tape_modulator.h"
#include "vox.h"
#include "delayline_reverse.h"


using namespace daisy;
using namespace daisysp;
using namespace funbox; 

#define SAMPLING_FREQUENCY_HZ 48000

// Declare a local daisy_petal for hardware access
DaisyPetal hw;
::daisy::Parameter mix, Lpf, regen, glide, int1, int2, expression;
bool      bypass;
Led led1, led2;

float delayModRate, delayModDepth, wetMix, dryMix, glide_float, delayFeedback;

// Alternate functions

bool update_switches;

bool force_reset;

bool mode_changed;
bool triggerMode;

bool            pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int             switch1[2], switch2[2], switch3[2], dip[4];

// Initialize "previous" p values
float psize, pmodify, pdelayTime, pmix;
float pLpf, pint1, pint2, pexpression;

// Expression
bool expression_pressed = false;
bool expressionControl = false;

// Midi
bool midi_control[6]; //  just knobs for now
float pknobValues[6]; // Used for Midi control logic

float knobValues[6];
int toggleValues[3];
bool dipValues[4];


// Params
float delayBaseSpeed;
float delaySpeedMultiplier_int1;
float delaySpeedMultiplier_int2;

int leftTogglePosition2 = 0;
int leftTogglePosition = 0;
int rightTogglePosition = 0;



//Bypass / Alternate mode footswitch

bool alternateMode = false;

// Tap Tempo
uint32_t tap_debounce_threshold = 100;  // is this about 50ms?
uint32_t tap_debounce_wait_timer = 100;
uint8_t tap_debounce_reset = 0;

int tap_sample_timer = 0;
int tap_delay_max_limit = 48000 * 4.0;  // 4 second max delay for now
bool tap_timer_on = false;

int delay_timer = 0;
int current_base_delay_in_samples = 24000; // start with half second delay
bool tap_LED_on = false;

// Sequencer
int sequence_stage = 0; // 0, 1 or 2
int sequence_lengths_in_samples[3] = {24000, 24000, 24000};
int sequence_timer = 0;
float int1_stage_multiplier = 1.0;
float int2_stage_multiplier = 1.0;
float base_stage_multiplier = 1.0;

// Tap Sequencer Mode
bool tapSequencerMode = false;
bool incrementNextSequence = false;

// Reverse Mode
int reverse_mode_timer = 500;
int reverse_mode_timer_threshold = 500;
//bool rStart = true; // Workaround for reverse starting as default, not sure why when everything sets reverse to false at startup

// Self Oscillate mode
int self_osc_hold_timer = 300;
int self_osc_hold_threshold = 300;
bool selfOscMode = false; // used for led indication only

daisysp::Svf resonantLPF;
daisysp::Svf resonantLPF2;

TapeModulator modTape;

// Synth
static VoiceManager<12> voice_handler;


/////////////////////////// TEST SETTINGS
bool dissonantMode = false;
int dissonant_mode_timer = 500;
int dissonant_mode_timer_threshold = 500;

bool chromaticMode = false;
int chromatic_mode_timer = 500;
int chromatic_mode_timer_threshold = 500;


///////////////////////////



#define MAX_DELAY static_cast<size_t>(4096 * 4.f)
#define MAX_DELAY_REV static_cast<size_t>(4096 * 4.f * 2.f) // Reverse delay must be double normal delay size to have same delay time (read and write heads are going opposite directions)
daisysp::DelayLine<float, MAX_DELAY> delayLine;
daisysp::DelayLine<float, MAX_DELAY> delayLine2;

DelayLineReverse<float, MAX_DELAY_REV> delayLineRev;
DelayLineReverse<float, MAX_DELAY_REV> delayLineRev2;

// NOTE: Not using float to integer conversion on delaylines, enough RAM space due to small buffer sizes so reduction not needed
struct delay
{
    daisysp::DelayLine<float, MAX_DELAY> *del;
    DelayLineReverse<float, MAX_DELAY_REV> *delRev;
    float                        currentDelay;
    float                        delayTarget;
    float                        feedback;
    float                        active = false;
    float                        speed = 1.0;  // range 0 to 1, controlled by knob parameter (put a fonepole for this to add "Glide" function)
    float                        inc_counter = 0;
    float                        current_sample = 0.0;
    float                        currentSpeed;
    bool                         isRight = false;
    bool                         reverseMode = false;

    daisysp::Svf LPF;

    float Process(float in)
    {
        //set delay times
        daisysp::fonepole(currentDelay, delayTarget, .0002f);

        del->SetDelay(currentDelay);
        delRev->SetDelay1(currentDelay * 2);  // Doubling delay time due to read and write heads going in opposite directions


        if (glide_float < 0.05) {
        	currentSpeed = speed;
        } else {
        	daisysp::fonepole(currentSpeed, speed, .001 * (1.0 - glide_float));
        }

    	float wowRate = 0.2f + 2.0f * delayModRate; 
    	float flutterRate = 2.0f + 5.0f * delayModRate;

    	float wowDepth = 2.0f * delayModDepth;
      	float flutterDepth = 2.0f * delayModDepth;

	    modTape.ProcessTapeSpeed(wowRate, flutterRate, wowDepth, flutterDepth, 0.7); 
	    float mod_speed = 0.0;
        if (!isRight) {
    		  mod_speed = modTape.GetTapeSpeedLeft() * 0.01;
        } else {
        	mod_speed = modTape.GetTapeSpeedRight() * 0.01;
        }


        inc_counter += (currentSpeed + mod_speed);

        // Basically a samplerate reducer with no interpolation
        float ampOut = 0.0;
        if (inc_counter >= 1.0) {
            inc_counter -= 1.0;

            if (!reverseMode) {
                current_sample = del->Read();
            } else {
                current_sample = delRev->ReadRev();
            }

            LPF.Process(current_sample);
            ampOut = LPF.Low() * 0.51; // THIS MULTIPLIER IS VERY SENSITIVE TO CREATING FEEDBACK, 0.6 is too much

            if (active) {
                del->Write((feedback * ampOut) + in);
                delRev->Write((feedback * ampOut) + in);
            } else {
                del->Write((feedback * ampOut)); // if not active, don't write any new sound to buffer
                delRev->Write((feedback * ampOut));
            }
        } else {

            LPF.Process(current_sample);  // Still need to process LPF at pedal samplerate 48kHz
            ampOut = LPF.Low() * 0.51;

        }

        return ampOut;
    }
};

delay             delay1;
delay             delay2;



bool knobMoved(float old_value, float new_value)
{
    float tolerance = 0.005;
    if (new_value > (old_value + tolerance) || new_value < (old_value - tolerance)) {
        return true;
    } else {
        return false;
    }
}



void setINT1(int index) {

	switch(index) {

		case 0:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 0.7492 :   // ChromaticMode: down 5 semitones
										  (dissonantMode) ? 0.39685 : 0.25;  // DissonantMode: Oct+3rd down    Normal: 2 Oct down

			break;
		case 1:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 0.7937  :  // ChromaticMode: down 4 semitones
										  (dissonantMode) ? 0.5297 : 0.33334;  // DissonantMode: 7th down    Normal: Oct+5th down
			break;
		case 2:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 0.8409  :  // ChromaticMode: down 3 semitones
										  (dissonantMode) ? 0.5946 : 0.5;  // DissonantMode: 6th down Normal: Oct down
			break;
		case 3:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 0.8909  :  // ChromaticMode: down 2 semitones
										  (dissonantMode) ? 0.8409 : 0.66667;  // DissonantMode: Minor 3rd down Normal: 5th down
			break;
		case 4:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 0.9439  :  // ChromaticMode: down 1 semitones
										  (dissonantMode) ? 0.8909 : 0.74915;  // DissonantMode: 2nd down Normal: 4th down
			break;
		case 5:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 1.0  :  // ChromaticMode: root
										  (dissonantMode) ? 1.0 : 1.0;
			break;
		case 6:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 1.0595  :  // ChromaticMode: up 1 semitones
										  (dissonantMode) ? 1.12246 : 1.3348;  // DissonantMode: 2nd up   Normal: 4th up
			break;
		case 7:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 1.1225  :  // ChromaticMode: up 2 semitones
										  (dissonantMode) ? 1.1892 : 1.5;  // DissonantMode: Minor 3rd up  Normal: 5th up
			break;
		case 8:

			  delaySpeedMultiplier_int1 = (chromaticMode) ?  1.1892  :  // ChromaticMode: up 3 semitones
										  (dissonantMode) ? 1.6818 : 2.0;  // DissonantMode: 6th up      Normal: Oct up
			break;
		case 9:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 1.2599  :  // ChromaticMode: up 4 semitones
										  (dissonantMode) ? 1.8877 : 3.0;  // DissonantMode: 7th up      Normal: Oct + 5th up
			break;
		case 10:

			  delaySpeedMultiplier_int1 = (chromaticMode) ? 1.3348  :  // ChromaticMode: up 5 semitones
										  (dissonantMode) ? 2.5198 : 4.0;  // DissonantMode:  Oct+3rd up   Normal: 2 Oct up
			break;

	}

}


void setINT2(int index) {
	switch(index) {

		case 0:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 0.7492 :   // ChromaticMode: down 5 semitones
										  (dissonantMode) ? 0.39685 : 0.25;  // DissonantMode: Oct+3rd down    Normal: 2 Oct down

			break;
		case 1:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 0.7937  :  // ChromaticMode: down 4 semitones
										  (dissonantMode) ? 0.5297 : 0.33334;  // DissonantMode: 7th down    Normal: Oct+5th down
			break;
		case 2:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 0.8409  :  // ChromaticMode: down 3 semitones
										  (dissonantMode) ? 0.5946 : 0.5;  // DissonantMode: 6th down Normal: Oct down
			break;
		case 3:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 0.8909  :  // ChromaticMode: down 2 semitones
										  (dissonantMode) ? 0.8409 : 0.66667;  // DissonantMode: Minor 3rd down Normal: 5th down
			break;
		case 4:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 0.9439  :  // ChromaticMode: down 1 semitones
										  (dissonantMode) ? 0.8909 : 0.74915;  // DissonantMode: 2nd down Normal: 4th down
			break;
		case 5:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 1.0  :  // ChromaticMode: root
										  (dissonantMode) ? 1.0 : 1.0;
			break;
		case 6:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 1.0595  :  // ChromaticMode: up 1 semitones
										  (dissonantMode) ? 1.12246 : 1.3348;  // DissonantMode: 2nd up   Normal: 4th up
			break;
		case 7:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 1.1225  :  // ChromaticMode: up 2 semitones
										  (dissonantMode) ? 1.1892 : 1.5;  // DissonantMode: Minor 3rd up  Normal: 5th up
			break;
		case 8:

			  delaySpeedMultiplier_int2 = (chromaticMode) ?  1.1892  :  // ChromaticMode: up 3 semitones
										  (dissonantMode) ? 1.6818 : 2.0;  // DissonantMode: 6th up      Normal: Oct up
			break;
		case 9:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 1.2599  :  // ChromaticMode: up 4 semitones
										  (dissonantMode) ? 1.8877 : 3.0;  // DissonantMode: 7th up      Normal: Oct + 5th up
			break;
		case 10:

			  delaySpeedMultiplier_int2 = (chromaticMode) ? 1.3348  :  // ChromaticMode: up 5 semitones
										  (dissonantMode) ? 2.5198 : 4.0;  // DissonantMode:  Oct+3rd up   Normal: 2 Oct up
			break;
	}

}

void updateSwitch1()
{


    if (toggleValues[0] == 0) { 
            leftTogglePosition2 = 0;
            base_stage_multiplier = 1.0;
            sequence_lengths_in_samples[0] = current_base_delay_in_samples * base_stage_multiplier;


    } else if (toggleValues[0] == 2) {
        	leftTogglePosition2 = 2;
            base_stage_multiplier = 0.5;
            sequence_lengths_in_samples[0] = current_base_delay_in_samples * base_stage_multiplier;

    } else {                    
        	leftTogglePosition2 = 1;
            base_stage_multiplier = 0.75;
            sequence_lengths_in_samples[0] = current_base_delay_in_samples * base_stage_multiplier;


    } 


}


void updateSwitch2()
{


    if (toggleValues[1] == 0) { 
            leftTogglePosition = 0;
            int1_stage_multiplier = 1.0;
            sequence_lengths_in_samples[1] = current_base_delay_in_samples * int1_stage_multiplier;

    } else if (toggleValues[1] == 2) {
        	leftTogglePosition = 2;
            int1_stage_multiplier = 0.5;
            sequence_lengths_in_samples[1] = current_base_delay_in_samples * int1_stage_multiplier;

    } else {                    
        	leftTogglePosition = 1;
            int1_stage_multiplier = 0.75;
            sequence_lengths_in_samples[1] = current_base_delay_in_samples * int1_stage_multiplier;

    } 


}


void updateSwitch3() 
{
    // DELAY ///////////////////////
    if (toggleValues[2] == 0) {          
            rightTogglePosition = 0;
            int2_stage_multiplier = 1.0;
            sequence_lengths_in_samples[2] = current_base_delay_in_samples * int2_stage_multiplier;

    } else if (toggleValues[2] == 2) { 
            rightTogglePosition = 2;
            int2_stage_multiplier = 0.5;
            sequence_lengths_in_samples[2] = current_base_delay_in_samples * int2_stage_multiplier;


    } else {
            rightTogglePosition = 1;
            int2_stage_multiplier = 0.75;
            sequence_lengths_in_samples[2] = current_base_delay_in_samples * int2_stage_multiplier;
    }
}


void UpdateButtons()
{
    // (De-)Activate bypass and toggle LED when left footswitch is let go
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        if(!expression_pressed) {
            if (alternateMode) {
                alternateMode = false;
                force_reset = true; // force parameter reset to enforce current knob settings when leaving alternate mode
                led1.Set(1.0f);
            } else {
                bypass = !bypass;
                led1.Set(bypass ? 0.0f : 1.0f);
            }
        }

    } 

    // Toggle Alternate mode by holding down the left footswitch, if not already in alternate mode and not in bypass
    if(hw.switches[Funbox::FOOTSWITCH_1].TimeHeldMs() >= 500 && !alternateMode && !bypass) { 
        alternateMode = true;
        led1.Set(0.5f);  // Dim LED in alternate mode
    }


    // Tap Footswitch Action

    // Check if footswitch is pressed, the debouce wait is done, and the footswitch was previously not pressed (not held)
    if (hw.switches[Funbox::FOOTSWITCH_2].FallingEdge()) { 
        //tap_debounce_wait_timer = 0; // Start bypass wait timer for debouce
        //tap_debounce_reset = 1;      // 1 means footswitch is still held

        // Tap processing

        // If tap has been pressed a second time
        if (tap_timer_on) {
            // Set the base delay time
            //  Set the number of stages (samples) based on minimum possible time / 4 (has to be able to be sped up by 4 for +2 octave)
            //  Then adjust base speed to make the actual delay match the tapped delay time

            if (tap_sample_timer >= 65536) {
                delay1.delayTarget = 16384;
                delay2.delayTarget = 16384;
            } else if  (tap_sample_timer >= 32768) {
                delay1.delayTarget = 8192;
                delay2.delayTarget = 8192;
            } else if  (tap_sample_timer >= 16384) {
                delay1.delayTarget = 4096;
                delay2.delayTarget = 4096;
            } else if  (tap_sample_timer >= 8192) {
                delay1.delayTarget = 2048;
                delay2.delayTarget = 2048;
            //} else if  (tap_sample_timer >= 4096) {
            } else {
                delay1.delayTarget = 1024;
                delay2.delayTarget = 1024;
            } 

            delayBaseSpeed = delay1.delayTarget / tap_sample_timer;
            delay1.speed = delayBaseSpeed;
            delay2.speed = delayBaseSpeed;
            current_base_delay_in_samples = tap_sample_timer;

            tap_sample_timer = 0; // reset the timer for next tap (don't turn the tap_timer_on to false, want to keep waiting for another tap)

            delay_timer = 0;  // reset the delay timer for blinking led

            sequence_timer = 0;
            sequence_stage = 0;

            sequence_lengths_in_samples[0] = current_base_delay_in_samples * base_stage_multiplier;
            sequence_lengths_in_samples[1] = current_base_delay_in_samples * int1_stage_multiplier;
            sequence_lengths_in_samples[2] = current_base_delay_in_samples * int2_stage_multiplier;



        } else {  // Else if this is the first tap
            if (alternateMode) {  // If in alternate mode and left footswitch is pressed, toggle Tap Sequencer Mode (on/off)
                tapSequencerMode = !tapSequencerMode;
            } else {
                if (tapSequencerMode) {  // If in tapSequencerMode, increment the sequence for each tap
                    incrementNextSequence = true;
                } else {                 // Else if in normal tap mode, set first tap
                    tap_timer_on = true;
                }
            }
        }



    }



    // Self oscillation by holding left footswitch
    if (hw.switches[Funbox::FOOTSWITCH_2].TimeHeldMs() >= 300) {
        selfOscMode = true;
    	delay1.feedback = 1.1;
    	delay2.feedback = 1.1;
  

    } else {
        delay1.feedback = delayFeedback;
    	delay2.feedback = delayFeedback;
        selfOscMode = false;
    }


}


void UpdateSwitches()
{

    // Detect any changes in switch positions

    // 3-way Switch 1
    bool changed1 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch1[i]].Pressed() != pswitch1[i]) {
            pswitch1[i] = hw.switches[switch1[i]].Pressed();
            changed1 = true;
            
        }
    }
    if (changed1 || update_switches) { // update_switches is for turning off preset
        if (pswitch1[0] == true) {
            toggleValues[0] = 0;
        } else if (pswitch1[1] == true) {
            toggleValues[0] = 2;
        } else {
            toggleValues[0] = 1;
        }
        updateSwitch1(); 
    }



    // 3-way Switch 2
    bool changed2 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch2[i]].Pressed() != pswitch2[i]) {
            pswitch2[i] = hw.switches[switch2[i]].Pressed();
            changed2 = true;
        }
    }
    if (changed2 || update_switches) {
        if (pswitch2[0] == true) {
            toggleValues[1] = 0;
        } else if (pswitch2[1] == true) {
            toggleValues[1] = 2;
        } else {
            toggleValues[1] = 1;
        }
        updateSwitch2(); 
    }

    // 3-way Switch 3
    bool changed3 = false;
    for(int i=0; i<2; i++) {
        if (hw.switches[switch3[i]].Pressed() != pswitch3[i]) {
            pswitch3[i] = hw.switches[switch3[i]].Pressed();
            changed3 = true;
        }
    }
    if (changed3 || update_switches) {
        if (pswitch3[0] == true) {
            toggleValues[2] = 0;
        } else if (pswitch3[1] == true) {
            toggleValues[2] = 2;
        } else {
            toggleValues[2] = 1;
        }
        updateSwitch3();
    }

    // Dip switches
    bool changed4 = false;
    for(int i=0; i<4; i++) {
        if (hw.switches[dip[i]].Pressed() != pdip[i]) {
            pdip[i] = hw.switches[dip[i]].Pressed();
            changed4 = true;

        }
    }
    // Update action for dipswitches
    if (changed4 || update_switches) {
        for (int i=0; i<4; i++) {
           dipValues[i] = pdip[i]; 
        }

        if (dipValues[1]) { // Dipswitch 2 = Reverse mode
            delay1.reverseMode = true;
            delay1.reverseMode = true;
        } else {
            delay1.reverseMode = false;
            delay1.reverseMode = false;
        }

        if (dipValues[2]) { // Dipswitch 3 = Dissonant Mode
            dissonantMode = true;
            dissonantMode = true;
        } else {
            dissonantMode = false;
            dissonantMode = false;
        }

        if (dipValues[3]) { // Dipswitch 4 = Chromatic Mode (overrides Dissonant mode)
            chromaticMode = true;
            chromaticMode = true;
        } else {
            chromaticMode = false;
            chromaticMode = false;
        }

    }



}


// This runs at a fixed rate, to prepare audio samples
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{

    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();
    led1.Update();
    led2.Update();

    UpdateButtons();
    UpdateSwitches();


    // Knob and Expression Processing ////////////////////

    float vmix = mix.Process();
    float vLpf = Lpf.Process();
    float vregen = regen.Process();
    float vglide = glide.Process();
    float vint1 = int1.Process();
    float vint2 = int2.Process(); 
   
    // Update globals
    glide_float = vglide;
    delayFeedback = vregen;


    // Knob 1
    if (!midi_control[0])   // If not under midi control, use knob ADC
        pknobValues[0] = knobValues[0] = vmix;
    else if (knobMoved(pknobValues[0], vmix))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[0] = false;

    // Knob 2
    if (!midi_control[1])   // If not under midi control, use knob ADC
        pknobValues[1] = knobValues[1] = vLpf;
    else if (knobMoved(pknobValues[1], vLpf))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[1] = false;

    // Knob 3
    if (!midi_control[2])   // If not under midi control, use knob ADC
        pknobValues[2] = knobValues[2] = vregen;
    else if (knobMoved(pknobValues[2], vregen))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[2] = false;

    // Knob 4
    if (!midi_control[3])   // If not under midi control, use knob ADC
        pknobValues[3] = knobValues[3] = vglide;
    else if (knobMoved(pknobValues[3], vglide))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[3] = false;
    

    // Knob 5
    if (!midi_control[4])   // If not under midi control, use knob ADC
        pknobValues[4] = knobValues[4] = vint1;
    else if (knobMoved(pknobValues[4], vint1))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[4] = false;
    

    // Knob 6
    if (!midi_control[5])   // If not under midi control, use knob ADC
        pknobValues[5] = knobValues[5] = vint2;
    else if (knobMoved(pknobValues[5], vint2))  // If midi controlled, watch for knob movement to end Midi control
        midi_control[5] = false;


    float vexpression = expression.Process(); // 0 is heel (up), 1 is toe (down)


    // Handle Normal or Alternate Mode Mix Controls
    //    A cheap mostly energy constant crossfade from SignalSmith Blog
    float x2 = 1.0 - vmix;
    float A = vmix*x2;
    float B = A * (1.0 + 1.4186 * A);
    float C = B + vmix;
    float D = B + x2;

    wetMix = C * C;
    dryMix = D * D;

    // If the Knob2 ADC reading has changed, and currently under expression control +/- 0.005 then give control to Knob2
    if (knobMoved(pLpf, vLpf) && expressionControl == true) {
        expressionControl = false;
    }

    if (!expressionControl) {
        pLpf = vLpf;
        float LPF_freq = 100.0 + vLpf  * vLpf * 14000.0;
        resonantLPF.SetFreq(LPF_freq);
        resonantLPF2.SetFreq(LPF_freq);
    }


    // If the expression ADC reading has changed +/- 0.005 and currently under Knob2 control, then give control to expression
    if (knobMoved(pexpression, vexpression) && expressionControl == false) {
        expressionControl = true;
    }

    if (expressionControl) {
        pexpression = vexpression;
        float LPF_freq = 100.0 + vexpression  * vexpression * 14000.0;
        resonantLPF.SetFreq(LPF_freq);
        resonantLPF2.SetFreq(LPF_freq);
    }


    if (!selfOscMode) {
        delay1.feedback = vregen;
        delay2.feedback = vregen;
    }


    // Set INT Parameters ///////////////
    if (knobMoved(pint1, vint1) || force_reset == true)
    {
        if (!alternateMode) {

              float temp_interval_setting = vint1;
              int index = std::floor(temp_interval_setting * 11.0);
              setINT1(index);

        } else {  
            delayModRate = vint1;

        }
        pint1 = vint1;
    }

    if (knobMoved(pint2, vint2) || force_reset == true)
    {
        if (!alternateMode) {

              float temp_interval_setting = vint2;
              int index = std::floor(temp_interval_setting * 11.0);
              setINT2(index);

        } else {  
            delayModDepth = vint2;

        }
        pint2 = vint2;
    }



    force_reset = false;

    float inputL, inputR, leftIn, rightIn, leftOut, rightOut;

    if(!bypass) {
        for (size_t i = 0; i < size; i++)
        {
            // Stereo or MISO 
            if (dipValues[0]) {  // Stereo
                inputL = in[0][i];
                inputR = in[1][i];
            } else {     //MISO
                inputL = in[0][i];
                inputR = in[0][i];
            }

            // Midi Synth
            float sum = 0.f;
            sum = voice_handler.Process() * 0.04; // was 0.5f, needs more volume reduction
            leftIn = inputL + sum;
            rightIn = inputR + sum;

            // Tap Tempo //
            if (tap_timer_on && tapSequencerMode == false) {
                tap_sample_timer += 1;
                if (tap_sample_timer >= tap_delay_max_limit) {  // If you tap once and dont tap before the max time is reached, ignore
                    tap_timer_on = false;
                    tap_sample_timer = 0;

                }
            }

            delay_timer += 1;
            if (delay_timer >= current_base_delay_in_samples / 2) { // Blink on/off once for each delay time
                delay_timer = 0;
                tap_LED_on = !tap_LED_on;

                if (tap_LED_on) {
                    if (selfOscMode) {
                        led2.Set(1.0f);
                    } else {
                        led2.Set(0.5f);
                    }
                } else {
                    led2.Set(0.0f);
                }

	        }
      
            //int sequence_time_multiplier = 1; //Change this to 2 and multiply below to double sequence step time (this is a Thermae feature) TODO Test this
      
	        // Sequencer //
	        if ((sequence_timer >= sequence_lengths_in_samples[sequence_stage] && tapSequencerMode == false) || (tapSequencerMode == true && incrementNextSequence == true)) {
                incrementNextSequence = false;
                sequence_stage += 1;
                if (sequence_stage == 3) {
                    sequence_stage = 0;
                }
                // If INT2 is OFF, skip INT2 and go back to base sequence
                if (sequence_stage == 2 && delaySpeedMultiplier_int2 == 1.0) {
                    sequence_stage = 0;
                }


                if (sequence_stage == 0) {
                    delay1.speed = delayBaseSpeed;
                    delay2.speed = delayBaseSpeed;
                } else if (sequence_stage == 1) {
                    delay1.speed = delaySpeedMultiplier_int1 * delayBaseSpeed;
                    delay2.speed = delaySpeedMultiplier_int1 * delayBaseSpeed;
                } else if (sequence_stage == 2) {
                    delay1.speed = delaySpeedMultiplier_int1 * delaySpeedMultiplier_int2 * delayBaseSpeed;
                    delay2.speed = delaySpeedMultiplier_int1 * delaySpeedMultiplier_int2 * delayBaseSpeed;
                }

                sequence_timer = 0;
            }
	    sequence_timer += 1;

	    float delay_out = delay1.Process(leftIn);
            resonantLPF.Process(delay_out); 
            float lpf_out = resonantLPF.Low();

	    float delay_out2 = delay2.Process(rightIn);
            resonantLPF2.Process(delay_out2);
            float lpf_out2 = resonantLPF2.Low();


	    out[0][i] = leftIn * dryMix + (lpf_out * wetMix * 1.1);
            out[1][i] = rightIn * dryMix + (lpf_out2 * wetMix * 1.1);

 
        }

    } else {
        for (size_t i = 0; i < size; i++)
        {
            // Stereo or MISO 
            if (dipValues[0]) {  // Stereo
                inputL = in[0][i];
                inputR = in[1][i];
            } else {     //MISO
                inputL = in[0][i];
                inputR = in[0][i];
            }
            out[0][i] = inputL;
            out[1][i] = inputR;
        }
    }
}


void OnNoteOff(float notenumber, float velocity)
{
    voice_handler.OnNoteOff(notenumber, velocity);
}

void OnNoteOn(float notenumber, float velocity)
{
    // Note Off can come in as Note On w/ 0 Velocity
    if(velocity == 0.f)
    {
        OnNoteOff(notenumber, velocity);
    }
    else
    {
         voice_handler.OnNoteOn(notenumber, velocity);

    }
}


// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
            NoteOnEvent p = m.AsNoteOn();
            OnNoteOn(p.note, p.velocity);
        }
        break;

        case NoteOff:
        {
            NoteOffEvent p = m.AsNoteOff();
            OnNoteOff(p.note, p.velocity);
        }
        break;

        case ControlChange:
        {

            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 14:
                    midi_control[0] = true;
                    knobValues[0] = ((float)p.value / 127.0f);
                    break;
                case 15:
                    midi_control[1] = true;
                    knobValues[1] = ((float)p.value / 127.0f);
                    break;
                case 16:
                    midi_control[2] = true;
                    knobValues[2] = ((float)p.value / 127.0f);
                    break;
                case 17:
                    midi_control[3] = true;
                    knobValues[3] = ((float)p.value / 127.0f);
                    break;
                case 18:
                    midi_control[4] = true;
                    knobValues[4] = ((float)p.value / 127.0f);
                    break;
                case 19:
                    midi_control[5] = true;
                    knobValues[5] = ((float)p.value / 127.0f);
                    break;


                default: break;
            }
            break;
        }
        default: break;
    }
}



int main(void)
{
    float samplerate;

    hw.Init();
    samplerate = hw.AudioSampleRate();


    hw.SetAudioBlockSize(48);


    alternateMode = false;

    delayLine.Init();
    delayLineRev.Init();
    delay1.del = &delayLine;
    delay1.delRev = &delayLineRev;
    delay1.delayTarget = 4096; // in samples
    delay1.feedback = 0.5;
    delay1.active = true;     // Default to no delay
    delay1.reverseMode = false;

    delayBaseSpeed = delay1.delayTarget / 24000;
    delay1.speed = 1.0 * delayBaseSpeed;


    delayLine2.Init();
    delayLineRev2.Init();
    delay2.del = &delayLine2;
    delay2.delRev = &delayLineRev2;
    delay2.delayTarget = 4096; // in samples
    delay2.feedback = 0.5;
    delay2.active = true;     // Default to no delay
    delay2.reverseMode = false;

    //delayBaseSpeed = delay2.delayTarget / 24000;
    delay2.speed = 1.0 * delayBaseSpeed;

    resonantLPF.Init(SAMPLING_FREQUENCY_HZ);
    resonantLPF.SetRes(0.8f);
    resonantLPF.SetFreq(10000.0);

    resonantLPF2.Init(SAMPLING_FREQUENCY_HZ);
    resonantLPF2.SetRes(0.8f);
    resonantLPF2.SetFreq(10000.0);

    delay1.LPF.Init(SAMPLING_FREQUENCY_HZ);
    delay1.LPF.SetRes(0.5f);
    delay1.LPF.SetDrive(0.55f);
    delay1.LPF.SetFreq(1300.0f);

    delay2.LPF.Init(SAMPLING_FREQUENCY_HZ);
    delay2.LPF.SetRes(0.5f);
    delay2.LPF.SetDrive(0.55f);
    delay2.LPF.SetFreq(1300.0f);

    delay2.isRight = true; // temporary workaround for stereo modulation

    delayModRate = 0.0;
    delayModDepth = 0.0;

    modTape.Init(SAMPLING_FREQUENCY_HZ);

    voice_handler.Init(SAMPLING_FREQUENCY_HZ);
    voice_handler.SetCutoff(1300.0);



    mix.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    Lpf.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);

    regen.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    glide.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);

    int1.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    int2.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, ::daisy::Parameter::LINEAR);
    expression.Init(hw.expression, 0.0f, 1.0f, Parameter::LINEAR); 

    // Alternate Parameters
    alternateMode = false;


    force_reset = false;


    switch1[0]= Funbox::SWITCH_1_LEFT;
    switch1[1]= Funbox::SWITCH_1_RIGHT;
    switch2[0]= Funbox::SWITCH_2_LEFT;
    switch2[1]= Funbox::SWITCH_2_RIGHT;
    switch3[0]= Funbox::SWITCH_3_LEFT;
    switch3[1]= Funbox::SWITCH_3_RIGHT;
    dip[0]= Funbox::SWITCH_DIP_1;
    dip[1]= Funbox::SWITCH_DIP_2;
    dip[2]= Funbox::SWITCH_DIP_3;
    dip[3]= Funbox::SWITCH_DIP_4;

    pswitch1[0]= false;
    pswitch1[1]= false;
    pswitch2[0]= false;
    pswitch2[1]= false;
    pswitch3[0]= false;
    pswitch3[1]= false;
    pdip[0]= false;
    pdip[1]= false;
    pdip[2]= false;
    pdip[3]= false;

    update_switches = true;

    // Midi
    for( int i = 0; i < 6; ++i ) 
        midi_control[i] = false;  


    // Init the LEDs and set activate bypass
    led1.Init(hw.seed.GetPin(Funbox::LED_1),false);
    led1.Update();
    bypass = true;

    led2.Init(hw.seed.GetPin(Funbox::LED_2),false);
    led2.Update();


    hw.InitMidi();
    hw.midi.StartReceive();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while(1)
    {
        hw.midi.Listen();
        // Handle MIDI Events
        while(hw.midi.HasEvents()) 
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }

       	System::Delay(100);
    }
}