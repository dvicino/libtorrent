// rTorrent - BitTorrent client
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

#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <istream>
#include <sigc++/bind.h>
#include <sigc++/hide.h>
#include <torrent/bencode.h>
#include <torrent/exceptions.h>
#include <torrent/torrent.h>

#include "download.h"
#include "manager.h"
#include "curl_get.h"

namespace core {

static void
connect_signal_network_log(Download* d, torrent::Download::SlotString s) {
  d->get_download().signal_network_log(s);
}

void
Manager::initialize() {
  torrent::Http::set_factory(m_poll.get_http_factory());
  m_httpQueue.slot_factory(m_poll.get_http_factory());

  CurlStack::init();

  if (!torrent::listen_open(m_portFirst, m_portLast))
    throw std::runtime_error("Could not open port for listening.");

  // Register slots to be called when a download is inserted/erased,
  // opened or closed.
  m_downloadList.slot_map_insert().insert("2_connect_network_log", sigc::bind(sigc::ptr_fun(&connect_signal_network_log), sigc::mem_fun(m_logComplete, &Log::push_front)));
  m_downloadList.slot_map_insert().insert("3_manager_start",       sigc::mem_fun(*this, &Manager::start));
  m_downloadList.slot_map_insert().insert("4_store_save",          sigc::mem_fun(m_downloadStore, &DownloadStore::save));

  m_downloadList.slot_map_erase().insert("1_hash_queue_remove",    sigc::mem_fun(m_hashQueue, &HashQueue::remove));
  m_downloadList.slot_map_erase().insert("1_store_remove",         sigc::mem_fun(m_downloadStore, &DownloadStore::remove));

  //m_downloadList.slot_map_open().insert("1_download_open",         sigc::mem_fun(&Download::open));
  m_downloadList.slot_map_open().insert("1_download_open",         sigc::mem_fun(&Download::call<void, &torrent::Download::open>));

  // Currently does not call stop, might want to add a function that
  // checks if we're running, and if so stop?
  m_downloadList.slot_map_close().insert("1_download_close",       sigc::mem_fun(&Download::call<void, &torrent::Download::close>));
  m_downloadList.slot_map_close().insert("1_hash_queue_remove",    sigc::mem_fun(m_hashQueue, &HashQueue::remove));

  m_downloadList.slot_map_start().insert("1_download_start",       sigc::mem_fun(&Download::call<void, &torrent::Download::start>));

  m_downloadList.slot_map_stop().insert("1_download_stop",         sigc::mem_fun(&Download::call<void, &torrent::Download::stop>));
  m_downloadList.slot_map_stop().insert("2_hash_resume_save",      sigc::mem_fun(&Download::call<void, &torrent::Download::hash_resume_save>));
  m_downloadList.slot_map_stop().insert("3_store_save",            sigc::mem_fun(m_downloadStore, &DownloadStore::save));
}

void
Manager::cleanup() {
  // Need to disconnect log signals? Not really since we won't receive
  // any more.

  torrent::cleanup();
  core::CurlStack::cleanup();
}

void
Manager::insert(std::string uri) {
  if (std::strncmp(uri.c_str(), "http://", 7) == 0) {
    create_http(uri);
  } else {
    std::fstream f(uri.c_str(), std::ios::in);
    create_final(&f);
  }
}

Manager::iterator
Manager::erase(DownloadList::iterator itr) {
  if ((*itr)->get_download().is_active())
    throw std::logic_error("core::Manager::erase(...) called on an active download");

  if (!(*itr)->get_download().is_open())
    throw std::logic_error("core::Manager::erase(...) called on an closed download");

  return m_downloadList.erase(itr);
}  

void
Manager::start(Download* d) {
  try {
    if (d->get_download().is_active())
      return;

    if (!d->get_download().is_open())
      m_downloadList.open(d);

    if (d->get_download().is_hash_checked())
      m_downloadList.start(d);
    else
      // This can cause infinit loops.
      m_hashQueue.insert(d, sigc::bind(sigc::mem_fun(m_downloadList, &DownloadList::start), d));

  } catch (torrent::local_error& e) {
    m_logImportant.push_front(e.what());
    m_logComplete.push_front(e.what());
  }
}

void
Manager::stop(Download* d) {
  try {
    m_downloadList.stop(d);

  } catch (torrent::local_error& e) {
    m_logImportant.push_front(e.what());
    m_logComplete.push_front(e.what());
  }
}

void
Manager::check_hash(Download* d) {
  bool restart = d->get_download().is_active();

  try {
    m_downloadList.close(d);
    d->get_download().hash_resume_clear();
    m_downloadList.open(d);

    if (d->get_download().is_hash_checking() ||
	d->get_download().is_hash_checked())
      throw std::logic_error("Manager::check_hash(...) closed the torrent but is_hash_check{ing,ed}() == true");

    if (m_hashQueue.find(d) != m_hashQueue.end())
      throw std::logic_error("Manager::check_hash(...) closed the torrent but it was found in m_hashQueue");

    if (restart)
      m_hashQueue.insert(d, sigc::bind(sigc::mem_fun(m_downloadList, &DownloadList::start), d));
    else
      m_hashQueue.insert(d, sigc::slot0<void>());

  } catch (torrent::local_error& e) {
    m_logImportant.push_front(e.what());
    m_logComplete.push_front(e.what());
  }
}  

void
Manager::create_http(const std::string& uri) {
  core::HttpQueue::iterator itr = m_httpQueue.insert(uri);

  (*itr)->signal_done().slots().push_front(sigc::bind(sigc::mem_fun(*this, &core::Manager::create_final),
						      (*itr)->get_stream()));
  (*itr)->signal_failed().slots().push_front(sigc::mem_fun(*this, &core::Manager::receive_http_failed));
}

void
Manager::create_final(std::istream* s) {
  try {
    m_downloadList.insert(s);

  } catch (torrent::local_error& e) {
    // What to do? Keep in list for now.
    m_logImportant.push_front(e.what());
    m_logComplete.push_front(e.what());
  }
}

void
Manager::receive_http_failed(std::string msg) {
  m_logImportant.push_front("Http download error: \"" + msg + "\"");
  m_logComplete.push_front("Http download error: \"" + msg + "\"");
}

}