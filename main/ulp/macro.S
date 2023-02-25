.macro  push rx
    st      \rx, r3, 0
    add     r3, r3, 1
.endm

.macro  pop rx
    sub     r3, r3, 1
    ld      \rx, r3, 0
.endm

.macro  psr
    .set    addr, (.+16)
    move    r1, addr
    push    r1
.endm

.macro  ret
    pop     r1
    jump    r1
.endm
