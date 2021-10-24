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
	call resize: rooms, 1
	let rooms[0].name = "entrance"
	let rooms[0].description = "A dark cave mouth yawns forbodingly here."
end function

function mainloop
	dim string inp
	dim string[] cmd
	dim cmp_q, cmp_quit, cmp_l, cmp_look
	while 1
		print rooms[croom].description
		input inp
		call split: cmd, inp
		
		call len: cmd
		if _ret == 0
			continue
		end if

		call strcmp: cmd[0], "q"
		let cmp_q = _ret
		call strcmp: cmd[0], "quit"
		let cmp_quit = _ret
		call strcmp: cmd[0], "l"
		let cmp_l = _ret
		call strcmp: cmd[0], "look"
		let cmp_look = _ret
		if cmp_q || cmp_quit
			return
		else if cmp_l || cmp_look
			print "You look around, but see nothing extraordinary."
		end if
	end while
end function