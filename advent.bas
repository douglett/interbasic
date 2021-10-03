call room1
die


function room1_test
	let inp
	let cmd
	let cmp_fart
	let cmp_quit

	while 1
		print "You are in a dark room. What to do?"
		input inp
		call split: inp: cmd
		# check for farting
		call strcmp: cmd[0], "fart": cmp_fart
		call strcmp: cmd[0], "quit": cmp_quit
		if cmp_fart
			print "You farted."
		# check for quitting
		else if cmp_quit
			print "You quit."
			break
		# we are not farting or quitting
		else
			print "You did not fart."
		end if
	end while
end function


function room1
	let desc = "You are in a dark room."
	let inp
	while 1
		print desc
		input inp
	end while
end function