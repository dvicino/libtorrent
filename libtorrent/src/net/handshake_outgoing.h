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

#ifndef LIBTORRENT_NET_HANDSHAKE_OUTGOING_H
#define LIBTORRENT_NET_HANDSHAKE_OUTGOING_H

#include "handshake.h"

namespace torrent {

class HandshakeOutgoing : public Handshake {
public:
  typedef enum {
    INACTIVE,
    CONNECTING,
    WRITE_HEADER,
    READ_HEADER1,
    READ_HEADER2
  } State;

  HandshakeOutgoing(SocketFd fd,
		    HandshakeManager* m,
		    const PeerInfo& p,
		    const std::string& infoHash,
		    const std::string& ourId);
  
  virtual void        read();
  virtual void        write();
  virtual void        except();

private:
  State               m_state;
  std::string         m_local;
};

}

#endif
