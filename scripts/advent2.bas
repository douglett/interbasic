type room_t
	member room_t string name
	member room_t string description
	member room_t string exit_n
	member room_t string exit_s
	member room_t string exit_e
	member room_t string exit_w

dim room_t[] rooms
dim croom
call main
die


function main
	call buildrooms
	print "Welcome to the depths."
	call mainloop
end function

function buildrooms
	call resize: rooms, 2
	# -----
	let rooms[0].name = "entrance"
	let rooms[0].description = "A dark cave mouth yawns forbodingly here."
	# let rooms[0].exit_n = "cave1"
	let rooms[0].exit_w = "forest1"
	# -----
	let rooms[1].name = "forest1"
	let rooms[1].description = "Dusk bathes this forest clearing in warm, mottled sunlight."
	let rooms[1].exit_e = "entrance"
end function

function mainloop
	dim string inp
	dim string[] cmd, exits
	dim do_look = 1
	dim cmp_q, cmp_quit, cmp_l, cmp_look
	dim cmp_e, cmp_east, cmp_w, cmp_west
	while 1
		# show room
		if do_look
			print rooms[croom].description
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
		call strcmp: cmd[0], "e"
		let cmp_e = _ret
		call strcmp: cmd[0], "east"
		let cmp_east = _ret
		call strcmp: cmd[0], "w"
		let cmp_w = _ret
		call strcmp: cmd[0], "west"
		let cmp_west = _ret
		# actions
		if cmp_q || cmp_quit
			return
		else if cmp_l || cmp_look
			print "You look around, but see nothing extraordinary."
			let do_look = 1
		else if cmp_e || cmp_east
			call move: 1
			if _ret
				print "You go east."
				let do_look = 1
			else
				print "You can't go east."
			end if
		else if cmp_w || cmp_west
			call move: 3
			if _ret
				print "You go west."
				let do_look = 1
			else
				print "You can't go west."
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
	if dir == 1
		let target = rooms[croom].exit_e
	else if dir == 3
		let target = rooms[croom].exit_w
	end if
	dim i, l
	call len: rooms
	let l = _ret
	while i < l
		print "here", i, l
		call strcmp: rooms[i].name, target
		if _ret
			let croom = i
			return 1
		end if
		let i = i + 1
	end while
	return 0
end function