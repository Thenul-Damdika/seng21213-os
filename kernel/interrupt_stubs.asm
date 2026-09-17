[BITS 32]

[GLOBAL timer_isr]
[EXTERN timer_handler]
[EXTERN scheduler_switch]

timer_isr:
    ; Save all general-purpose registers
    pusha

    ; timer_handler() returns:
    ;   0 = no context switch
    ;   1 = switch process
    call timer_handler

    test eax, eax
    jz .no_switch

    ; Current ESP points to the saved register frame
    push esp
    call scheduler_switch
    add esp, 4

    ; scheduler_switch() returns the next process ESP
    mov esp, eax

.no_switch:

    ; End Of Interrupt to master PIC
    mov al, 0x20
    out 0x20, al

    ; Restore registers
    popa

    ; Restore EIP, CS and EFLAGS
    iretd
