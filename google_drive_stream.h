#include <string>

#include <ppapi/cpp/instance.h>
#include <ppapi/utility/completion_callback_factory.h>

#include <sparsehash/sparsetable>

#include <taglib/tiostream.h>

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

  void Precache();

 private:
  void Log(const std::string& message);
  void ReallyLog(int32_t, const std::string& message);

  bool CheckCache(int start, int end);
  void FillCache(int start, TagLib::ByteVector data);
  TagLib::ByteVector GetCached(int start, int end);

  static void LoadFinished(void*, int32_t);

  std::string url_;
  long length_;
  long cursor_;
  pp::InstanceHandle instance_;
  pp::CompletionCallbackFactory<GoogleDriveStream> factory_;
  google::sparsetable<char> cache_;
};
