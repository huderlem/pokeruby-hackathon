LittlerootTown_MapScripts:: @ 814D509
	map_script 3, LittlerootTown_MapScript1_14D514
	.byte 0

LittlerootTown_MapScript1_14D514:: @ 814D514
	setflag FLAG_VISITED_LITTLEROOT_TOWN
	end

LittlerootTown_Sign_StartHacking:: @ 814C087
	msgbox Text_LittlerootTown_StartHacking, 3
	end
