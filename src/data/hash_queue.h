#ifndef LIBTORRENT_HASH_QUEUE_H
#define LIBTORRENT_HASH_QUEUE_H

#include <string>
#include <sigc++/signal.h>
#include <algo/ref_anchored.h>

#include "storage_chunk.h"

#include "utils/task.h"

namespace torrent {

class HashChunk;

// Calculating hash of incore memory is blindingly fast, it's always
// the loading from swap/disk that takes time. So with the exception
// of large resumed downloads, try to check the hash immediately. This
// helps us in getting as much done as possible while the pages are in
// memory.

class HashQueue {
public:
  // SignalDone: (Return void)
  // unsigned int - index of chunk
  // std::string  - chunk hash

  typedef algo::RefAnchored<StorageChunk>       Chunk;
  typedef sigc::slot2<void, Chunk, std::string> SlotDone;

  HashQueue();
  ~HashQueue() { clear(); }

  void                add(Chunk c, SlotDone d, const std::string& id);

  bool                has(uint32_t index, const std::string& id);

  void                remove(const std::string& id);
  void                clear();

  void                work();

private:
  struct Node {
    Node(HashChunk* c, const std::string& i, SlotDone d) :
      m_chunk(c), m_id(i), m_done(d), m_willneed(false) {}

    uint32_t          get_index();

    HashChunk*        m_chunk;
    std::string       m_id;
    SlotDone          m_done;

    bool              m_willneed;
  };

  bool                check(bool force);

  void                willneed(int count);

  typedef std::list<Node> ChunkList;

  uint16_t            m_tries;
  ChunkList           m_chunks;

  Task                m_taskWork;
};

}

#endif