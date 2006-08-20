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

#include <cstring>
#include <sstream>

#include "data/content.h"
#include "download/chunk_statistics.h"
#include "download/download_main.h"

#include "peer_connection_seed.h"

namespace torrent {

PeerConnectionSeed::~PeerConnectionSeed() {
//   if (m_download != NULL && m_down->get_state() != ProtocolRead::READ_BITFIELD)
//     m_download->bitfield_counter().dec(m_peerChunks.bitfield()->bitfield());

//   priority_queue_erase(&taskScheduler, &m_taskSendChoke);
}

void
PeerConnectionSeed::initialize_custom() {
//   if (m_download->content()->chunks_completed() != 0) {
//     m_up->write_bitfield(m_download->content()->bitfield()->size_bytes());

//     m_up->buffer()->prepare_end();
//     m_up->set_position(0);
//     m_up->set_state(ProtocolWrite::WRITE_BITFIELD_HEADER);
//   }
}

void
PeerConnectionSeed::update_interested() {
  // We assume this won't be called.
}

void
PeerConnectionSeed::receive_finished_chunk(__UNUSED int32_t i) {
  // We assume this won't be called.
}

bool
PeerConnectionSeed::receive_keepalive() {
  if (cachedTime - m_timeLastRead > rak::timer::from_seconds(240))
    return false;

  // There's no point in adding ourselves to the write poll if the
  // buffer is full, as that will already have been taken care of.
  if (m_up->get_state() == ProtocolWrite::IDLE &&
      m_up->can_write_keepalive()) {

    write_insert_poll_safe();
    m_up->write_keepalive();
  }

  return true;
}

// We keep the message in the buffer if it is incomplete instead of
// keeping the state and remembering the read information. This
// shouldn't happen very often compared to full reads.
inline bool
PeerConnectionSeed::read_message() {
  ProtocolBuffer<512>* buf = m_down->buffer();

  if (buf->remaining() < 4)
    return false;

  // Remember the start of the message so we may reset it if we don't
  // have the whole message.
  ProtocolBuffer<512>::iterator beginning = buf->position();

  uint32_t length = buf->read_32();

  if (length == 0) {
    // Keepalive message.
    m_down->set_last_command(ProtocolBase::KEEP_ALIVE);

    return true;

  } else if (buf->remaining() < 1) {
    buf->set_position_itr(beginning);
    return false;

  } else if (length > (1 << 20)) {
    throw network_error("PeerConnectionSeed::read_message() got an invalid message length.");
  }
    
  // We do not verify the message length of those with static
  // length. A bug in the remote client causing the message start to
  // be unsyncronized would in practically all cases be caught with
  // the above test.
  //
  // Those that do in some weird way manage to produce a valid
  // command, will not be able to do any damage as malicious
  // peer. Those cases should be caught elsewhere in the code.

  // Temporary.
  m_down->set_last_command((ProtocolBase::Protocol)buf->peek_8());

  switch (buf->read_8()) {
  case ProtocolBase::CHOKE:
    m_down->set_choked(true);
    return true;

  case ProtocolBase::UNCHOKE:
    m_down->set_choked(false);
    return true;

  case ProtocolBase::INTERESTED:
    set_remote_interested();
    return true;

  case ProtocolBase::NOT_INTERESTED:
    set_remote_not_interested();
    return true;

  case ProtocolBase::HAVE:
    if (!m_down->can_read_have_body())
      break;

    read_have_chunk(buf->read_32());
    return true;

  case ProtocolBase::REQUEST:
    if (!m_down->can_read_request_body())
      break;

    if (!m_up->choked()) {
      write_insert_poll_safe();
      read_request_piece(m_down->read_request());

    } else {
      m_down->read_request();
    }

    return true;

  case ProtocolBase::PIECE:
    throw network_error("Received a piece but the connection is strictly for seeding.");

  case ProtocolBase::CANCEL:
    if (!m_down->can_read_cancel_body())
      break;

    read_cancel_piece(m_down->read_request());
    return true;

  default:
    throw network_error("Received unsupported message type.");
  }

  // We were unsuccessfull in reading the message, need more data.
  buf->set_position_itr(beginning);
  return false;
}

void
PeerConnectionSeed::event_read() {
  m_timeLastRead = cachedTime;

  // Need to make sure ProtocolBuffer::end() is pointing to the end of
  // the unread data, and that the unread data starts from the
  // beginning of the buffer. Or do we use position? Propably best,
  // therefor ProtocolBuffer::position() points to the beginning of
  // the unused data.

  try {
    
    // Normal read.
    //
    // We rarely will read zero bytes as the read of 64 bytes will
    // almost always either not fill up or it will require additional
    // reads.
    //
    // Only loop when end hits 64.

    do {

      if (m_down->buffer()->size_end() == read_size)
        throw internal_error("PeerConnectionSeed::event_read() m_down->buffer()->size_end() == read_size.");

      m_down->buffer()->move_end(read_stream_throws(m_down->buffer()->end(), read_size - m_down->buffer()->size_end()));
        
      while (read_message());
        
      if (m_down->buffer()->size_end() == read_size) {
        read_buffer_move_unused();

      } else {
        read_buffer_move_unused();
        return;
      }

      // Figure out how to get rid of the shouldLoop boolean.
    } while (true);

  // Exception handlers:

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this);

  } catch (blocked_connection& e) {
    m_download->info()->signal_network_log().emit("Momentarily blocked read connection.");
    m_download->connection_list()->erase(this);

  } catch (network_error& e) {
    m_download->info()->signal_network_log().emit(e.what());

    m_download->connection_list()->erase(this);

  } catch (storage_error& e) {
    m_download->info()->signal_storage_error().emit(e.what());
    m_download->connection_list()->erase(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection read fd(" << get_fd().get_fd() << ',' << m_down->get_state() << ',' << m_down->last_command() << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

inline void
PeerConnectionSeed::fill_write_buffer() {
  // No need to use delayed choke as we are a seeder.
  if (m_sendChoked && m_up->can_write_choke()) {
    m_sendChoked = false;
    m_up->write_choke(m_up->choked());

    if (m_up->choked()) {
      m_download->upload_throttle()->erase(m_peerChunks.upload_throttle());
      up_chunk_release();
      m_peerChunks.upload_queue()->clear();

    } else {
      m_download->upload_throttle()->insert(m_peerChunks.upload_throttle());
    }
  }

  if (!m_up->choked() &&
      !m_peerChunks.upload_queue()->empty() &&
      m_up->can_write_piece())
    write_prepare_piece();
}

void
PeerConnectionSeed::event_write() {
  try {
  
    do {

      switch (m_up->get_state()) {
      case ProtocolWrite::IDLE:

        // We might have buffered keepalive message or similar, but
        // 'end' should remain at the start of the buffer.
        if (m_up->buffer()->size_end() != 0)
          throw internal_error("PeerConnectionSeed::event_write() ProtocolWrite::IDLE in a wrong state.");

        // Fill up buffer.
        fill_write_buffer();

        if (m_up->buffer()->size_position() == 0) {
          manager->poll()->remove_write(this);
          return;
        }

        m_up->set_state(ProtocolWrite::MSG);
        m_up->buffer()->prepare_end();

      case ProtocolWrite::MSG:
        m_up->buffer()->move_position(write_stream_throws(m_up->buffer()->position(), m_up->buffer()->remaining()));

        if (m_up->buffer()->remaining())
          return;

        m_up->buffer()->reset();

        if (m_up->last_command() != ProtocolBase::PIECE) {
          // Break or loop? Might do an ifelse based on size of the
          // write buffer. Also the write buffer is relatively large.
          m_up->set_state(ProtocolWrite::IDLE);
          break;
        }

        // We're uploading a piece.
        load_up_chunk();
        m_up->set_state(ProtocolWrite::WRITE_PIECE);

      case ProtocolWrite::WRITE_PIECE:
        if (!up_chunk())
          return;

        m_up->set_state(ProtocolWrite::IDLE);

        break;

      default:
        throw internal_error("PeerConnectionSeed::event_write() wrong state.");
      }

    } while (true);

  } catch (close_connection& e) {
    m_download->connection_list()->erase(this);

  } catch (blocked_connection& e) {
    m_download->info()->signal_network_log().emit("Momentarily blocked write connection.");
    m_download->connection_list()->erase(this);

  } catch (network_error& e) {
    m_download->info()->signal_network_log().emit(e.what());
    m_download->connection_list()->erase(this);

  } catch (storage_error& e) {
    m_download->info()->signal_storage_error().emit(e.what());
    m_download->connection_list()->erase(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection write fd(" << get_fd().get_fd() << ',' << m_up->get_state() << ',' << m_up->last_command() << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void
PeerConnectionSeed::read_have_chunk(uint32_t index) {
  if (index >= m_peerChunks.bitfield()->size_bits())
    throw network_error("Peer sent HAVE message with out-of-range index.");

  if (m_peerChunks.bitfield()->get(index))
    return;

  m_download->chunk_statistics()->received_have_chunk(&m_peerChunks, index, m_download->content()->chunk_size());
  //m_download->chunk_selector()->received_have_chunk(&m_peerChunks, index);

  if (m_peerChunks.bitfield()->is_all_set())
    throw close_connection();
}

}