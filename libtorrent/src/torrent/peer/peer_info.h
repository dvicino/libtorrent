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

#ifndef LIBTORRENT_PEER_INFO_H
#define LIBTORRENT_PEER_INFO_H

#include <torrent/hash_string.h>
#include <torrent/peer/client_info.h>

namespace torrent {

class LIBTORRENT_EXPORT PeerInfo {
public:
  friend class Handshake;
  friend class HandshakeManager;
  friend class PeerList;

  static const int flag_connected = (1 << 0);
  static const int flag_incoming  = (1 << 1);
  static const int flag_handshake = (1 << 2);

  PeerInfo(const sockaddr* address);
  ~PeerInfo();

  bool                is_connected() const                  { return m_flags & flag_connected; }
  bool                is_incoming() const                   { return m_flags & flag_incoming; }
  bool                is_handshake() const                  { return m_flags & flag_handshake; }

  int                 flags() const                         { return m_flags; }

  const HashString&   id() const                            { return m_id; }
  const ClientInfo&   client_info() const                   { return m_clientInfo; }

  const char*         options() const                       { return m_options; }
  const sockaddr*     socket_address() const                { return m_address; }

  uint16_t            listen_port() const                   { return m_listenPort; }

  uint32_t            failed_counter() const                { return m_failedCounter; }
  void                set_failed_counter(uint32_t c)        { m_failedCounter = c; }

  uint32_t            transfer_counter() const              { return m_transferCounter; }
  void                set_transfer_counter(uint32_t c)      { m_transferCounter = c; }

  uint32_t            last_connection() const               { return m_lastConnection; }
  void                set_last_connection(uint32_t tvsec)   { m_lastConnection = tvsec; }

protected:
  void                set_flags(int flags)                  { m_flags |= flags; }
  void                unset_flags(int flags)                { m_flags &= ~flags; }

  HashString&         mutable_id()                          { return m_id; }
  ClientInfo&         mutable_client_info()                 { return m_clientInfo; }

  void                set_port(uint16_t port);
  void                set_listen_port(uint16_t port)        { m_listenPort = port; }

  char*               set_options()                         { return m_options; }

private:
  PeerInfo(const PeerInfo&);
  void operator = (const PeerInfo&);

  // Replace id with a char buffer, or a cheap struct?
  int                 m_flags;
  HashString          m_id;
  ClientInfo          m_clientInfo;

  char                m_options[8];

  uint32_t            m_failedCounter;
  uint32_t            m_transferCounter;
  uint32_t            m_lastConnection;

  uint16_t            m_listenPort;

  // Replace this with a union. Since the user never copies PeerInfo
  // it should be safe to not require sockaddr_in6 to be part of it.
  sockaddr*           m_address;
};

}

#endif
