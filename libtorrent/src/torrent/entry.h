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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_ENTRY_H
#define LIBTORRENT_ENTRY_H

#include <list>
#include <torrent/common.h>

namespace torrent {

class ContentFile;

class Entry {
public:
  typedef std::pair<uint32_t, uint32_t> Range;
  typedef std::list<std::string>        Path;

  typedef enum {
    STOPPED = 0,
    NORMAL,
    HIGH
  } Priority;

  Entry() : m_entry(NULL) {}
  Entry(ContentFile* e) : m_entry(e) {}
  
  uint64_t        get_size();

  // Chunks of this file completed.
  uint32_t        get_completed();

  // Chunk index this file spans.
  uint32_t        get_chunk_begin();
  uint32_t        get_chunk_end();

  // Need this?
  //uint64_t        get_byte_begin();
  //uint64_t        get_byte_end();

  // Relative to root of the torrent.
  std::string     get_path();
  const Path&     get_path_list();

  void            set_path_list(const Path& l);

  Priority        get_priority();

  // Remember to call update_priorities
  void            set_priority(Priority p);

private:
  ContentFile*    m_entry;
};

}

#endif
