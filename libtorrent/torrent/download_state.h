#ifndef LIBTORRENT_DOWNLOAD_STATE_H
#define LIBTORRENT_DOWNLOAD_STATE_H

#include "bitfield_counter.h"
#include "delegator.h"
#include "peer_info.h"
#include "rate.h"
#include "settings.h"
#include "content/content.h"
#include "data/hash_torrent.h"
#include <list>

namespace torrent {

extern HashQueue hashQueue;
extern HashTorrent hashTorrent;

class PeerConnection;

// Here goes all those things that Peer* and Delegator needs.
class DownloadState {
 public:
  typedef std::list<PeerInfo>        Peers;
  typedef std::list<PeerConnection*> Connections;

  DownloadState();
  ~DownloadState();

  PeerInfo&     me()              { return m_me; }
  std::string&  hash()            { return m_hash; }

  Content&      content()         { return m_content; }
  Connections&  connections()     { return m_connections; }
  Delegator&    delegator()       { return m_delegator; }
  Peers&        available_peers() { return m_availablePeers; }

  uint64_t& bytesDownloaded() { return m_bytesDownloaded; }
  uint64_t& bytesUploaded() { return m_bytesUploaded; }

  Rate& rateUp() { return m_rateUp; }
  Rate& rateDown() { return m_rateDown; }

  DownloadSettings& settings() { return m_settings; }
  const DownloadSettings& settings() const { return m_settings; }

  void removeConnection(PeerConnection* p);

  int canUnchoke();
  void chokeBalance();

  void chunkDone(Storage::Chunk& c);

  BitFieldCounter& bfCounter() { return m_bfCounter; }

  int countConnections() const; 

  void addConnection(int fd, const PeerInfo& p);

  void download_stats(uint64_t& down, uint64_t& up, uint64_t& left);

  void connect_peers();

  // Incoming signals.
  void receive_hashdone(std::string id, Storage::Chunk c, std::string hash);

private:
  // Disable
  DownloadState(const DownloadState&);
  void operator = (const DownloadState&);

  uint64_t m_bytesUploaded;
  uint64_t m_bytesDownloaded;

  PeerInfo m_me;
  std::string m_hash;
  
  Content m_content;
  Delegator m_delegator;
  Connections m_connections;
  Peers m_availablePeers;

  DownloadSettings m_settings;
  Rate m_rateUp;
  Rate m_rateDown;
  
  BitFieldCounter m_bfCounter;
};

}

#endif
