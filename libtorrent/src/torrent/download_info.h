// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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

#ifndef LIBTORRENT_DOWNLOAD_INFO_H
#define LIBTORRENT_DOWNLOAD_INFO_H

#include <list>
#include <string>
#include <inttypes.h>
#include <rak/socket_address.h>
#include <sigc++/signal.h>

#include <torrent/rate.h>
#include <torrent/hash_string.h>

namespace torrent {

class FileList;
class DownloadMain;

// This will become a Download 'handle' of kinds.

class DownloadInfo {
public:
  typedef sigc::slot0<uint64_t>                                        slot_stat_type;
  typedef sigc::signal0<void>                                          signal_void_type;
  typedef sigc::signal1<void, const std::string&>                      signal_string_type;
  typedef sigc::signal1<void, uint32_t>                                signal_chunk_type;
  typedef sigc::signal3<void, const std::string&, const char*, size_t> signal_dump_type;

  enum State {
    NONE,
    COMPLETED,
    STARTED,
    STOPPED
  };

  static const int flag_open                = (1 << 0);
  static const int flag_active              = (1 << 1);
  static const int flag_compact             = (1 << 2);
  static const int flag_accepting_new_peers = (1 << 3);
  static const int flag_private             = (1 << 4);
  static const int flag_meta_download       = (1 << 5);
  static const int flag_pex_enabled         = (1 << 6);
  static const int flag_pex_active          = (1 << 7);

  DownloadInfo();

  const std::string&  name() const                                 { return m_name; }
  void                set_name(const std::string& s)               { m_name = s; }

  const HashString&   hash() const                                 { return m_hash; }
  HashString&         mutable_hash()                               { return m_hash; }

  const HashString&   hash_obfuscated() const                      { return m_hashObfuscated; }
  HashString&         mutable_hash_obfuscated()                    { return m_hashObfuscated; }

  const HashString&   local_id() const                             { return m_localId; }
  HashString&         mutable_local_id()                           { return m_localId; }

  bool                is_open() const                              { return m_flags & flag_open; }
  bool                is_active() const                            { return m_flags & flag_active; }
  bool                is_compact() const                           { return m_flags & flag_compact; }
  bool                is_accepting_new_peers() const               { return m_flags & flag_accepting_new_peers; }
  bool                is_private() const                           { return m_flags & flag_private; }
  bool                is_meta_download() const                     { return m_flags & flag_meta_download; }
  bool                is_pex_enabled() const                       { return m_flags & flag_pex_enabled; }
  bool                is_pex_active() const                        { return m_flags & flag_pex_active; }

  void                set_flags(int flags)                         { m_flags |= flags; }
  void                unset_flags(int flags)                       { m_flags &= ~flags; }
  void                change_flags(int flags, bool state)          { if (state) set_flags(flags); else unset_flags(flags); }

  void                set_private()                                { set_flags(flag_private); unset_flags(flag_pex_enabled); }
  void                set_pex_enabled()                            { if (!is_private()) set_flags(flag_pex_enabled); }

  const Rate*         up_rate() const                              { return &m_upRate; }
  const Rate*         down_rate() const                            { return &m_downRate; }
  const Rate*         skip_rate() const                            { return &m_skipRate; }

  Rate*               mutable_up_rate() const                      { return &m_upRate; }
  Rate*               mutable_down_rate() const                    { return &m_downRate; }
  Rate*               mutable_skip_rate() const                    { return &m_skipRate; }

  uint64_t            uploaded_baseline() const                    { return m_uploadedBaseline; }
  uint64_t            uploaded_adjusted() const                    { return std::max<int64_t>(m_upRate.total() - uploaded_baseline(), 0); }
  void                set_uploaded_baseline(uint64_t b)            { m_uploadedBaseline = b; }

  uint64_t            completed_baseline() const                   { return m_completedBaseline; }
  uint64_t            completed_adjusted() const                   { return std::max<int64_t>(m_slotStatCompleted() - completed_baseline(), 0); }
  void                set_completed_baseline(uint64_t b)           { m_completedBaseline = b; }

  size_t              metadata_size() const                        { return m_metadataSize; }
  void                set_metadata_size(size_t size)               { m_metadataSize = size; }

  uint32_t            size_pex() const                             { return m_sizePex; }
  void                set_size_pex(uint32_t b)                     { m_sizePex = b; }

  uint32_t            max_size_pex() const                         { return m_maxSizePex; }
  void                set_max_size_pex(uint32_t b)                 { m_maxSizePex = b; }

  uint32_t            max_size_pex_list() const                    { return 200; }

  // Unix epoche, 0 == unknown.
  uint32_t            creation_date() const                        { return m_creationDate; }
  uint32_t            load_date() const                            { return m_loadDate; }

  uint32_t            http_timeout() const                         { return 60; }
  uint32_t            udp_timeout() const                          { return 30; }
  uint32_t            udp_tries() const                            { return 2; }

  // These signals are also used internally, so do not clear them or
  // trigger them.
  signal_void_type&   signal_initial_hash() const                  { return m_signalInitialHash; }
  signal_void_type&   signal_download_done() const                 { return m_signalDownloadDone; }

  signal_string_type& signal_network_log() const                   { return m_signalNetworkLog; }
  signal_string_type& signal_storage_error() const                 { return m_signalStorageError; }
  signal_dump_type&   signal_tracker_dump() const                  { return m_signalTrackerDump; }

  // The list of addresses is guaranteed to be sorted and unique.
  signal_void_type&   signal_tracker_success() const               { return m_signalTrackerSuccess; }
  signal_string_type& signal_tracker_failed() const                { return m_signalTrackerFailed; }

  signal_chunk_type&  signal_chunk_passed() const                  { return m_signalChunkPassed; }
  signal_chunk_type&  signal_chunk_failed() const                  { return m_signalChunkFailed; }

  //
  // Libtorrent internal:
  //

  void                set_creation_date(uint32_t d)                { m_creationDate = d; }

  slot_stat_type&     slot_left()                                  { return m_slotStatLeft; }
  slot_stat_type&     slot_completed()                             { return m_slotStatCompleted; }

private:
  std::string         m_name;
  HashString          m_hash;
  HashString          m_hashObfuscated;
  HashString          m_localId;

  int                 m_flags;

  mutable Rate        m_upRate;
  mutable Rate        m_downRate;
  mutable Rate        m_skipRate;

  uint64_t            m_uploadedBaseline;
  uint64_t            m_completedBaseline;
  uint32_t            m_sizePex;
  uint32_t            m_maxSizePex;
  size_t              m_metadataSize;

  uint32_t            m_creationDate;
  uint32_t            m_loadDate;

  slot_stat_type      m_slotStatLeft;
  slot_stat_type      m_slotStatCompleted;

  mutable signal_void_type    m_signalInitialHash;
  mutable signal_void_type    m_signalDownloadDone;

  mutable signal_string_type  m_signalNetworkLog;
  mutable signal_string_type  m_signalStorageError;
  mutable signal_dump_type    m_signalTrackerDump;

  mutable signal_void_type    m_signalTrackerSuccess;
  mutable signal_string_type  m_signalTrackerFailed;

  mutable signal_chunk_type   m_signalChunkPassed;
  mutable signal_chunk_type   m_signalChunkFailed;
};

}

#endif
