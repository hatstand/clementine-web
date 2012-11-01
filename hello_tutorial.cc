/// Copyright (c) 2012 The Native Client Authors. All rights reserved.
/// Use of this source code is governed by a BSD-style license that can be
/// found in the LICENSE file.
///
/// @file hello_tutorial.cc
/// This example demonstrates loading, running and scripting a very simple NaCl
/// module.  To load the NaCl module, the browser first looks for the
/// CreateModule() factory method (at the end of this file).  It calls
/// CreateModule() once to load the module code from your .nexe.  After the
/// .nexe code is loaded, CreateModule() is not called again.
///
/// Once the .nexe code is loaded, the browser than calls the CreateInstance()
/// method on the object returned by CreateModule().  It calls CreateInstance()
/// each time it encounters an <embed> tag that references your NaCl module.
///
/// The browser can talk to your NaCl module via the postMessage() Javascript
/// function.  When you call postMessage() on your NaCl module from the browser,
/// this becomes a call to the HandleMessage() method of your pp::Instance
/// subclass.  You can send messages back to the browser by calling the
/// PostMessage() method on your pp::Instance.  Note that these two methods
/// (postMessage() in Javascript and PostMessage() in C++) are asynchronous.
/// This means they return immediately - there is no waiting for the message
/// to be handled.  This has implications in your program design, particularly
/// when mutating property values that are exposed to both the browser and the
/// NaCl module.

#include <cstdio>
#include <map>
#include <string>
#include <sstream>

#include <setjmp.h>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

#include <taglib/tiostream.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>


using pp::URLLoader;
using pp::URLRequestInfo;
using pp::CompletionCallbackFactory;

using std::map;
using std::string;
using std::istringstream;
using std::vector;
using std::getline;

namespace coroutine
{
  enum {
    kStackSize = 1<<16
  };

  static jmp_buf mainLabel, childLabel;
  //-----------------------------------
  void Block(void) {
    if (!setjmp(childLabel) )
    {   
      longjmp(mainLabel,1);
    } 
  }
  //-----------------------------------
  void Resume(void) {
    if (!setjmp(mainLabel) )
    {   
      longjmp(childLabel,1);
    } 
  }
  //-----------------------------------
  void Create(void (*start)(void*), void* arg) {
    if (!setjmp(mainLabel) )
    {   
      alloca(kStackSize);
      start(arg);
      longjmp(mainLabel,1);
    } 
  }
  //-----------------------------------
  void update(void* foo, int bar) 
  {
    Resume();
  }
  //-----------------------------------------------------------------------------
  void Flush(void) {
      pp::Module::Get()->core()->CallOnMainThread(
          0, pp::CompletionCallback(update, 0), 0);
      Block();
  }

}

vector<string> Split(string s, char delim) {
  istringstream stream(s);
  vector<string> split;
  string item;
  while (std::getline(stream, item, delim)) {
    split.push_back(item);
  }

  return split;
}

class GoogleDriveStream : public TagLib::IOStream {
 public:
  GoogleDriveStream(
      const std::string& url,
      const long length,
      pp::InstanceHandle instance);

  // TagLib::IOStream
  virtual TagLib::FileName name() const;
  virtual TagLib::ByteVector readBlock(ulong length);
  virtual void writeBlock(const TagLib::ByteVector&) {};
  virtual void insert(const TagLib::ByteVector&, ulong, ulong) {};
  virtual void removeBlock(ulong, ulong) {};
  virtual bool readOnly() const { return true; }
  virtual bool isOpen() const { return true; }
  virtual void seek(long offset, TagLib::IOStream::Position p);
  virtual void clear();
  virtual long tell() const;
  virtual long length();
  virtual void truncate(long) {};

 private:
  void Log(const string& message);
  void ReallyLog(int32_t, const string& message);

  static void LoadFinished(void*, int32_t);

  std::string url_;
  long length_;
  long cursor_;
  pp::InstanceHandle instance_;
  CompletionCallbackFactory<GoogleDriveStream> factory_;
};

