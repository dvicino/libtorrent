#ifndef CURL_GET_H
#define CURL_GET_H

#include <iosfwd>
#include <string>
#include <curl/curl.h>
#include <torrent/http.h>
#include <sigc++/signal.h>

struct CURLMsg;

class CurlGet : public torrent::Http {
 public:
  friend class CurlStack;

  CurlGet(CurlStack* s);
  virtual ~CurlGet();

  static CurlGet*    new_object(CurlStack* s);

  void               start();
  void               close();

  bool               is_busy() { return m_handle; }

 protected:
  CURL*              handle() { return m_handle; }

  void               perform(CURLMsg* msg);

 private:
  CurlGet(const CurlGet&);
  void operator = (const CurlGet&);

  friend size_t      curl_get_receive_write(void* data, size_t size, size_t nmemb, void* handle);

  CURL*              m_handle;

  CurlStack*         m_stack;
};

#endif
