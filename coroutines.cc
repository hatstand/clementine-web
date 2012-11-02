#include "coroutines.h"

#include <ppapi/cpp/completion_callback.h>

namespace coroutine
{
  static jmp_buf mainLabel;
  static jmp_buf childLabel;

  //-----------------------------------
  void Block(void) {
    if (!setjmp(childLabel)) {
      longjmp(mainLabel,1);
    }
  }
  //-----------------------------------
  void Resume(void) {
    if (!setjmp(mainLabel)) {
      longjmp(childLabel,1);
    }
  }
  //-----------------------------------
  void Create(void (*start)(void*), void* arg) {
    if (!setjmp(mainLabel)) {
      alloca(kStackSize);
      start(arg);
      longjmp(mainLabel,1);
    }
  }
  //-----------------------------------
  void update(void* foo, int bar) {
    Resume();
  }
  //-----------------------------------------------------------------------------
  void Flush(void) {
      pp::Module::Get()->core()->CallOnMainThread(
          0, pp::CompletionCallback(update, 0), 0);
      Block();
  }

}
