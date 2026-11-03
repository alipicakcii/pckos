#include <user/lib/io.hh>



extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
  
      print("merhaba pckos\n");      
      
      user::exit(0);
}
