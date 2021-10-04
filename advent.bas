let current_room = "room1"

call mainloop
die


function mainloop
	local cmp1
	local cmp2
	while 1
		call strcmp: current_room, "room1": cmp1
		call strcmp: current_room, "room2": cmp2
		if cmp1
			call room1
		else if cmp2
			call room2
		else
			print "Unknown room:", current_room
			die
		end if
	end while
end function

function default_actions
	local cmd = _arg1
	#call split: _arg1: cmd
	# command checking
	local cmp_quit
	local cmp_q
	local cmp_fart
	call strcmp: cmd[0], "quit": cmp_quit
	call strcmp: cmd[0], "q": cmp_q
	call strcmp: cmd[0], "fart": cmp_fart
	if cmp_quit || cmp_q
		print "You quit!"
		die
	else if cmp_fart
		print "You do a fart."
	end if
	#call free: cmd
end function

function room1
	local inp
	local cmd
	local cmp_east
	local cmp_e
	while 1
		print "You are in a dark room."
		print "exits: east"
		input inp
		call split: inp: cmd
		# all room actions
		call default_actions: cmd
		# room1 actions
		call strcmp: cmd[0], "east": cmp_east
		call strcmp: cmd[0], "e": cmp_e
		if cmp_east || cmp_e
			let current_room = "room2"
			return
		end if
		call free: cmd
	end while
end function

function room2
	local inp
	local cmd
	local cmp_west
	local cmp_w
	while 1
		print "You are in a bright room."
		print "exits: west"
		input inp
		call split: inp: cmd
		# all room actions
		call default_actions: cmd
		# room2 actions
		call strcmp: cmd[0], "west": cmp_west
		call strcmp: cmd[0], "w": cmp_w
		if cmp_west || cmp_w
			let current_room = "room1"
			return
		end if
		call free: cmd
	end while
end function