
		.proc egoram

		.proc waitForCommand				;Wait for completion of the previous command
		.if EGO_RAM_PRESENT
waitForCommand
		lda ego_reg.STATUS
		bmi waitForCommand
		.endif
		rts
		.endp
	
		.endp
