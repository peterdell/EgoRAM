;	@com.wudsn.ide.lng.mainsourcefile=EgoUridium.asm

	.if 0 = 1
	icl "EgoUridium.asm"
	.endif
	
loop	lda vcount
	cmp #15
	bcc loop
	ldx #0
kernel	sta wsync
	stx colbk
	inx
	cpx #192
	bne kernel
	lda #0
	sta wsync
	sta colbk

	lda trig0
	beq fire
	lda consol
	and #1
	beq start
	jmp loop

fire
start
	lda #0
	jmp main
