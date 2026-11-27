.text
    lui   $s0, 0x1000       # data base = 0x10000000
    addiu $t0, $zero, 4096  # 16 KiB = 4096 words
    addiu $t1, $zero, 1

Init:
    sw    $t1, 0($s0)
    addiu $t1, $t1, 1
    addiu $s0, $s0, 4
    addiu $t0, $t0, -1
    bne   $t0, $zero, Init

    addiu $s2, $zero, 3     # scan the array three times
    addu  $s3, $zero, $zero # checksum

Scan:
    lui   $s0, 0x1000
    addiu $s4, $zero, 4096
ScanLoop:
    lw    $t1, 0($s0)
    addu  $s3, $s3, $t1
    addiu $s0, $s0, 4
    addiu $s4, $s4, -1
    bne   $s4, $zero, ScanLoop
    addiu $s2, $s2, -1
    bne   $s2, $zero, Scan

    addiu $v0, $zero, 10
    syscall
