#ifndef LIBTORRENT_HASH_TORRENT_H
#define LIBTORRENT_HASH_TORRENT_H

#include <string>
#include "hash_queue.h"

namespace torrent {

class Storage;

class HashTorrent {
public:
  typedef sigc::slot0<void> SlotDone;
  
  HashTorrent(HashQueue* queue) : m_position(0), m_outstanding(0), m_queue(queue) {}
  ~HashTorrent()                { clear(); }

  void add(const std::string& id,
	   Storage* storage,
	   SlotDone torrentDone,
	   HashQueue::SlotDone slotDone);
  
  void clear();
  void remove(const std::string& id);

private:
  struct Node {
    Node(const std::string& i, Storage* s, SlotDone t, HashQueue::SlotDone& d) :
      id(i), storage(s), torrentDone(t), chunkDone(d) {}

    std::string         id;
    Storage*            storage;
    SlotDone            torrentDone;
    HashQueue::SlotDone chunkDone;
  };

  typedef std::list<Node> List;

  void queue(unsigned int s);

  void receive_chunkdone(HashQueue::Chunk c, std::string hash);

  unsigned int m_position;
  unsigned int m_outstanding;

  List         m_list;
  HashQueue*   m_queue;
};

}

#endif
