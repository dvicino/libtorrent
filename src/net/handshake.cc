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

#include "torrent/exceptions.h"
#include "handshake.h"
#include "handshake_manager.h"

namespace torrent {

Handshake::~Handshake() {
  if (m_fd >= 0)
    throw internal_error("Handshake dtor called but m_fd is still open");

  delete [] m_buf;
}

void
Handshake::close() {
  if (m_fd < 0)
    return;

  close_socket(m_fd);
  m_fd = -1;
}

void
Handshake::send_connected() {
  m_manager->receive_connected(this);
}

void
Handshake::send_failed() {
  m_manager->receive_failed(this);
}

bool
Handshake::recv1() {
  if (m_pos == 0 && !read_buf(m_buf, 1, m_pos))
    return false;

  int l = (unsigned char)m_buf[0];

  if (!read_buf(m_buf + m_pos, l + 29, m_pos))
    return false;

  m_peer.set_options(std::string(m_buf + 1 + l, 8));
  m_hash = std::string(m_buf + 9 + l, 20);

  if (std::string(m_buf + 1, l) != "BitTorrent protocol") {
    throw communication_error("Peer returned wrong protocol identifier");
  } else {
    return true;
  }
}

bool
Handshake::recv2() {
  if (!read_buf(m_buf + m_pos, 20, m_pos))
    return false;

  m_peer.set_id(std::string(m_buf, 20));

  return true;
}  

}