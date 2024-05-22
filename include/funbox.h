namespace funbox
{
	class Funbox
	{
		public:
			enum Sw
			{
				FOOTSWITCH_1 = 0,
				FOOTSWITCH_2 = 1,
				SWITCH_1_LEFT = 2,
				SWITCH_1_RIGHT = 3, 
				SWITCH_2_LEFT = 4,
				SWITCH_2_RIGHT = 5, 
				SWITCH_3_LEFT = 6,
				SWITCH_3_RIGHT = 7,
				SWITCH_DIP_1 = 8,
				SWITCH_DIP_2 = 9,
				SWITCH_DIP_3 = 10, // On v2/v3 boards only, not v1
				SWITCH_DIP_4 = 11  // On v2/v3 boards only, not v1
			};

			enum Knob
			{
				KNOB_1 = 0,
				KNOB_2 = 2,
				KNOB_3 = 4,
				KNOB_4 = 1,
				KNOB_5 = 3,
				KNOB_6 = 5,
                                EXPRESSION = 6 // On v3 board only
			};
			
			enum LED
			{
				LED_1 = 22,
				LED_2 = 23
			};
       
                        // Note: MIDI Input is available on v2/v3 board by pin 37, D30
                        //       MIDI Out is available on v2 board by pin 36, D29

	};
}
