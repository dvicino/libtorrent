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

#include <cerrno>
#include <sstream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <algo/algo.h>

#include "torrent/exceptions.h"
#include "download/download_net.h"

#include "download/download_state.h"
#include "peer_connection.h"
#include "net/poll.h"
#include "settings.h"

using namespace algo;

namespace torrent {

PeerConnection*
PeerConnection::create(SocketFd fd, const PeerInfo& p, DownloadState* d, DownloadNet* net) {
  PeerConnection* pc = new PeerConnection;
  pc->set(fd, p, d, net);

  return pc;
}

void PeerConnection::set(SocketFd fd, const PeerInfo& p, DownloadState* d, DownloadNet* net) {
  if (m_fd.is_valid())
    throw internal_error("Tried to re-set PeerConnection");

  m_fd = fd;
  m_peer = p;
  m_download = d;
  m_net = net;

  m_fd.set_throughput();

  m_requests.set_delegator(&m_net->get_delegator());
  m_requests.set_bitfield(&m_bitfield.get_bitfield());

  if (d == NULL || !p.is_valid() || !m_fd.is_valid())
    throw internal_error("PeerConnection set recived bad input");

  // Set the bitfield size and zero it
  m_bitfield = BitFieldExt(d->get_chunk_total());

  if (m_bitfield.begin() == NULL)
    throw internal_error("PeerConnection::set(...) did not properly initialize m_bitfield"); 

  Poll::read_set().insert(this);
  Poll::write_set().insert(this);
  Poll::except_set().insert(this);

  m_up.m_buf.reset_end();
  m_down.m_buf.reset_end();

  if (!d->get_content().get_bitfield().all_zero()) {
    // Send bitfield to peer.
    bufCmd(BITFIELD, 1 + m_download->get_content().get_bitfield().size_bytes());
    m_up.length = m_up.m_buf.size();
    m_up.m_buf.reset_end();

    m_up.state = WRITE_MSG;
  }
    
  m_taskKeepAlive.insert(Timer::current() + 120 * 1000000);

  m_lastMsg = Timer::current();
}

void PeerConnection::read() {
  Piece piece;

  m_lastMsg = Timer::cache();

  try {
    
  evil_goto_read:
    
  switch (m_down.state) {
  case IDLE:
    m_down.m_buf.reset_end();
    m_down.state = READ_LENGTH;

  case READ_LENGTH:
    m_down.m_buf.move_end(read_buf(m_down.m_buf.end(), 4 - m_down.m_buf.size()));

    if (m_down.m_buf.size() != 4)
      return;

    m_down.m_buf.reset_end();
    m_down.lengthOrig = m_down.length = bufR32(true); // Peek so we don't need to reset again.

    if (m_down.length == 0) {
      // Received ping.

      m_down.state = IDLE;
      m_down.lastCommand = KEEP_ALIVE;

      return;

    } else if (m_down.length > (1 << 17) + 9) {
      std::stringstream s;
      s << "Recived packet with length 0x" << std::hex << m_down.length;

      throw communication_error(s.str());
    }
    
    m_down.state = READ_TYPE;

    // TMP
    if (m_down.m_buf.size() != 0)
      throw internal_error("Length bork bork bork1");

    // TODO: Read up to 9 or something.
  case READ_TYPE:
    m_down.m_buf.move_end(read_buf(m_down.m_buf.end(), 1));

    if (m_down.m_buf.size() != 1)
      return;

    m_down.m_buf.reset_end();

    switch (m_down.m_buf.peek_uint8()) {
    case REQUEST:
    case CANCEL:
      if (m_down.length != 13)
	throw communication_error("Recived request/cancel command with wrong size");

      break;

    case HAVE:
      if (m_down.length != 5)
	throw communication_error("Recived have command with wrong size");
      
      break;

    case PIECE:
      if (m_down.length < 9 || m_down.length > 9 + (1 << 17))
	throw communication_error("Received piece message with bad length");

      m_down.length = 9;

      break;

    case BITFIELD:
      if (m_down.length != 1 + m_bitfield.size_bytes()) {
	std::stringstream s;

	s << "Recived bitfield message with wrong size " << m_down.length
	  << ' ' << m_bitfield.size_bytes() << ' ';

	throw communication_error(s.str());

      } else if (m_down.lastCommand != NONE) {
	throw communication_error("BitField received after other commands");
      }

      //m_net->signal_network_log().emit("Receiving bitfield");

      //m_down.m_buf.reset_end();
      m_down.m_pos2 = 0;
      m_down.state = READ_BITFIELD;

      goto evil_goto_read;

    default:
      if (m_down.m_buf.peek_uint8() > CANCEL)
	throw communication_error("Received unknown protocol command");

      // Handle 1 byte long messages here.
      //m_net->signal_network_log().emit("Receiving some commmand");

      break;
    };

    // Keep the command byte in the buffer.
    m_down.state = READ_MSG;
    // Read here so the next writes are at the right position.
    m_down.lastCommand = (Protocol)m_down.m_buf.read_uint8();

  case READ_MSG:
    if (m_down.length > 1)
      m_down.m_buf.move_end(read_buf(m_down.m_buf.end(), m_down.length - m_down.m_buf.size()));

    if (m_down.m_buf.size() != m_down.length)
      return;

    m_down.m_buf.reset_end();

    switch (m_down.m_buf.peek_uint8()) {
    case PIECE:
      if (m_down.lengthOrig == 9) {
	// Some clients send zero length messages when we request pieces
	// they don't have.
	m_net->signal_network_log().emit("Received piece with length zero");

	m_down.state = IDLE;
	goto evil_goto_read;
      }

      m_down.m_buf.read_uint8();
      piece.set_index(bufR32());
      piece.set_offset(bufR32());
      piece.set_length(m_down.lengthOrig - 9);
      
      m_down.m_pos2 = 0;
      
      if (m_requests.downloading(piece)) {
	m_down.state = READ_PIECE;
	load_chunk(m_requests.get_piece().get_index(), m_down);

      } else {
	// We don't want the piece,
	m_down.length = piece.get_length();
	m_down.state = READ_SKIP_PIECE;

	m_net->signal_network_log().emit("Receiving piece we don't want from " + m_peer.get_dns());
      }

      goto evil_goto_read;

    default:
      // parseReadBuf() will read the cmd.
      parseReadBuf();
      
      m_down.state = IDLE;
      goto evil_goto_read;
    }

  case READ_BITFIELD:
    m_down.m_pos2 += read_buf(m_bitfield.begin() + m_down.m_pos2, m_bitfield.size_bytes() - m_down.m_pos2);

    if (m_down.m_pos2 != m_bitfield.size_bytes())
      return;

    m_bitfield.update_count();

    if (!m_bitfield.all_zero() && m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
      m_up.interested = m_sendInterested = true;
      
    } else if (m_bitfield.all_set() && m_download->get_content().is_done()) {
      // Both sides are done so we might as well close the connection.
      throw close_connection();
    }

    m_down.state = IDLE;
    m_download->get_bitfield_counter().inc(m_bitfield.get_bitfield());

    Poll::write_set().insert(this);
    goto evil_goto_read;

  case READ_PIECE:
    if (!m_requests.is_downloading())
      throw internal_error("READ_PIECE state but RequestList is not downloading");

    if (!m_requests.is_wanted()) {
      m_down.state = READ_SKIP_PIECE;
      m_down.length = m_requests.get_piece().get_length() - m_down.m_pos2;
      m_down.m_pos2 = 0;

      m_requests.skip();

      goto evil_goto_read;
    }

    if (!readChunk())
      return;

    m_down.state = IDLE;
    m_tryRequest = true;

    m_requests.finished();
    
    // TODO: Find a way to avoid this remove/insert cycle.
    m_taskStall.remove();
    
    if (m_requests.get_size())
      m_taskStall.insert(Timer::cache() + m_download->get_settings().stallTimeout);

    // TODO: clear m_down.data?

    Poll::write_set().insert(this);

    goto evil_goto_read;

  case READ_SKIP_PIECE:
    if (m_down.m_pos2 != 0)
      throw internal_error("READ_SKIP_PIECE m_down.pos != 0");

    m_down.m_pos2 = read_buf(m_down.m_buf.begin(),
			     std::min<int>(m_down.length, m_down.m_buf.reserved()));

    if (m_down.m_pos2 == 0)
      return;

    m_throttle.down().insert(m_down.m_pos2);

    m_down.length -= m_down.m_pos2;
    m_down.m_pos2 = 0;

    if (m_down.length == 0) {
      // Done with this piece.
      m_down.state = IDLE;
      m_tryRequest = true;

      m_taskStall.remove();

      if (m_requests.get_size())
	m_taskStall.insert(Timer::cache() + m_download->get_settings().stallTimeout);
    }

    goto evil_goto_read;

  default:
    throw internal_error("peer_connectino::read() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_net->remove_connection(this);

  } catch (network_error& e) {
    m_net->signal_network_log().emit(e.what());

    m_net->remove_connection(this);

  } catch (storage_error& e) {
    m_download->signal_storage_error().emit(e.what());
    m_net->remove_connection(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection read fd(" << m_fd.get_fd() << ") state(" << m_down.state << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void PeerConnection::write() {
  bool s;
  int previous, maxBytes;

  try {

  evil_goto_write:

  switch (m_up.state) {
  case IDLE:
    m_up.m_buf.reset_end();

    // Keep alives must set the 5th bit to something IDLE

    if (m_shutdown)
      throw close_connection();

    fillWriteBuf();

    if (m_up.m_buf.size() == 0)
      return Poll::write_set().erase(this);

    m_up.state = WRITE_MSG;
    m_up.length = m_up.m_buf.size();
    m_up.m_buf.reset_end();

  case WRITE_MSG:
    m_up.m_buf.move_end(write_buf(m_up.m_buf.end(), m_up.length - m_up.m_buf.size()));

    if (m_up.m_buf.size() != m_up.length)
      return;

    switch (m_up.lastCommand) {
    case BITFIELD:
      m_up.state = WRITE_BITFIELD;
      m_up.m_pos2 = 0;

      goto evil_goto_write;

    case PIECE:
      // TODO: Do this somewhere else, and check to see if we are already using the right chunk
      if (m_sends.empty())
	throw internal_error("Tried writing piece without any requests in list");	  
	
      m_up.data = m_download->get_content().get_storage().get_chunk(m_sends.front().get_index(), MemoryChunk::prot_read);
      m_up.state = WRITE_PIECE;
      m_up.m_pos2 = 0;

      if (!m_up.data.is_valid())
	throw storage_error("Could not create a valid chunk");

      goto evil_goto_write;
      
    default:
      //m_net->signal_network_log().emit("Wrote message to peer");

      m_up.state = IDLE;
      return;
    }

  case WRITE_BITFIELD:
    m_up.m_pos2 += write_buf(m_download->get_content().get_bitfield().begin() + m_up.m_pos2,
			     m_download->get_content().get_bitfield().size_bytes() - m_up.m_pos2);

    if (m_up.m_pos2 == m_download->get_content().get_bitfield().size_bytes())
      m_up.state = IDLE;

    return;

  case WRITE_PIECE:
    if (m_sends.empty())
      throw internal_error("WRITE_PIECE on an empty list");

    previous = m_up.m_pos2;
    maxBytes = m_throttle.left();
    
    if (maxBytes == 0) {
      Poll::write_set().erase(this);
      return;
    }

    if (maxBytes < 0)
      throw internal_error("PeerConnection::write() got maxBytes <= 0");

    s = writeChunk(maxBytes);

    m_throttle.up().insert(m_up.m_pos2 - previous);
    m_throttle.spent(m_up.m_pos2 - previous);

    m_net->get_rate_up().insert(m_up.m_pos2 - previous);

    if (!s)
      return;

    if (m_sends.empty())
      m_up.data = Storage::Chunk();

    m_sends.pop_front();

    m_up.state = IDLE;
    return;

  default:
    throw internal_error("PeerConnection::write() called on object in wrong state");
  }

  } catch (close_connection& e) {
    m_net->remove_connection(this);

  } catch (network_error& e) {
    m_net->signal_network_log().emit(e.what());
    m_net->remove_connection(this);

  } catch (storage_error& e) {
    m_download->signal_storage_error().emit(e.what());
    m_net->remove_connection(this);

  } catch (base_error& e) {
    std::stringstream s;
    s << "Connection write fd(" << m_fd.get_fd() << ") state(" << m_up.state << ") \"" << e.what() << '"';

    e.set(s.str());

    throw;
  }
}

void PeerConnection::except() {
  m_net->signal_network_log().emit("Connection exception: " + std::string(strerror(errno)));

  m_net->remove_connection(this);
}

void PeerConnection::parseReadBuf() {
  uint32_t index, offset, length;
  SendList::iterator rItr;
  std::stringstream str;

  Protocol curCmd = (Protocol)m_down.m_buf.read_uint8();

  switch (curCmd) {
  case CHOKE:
    m_down.choked = true;
    m_requests.cancel();

    m_taskStall.remove();

    return;

  case UNCHOKE:
    m_down.choked = false;
    m_tryRequest = true;
    
    return Poll::write_set().insert(this);

  case INTERESTED:
    m_down.interested = true;

    // If we want to send stuff.
    if (m_up.choked &&
	m_net->can_unchoke() > 0) {
      choke(false);
    }
    
    return;

  case NOT_INTERESTED:
    m_down.interested = false;

    // Choke this uninterested peer and unchoke someone else.
    if (!m_up.choked) {
      choke(true);

      m_net->choke_balance();
    }

    return;

  case REQUEST:
  case CANCEL:
    if (m_up.choked)
      return;

    index = bufR32();
    offset = bufR32();
    length = bufR32();

    rItr = std::find(m_sends.begin(), m_sends.end(),
		     Piece(index, offset, length));
      
    if (curCmd == REQUEST) {
      if (rItr != m_sends.end())
	m_sends.erase(rItr);
      
      m_sends.push_back(Piece(index, offset, length));
      Poll::write_set().insert(this);

    } else if (rItr != m_sends.end()) {

      // Only cancel if we're not writing it.
      if (rItr != m_sends.begin() || m_up.lastCommand != PIECE || m_up.state == IDLE)
	m_sends.erase(rItr);
    }

    return Poll::write_set().insert(this);

  case HAVE:
    index = bufR32();

    if (index >= m_bitfield.size_bits())
      throw communication_error("Recived HAVE command with invalid value");

    if (!m_bitfield[index]) {
      m_bitfield.set(index, true);
      m_download->get_bitfield_counter().inc(index);
    }
    
    if (!m_up.interested && m_net->get_delegator().get_select().interested(index)) {
      // We are interested, send flag if not already set.
      m_sendInterested = !m_up.interested;
      m_up.interested = true;

      Poll::write_set().insert(this);
    }

    // Make sure m_tryRequest is set even if we were previously
    // interested. Super-Seeders seem to cause it to stall while we
    // are interested, but m_tryRequest is cleared.
    m_tryRequest = true;
    m_ratePeer.insert(m_download->get_content().get_storage().get_chunk_size());

    return;

  default:
    str << "Peer sent unsupported command " << curCmd;

    // TODO: this is a communication error.
    throw communication_error(str.str());
  };
}

// Don't depend on m_up.length!
void PeerConnection::fillWriteBuf() {
  if (m_sendChoked) {
    m_sendChoked = false;

    if ((Timer::cache() - m_lastChoked).usec() < 10 * 1000000) {
      // Wait with the choke message.
      m_taskSendChoke.insert(m_lastChoked + 10 * 1000000);

    } else {
      // CHOKE ME
      bufCmd(m_up.choked ? CHOKE : UNCHOKE, 1);
      
      m_lastChoked = Timer::cache();

      if (m_up.choked) {
	// Clear the request queue and mmaped chunk.
	m_sends.clear();
	m_up.data = Storage::Chunk();
	
	m_throttle.idle();
	
      } else {
	m_throttle.activate();
      }
    }
  }

  if (m_sendInterested) {
    bufCmd(m_up.interested ? INTERESTED : NOT_INTERESTED, 1);

    m_sendInterested = false;
  }

  uint32_t pipeSize;

  if (m_tryRequest && !m_down.choked && m_up.interested &&

      m_down.state != READ_SKIP_PIECE &&
      m_net->should_request(m_stallCount) &&
      m_requests.get_size() < (pipeSize = m_net->pipe_size(m_throttle.down()))) {

    m_tryRequest = false;

    while (m_requests.get_size() < pipeSize && m_up.m_buf.reserved_left() >= 16 && request_piece())

      if (m_requests.get_size() == 1) {
	if (m_taskStall.is_scheduled())
	  throw internal_error("Only one request, but we're already in task stall");
	
	m_tryRequest = true;
	m_taskStall.insert(Timer::cache() + m_download->get_settings().stallTimeout);
      }	
  }

  // Max buf size 17 * 'req pipe' + 10

  while (!m_haveQueue.empty() &&
	 m_up.m_buf.reserved_left() >= 9) {
    bufCmd(HAVE, 5);
    bufW32(m_haveQueue.front());

    m_haveQueue.pop_front();
  }

  if (!m_up.choked &&
      !m_sendChoked &&
      !m_sends.empty() &&
      m_up.m_buf.reserved_left() >= 13) {
    // Sending chunk to peer.

    // This check takes care of all possible errors in lenght and offset.
    if (m_sends.front().get_length() > (1 << 17) ||
	m_sends.front().get_length() == 0 ||

	m_sends.front().get_length() + m_sends.front().get_offset() >
	m_download->get_content().get_chunksize(m_sends.front().get_index())) {

      std::stringstream s;

      s << "Peer requested a piece with invalid length or offset: "
	<< m_sends.front().get_length() << ' '
	<< m_sends.front().get_offset();

      throw communication_error(s.str());
    }
      
    if (m_sends.front().get_index() < 0 ||
	m_sends.front().get_index() >= (signed)m_download->get_chunk_total() ||
	!m_download->get_content().get_bitfield()[m_sends.front().get_index()]) {
      std::stringstream s;

      s << "Peer requested a piece with invalid index: " << m_sends.front().get_index();

      throw communication_error(s.str());
    }

    bufCmd(PIECE, 9 + m_sends.front().get_length());
    bufW32(m_sends.front().get_index());
    bufW32(m_sends.front().get_offset());
  }
}

void PeerConnection::sendHave(int index) {
  m_haveQueue.push_back(index);

  if (m_download->get_content().is_done()) {
    // We're done downloading.

    if (m_bitfield.all_set()) {
      // Peer is done, close connection.
      m_shutdown = true;

    } else {
      m_sendInterested = m_up.interested;
      m_up.interested = false;
    }

  } else if (m_up.interested && !m_net->get_delegator().get_select().interested(m_bitfield.get_bitfield())) {
    // TODO: Optimize?
    m_sendInterested = true;
    m_up.interested = false;
  }

  if (m_requests.has_index(index))
    throw internal_error("PeerConnection::sendHave(...) found a request with the same index");

  // TODO: Also send cancel messages!

  // TODO: Remove this so we group the have messages with other stuff.
  Poll::write_set().insert(this);
}

void
PeerConnection::task_keep_alive() {
  // Check if remote peer is dead.
  if (Timer::cache() - m_lastMsg > 240 * 1000000) {
    m_net->remove_connection(this);
    return;
  }

  if (m_up.state == IDLE) {
    // TODO: don't use bufCmd
    m_up.m_buf.reset_end();
    m_up.m_buf.write_uint32(0);
    m_up.length = m_up.m_buf.size();
    m_up.m_buf.reset_end();

    m_up.lastCommand = KEEP_ALIVE;
    m_up.state = WRITE_MSG;

    Poll::write_set().insert(this);
  }

  m_tryRequest = true;
  m_taskKeepAlive.insert(Timer::cache() + 120 * 1000000);
}

void
PeerConnection::task_send_choke() {
  m_sendChoked = true;

  Poll::write_set().insert(this);
}

void
PeerConnection::task_stall() {
  m_stallCount++;
  m_requests.stall();

  // Make sure we regulary call task_stall() so stalled queues with new
  // entries get those new ones stalled if needed.
  m_taskStall.insert(Timer::cache() + m_download->get_settings().stallTimeout);

  //m_net->signal_network_log().emit("Peer stalled " + m_peer.get_dns());
}

}

