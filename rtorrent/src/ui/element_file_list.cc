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

#include "display/window_file_list.h"

#include "control.h"
#include "element_file_list.h"

namespace ui {

ElementFileList::ElementFileList(core::Download* d) :
  m_download(d),
  m_window(NULL),
  m_focus(0) {

  m_bindings[' '] = sigc::mem_fun(*this, &ElementFileList::receive_priority);
  m_bindings['*'] = sigc::mem_fun(*this, &ElementFileList::receive_change_all);
  m_bindings[KEY_DOWN] = sigc::mem_fun(*this, &ElementFileList::receive_next);
  m_bindings[KEY_UP] = sigc::mem_fun(*this, &ElementFileList::receive_prev);
}

void
ElementFileList::activate(Control* c, MItr mItr) {
  if (m_window != NULL)
    throw std::logic_error("ui::ElementFileList::activate(...) called on an object in the wrong state");

  c->get_input().push_front(&m_bindings);

  *mItr = m_window = new WFileList(m_download, &m_focus);
}

void
ElementFileList::disable(Control* c) {
  if (m_window == NULL)
    throw std::logic_error("ui::ElementFileList::disable(...) called on an object in the wrong state");

  c->get_input().erase(&m_bindings);

  delete m_window;
  m_window = NULL;
}

void
ElementFileList::receive_next() {
  if (m_window == NULL)
    throw std::logic_error("ui::ElementFileList::receive_next(...) called on a disabled object");

  if (++m_focus >= m_download->get_download().get_entry_size())
    m_focus = 0;

  m_window->mark_dirty();
}

void
ElementFileList::receive_prev() {
  if (m_window == NULL)
    throw std::logic_error("ui::ElementFileList::receive_prev(...) called on a disabled object");

  if (m_download->get_download().get_entry_size() == 0)
    return;

  if (m_focus != 0)
    --m_focus;
  else 
    m_focus = m_download->get_download().get_entry_size() - 1;

  m_window->mark_dirty();
}

void
ElementFileList::receive_priority() {
  if (m_window == NULL)
    throw std::logic_error("ui::ElementFileList::receive_prev(...) called on a disabled object");

  if (m_focus >= m_download->get_download().get_entry_size())
    return;

  torrent::Entry e = m_download->get_download().get_entry(m_focus);

  e.set_priority(next_priority(e.get_priority()));

  m_download->get_download().update_priorities();
  m_window->mark_dirty();
}

void
ElementFileList::receive_change_all() {
  if (m_window == NULL)
    throw std::logic_error("ui::ElementFileList::receive_prev(...) called on a disabled object");

  if (m_focus >= m_download->get_download().get_entry_size())
    return;

  Priority p = next_priority(m_download->get_download().get_entry(m_focus).get_priority());

  for (int i = 0, e = m_download->get_download().get_entry_size(); i != e; ++i)
    m_download->get_download().get_entry(i).set_priority(p);

  m_download->get_download().update_priorities();
  m_window->mark_dirty();
}

ElementFileList::Priority
ElementFileList::next_priority(Priority p) {
  switch(p) {
  case torrent::Entry::STOPPED:
    return torrent::Entry::HIGH;

  case torrent::Entry::NORMAL:
    return torrent::Entry::STOPPED;

  case torrent::Entry::HIGH:
    return torrent::Entry::NORMAL;
	
  default:
    return torrent::Entry::NORMAL;
  };
}

}
