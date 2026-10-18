lookup slang
if ok.yes
lookup algol
lookup slang
if ok.no
nosuchprog
lookup absentone
if ok.no
lookup fromhost
lookup absentone
if ok.yes
nosuchprog
lookup tohost
lookup slang
if ok.no
(
nosuchprog
lookup absentone
)
lookup slang
if ok.yes
(
lookup algol
lookup fromhost
)
lookup tohost
