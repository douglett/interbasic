type room_t
	member room_t string name
	member room_t string description
	member room_t string exit_n
	member room_t string exit_s
	member room_t string exit_e
	member room_t string exit_w
	member room_t string action1
	member room_t string takeaction1

dim room_t[] rooms
dim croom
call main
die


function main
	call buildrooms
	print "Welcome to the depths."
	call mainloop
	print ""
	if _ret
		print "You win!"
	else
		print "You are dead."
	end if
	print "Thanks for playing."
end function

function buildrooms
	call resize: rooms, 7
	# -----
	let rooms[0].name = "entrance"
	let rooms[0].description = "A dark cave mouth yawns forbodingly here."
	let rooms[0].exit_n = "cave1"
	let rooms[0].exit_w = "forest1"
	# -----
	let rooms[1].name = "forest1"
	let rooms[1].description = "Dusk bathes this forest clearing in warm, mottled sunlight."
	let rooms[1].exit_e = "entrance"
	# -----
	let rooms[2].name = "cave1"
	let rooms[2].description = "It's pitch black. If only I had a torch..."
	let rooms[2].action1 = "torch"
	let rooms[2].takeaction1 = "You light the torch. The room brightens up."
	# -----
	let rooms[3].name = "cave2"
	let rooms[3].description = "A damp and mysterious cave. A torch flickers here."
	let rooms[3].exit_e = "goblin1"
	let rooms[3].exit_w = "dragon1"
	# -----
	let rooms[4].name = "dragon1"
	let rooms[4].description = "Oops! A horrible dragon is here! A half-eaten knight lays near you, holding a sword."
	let rooms[4].action1 = "sword"
	let rooms[4].exit_e = "cave2"
	let rooms[4].takeaction1 = "You thought you were tougher than a knight? No, you were weak an tender, and rather succulent when roasted, according the dragon."
	# -----
	let rooms[5].name = "goblin1"
	let rooms[5].description = "A snarling hobgobling chews some filthy meat here. He sits with his back to you, next to a rather large rock."
	let rooms[5].exit_w = "cave2"
	let rooms[5].action1 = "rock"
	let rooms[5].takeaction1 = "With a hefty crack you bring the rock down, splitting his skull. A fitting end. There was something behind him... a crack in the wall."
	# -----
	let rooms[6].name = "exit"
	let rooms[6].description = "You escape from the cave into the medow beyond. Your nightmare adventure is finally at an end! You roll around jubilantly in the grass, disturbing the meadow badgers, which eat you."
end function

function mainloop
	dim string inp, dirstr
	dim string[] cmd, exits
	dim do_look = 1
	dim cmp_q, cmp_quit, cmp_l, cmp_look
	dim cmp_n, cmp_north, cmp_s, cmp_south, cmp_e, cmp_east, cmp_w, cmp_west
	dim cmp_action1, action_torch, action_sword, action_rock
	while 1
		# show room
		if do_look
			print rooms[croom].description
			if croom == 6  # final room
				return 1
			end if
			call make_exit_str
			print "exits:", _ret
			let do_look = 0
		end if
		# get input
		input inp
		call split: cmd, inp
		call len: cmd
		if _ret == 0
			continue
		end if
		# querys
		call strcmp: cmd[0], "q"
		let cmp_q = _ret
		call strcmp: cmd[0], "quit"
		let cmp_quit = _ret
		call strcmp: cmd[0], "l"
		let cmp_l = _ret
		call strcmp: cmd[0], "look"
		let cmp_look = _ret
		call strcmp: cmd[0], "n"
		let cmp_n = _ret
		call strcmp: cmd[0], "north"
		let cmp_north = _ret
		call strcmp: cmd[0], "s"
		let cmp_s = _ret
		call strcmp: cmd[0], "south"
		let cmp_south = _ret
		call strcmp: cmd[0], "e"
		let cmp_e = _ret
		call strcmp: cmd[0], "east"
		let cmp_east = _ret
		call strcmp: cmd[0], "w"
		let cmp_w = _ret
		call strcmp: cmd[0], "west"
		let cmp_west = _ret
		# room specific actions
		call strcmp: cmd[0], rooms[croom].action1
		let cmp_action1 = _ret
		call strcmp: cmd[0], "torch"
		let action_torch = _ret
		call strcmp: cmd[0], "sword"
		let action_sword = _ret
		call strcmp: cmd[0], "rock"
		let action_rock = _ret
		# actions
		if cmp_q || cmp_quit
			print "You lie down and rot."
			return
		else if cmp_l || cmp_look
			print "You look around you."
			let do_look = 1
		else if cmp_n || cmp_north || cmp_s || cmp_south || cmp_w || cmp_west || cmp_e || cmp_east
			if cmp_n || cmp_north
				let dirstr = "north"
				call move: 0
			else if cmp_s || cmp_south
				let dirstr = "south"
				call move: 2
			else if cmp_e || cmp_east
				let dirstr = "east"
				call move: 1
			else if cmp_w || cmp_west
				let dirstr = "west"
				call move: 3
			end if
			if _ret
				print "You go " dirstr "."
				let do_look = 1
			else
				print "You can't go " dirstr "."
			end if
		else if cmp_action1
			print "You try to use the " rooms[croom].action1 "."
			print rooms[croom].takeaction1
			if action_torch
				let croom = 3  # hard coded. blah 
			else if action_sword
				return 0
			else if action_rock
				let rooms[croom].exit_e = "exit"
			end if
		else
			print "You flail around uselessly."
		end if
	end while
end function

function make_exit_str
	dim string[] exits
	dim room_t& r = rooms[croom]
	call len: r.exit_n
	if _ret > 0
		call push: exits, "n"
	end if
	call len: r.exit_s
	if _ret > 0
		call push: exits, "s"
	end if
	call len: r.exit_e
	if _ret > 0
		call push: exits, "e"
	end if
	call len: r.exit_w
	if _ret > 0
		call push: exits, "w"
	end if
	call join: exits, ", "
	return _ret
end function

function move
	dim dir = _arg1
	dim string target
	if dir == 0
		let target = rooms[croom].exit_n
	else if dir == 1
		let target = rooms[croom].exit_e
	else if dir == 2
		let target = rooms[croom].exit_s
	else if dir == 3
		let target = rooms[croom].exit_w
	end if
	dim i, l
	call len: rooms
	let l = _ret
	while i < l
		# print "here", i, l
		call strcmp: rooms[i].name, target
		if _ret
			let croom = i
			return 1
		end if
		let i = i + 1
	end while
	return 0
end function