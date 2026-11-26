.text
    lui   $s0, 0x1000       # A = 0x10000000
    lui   $t0, 0x0001       # 64 KiB stride
    addu  $s1, $s0, $t0     # B = A + 64 KiB, same direct-mapped set
    lw    $t2, 0($s0)       # A
    lw    $t3, 0($s1)       # B evicts A in test12
    lw    $t2, 0($s0)       # A evicts B in test12
    lw    $t3, 0($s1)       # B evicts A in test12
    addiu $v0, $zero, 10
    syscall
