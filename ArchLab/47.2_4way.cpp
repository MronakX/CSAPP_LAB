#/* $begin ncopy-ys */
##################################################################
# ncopy.ys - Copy a src block of len words to dst.
# Return the number of positive words (>0) contained in src.
#
# Include your name and ID here.
#
# Describe how and why you modified the baseline code.
#
##################################################################
# Do not modify this portion
# Function prologue.
# %rdi = src, %rsi = dst, %rdx = len
ncopy:

##################################################################
# You can modify this portion
	# Loop header
	xorq %rax,%rax		# count = 0;
	iaddq $-4, %rdx
	jl rest

Loop:	mrmovq (%rdi), %r10	#load_adventure_here
	mrmovq 8(%rdi), %r11
	rmmovq %r10, (%rsi)	# ...and store it to dst
	rmmovq %r11, 8(%rsi)
	andq %r10, %r10
	jle Npos1		# if so, goto Npos:
	iaddq $1, %rax		# count++

Npos1:	
	andq %r11, %r11
	jle Loop2
	iaddq $1, %rax
Loop2:
	mrmovq 16(%rdi), %r8	#load_adventure_here
	mrmovq 24(%rdi), %r9
	rmmovq %r8, 16(%rsi)	# ...and store it to dst
	rmmovq %r9, 24(%rsi)
	andq %r8, %r8
	jle Npos2		# if so, goto Npos:
	iaddq $1, %rax		# count++
	
Npos2:
	andq %r9, %r9
	jle test
	iaddq $1, %rax
	
test:
	iaddq $32, %rdi
	iaddq $32, %rsi
	iaddq $-4, %rdx
	jge Loop
rest:
	iaddq $4, %rdx
	je end
	mrmovq (%rdi), %r10
	andq %r10, %r10
	rmmovq %r10, (%rsi)
	jle testr1
	iaddq $1, %rax
	
testr1:
	iaddq $-1, %rdx
	je end

Nposr1:
	mrmovq 8(%rdi), %r11
	andq %r11, %r11
	rmmovq %r11, 8(%rsi)
	jle testr2
	iaddq $1, %rax
testr2:
	iaddq $-1, %rdx
	je end

Nposr2:
	mrmovq 16(%rdi), %r8
	andq %r8, %r8
	rmmovq %r8, 16(%rsi)
	jle end
	iaddq $1, %rax
	
end:
	ret
	
##################################################################
# Do not modify the following section of code
# Function epilogue.
Done:
	ret
##################################################################
# Keep the following label at the end of your function
End:
#/* $end ncopy-ys */