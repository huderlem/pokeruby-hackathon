HackGym_MapScripts::
	.byte 0

S_HackGym_Statue::
	msgbox Text_HackGym_Statue, 4
	releaseall
	end

Text_HackGym_Statue::
	.string "RANDOMLY-GENERATED\n"
	.string "POKéMON GYM$"

S_HackGym_Leader::
	trainerbattle 1, TRAINER_WATTSON, 0, Text_HackGym_Leader_Text1, Text_HackGym_Leader_Text2, S_HackGym_Leader_OnDefeated
	msgbox Text_HackGym_Leader_After, 4
	release
	end

S_HackGym_Leader_OnDefeated::
	setflag FLAG_RECEIVED_TM40
	end

Text_HackGym_Leader_Text1::
	.string "I am MR. RAND. Battle me!$"

Text_HackGym_Leader_Text2::
	.string "Amazing, well done!$"

Text_HackGym_Leader_After::
	.string "You already beat me.$"

S_HackGym_Regenerate::
	lockall
	mapgen
	releaseall
	end

S_HackGym_Trainer1::
	trainerbattle 0, TRAINER_WILL, 0, Text_HackGym_Trainer1_Text1, Text_HackGym_Trainer1_Text2
	msgbox Text_HackGym_Trainer1_Text3, 6
	end

Text_HackGym_Trainer1_Text1::
	.string "I am trainer 1.$"

Text_HackGym_Trainer1_Text2::
	.string "You beat me, trainer 1.$"

Text_HackGym_Trainer1_Text3::
	.string "You already beat me!$"

S_HackGym_Trainer2::
	trainerbattle 0, TRAINER_WILL, 0, Text_HackGym_Trainer2_Text1, Text_HackGym_Trainer2_Text2
	msgbox Text_HackGym_Trainer2_Text3, 6
	end

Text_HackGym_Trainer2_Text1::
	.string "I am trainer 2.$"

Text_HackGym_Trainer2_Text2::
	.string "You beat me, trainer 2.$"

Text_HackGym_Trainer2_Text3::
	.string "Move along!$"
