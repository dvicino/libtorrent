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

#include "config.h"

#include <algo/algo.h>

#include "torrent/exceptions.h"
#include "storage_chunk.h"

using namespace algo;

namespace torrent {

bool StorageChunk::is_valid() {
  return !m_nodes.empty() &&
    std::find_if(m_nodes.begin(), m_nodes.end(),
		 bool_not(call_member(member(&StorageChunk::Node::chunk),
				      &FileChunk::is_valid)))
    == m_nodes.end();
}

StorageChunk::Node&
StorageChunk::get_position(unsigned int pos) {
  if (pos >= m_size)
    throw internal_error("Tried to get StorageChunk position out of range.");

  Nodes::iterator itr = m_nodes.begin();

  while (itr != m_nodes.end()) {
    if (pos < (*itr)->position + (*itr)->chunk.size()) {

      if ((*itr)->length == 0)
	throw internal_error("StorageChunk::get_position(...) tried to return a node with length 0");

      return **itr;
    }

    ++itr;
  }
  
  throw internal_error("StorageChunk might be mangled, get_position failed horribly");
}

// Each add calls vector's reserve adding 1. This should keep
// the size of the vector at exactly what we need. Though it
// will require a few more cycles, it won't matter as we only
// rarely have more than 1 or 2 nodes.
FileChunk&
StorageChunk::add_file(unsigned int length) {
  m_nodes.reserve(m_nodes.size() + 1);

  m_size += length;

  return (*m_nodes.insert(m_nodes.end(), new Node(m_size - length, length)))->chunk;
}

void
StorageChunk::clear() {
  std::for_each(m_nodes.begin(), m_nodes.end(),
		delete_on());

  m_size = 0;
  m_nodes.clear();
}

}
