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

#include <functional>
#include <sstream>
#include <sigc++/signal.h>

#include "torrent/exceptions.h"
#include "tracker_control.h"
#include "tracker_http.h"

namespace torrent {

// m_tries is -1 if last connection wasn't successfull or we haven't tried yet.

TrackerControl::TrackerControl(const std::string& hash, const std::string& key) :
  m_tries(-1),
  m_interval(1800),
  m_state(TrackerInfo::STOPPED),
  m_taskTimeout(sigc::mem_fun(*this, &TrackerControl::query_current)) {
  
  m_info.set_hash(hash);
  m_info.set_key(key);

  m_itr = m_list.end();
}

void
TrackerControl::add_url(int group, const std::string& url) {
  if (m_itr != m_list.end() && m_itr->second->is_busy())
    throw internal_error("Added tracker url while the current tracker is busy");

  if (url.empty())
    throw input_error("Tried to add an empty tracker url");

  TrackerHttp* t = new TrackerHttp(&m_info, url);
  
  t->signal_done().connect(sigc::mem_fun(*this, &TrackerControl::receive_done));
  t->signal_failed().connect(sigc::mem_fun(*this, &TrackerControl::receive_failed));

  m_list.insert(group, t);

  // Set to the first element since we can't be certain the last one
  // wasn't invalidated. Don't allow when busy?
  m_itr = m_list.begin();
}

void
TrackerControl::set_next_time(Timer interval) {
  if (m_taskTimeout.is_scheduled())
    m_taskTimeout.insert(std::max(Timer::cache() + interval, m_timerMinInterval));
}

Timer
TrackerControl::get_next_time() {
  return m_taskTimeout.is_scheduled() ? std::max(m_taskTimeout.get_time() - Timer::cache(), Timer(0)) : 0;
}

bool
TrackerControl::is_busy() {
  if (m_itr == m_list.end())
    return false;
  else
    return m_itr->second->is_busy();
}

void
TrackerControl::send_state(TrackerInfo::State s) {
  if ((m_state == TrackerInfo::STOPPED && s == TrackerInfo::STOPPED) || m_itr == m_list.end())
    return;

  m_tries = -1;
  m_state = s;
  m_timerMinInterval = 0;

  // Reset the target tracker since we're doing a new request.
  m_itr->second->close();
  m_itr = m_list.begin();

  query_current();
  m_taskTimeout.remove();
}

void
TrackerControl::cancel() {
  if (m_itr == m_list.end())
    return;

  m_itr->second->close();
  m_itr = m_list.begin();
}

void
TrackerControl::receive_done(Bencode& bencode) {
  PeerList l;

  m_signalBencode.emit(bencode);

  try {

  parse_check_failure(bencode);
  parse_fields(bencode);

  if (bencode["peers"].is_string())
    parse_peers_compact(l, bencode["peers"].as_string());
  else
    parse_peers_normal(l, bencode["peers"].as_list());

  } catch (bencode_error& e) {
    return receive_failed(e.what());
  }

  // Successful tracker request, rearrange the list.
  m_list.promote(m_itr);
  m_itr = m_list.begin();

  if (m_state != TrackerInfo::STOPPED) {
    m_state = TrackerInfo::NONE;
    
    m_taskTimeout.insert(Timer::cache() + (int64_t)m_interval * 1000000);
  }

  m_signalPeers.emit(l);
}

void
TrackerControl::receive_failed(const std::string& msg) {
  ++m_itr;

  if (m_itr == m_list.end())
    m_itr = m_list.begin();

  if (m_state != TrackerInfo::STOPPED) {
    // TODO: Add support for multiple trackers. Iterate if m_failed > X.
    m_taskTimeout.insert(Timer::cache() + 20 * 1000000);
  }

  m_signalFailed.emit(msg);
}

void
TrackerControl::query_current() {
  if (m_itr == m_list.end())
    throw internal_error("TrackerControl tried to send with an invalid m_itr");

  m_itr->second->send_state(m_state, m_slotStatDown(), m_slotStatUp(), m_slotStatLeft());
}

void
TrackerControl::parse_check_failure(const Bencode& b) {
  if (!b.is_map())
    throw bencode_error("Root not a bencoded map");

  if (b.has_key("failure reason"))
    throw bencode_error("Failure reason \"" + b["failure reason"].as_string() + "\"");
}

void
TrackerControl::parse_fields(const Bencode& b) {
  if (b.has_key("interval") && b["interval"].is_value())
    m_interval = std::max<int64_t>(60, b["interval"].as_value());
  
  if (b.has_key("min interval") && b["min interval"].is_value())
    m_timerMinInterval = Timer::cache() + std::max<int64_t>(0, b["min interval"].as_value()) * 1000000;

  if (b.has_key("tracker id") && b["tracker id"].is_string())
    m_itr->second->set_tracker_id(b["tracker id"].as_string());
}

PeerInfo
TrackerControl::parse_peer(const Bencode& b) {
  PeerInfo p;
	
  if (!b.is_map())
    return p;

  for (Bencode::Map::const_iterator itr = b.as_map().begin(); itr != b.as_map().end(); ++itr) {
    if (itr->first == "ip" &&
	itr->second.is_string()) {
      p.set_dns(itr->second.as_string());
	    
    } else if (itr->first == "peer id" &&
	       itr->second.is_string()) {
      p.set_id(itr->second.as_string());
	    
    } else if (itr->first == "port" &&
	       itr->second.is_value()) {
      p.set_port(itr->second.as_value());
    }
  }
	
  return p;
}

void
TrackerControl::parse_peers_normal(PeerList& l, const Bencode::List& b) {
  for (Bencode::List::const_iterator itr = b.begin(); itr != b.end(); ++itr) {
    PeerInfo p = parse_peer(*itr);
	  
    if (p.is_valid())
      l.push_back(p);
  }
}  

void
TrackerControl::parse_peers_compact(PeerList& l, const std::string& s) {
  for (std::string::const_iterator itr = s.begin(); itr + 6 <= s.end();) {

    std::stringstream buf;

    buf << (int)(unsigned char)*itr++ << '.'
	<< (int)(unsigned char)*itr++ << '.'
	<< (int)(unsigned char)*itr++ << '.'
	<< (int)(unsigned char)*itr++;

    uint16_t port = (unsigned short)((unsigned char)*itr++) << 8;
    port += (uint16_t)((unsigned char)*itr++);

    l.push_back(PeerInfo("", buf.str(), port));
  }
}

}