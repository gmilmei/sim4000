begin
   integer i, sum;
   real r;
   integer procedure twice(n);
   value n; integer n;
   twice := n * 2;

   sum := 0;
   for i := 1 step 1 until 10 do sum := sum + i;
   r := 1.5 * 2.0;
   if twice(sum) = 110 and r = 3.0 then
      write(out, <:Protected arithmetic ok<10>:>)
   else
      write(out, <:Protected arithmetic failed<10>:>)
end
