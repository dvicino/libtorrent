// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include "torrent/exceptions.h"
#include "torrent/chunk_manager.h"

#include "chunk_list.h"
#include "chunk.h"
#include "globals.h"

namespace torrent {

struct chunk_list_earliest_modified {
  chunk_list_earliest_modified() : m_time(cachedTime) {}

  void operator () (ChunkListNode* node) {
    if (node->time_modified() < m_time && node->time_modified() != rak::timer())
      m_time = node->time_modified();
  }

  rak::timer m_time;
};

struct chunk_list_sort_index {
  bool operator () (ChunkListNode* node1, ChunkListNode* node2) {
    return node1->index() < node2->index();
  }
};

inline bool
ChunkList::is_queued(ChunkListNode* node) {
  return std::find(m_queue.begin(), m_queue.end(), node) != m_queue.end();
}

bool
ChunkList::has_chunk(size_type index, int prot) const {
  return Base::at(index).is_valid() && Base::at(index).chunk()->has_permissions(prot);
}

void
ChunkList::resize(size_type s) {
  if (!empty())
    throw internal_error("ChunkList::resize(...) called on an non-empty object.");

  Base::resize(s);

  uint32_t index = 0;

  for (iterator itr = begin(), last = end(); itr != last; ++itr, ++index)
    itr->set_index(index);
}

void
ChunkList::clear() {
  // Don't do any sync'ing as whomever decided to shut down really
  // doesn't care, so just de-reference all chunks in queue.
  for (Queue::iterator itr = m_queue.begin(), last = m_queue.end(); itr != last; ++itr) {
    if ((*itr)->references() != 1 || (*itr)->writable() != 1)
      throw internal_error("ChunkList::clear() called but a node in the queue is still referenced.");
    
    (*itr)->dec_rw();
    clear_chunk(*itr);
  }

  m_queue.clear();

  if (std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::chunk)) != end() ||
      std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::references)) != end() ||
      std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::writable)) != end())
    throw internal_error("ChunkList::clear() called but a valid node was found.");

  Base::clear();
}

ChunkHandle
ChunkList::get(size_type index, bool writable) {
  ChunkListNode* node = &Base::at(index);

  if (!node->is_valid()) {
    CreateChunk chunk = m_slotCreateChunk(index, writable);

    if (chunk.first == NULL)
      return ChunkHandle::from_error(chunk.second);

    // Would be cleaner to do this before creating the chunk.
    if (!m_manager->allocate(chunk.first->size())) {
      delete chunk.first;
      return ChunkHandle::from_error(rak::error_number::e_nomem);
    }

    node->set_chunk(chunk.first);
    node->set_time_modified(rak::timer());

  } else if (writable && !node->chunk()->is_writable()) {
    CreateChunk chunk = m_slotCreateChunk(index, writable);

    if (chunk.first == NULL)
      return ChunkHandle::from_error(chunk.second);

    delete node->chunk();

    node->set_chunk(chunk.first);
    node->set_time_modified(rak::timer());
  }

  node->inc_references();

  if (writable) {
    node->inc_writable();

    // Make sure that periodic syncing uses async on any subsequent
    // changes even if it was triggered before this get.
    node->set_sync_triggered(false);
  }

  return ChunkHandle(node, writable);
}

// The chunks in 'm_queue' have been modified and need to be synced
// when appropriate. Hopefully keeping the chunks mmap'ed for a while
// will allow us to schedule writes at more resonable intervals.

void
ChunkList::release(ChunkHandle* handle) {
  if (!handle->is_valid())
    throw internal_error("ChunkList::release(...) received an invalid handle.");

  if (handle->object() < &*begin() || handle->object() >= &*end())
    throw internal_error("ChunkList::release(...) received an unknown handle.");

  if (handle->object()->references() <= 0 || (handle->is_writable() && handle->object()->writable() <= 0))
    throw internal_error("ChunkList::release(...) received a node with bad reference count.");

  if (handle->is_writable()) {

    if (handle->object()->writable() == 1) {
      if (is_queued(handle->object()))
        throw internal_error("ChunkList::release(...) tried to queue an already queued chunk.");

      // Only add those that have a modification time set?
      //
      // Only chunks that are not already in the queue will execute
      // this branch.
      m_queue.push_back(handle->object());

    } else {
      handle->object()->dec_rw();
    }

  } else {
    handle->object()->dec_references();

    if (handle->object()->references() == 0) {
      if (is_queued(handle->object()))
        throw internal_error("ChunkList::release(...) tried to unmap a queued chunk.");

      clear_chunk(handle->object());
    }
  }

  handle->clear();
}

