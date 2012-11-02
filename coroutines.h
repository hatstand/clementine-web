#include <setjmp.h>

#include <ppapi/cpp/module.h>

// http://mainroach.blogspot.ch/2012/10/simulating-blocking-ppapi-functions-in.html
// https://github.com/mainroach/nacl-examples/tree/master/co-routines

namespace coroutine
{
  enum {
    kStackSize = 1<<16
  };

  //-----------------------------------
  void Block();
  //-----------------------------------
  void Resume();
  //-----------------------------------
  void Create(void (*start)(void*), void* arg);
  //-----------------------------------
  void update(void* foo, int bar);
  //-----------------------------------------------------------------------------
  void Flush();
}
