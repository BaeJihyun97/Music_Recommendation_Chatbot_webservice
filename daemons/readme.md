compile option   
   
gcc -g -o app_gw app_gw.c -ldl -lpthread   
gcc -g -o app_man app_man.c   
gcc -g -o app_del app_del.c   
   
   
run   
./app_man.c   
   
   
check   
ps -ef | grep app_
   
   
kill   
kill -9 [app_man pid]   
   
   
This calls setpgrp(). please check ps.  
