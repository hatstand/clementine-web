#include "google_drive_stream.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <ppapi/cpp/url_loader.h>
#include <ppapi/cpp/url_request_info.h>
#include <ppapi/cpp/url_response_info.h>

#include "coroutines.h"

using std::istringstream;
using std::map;
using std::string;
using std::vector;

using pp::URLRequestInfo;
using pp::URLLoader;

namespace {

vector<string> Split(string s, char delim) {
  istringstream stream(s);
  vector<string> split;
  string item;
  while (std::getline(stream, item, delim)) {
    split.push_back(item);
  }

  return split;
}

}  // namespace

GoogleDriveStream::GoogleDriveStream(
    const string& url,
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
