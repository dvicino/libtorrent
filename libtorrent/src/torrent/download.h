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

#ifndef LIBTORRENT_DOWNLOAD_H
#define LIBTORRENT_DOWNLOAD_H

#include <torrent/peer.h>

#include <iosfwd>
#include <list>
#include <vector>
#include <string>
#include <inttypes.h>
#include <sigc++/slot.h>
#include <sigc++/connection.h>

namespace torrent {

typedef std::list<Peer> PList;

class DownloadWrapper;
class FileList;
class Rate;
class Object;
class Peer;
class TrackerList;

// Download is safe to copy and destory as it is just a pointer to an
// internal class.

class Download {
public:
  static const uint32_t numwanted_diabled = ~(uint32_t)0;

  Download(DownloadWrapper* d = NULL) : m_ptr(d) {}

  // Not active atm. Opens and prepares/closes the files.
  void                 open();
  void                 close();

  // Torrent must be open for calls to hash_check(bool) and
  // hash_resume_save(). hash_resume_clear() removes resume data from
  // the bencode'ed torrent.
  void                 hash_check(bool resume = true);
  void                 hash_resume_save();
  void                 hash_resume_clear();

  // Start/stop the download. The torrent must be open.
  void                 start();
  void                 stop();

  // Does not check if the download has been removed.
  bool                 is_valid() const { return m_ptr; }

  bool                 is_open() const;
  bool                 is_active() const;
  bool                 is_tracker_busy() const;

  bool                 is_hash_checked() const;
  bool                 is_hash_checking() const;

  // Returns "" if the object is not valid.
  std::string          name() const;
  std::string          info_hash() const;
  std::string          local_id() const;

  // Unix epoche, 0 == unknown.
  uint32_t             creation_date() const;

  Object&              bencode();
  const Object&        bencode() const;

  FileList             file_list() const;
  TrackerList          tracker_list() const;

  Rate*                down_rate();
  const Rate*          down_rate() const;

  Rate*                up_rate();
  const Rate*          up_rate() const;

  // Bytes completed.
  uint64_t             bytes_done() const;
  // Size of the torrent.
  uint64_t             bytes_total() const;

  uint32_t             chunks_size() const;
  uint32_t             chunks_done() const;
  uint32_t             chunks_total() const;
  uint32_t             chunks_hashed() const;

  const uint8_t*       chunks_seen() const;

  const unsigned char* bitfield_data() const;
  uint32_t             bitfield_size() const;

  uint32_t             peers_min() const;
  uint32_t             peers_max() const;
  uint32_t             peers_connected() const;
  uint32_t             peers_not_connected() const;
  uint32_t             peers_complete() const;
  uint32_t             peers_accounted() const;

  uint32_t             peers_currently_unchoked() const;
  uint32_t             peers_currently_interested() const;

  uint32_t             uploads_max() const;
  
  void                 set_peers_min(uint32_t v);
  void                 set_peers_max(uint32_t v);

  void                 set_uploads_max(uint32_t v);

  typedef enum {
    CONNECTION_LEECH,
    CONNECTION_SEED
  } ConnectionType;

  ConnectionType       connection_type() const;
  void                 set_connection_type(ConnectionType t);

  // Call this when you want the modifications of the download priorities
  // in the entries to take effect. It is slightly expensive as it rechecks
  // all the peer bitfields to see if we are still interested.
  void                 update_priorities();

  // If you create a peer list, you *must* keep it up to date with the signals
  // peer_{connected,disconnected}. Otherwise you may experience undefined
  // behaviour when using invalid peers in the list.
  void                 peer_list(PList& pList);
  Peer                 peer_find(const std::string& id);

  void                 disconnect_peer(Peer p);

  typedef sigc::slot0<void>                     slot_void_type;
  typedef sigc::slot1<void, const std::string&> slot_string_type;

  typedef sigc::slot1<void, Peer>               slot_peer_type;
  typedef sigc::slot1<void, std::istream*>      slot_istream_type;
  typedef sigc::slot1<void, uint32_t>           slot_chunk_type;

  // signal_download_done is a delayed signal so it is safe to
  // stop/close the torrent when received. The signal is only emitted
  // when the torrent is active, so hash checking will not trigger it.
  sigc::connection    signal_download_done(slot_void_type s);
  sigc::connection    signal_hash_done(slot_void_type s);

  sigc::connection    signal_peer_connected(slot_peer_type s);
  sigc::connection    signal_peer_disconnected(slot_peer_type s);

  sigc::connection    signal_tracker_succeded(slot_void_type s);
  sigc::connection    signal_tracker_failed(slot_string_type s);

  sigc::connection    signal_chunk_passed(slot_chunk_type s);
  sigc::connection    signal_chunk_failed(slot_chunk_type s);

  // Various network log message signals.
  sigc::connection    signal_network_log(slot_string_type s);

  // Emits error messages if there are problems opening files for
  // read/write when the download is active. The client should stop
  // the download if it receive any of these as it will not be able to
  // continue.
  sigc::connection    signal_storage_error(slot_string_type s);

  DownloadWrapper*    ptr() { return m_ptr; }

private:
  DownloadWrapper*    m_ptr;
};

}

#endif

