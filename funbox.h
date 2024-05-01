namespace Funbox
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
				SWITCH_DIP_2 = 9
			};

			enum Knob
			{
				KNOB_1 = 0,
				KNOB_2 = 2,
				KNOB_3 = 4,
				KNOB_4 = 1,
				KNOB_5 = 3,
				KNOB_6 = 5
			};
			
			enum LED
			{
				LED_1 = 22,
				LED_2 = 23
			};
	};
}
