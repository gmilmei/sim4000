; Minimal FP-loadable program for the native assembler checkpoint.
; Entry offset 4 skips the two reserved words.
; Calculate 123 + 321, then remain in a relative self-loop for inspection.
s. w.
    0, 0
    al w0 123
    al w1 321
    wa w0 2
    jl. 0
e.
