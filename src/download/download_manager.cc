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

#include <algo/algo.h>

#include "torrent/exceptions.h"

#include "download_manager.h"
#include "download_wrapper.h"

namespace torrent {

using namespace algo;

void
DownloadManager::add(DownloadWrapper* d) {
  if (find(d->get_hash()))
    throw input_error("Could not add download, info-hash already exists.");

  m_downloads.push_back(d);
}

void
DownloadManager::remove(const std::string& hash) {
  DownloadList::iterator itr = std::find_if(m_downloads.begin(), m_downloads.end(),
					    eq(ref(hash), call_member(&DownloadWrapper::get_hash)));

  if (itr == m_downloads.end())
    throw client_error("Tried to remove a DownloadMain that doesn't exist");
    
  delete *itr;
  m_downloads.erase(itr);
}

void
DownloadManager::clear() {
  while (!m_downloads.empty()) {
    delete m_downloads.front();
    m_downloads.pop_front();
  }
}

DownloadWrapper*
DownloadManager::find(const std::string& hash) {
  DownloadList::iterator itr = std::find_if(m_downloads.begin(), m_downloads.end(),
					    eq(ref(hash), call_member(&DownloadWrapper::get_hash)));

  return itr != m_downloads.end() ? *itr : NULL;
}

}