inline void
ChunkList::clear_chunk(ChunkListNode* node) {
  if (!node->is_valid())
    throw internal_error("ChunkList::clear_chunk(...) !node->is_valid().");

  uint32_t size = node->chunk()->size();

  delete node->chunk();
  node->set_chunk(NULL);

  m_manager->deallocate(size);
}

inline bool
ChunkList::sync_chunk(ChunkListNode* node, int flags, bool cleanup) {
  if (node->references() <= 0 || node->writable() <= 0)
    throw internal_error("ChunkList::sync_chunk(...) got a node with invalid reference count.");

  // Using sync for the time being.
  if (!node->chunk()->sync(flags))
    return false;

  node->set_sync_triggered(true);

  if (!cleanup)
    return true;

  node->dec_rw();
 
  if (node->references() == 0)
    clear_chunk(node);

  return true;
}

uint32_t
ChunkList::sync_chunks(int flags) {
  Queue::iterator split;

  if (flags & sync_all)
    split = m_queue.begin();
  else
    split = std::stable_partition(m_queue.begin(), m_queue.end(), rak::not_equal(1, std::mem_fun(&ChunkListNode::writable)));

  if ((flags & sync_use_timeout))
//   if ((flags & sync_use_timeout) &&
//       std::for_each(split, m_queue.end(), chunk_list_earliest_modified()).m_time + rak::timer::from_seconds(m_manager->timeout_sync()) >= cachedTime)
    return 0;

  // Allow a flag that does more culling, so that we only get large
  // continous sections.
  //
  // How does this interact with timers, should be make it so that
  // only areas with timers are (preferably) synced?

  std::sort(split, m_queue.end());
  
  // Add a flag for not checking diskspace.

  if (!(flags & (sync_safe | sync_sloppy)) && (m_slotFreeDiskspace() <= m_manager->safe_free_diskspace()))
    flags |= sync_safe;

  uint32_t failed = 0;

  for (Queue::iterator itr = split, last = m_queue.end(); itr != last; ++itr) {
    
    // We can easily skip pieces by swap_iter, so there should be no
    // problem being selective about the ranges we sync.

    // Use a function for checking the next few chunks and see how far
    // we want to sync. When we want to sync everything use end. Call
    // before the loop, or add a check.

    // if we don't want to sync, swap and break.

    // These might be outside the loop?
    int syncFlags;
    bool syncCleanup;

    // Using if statements since some linkers have problem with static
    // const int members inside the ?: operators. The compiler should
    // be optimizing this anyway.

    if (flags & sync_force) {
      syncCleanup = true;

      if (flags & sync_safe)
        syncFlags = MemoryChunk::sync_sync;
      else
        syncFlags = MemoryChunk::sync_async;

    } else if (flags & sync_safe) {
      
      if ((*itr)->sync_triggered()) {
        syncCleanup = true;
        syncFlags = MemoryChunk::sync_sync;
      } else {
        syncCleanup = false;
        syncFlags = MemoryChunk::sync_async;
      }

    } else {
      syncCleanup = true;
      syncFlags = MemoryChunk::sync_async;
    }

//     if (syncFlags == MemoryChunk::sync_sync)
//       throw internal_error("Bork Bork Sync");

    if (!sync_chunk(*itr, syncFlags, syncCleanup)) {
      failed++;
      std::iter_swap(itr, split++);
      
      continue;
    }

    (*itr)->set_sync_triggered(true);

    if (!syncCleanup)
      std::iter_swap(itr, split++);
  }

  m_queue.erase(split, m_queue.end());
  return failed;
}

}