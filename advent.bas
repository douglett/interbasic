let current_room = "room1"

call mainloop
print "The end.", _ret
die


function mainloop
	while 1
		call strcmp: current_room, "room1"
		local cmp1 = _ret
		call strcmp: current_room, "room2"
		local cmp2 = _ret
		if cmp1
			call room1
		else if cmp2
			call room2
		else
			print "Unknown room:", current_room
			die
		end if

		if _ret
			return _ret
		end if
	end while
end function

function default_actions
	local cmd = _arg1
	#call split: _arg1: cmd
	# command checking
	call strcmp: cmd[0], "quit"
	local cmp_quit = _ret
	call strcmp: cmd[0], "q"
	local cmp_q = _ret
	call strcmp: cmd[0], "fart"
	local cmp_fart = _ret
	if cmp_quit || cmp_q
		print "You quit!"
		return 1000
	else if cmp_fart
		print "You do a fart."
	end if
	#call free: cmd
end function

function room1
	local inp
	while 1
		print "You are in a dark room."
		print "exits: east"
		input inp
		call split: inp
		local cmd = _ret
		# all room actions
		call default_actions: cmd
		if _ret
			return _ret
		end if
		# room1 actions
		call strcmp: cmd[0], "east"
		local cmp_east = _ret
		call strcmp: cmd[0], "e"
		local cmp_e = _ret
		if cmp_east || cmp_e
			print "You walk east."
			let current_room = "room2"
			return
		end if
		call free: cmd
	end while
end function

function room2
	local inp
	while 1
		print "You are in a bright room."
		print "exits: west"
		input inp
		call split: inp
		local cmd = _ret
		# all room actions
		call default_actions: cmd
		if _ret
			return _ret
		end if
		# room2 actions
		call strcmp: cmd[0], "west"
		local cmp_west = _ret
		call strcmp: cmd[0], "w"
		local cmp_w = _ret
		if cmp_west || cmp_w
			print "You walk west."
			let current_room = "room1"
			return
		end if
		call free: cmd
	end while
end function