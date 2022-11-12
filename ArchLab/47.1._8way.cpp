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
	iaddq $-8, %rdx
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
	mrmovq 16(%rdi), %r10	#load_adventure_here
	mrmovq 24(%rdi), %r11
	rmmovq %r10, 16(%rsi)	# ...and store it to dst
	rmmovq %r11, 24(%rsi)
	andq %r10, %r10
	jle Npos2		# if so, goto Npos:
	iaddq $1, %rax		# count++
	
Npos2:
	andq %r11, %r11
	jle Loop3
	iaddq $1, %rax

Loop3:	mrmovq 32(%rdi), %r10	#load_adventure_here
	mrmovq 40(%rdi), %r11
	rmmovq %r10, 32(%rsi)	# ...and store it to dst
	rmmovq %r11, 40(%rsi)
	andq %r10, %r10
	jle Npos3		# if so, goto Npos:
	iaddq $1, %rax		# count++

Npos3:	
	andq %r11, %r11
	jle Loop4
	iaddq $1, %rax

Loop4:	mrmovq 48(%rdi), %r10	#load_adventure_here
	mrmovq 56(%rdi), %r11
	rmmovq %r10, 48(%rsi)	# ...and store it to dst
	rmmovq %r11, 56(%rsi)
	andq %r10, %r10
	jle Npos4		# if so, goto Npos:
	iaddq $1, %rax		# count++

Npos4:	
	andq %r11, %r11
	jle test
	iaddq $1, %rax
	
test:
	iaddq $64, %rdi
	iaddq $64, %rsi
	iaddq $-8, %rdx
	jge Loop

rest:
	iaddq $8, %rdx
	je end
	mrmovq (%rdi), %r10
    mrmovq 8(%rdi), %r11
	rmmovq %r10, (%rsi)
	andq %r10, %r10
	jle testr0
	iaddq $1, %rax
	
testr0:
	iaddq $-1, %rdx
	je end

Nposr1:
    mrmovq 16(%rdi), %r10
	rmmovq %r11, 8(%rsi)
	andq %r11, %r11
	jle testr1
	iaddq $1, %rax
testr1:
	iaddq $-1, %rdx
	je end

Nposr2:
	mrmovq 24(%rdi), %r11
	rmmovq %r10, 16(%rsi)
	andq %r10, %r10
	jle testr2
	iaddq $1, %rax
testr2:
	iaddq $-1, %rdx
	je end

Nposr3:
	mrmovq 32(%rdi), %r10
	rmmovq %r11, 24(%rsi)
	andq %r11, %r11
	jle testr3
	iaddq $1, %rax
testr3:
	iaddq $-1, %rdx
	je end

Nposr4:
	mrmovq 40(%rdi), %r11
	rmmovq %r10, 32(%rsi)
	andq %r10, %r10
	jle testr4
	iaddq $1, %rax
testr4:
	iaddq $-1, %rdx
	je end

Nposr5:
	mrmovq 48(%rdi), %r10
	rmmovq %r11, 40(%rsi)
	andq %r11, %r11
	jle testr5
	iaddq $1, %rax
testr5:
	iaddq $-1, %rdx
	je end

Nposr6:
	rmmovq %r10, 48(%rsi)
    andq %r10, %r10
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