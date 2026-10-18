# Proposal: Device stack for suspend and resume

Status: IDEA (not implemented), noted 2026-10-18.

Writing a device number to the device selector shall suspend the
operation of the currently active device and push it on a stack. A
write to a second register performs the pull and resumes the suspended
device.

Benefits:

- Interrupt routines on the Atari can simply activate their device
  without reading and temporarily storing the previous device id —
  select on entry, pull on exit.
- Using the graphics device can suspend the MOS6502 CPU device: a long
  EXECUTE no longer blocks graphics commands, and the virtual CPU
  resumes afterwards.

## Open design questions

- "Suspend" must cover the parser state, not just the device number:
  the active device may be mid-header or mid-stream (`ego_state`,
  `ego_cmd`, `ego_params`/`ego_param_idx`, stream in/out pointers), so
  the stack holds the parser context per entry, and the parser state
  becomes part of what EGO_REG_DEVICE pushes and the pull restores.
- Suspending a device whose command is currently executing on core 1
  (e.g. MOS6502 EXECUTE) means interrupting or time-slicing the
  handler — today handlers run to completion. This is the hard part.
- Stack depth (nesting level of interrupt routines, e.g. 4 or 8
  entries) and the behavior on overflow/underflow.
- Which register performs the pull: a new EGO_REG (append to EgoReg /
  ego_reg) whose written value is ignored, or a readable "pop"
  register.
- Whether writing the selector keeps its current "switch and abort"
  semantics when the stack is full or for clients that never pull.
