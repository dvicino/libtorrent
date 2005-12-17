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

#include <stdexcept>
#include <sigc++/bind.h>
#include <sigc++/hide.h>
#include <torrent/exceptions.h>

#include "option_handler.h"

void
OptionHandler::insert(const std::string& key, OptionHandlerBase* opt) {
  iterator itr = find(key);

  if (itr == end()) {
    Base::insert(value_type(key, opt));
  } else {
    delete itr->second;
    itr->second = opt;
  }
}

void
OptionHandler::erase(const std::string& key) {
  iterator itr = find(key);

  if (itr == end())
    return;

  delete itr->second;
  Base::erase(itr);
}

void
OptionHandler::clear() {
  for (iterator itr = begin(), last = end(); itr != last; ++itr)
    delete itr->second;

  Base::clear();
}

void
OptionHandler::process(const std::string& key, const std::string& arg) const {
  const_iterator itr = find(key);

  if (itr == end())
    throw torrent::input_error("Could not find option key \"" + key + "\".");

  itr->second->process(key, arg);
}

void
OptionHandler::process_command(const std::string& command) const {
  std::string::size_type pos = command.find('=');

  if (pos == std::string::npos)
    throw torrent::input_error("Option handler could not find '=' in command.");

  process(command.substr(0, pos), command.substr(pos + 1, std::string::npos));
}
