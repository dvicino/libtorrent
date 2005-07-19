// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

#include <cstring>

#include "torrent/exceptions.h"
#include "download/download_state.h"

#include "settings.h"

namespace torrent {

void
DownloadState::update_endgame() {
  if (m_content.get_chunks_completed() + m_slotDelegatedChunks() + m_settings->endgameBorder
      >= get_chunk_total())
    m_slotSetEndgame(true);
}

void
DownloadState::chunk_done(unsigned int index) {
  Storage::Chunk c = m_content.get_storage().get_chunk(index, MemoryChunk::prot_read);

  if (!c.is_valid())
    throw internal_error("DownloadState::chunk_done(...) called with an index we couldn't retrive from storage");

  m_slotHashCheckAdd(c);
}

uint64_t
DownloadState::bytes_left() {
  uint64_t left = m_content.get_size() - m_content.get_bytes_completed();

  if (left > ((uint64_t)1 << 60))
    throw internal_error("DownloadState::bytes_left() is too large"); 

  if (m_content.get_chunks_completed() == get_chunk_total() && left != 0)
    throw internal_error("DownloadState::bytes_left() has an invalid size"); 

  return left;
}

void
DownloadState::receive_hash_done(Storage::Chunk c, std::string hash) {
  if (std::memcmp(hash.c_str(), m_content.get_hash_c(c->get_index()), 20) == 0) {

    m_content.mark_done(c->get_index());
    m_signalChunkPassed.emit(c->get_index());

    update_endgame();

  } else {
    m_signalChunkFailed.emit(c->get_index());
  }
}  

}
