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

# test string splitting
print "splitting"
let words
call split: "  hello   world string ": words
#let words = _ret
# number of splits
call len: words
let words_len = _ret
print "len", words_len
# 2nd item in list
call at: words, 1
let w1 = _ret
print "word 1"; w1
# testing short arrays
print "words[1]", words[1]

# object properties
call make
let myobj = _ret
call setprop: myobj, "prop1", 1000
# testing short properties
print "myobj.prop1 is", myobj.prop1

print "the end"
unlet d  # will be undefined if it exists, otherwise ok
die

function f
	print "this is function f"
	print "args"; _arg1; _arg2
end function