GoogleDriveStream::GoogleDriveStream(
    const std::string& url,
    const long length,
    pp::InstanceHandle instance)
  : url_(url),
    length_(length),
    cursor_(0),
    instance_(instance),
    factory_(this) {
  Log(__PRETTY_FUNCTION__);
}

void GoogleDriveStream::Log(const string& message) {
  ReallyLog(0, message);
}

void GoogleDriveStream::ReallyLog(int32_t, const string& message) {
  pp::Instance instance(instance_.pp_instance());
  instance.PostMessage(message);
}

TagLib::FileName GoogleDriveStream::name() const {
  return "";
}

TagLib::ByteVector GoogleDriveStream::readBlock(ulong length) {
  Log(__PRETTY_FUNCTION__);
  const int start = cursor_;
  const int end = start + length - 1;

  char h[1024];
  snprintf(h, sizeof(h), "%d", instance_.pp_instance());

  Log(h);

  URLRequestInfo request(instance_);
  if (request.is_null()) {
    return TagLib::ByteVector();
  }
  request.SetURL(url_);
  request.SetMethod("GET");
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "Range: bytes=%d-%d", start, end);
  request.SetHeaders(buffer);

  Log(buffer);

  URLLoader* loader = new URLLoader(instance_);
  int32_t ret = loader->Open(request, pp::CompletionCallback(&GoogleDriveStream::LoadFinished, NULL));
  if (ret != PP_OK_COMPLETIONPENDING) {
    char log_buffer[1024];
    snprintf(log_buffer, sizeof(log_buffer), "Error: %d", ret);
    Log(string(log_buffer));
    return TagLib::ByteVector();
  }

  coroutine::Block();

  string headers_string = loader->GetResponseInfo().GetHeaders().AsString();
  vector<string> header_lines = Split(headers_string, '\n');
  map<string, string> headers;
  for (vector<string>::const_iterator it = header_lines.begin();
       it != header_lines.end(); ++it) {
    vector<string> line = Split(*it, ':');
    headers[line[0]] = line[1];
  }

  string length_header = headers["Content-Length"];
  int response_length = atoi(length_header.c_str());

  char log_buffer[1024];
  snprintf(log_buffer, sizeof(log_buffer), "Read: %d/%ld", response_length, length);
  Log(string(log_buffer));

  char* response_buffer = new char[response_length];
  ret = loader->ReadResponseBody(
      response_buffer, response_length, pp::CompletionCallback(&GoogleDriveStream::LoadFinished, this));
  if (ret != PP_OK_COMPLETIONPENDING) {
    char log_buffer[1024];
    snprintf(log_buffer, sizeof(log_buffer), "Error: %d", ret);
    Log(string(log_buffer));
    return TagLib::ByteVector();
  }

  coroutine::Block();

  TagLib::ByteVector bytes(response_buffer, response_length);
  delete[] response_buffer;
  cursor_ += bytes.size();
  return bytes;
}

void GoogleDriveStream::LoadFinished(void* me, int32_t ret) {
  if (me) {
    GoogleDriveStream* instance = reinterpret_cast<GoogleDriveStream*>(me);
    char log_buffer[1024];
    snprintf(log_buffer, sizeof(log_buffer), "Error: %d", ret);
    instance->Log(string(log_buffer));
  }
  coroutine::Resume();
}

void GoogleDriveStream::seek(long offset, TagLib::IOStream::Position p) {
  Log(__PRETTY_FUNCTION__);
  int old_cursor = cursor_;
  switch (p) {
    case TagLib::IOStream::Beginning:
      cursor_ = offset;
      break;

    case TagLib::IOStream::Current:
      cursor_ = cursor_ + offset;
      break;

    case TagLib::IOStream::End:
      cursor_ = length() - abs(offset);
      break;
  }
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "%d %ld %ld %d", old_cursor, offset, cursor_, p);
  Log(string(buffer));
}

