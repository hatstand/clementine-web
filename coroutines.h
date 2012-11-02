#include <setjmp.h>

#include <ppapi/cpp/module.h>

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
