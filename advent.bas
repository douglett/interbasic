call room1
die


function room1
	let inp
	let cmd
	let cmp
	
	while 1
		print "You are in a dark room. What to do?"
		input inp
		call split: inp: cmd
		call strcmp: cmd[0], "fart": cmp
		if cmp
			print "You farted."
		end if
		if cmp != 1
			print "You did not fart."
		end if
	end while
end function