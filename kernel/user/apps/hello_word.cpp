#include <user/lib/io.hh>




int _global =80;

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
  
      int *ptr = new int;
      *ptr =55;
       int stack = 5;

      print("global_value=%d global_addr=%p\n", _global, &_global);
      print("ptr_value=%d ptr_addr=%p\n", *ptr, ptr);
      print("stack_value=%d stack_addr=%p\n", stack, &stack);
      
      user::exit(0);
}