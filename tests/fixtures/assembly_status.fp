checkbad=slang checksrc
if ok.yes
badcheck=tohost conv.bin checkbad
if ok.no
lookup fromhost
lookup checkbad
clear temp checkbad
checkgood=slang checkvalid
if ok.no
nosuchprog
if ok.yes
checkbin=tohost conv.bin checkgood
lookup checkgood
clear temp checkgood
lookup slang
