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

#include <errno.h>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "memory_chunk.h"

namespace torrent {

uint32_t MemoryChunk::m_pagesize = getpagesize();

inline void
MemoryChunk::align_pair(uint32_t& offset, uint32_t& length) const {
  offset += page_align();

  length += offset % m_pagesize;
  offset -= offset % m_pagesize;
}

MemoryChunk::MemoryChunk(char* ptr, char* begin, char* end, int prot, int flags) :
  m_ptr(ptr),
  m_begin(begin),
  m_end(end),
  m_prot(prot),
  m_flags(flags) {

  if (ptr == NULL)
    throw internal_error("MemoryChunk::MemoryChunk(...) received ptr == NULL");

  if (page_align() >= m_pagesize)
    throw internal_error("MemoryChunk::MemoryChunk(...) received an page alignment >= page size");

  if ((ptrdiff_t)ptr % m_pagesize)
    throw internal_error("MemoryChunk::MemoryChunk(...) is not aligned to a page");
}

void
MemoryChunk::unmap() {
  if (!is_valid())
    throw internal_error("MemoryChunk::unmap() called on an invalid object");

  if (munmap(m_ptr, m_end - m_ptr))
    throw internal_error("MemoryChunk::unmap() system call failed");
}  

void
MemoryChunk::incore(char* buf, uint32_t offset, uint32_t length) {
  if (!is_valid())
    throw internal_error("Called MemoryChunk::incore(...) on an invalid object");

  if (!is_valid_range(offset, length))
    throw internal_error("MemoryChunk::incore(...) received out-of-range input");

  align_pair(offset, length);

#if USE_MINCORE_UNSIGNED
  if (mincore(m_ptr + offset, length, (unsigned char*)buf))
#else
  if (mincore(m_ptr + offset, length, (char*)buf))
#endif
    throw storage_error("System call mincore failed for MemoryChunk");
}

bool
MemoryChunk::advise(uint32_t offset, uint32_t length, int advice) {
  if (!is_valid())
    throw internal_error("Called MemoryChunk::advise() on an invalid object");

  if (!is_valid_range(offset, length))
    throw internal_error("MemoryChunk::advise(...) received out-of-range input");

  align_pair(offset, length);

  if (madvise(m_ptr + offset, length, advice) == 0)
    return true;

  else if (errno == EINVAL || (errno == ENOMEM && advice != advice_willneed) || errno == EBADF)
    throw internal_error("MemoryChunk::advise(...) " + std::string(strerror(errno)));
  
  else
    return false;			 
}

void
MemoryChunk::sync(uint32_t offset, uint32_t length, int flags) {
  if (!is_valid())
    throw internal_error("Called MemoryChunk::sync() on an invalid object");

  if (!is_valid_range(offset, length))
    throw internal_error("MemoryChunk::sync(...) received out-of-range input");

  align_pair(offset, length);
  
  if (msync(m_ptr + offset, length, flags))
    throw internal_error("MemoryChunk::sync(...) failed call to msync");
}    

bool
MemoryChunk::is_incore(uint32_t offset, uint32_t length) {
  uint32_t size = pages_touched(offset, length);
  char buf[size];
  
  incore(buf, offset, length);

  return std::find(buf, buf + size, 0) == buf + size;
}

}