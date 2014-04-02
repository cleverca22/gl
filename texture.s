nop
nop
mov r0,vary
fadd r0,r0,r5
mov r1,vary
fadd r1,r1,r5
#mov r3,unif
mov t0t,r1
mov t0s,r0
nop
nop
nop
# load texture pixel
nop			;nop			;ldtmu0
nop						;sbwait

mov r0,vary
fadd r0,r0,r5
nop			; mov r1.8a, r0

mov r0,vary
fadd r0,r0,r5
nop			; mov r1.8b, r0

mov r0,vary
fadd r0,r0,r5
nop			; mov r1.8c, r0

nop			; mov r1.8d, 1.0

mov ra5, r1

nop			; v8muld ra4,r1,r4			# mult the varying color by the texture color


nop			; mov r0,r4.8dr				# texture alpha into r0
nop			; v8muld ra1, ra4, r0	;loadc	# r0 * texture -> ra1
# store texture pixel in ra0
nop			; mov r1,r4.8dr				# tile alpha into r1
nop			; v8muld ra2, r4, r1			# r1 * tile -> ra2
ldi r2,0xffffffff
v8subs r1,r2,r0
nop			; v8muld r3, ra2, r1			# above times (1-alphaA)
v8adds r0,r3,ra1							# r0 now has the final color?
mov tlbc,r0		;nop			;thrend
nop
nop ; nop; sbdone
