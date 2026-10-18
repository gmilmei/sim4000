begin
  comment Flush the definition reply before waiting, so two independent
          clients can prove that their transports coexist. If a second
          blocking wait is refused, use bounded status queries instead;
  integer array c(1:30), r(1:26);
  integer i, result, transportno, queries;
  boolean polling;
  for i:=1 step 1 until 30 do c(i):=-1;
  c(1):=c(2):=0;
  c(3):=long <:ptr:> shift (-24);
  c(4):=c(5):=c(6):=0;
  c(9):=long <:dev:> shift (-24);
  c(10):=c(11):=c(12):=0;
  c(21):=long <:pin:> shift (-24);
  c(22):=c(23):=c(24):=0;
  c(26):=long <:plp:> shift (-24);
  c(27):=c(28):=c(29):=0;
  result:=transfer(2,c,30,r,11);
  write(out,<:primo define :>,<<d>,result,<: reply :>,r(1),
            <: transport :>,r(2),<:<10>:>);
  setposition(out,0,0);
  if result=0 and r(1)=0 then begin
    transportno:=r(2);
    for i:=1 step 1 until 9 do c(i):=-1;
    c(2):=transportno;
    result:=transfer(6,c,9,r,26);
    polling:=result=0 and r(1)=3;
    if polling then begin
      write(out,<:primo wait unavailable: polling<10>:>);
      setposition(out,0,0);
      queries:=0;
poll: result:=transfer(4,c,9,r,26);
      queries:=queries+1;
      if result=0 and r(1)=0 and r(21)<5 and queries<1000000
      then goto poll;
    end;
    if polling then write(out,<:primo poll :>)
               else write(out,<:primo wait :>);
    write(out,<<d>,result,<: reply :>,r(1),
              <: state :>,r(21),<: cause :>,r(25),<:<10>:>);
  end;
end
