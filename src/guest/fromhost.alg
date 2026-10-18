; Native fromhost utility, derived from the historical TFROMDOS.ALG.
; The monitor endpoint remains <:dos:> until a rebuilt monitor provides host.
; Host text uses LF; conv.bin preserves the transferred bytes.
begin
   zone zi(768*6,1,znp);
   zone zo(768*6,1,stderror);
   integer array ia(1:10),ans(1:8);
   long array toname(1:2),la,fromname,dummy(1:11);
   integer i,j,p,open_close_op,open_st,open_close_mode;
   boolean finis,conv;

<********************************************************************
*****
*****               Block procedure for host-file interface
*****
*********************************************************************>

   procedure znp(z,s,b);
   zone z; integer s,b;

   begin
      integer array zia(1:20),sia(1:12),ia(1:10);
      integer i,j;

      getzone6(z,zia);
      getshare6(z,sia,1);

      if zia(2)=0 then
      begin
         open_st:=0;
         zia(2):=long <:dos:> shift (-24);
         zia(3):=zia(4):=zia(5):=0;
         setzone6(z,zia);

         if monitor(4,z,0,ia)=0 then err(<:host-file process not found:>,0);
         i:=monitor(8,z,0,ia);
         if i<>0 then err(<:reserve host-file process result:>,i);

         sia(4):=open_close_op shift 12 + open_close_mode;
         setshare6(z,sia,1);
         repeat
            monitor(16,z,1,zia);
            i:=monitor(18,z,1,ans);
         until open_close_op extract 1=0 or i<>1 or ans(1)<>0 or ans(2)<>0;
         open_st:=ans(1);
         finis:=false;
      end else

      begin
         if s extract 1=1 then stderror(z,s,b);
         if sia(4) shift (-12)=3 then
         begin  <* input *>
            finis:=b=0;
            if finis then b:=2;  
         end;
      end;
   end proc. znp;

<********************************************************************
*****
*****                  Error reporting procedures
*****
*********************************************************************>

   procedure err(s,i);
   string s; integer i;

   begin
      write(out,s,<< b>,i,"nl",1);
      trapmode:=-1;
      trap(1);
   end proc. err;
   


   procedure par_err(s,a);
   string s; long array a;

   begin
      write(out,<:*** :>,s);
      if a(1)=extend 1 shift 47 then else
      if a(1) shift (-40)>0 then write(out,a) else write(out,a(1));
      outchar(out,'nl');
      trapmode:=-1;
      trap(1);
   end proc. par_err;

<********************************************************************
*****
*****                      get call parameters
*****
*********************************************************************>

   if nsystem(4,1,toname)<>6 shift 12+10 then nsystem(4,0,<:l.side:>);
   nsystem(4,0,toname);
   
   if nsystem(4,1,toname)<>6 shift 12+10 then err(<:*** no left side in call:>,0);
   nsystem(4,0,toname);

   p:=2;
   fromname(1):=0;
   conv:=false;
   open_close_mode:=2;  <* default: host text, byte-preserving character mode *>

   for i:=nsystem(4,increase(p),la) while i<>0 do
   begin
      if i shift (-12)<>4 then par_err(<:param: :>,la);
      
      if nsystem(4,p,dummy) shift (-12) <> 8 then
      begin  <* from-name *>
         if i extract 12 < 10 or
            fromname(1)<>0 then par_err(<:bad param: :>,la);
         tofrom(fromname,la,44);
      end else

      if la(1)=long <:conv:> then
      begin
         if conv then par_err(<:param. repeated: :>,la);
         if nsystem(4,increase(p),la)<>8 shift 12+10 then
            par_err(<:bad param: :>,la);
         conv:=true;
         if la(1)=long <:text:> or la(1)=long <:iso:> or
            la(1)=long <:isote:> add 'x' then open_close_mode:=2 else
         if la(1)=long <:bin:> or la(1)=long <:no:> then open_close_mode:=0 else
            par_err(<:unknown conversion: :>,la);
      end else par_err(<:param: :>,la);
   end;  

   if fromname(1)=0 then err(<:no source file:>,0);


<********************************************************************
*****
*****                    create or open outfile
*****
*********************************************************************>

   open(zo,4,toname,0);
   i:=if monitor(52,zo,0,ia)=0 then monitor(8,zo,0,ia) else -1;
   if i<>0 then
   begin
      monitor(64,zo,0,ia);
      ia(1):=1;
      ia(6):=systime(7,0,0.0);
      for i:=2,3,4,5,7,8,9,10 do ia(i):=0;
      i:=monitor(40,zo,0,ia);
      if i<>0 then err(<:*** create outfile result:>,i);
   end;

<********************************************************************
*****
*****                         open host file
*****
*********************************************************************>


   open(zi,0,<::>,1 shift 18);
   open_close_op:=21;
   write(zi,fromname);
   setposition(zi,0,0);
   if open_st<>0 then err(<:*** cannot open host file, result:>,open_st);

<********************************************************************
*****
*****                           copy file
*****
*********************************************************************>


   repeat
      i:=inrec6(zi,0);
      if -,finis then
      begin
         j:=outrec6(zo,0);
         if j<i then
         begin
            inrec6(zi,j);
            outrec6(zo,j);
            tofrom(zo,zi,j);
            i:=i-j;
         end;
         inrec6(zi,i);
         outrec6(zo,i);
         tofrom(zo,zi,i);
      end;
   until finis;

   close_cut(zo,true);
   close(zi,false);
   open(zi,0,<::>,0);
   open_close_op:=20;
   open_close_mode:=0;
   write(zi,fromname);
   close(zi,true);
   trapmode:=-1;
   write(out,true,if len(fromname)<40 then 40 else len(fromname),fromname,
         << dd ddd ddd>,extend ans(2) shift 24 add ans(3),<: bytes<10>:>);

end