void GoogleDriveStream::clear() {
  Log(__PRETTY_FUNCTION__);
  cursor_ = 0;
}

long GoogleDriveStream::tell() const {
  return cursor_;
}

long GoogleDriveStream::length() {
  Log(__PRETTY_FUNCTION__);
  return length_;
}

class HelloTutorialInstance;
struct Job {
  Job() : instance(NULL) {}
  pp::InstanceHandle instance;
  std::string url;
  int length;
  HelloTutorialInstance* iface;
};


/// The Instance class.  One of these exists for each instance of your NaCl
/// module on the web page.  The browser will ask the Module object to create
/// a new Instance for each occurence of the <embed> tag that has these
/// attributes:
///     type="application/x-nacl"
///     src="hello_tutorial.nmf"
/// To communicate with the browser, you must override HandleMessage() for
/// receiving messages from the browser, and use PostMessage() to send messages
/// back to the browser.  Note that this interface is asynchronous.
class HelloTutorialInstance : public pp::Instance {
 public:
  /// The constructor creates the plugin-side instance.
  /// @param[in] instance the handle to the browser-side plugin instance.
  explicit HelloTutorialInstance(PP_Instance instance)
      : pp::Instance(instance),
        factory_(this)
  {}
  virtual ~HelloTutorialInstance() {}

  /// Handler for messages coming in from the browser via postMessage().  The
  /// @a var_message can contain anything: a JSON string; a string that encodes
  /// method names and arguments; etc.  For example, you could use
  /// JSON.stringify in the browser to create a message that contains a method
  /// name and some parameters, something like this:
  ///   var json_message = JSON.stringify({ "myMethod" : "3.14159" });
  ///   nacl_module.postMessage(json_message);
  /// On receipt of this message in @a var_message, you could parse the JSON to
  /// retrieve the method name, match it to a function call, and then call it
  /// with the parameter.
  /// @param[in] var_message The message posted by the browser.
  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_string()) {
      return;
    }

    PostMessage("About to tag:");
    PostMessage(var_message.AsString());

    Job* job = new Job;
    job->instance = this;
    job->url = var_message.AsString();
    job->length = 10468677;
    job->iface = this;

    coroutine::Create(HelloTutorialInstance::TagFile, job);
  }

 private:
  static void TagFile(void* arg) {
    Job* job = reinterpret_cast<Job*>(arg);

    GoogleDriveStream* stream = new GoogleDriveStream(
        job->url,
        job->length,
        job->instance);
    (void)stream;

    TagLib::ID3v2::FrameFactory* factory =
        TagLib::ID3v2::FrameFactory::instance();

    TagLib::MPEG::File tag(
        stream, factory, TagLib::AudioProperties::Accurate);

    job->iface->PostMessage(tag.tag()->title().to8Bit(true));

    delete job;

    coroutine::Flush();
  }

  void SendMessage(int32_t, const std::string message) {
    PostMessage(message);
  }

  pp::CompletionCallbackFactory<HelloTutorialInstance> factory_;
};

/// The Module class.  The browser calls the CreateInstance() method to create
/// an instance of your NaCl module on the web page.  The browser creates a new
/// instance for each <embed> tag with type="application/x-nacl".
class HelloTutorialModule : public pp::Module {
 public:
  HelloTutorialModule() : pp::Module() {}
  virtual ~HelloTutorialModule() {}

  /// Create and return a HelloTutorialInstance object.
  /// @param[in] instance The browser-side instance.
  /// @return the plugin-side instance.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new HelloTutorialInstance(instance);
  }
};

namespace pp {
/// Factory function called by the browser when the module is first loaded.
/// The browser keeps a singleton of this module.  It calls the
/// CreateInstance() method on the object you return to make instances.  There
/// is one instance per <embed> tag on the page.  This is the main binding
/// point for your NaCl module with the browser.
Module* CreateModule() {
  return new HelloTutorialModule();
}
}  // namespace pp
