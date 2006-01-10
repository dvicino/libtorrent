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

#ifndef LIBTORRENT_DATA_HASH_TORRENT_H
#define LIBTORRENT_DATA_HASH_TORRENT_H

#include <string>
#include <inttypes.h>
#include <rak/functional.h>
#include <rak/ranges.h>

#include "data/chunk_handle.h"

namespace torrent {

class ChunkList;
class HashQueue;
class DownloadWrapper;

class HashTorrent {
public:
  typedef rak::ranges<uint32_t> Ranges;

  typedef rak::mem_fun1<DownloadWrapper, void, ChunkHandle>           SlotCheckChunk;
  typedef rak::mem_fun0<DownloadWrapper, void>                        SlotInitialHash;
  typedef rak::mem_fun1<DownloadWrapper, void, const std::string&>    SlotStorageError;
  
  HashTorrent(ChunkList* c);
  ~HashTorrent() { clear(); }

  void                start();
  void                clear();

  bool                is_checking()                          { return m_outstanding >= 0; }
  bool                is_checked();

  Ranges&             ranges()                               { return m_ranges; }

  uint32_t            position() const                       { return m_position; }

  HashQueue*          get_queue()                            { return m_queue; }
  void                set_queue(HashQueue* q)                { m_queue = q; }

  void                slot_check_chunk(SlotCheckChunk s)     { m_slotCheckChunk = s; }
  void                slot_initial_hash(SlotInitialHash s)   { m_slotInitialHash = s; }
  void                slot_storage_error(SlotStorageError s) { m_slotStorageError = s; }

  void                receive_chunkdone();
  
private:
  void                queue();

  unsigned int        m_position;
  int                 m_outstanding;
  Ranges              m_ranges;

  ChunkList*          m_chunkList;
  HashQueue*          m_queue;

  SlotCheckChunk      m_slotCheckChunk;
  SlotInitialHash     m_slotInitialHash;
  SlotStorageError    m_slotStorageError;
};

}

#endif
