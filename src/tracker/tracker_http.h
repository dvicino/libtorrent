#ifndef LIBTORRENT_TRACKER_HTTP_H
#define LIBTORRENT_TRACKER_HTTP_H

#include <list>
#include <iosfwd>
#include <inttypes.h>

#include "torrent/bencode.h"

#include "peer_info.h"
#include "tracker_state.h"

struct sockaddr_in;

namespace torrent {

class Http;

// TODO: Use a base class when we implement UDP tracker support.
class TrackerHttp {
public:
  typedef std::list<PeerInfo>                          PeerList;
  typedef sigc::signal1<void, Bencode&>                SignalDone;
  typedef sigc::signal1<void, std::string>             SignalFailed;

  TrackerHttp();
  ~TrackerHttp();

  bool               is_busy()                         { return m_data != NULL; }

  void               send_state(TrackerState state,
				uint64_t down,
				uint64_t up,
				uint64_t left);

  void               close();

  void               set_url(const std::string& url)   { m_url = url; }
  void               set_hash(const std::string& hash) { m_hash = hash; }
  void               set_key(const std::string& key)   { m_key = key; }
  void               set_compact(bool c)               { m_compact = c; }
  void               set_numwant(int16_t n)            { m_numwant = n; }
  
  void               set_me(const PeerInfo* me)        { m_me = me; }

  SignalDone&        signal_done()                     { return m_signalDone; }
  SignalFailed&      signal_failed()                   { return m_signalFailed; }

private:
  // Don't allow ctor.
  TrackerHttp(const TrackerHttp& t);
  void operator = (const TrackerHttp& t);

  void               receive_done();
  void               receive_failed(std::string msg);

  static void        escape_string(const std::string& src, std::ostream& stream);

  Http*              m_get;
  std::stringstream* m_data;

  std::string        m_url;
  std::string        m_hash;
  std::string        m_key;

  bool               m_compact;
  int16_t            m_numwant;

  const PeerInfo*    m_me;

  SignalDone         m_signalDone;
  SignalFailed       m_signalFailed;
};

} // namespace torrent

#endif // LIBTORRENT_TRACKER_QUERY_H