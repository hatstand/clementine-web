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

#include <taglib/mp4file.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>

#include <json/reader.h>
#include <json/writer.h>

#include "coroutines.h"
#include "google_drive_stream.h"

using std::string;

using pp::CompletionCallbackFactory;

class HelloTutorialInstance;
struct Job {
  Job() : instance(NULL) {}
  pp::InstanceHandle instance;
  string url;
  int length;
  HelloTutorialInstance* iface;
  string id;
  string filename;
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

    Json::Value root;
    Json::Reader reader;
    bool ok = reader.parse(var_message.AsString(), root);
    if (!ok) {
      PostMessage("Error");
      return;
    }

    string url = root["url"].asString();
    int length = root["length"].asInt();

    Job* job = new Job;
    job->instance = this;
    job->url = url;
    job->length = length;
    job->iface = this;
    job->id = root["id"].asString();
    job->filename = root["filename"].asString();

    coroutine::Create(HelloTutorialInstance::TagFile, job);
  }

 private:
  static void TagFile(void* arg) {
    Job* job = reinterpret_cast<Job*>(arg);

    GoogleDriveStream* stream = new GoogleDriveStream(
        job->url,
        job->length,
        job->instance);
    stream->Precache();

    TagLib::ID3v2::FrameFactory* factory =
        TagLib::ID3v2::FrameFactory::instance();

    TagLib::MPEG::File tag(
          stream, factory, TagLib::AudioProperties::Accurate);

    Json::FastWriter writer;
    Json::Value root;
    root["id"] = job->id;
    root["filename"] = job->filename;

    if (tag.tag()) {
      root["title"] = tag.tag()->title().to8Bit(true);
      root["artist"] = tag.tag()->artist().to8Bit(true);
      root["album"] = tag.tag()->album().to8Bit(true);
    }

    job->iface->PostMessage(writer.write(root));

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
