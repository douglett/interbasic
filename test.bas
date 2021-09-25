let a = 1
let b = "hello world"  # a string literal!
let c = 0

print a, b; 101

if a
	print "a is " a
	print "you printed a"
end if

if c == a
	print "c is " c
	print "you printed c"
	if a
		print "testing a"
	end if
end if

call f: a, b

print "splitting"
call split: "hello   world"
let blah = _ret
call len: blah
let blah_len = _ret
print "len", blah_len
let r1 = blah[0]
print "r1", r1
#print "[" blah[0] "] [" blah[1] "]"

print "the end"
unlet d  # will be undefined if it exists, otherwise ok
die

function f
	print "this is function f"
	print "args"; _arg1; _arg2
end function