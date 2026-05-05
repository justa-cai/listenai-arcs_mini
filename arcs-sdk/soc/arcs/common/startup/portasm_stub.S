/*
 * Weak stubs for FreeRTOS portable symbols (irq_entry, exc_entry,
 * default_intexc_handler). When FreeRTOS is linked, its portasm.S
 * provides strong definitions that override these. When FreeRTOS is
 * disabled (e.g. minimal loader), these spin-forever stubs satisfy the
 * linker; interrupts/exceptions are not expected in that context.
 */

.weak exc_entry
.type exc_entry, @function
exc_entry:
1:  j 1b
.size exc_entry, . - exc_entry

.weak irq_entry
.type irq_entry, @function
irq_entry:
1:  j 1b
.size irq_entry, . - irq_entry

.weak default_intexc_handler
.type default_intexc_handler, @function
default_intexc_handler:
1:  j 1b
.size default_intexc_handler, . - default_intexc_handler
