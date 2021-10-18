dim string current_room = "room1"

dim maf = 2+2-3
print "quick maf", maf
print "quick maf", 2+2-3

call mainloop
print "The end.", _ret
die


function mainloop
	dim cmp1, cmp2
	while 1
		call strcmp: current_room, "room1"
		let cmp1 = _ret
		call strcmp: current_room, "room2"
		let cmp2 = _ret
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
	dim array& cmd = _arg1
	# call split: _arg1: cmd
	# command checking
	call strcmp: cmd[0], "quit"
	dim cmp_quit = _ret
	call strcmp: cmd[0], "q"
	dim cmp_q = _ret
	call strcmp: cmd[0], "fart"
	dim cmp_fart = _ret
	if cmp_quit || cmp_q
		print "You quit!"
		return 1000
	else if cmp_fart
		print "You do a fart."
	end if
	#call free: cmd
end function

function room1
	dim cmp_east, cmp_e
	dim string inp
	dim array cmd
	while 1
		print "You are in a dark room."
		print "exits: east"
		input inp
		call resize: cmd, 0
		call split: cmd, inp
		# sanity check
		call len: cmd
		if _ret == 0
			continue
		end if
		# all room actions
		call default_actions: cmd
		if _ret
			return _ret
		end if
		# room1 actions
		call strcmp: cmd[0], "east"
		let cmp_east = _ret
		call strcmp: cmd[0], "e"
		let cmp_e = _ret
		if cmp_east || cmp_e
			print "You walk east."
			let current_room = "room2"
			return
		end if
		# call free: cmd
	end while
end function

function room2
	dim cmp_west, cmp_w
	dim string inp
	dim array cmd
	while 1
		print "You are in a bright room."
		print "exits: west"
		input inp
		call resize: cmd, 0
		call split: cmd, inp
		# all room actions
		call default_actions: cmd
		if _ret
			return _ret
		end if
		# room2 actions
		call strcmp: cmd[0], "west"
		let cmp_west = _ret
		call strcmp: cmd[0], "w"
		let cmp_w = _ret
		if cmp_west || cmp_w
			print "You walk west."
			let current_room = "room1"
			return
		end if
		# call free: cmd
	end while
end function