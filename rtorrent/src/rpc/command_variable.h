// rTorrent - BitTorrent client
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

#ifndef RTORRENT_RPC_COMMAND_VARIABLES_H
#define RTORRENT_RPC_COMMAND_VARIABLES_H

#include <string>
#include <limits>
#include <inttypes.h>
#include <torrent/object.h>

#include "command.h"

namespace rpc {

class CommandVariable : public Command {
public:
  typedef target_wrapper<void>::cleaned_type cleaned_type;

  CommandVariable(const torrent::Object& v = torrent::Object()) : m_variable(v) {}
  
  const torrent::Object variable() const                         { return m_variable; }
  void                  set_variable(const torrent::Object& var) { m_variable = var; }

  static const torrent::Object set_bool(Command* rawCommand, cleaned_type target, const torrent::Object& args);
  static const torrent::Object get_bool(Command* rawCommand, cleaned_type target, const torrent::Object& args);

  static const torrent::Object set_value(Command* rawCommand, cleaned_type target, const torrent::Object& args);
  static const torrent::Object get_value(Command* rawCommand, cleaned_type target, const torrent::Object& args);

  static const torrent::Object set_string(Command* rawCommand, cleaned_type target, const torrent::Object& args);
  static const torrent::Object get_string(Command* rawCommand, cleaned_type target, const torrent::Object& args);

private:
  torrent::Object    m_variable;
};

class CommandObjectPtr : public Command {
public:
  typedef target_wrapper<void>::cleaned_type cleaned_type;

  CommandObjectPtr(torrent::Object* obj = NULL) : m_object(obj) {}
  
  const torrent::Object* object() const                   { return m_object; }
  void                   set_object(torrent::Object* obj) { m_object = obj; }

  static const torrent::Object set_generic(Command* rawCommand, cleaned_type target, const torrent::Object& args);
  static const torrent::Object get_generic(Command* rawCommand, cleaned_type target, const torrent::Object& args);

//   static const torrent::Object set_bool(Command* rawCommand, cleaned_type target, const torrent::Object& args);
//   static const torrent::Object get_bool(Command* rawCommand, cleaned_type target, const torrent::Object& args);

//   static const torrent::Object set_value(Command* rawCommand, cleaned_type target, const torrent::Object& args);
//   static const torrent::Object get_value(Command* rawCommand, cleaned_type target, const torrent::Object& args);

//   static const torrent::Object set_string(Command* rawCommand, cleaned_type target, const torrent::Object& args);
//   static const torrent::Object get_string(Command* rawCommand, cleaned_type target, const torrent::Object& args);

private:
  torrent::Object*    m_object;
};

}

#endif
