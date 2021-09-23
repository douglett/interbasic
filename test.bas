let a = 1
let b = "hello world"  # a string literal!
let c = 0

print a, b; 101

if a
	print "a is " a
	print "you printed a"
end if

if c
	print "c is " c
	print "you printed c"
	if a
		print "testing a"
	end if
end if

call f

print "the end"
unlet d  # will be undefined if it exists, otherwise ok
die

function f
	print "this is function f"
end function