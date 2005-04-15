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

class Sub {
 public:
  friend class PeerConnection;

  Sub() :
    choked(true),
    interested(false),
    state(IDLE),
    lastCommand(NONE),
    length(0),
    lengthOrig(0)
    {}

  bool c_choked() const { return choked; }
  bool c_interested() const { return interested; }

 protected:
  bool choked;
  bool interested;
  
  State state;
  Protocol lastCommand;

  ProtocolBuffer<512> m_buf;
  //ProtocolBuffer<512>::iterator m_pos;

  unsigned int m_pos2;

  unsigned int length;
  unsigned int lengthOrig;
  
  Storage::Chunk data;
};