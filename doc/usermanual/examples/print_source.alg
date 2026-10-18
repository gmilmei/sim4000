begin
   zone source(128,1,stderror), printer(128,1,stderror);
   integer character;
   open(source,4,<:cpuregtest:>,0);
   open(printer,0,<:printer:>,0);
   readchar(source,character);
   while character<>25 do
   begin
      outchar(printer,character);
      readchar(source,character)
   end;
   close(printer,true);
   close(source,true)
end
