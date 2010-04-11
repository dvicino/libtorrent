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

#ifndef RTORRENT_UTILS_COMMAND_HELPERS_H
#define RTORRENT_UTILS_COMMAND_HELPERS_H

#include "rpc/command_new_slot.h"
#include "rpc/command_function.h"
#include "rpc/parse_commands.h"

namespace rpc {
  class CommandVariable;
  class CommandObjectPtr;
}

// By using a static array we avoid allocating the variables on the
// heap. This should reduce memory use and improve cache locality.
#define COMMAND_NEW_SLOTS_SIZE      500

#define ADDING_COMMANDS

extern rpc::command_base commandNewSlots[COMMAND_NEW_SLOTS_SIZE];
extern rpc::command_base* commandNewSlotItr;

void initialize_commands();

//
// New std::function based command_base helper functions:
//

#define CMD2_A_FUNCTION(key, function, slot, parm, doc)      \
  commandNewSlotItr->set_function<rpc::command_base_is_type<rpc::function>::type>(slot); \
  rpc::commands.insert_type(key, commandNewSlotItr++, &rpc::function,   \
                    rpc::CommandMap::flag_dont_delete | rpc::CommandMap::flag_public_xmlrpc, NULL, NULL);

#define CMD2_A_FUNCTION_PRIVATE(key, function, slot, parm, doc)      \
  commandNewSlotItr->set_function<rpc::command_base_is_type<rpc::function>::type>(slot); \
  rpc::commands.insert_type(key, commandNewSlotItr++, &rpc::function,   \
                    rpc::CommandMap::flag_dont_delete, NULL, NULL);

#define CMD2_ANY(key, slot)          CMD2_A_FUNCTION(key, command_base_call<rpc::target_type>, slot, "i:", "")

#define CMD2_ANY_P(key, slot)        CMD2_A_FUNCTION_PRIVATE(key, command_base_call<rpc::target_type>, slot, "i:", "")
#define CMD2_ANY_V(key, slot)        CMD2_A_FUNCTION(key, command_base_call_list<rpc::target_type>, object_convert_void(slot), "i:", "")
#define CMD2_ANY_L(key, slot)        CMD2_A_FUNCTION(key, command_base_call_list<rpc::target_type>, slot, "A:", "")

#define CMD2_ANY_VALUE(key, slot)    CMD2_A_FUNCTION(key, command_base_call_value<rpc::target_type>, slot, "i:i", "")
#define CMD2_ANY_VALUE_V(key, slot)  CMD2_A_FUNCTION(key, command_base_call_value<rpc::target_type>, object_convert_void(slot), "i:i", "")

#define CMD2_ANY_STRING(key, slot)   CMD2_A_FUNCTION(key, command_base_call_string<rpc::target_type>, slot, "i:s", "")
#define CMD2_ANY_STRING_V(key, slot) CMD2_A_FUNCTION(key, command_base_call_string<rpc::target_type>, object_convert_void(slot), "i:s", "")

#define CMD2_ANY_LIST(key, slot)     CMD2_A_FUNCTION(key, command_base_call_list<rpc::target_type>, slot, "i:", "")

#define CMD2_DL(key, slot)           CMD2_A_FUNCTION(key, command_base_call<core::Download*>, slot, "i:", "")
#define CMD2_DL_V(key, slot)         CMD2_A_FUNCTION(key, command_base_call<core::Download*>, object_convert_void(slot), "i:", "")
#define CMD2_DL_VALUE(key, slot)     CMD2_A_FUNCTION(key, command_base_call_value<core::Download*>, slot, "i:", "")
#define CMD2_DL_VALUE_V(key, slot)   CMD2_A_FUNCTION(key, command_base_call_value<core::Download*>, object_convert_void(slot), "i:", "")
#define CMD2_DL_STRING(key, slot)    CMD2_A_FUNCTION(key, command_base_call_string<core::Download*>, slot, "i:", "")
#define CMD2_DL_STRING_V(key, slot)  CMD2_A_FUNCTION(key, command_base_call_string<core::Download*>, object_convert_void(slot), "i:", "")
#define CMD2_DL_LIST(key, slot)      CMD2_A_FUNCTION(key, command_base_call_list<core::Download*>, slot, "i:", "")

#define CMD2_DL_VALUE_P(key, slot)   CMD2_A_FUNCTION_PRIVATE(key, command_base_call_value<core::Download*>, slot, "i:", "")
#define CMD2_DL_STRING_P(key, slot)  CMD2_A_FUNCTION_PRIVATE(key, command_base_call_string<core::Download*>, slot, "i:", "")

#define CMD2_FILE(key, slot)         CMD2_A_FUNCTION(key, command_base_call<torrent::File*>, slot, "i:", "")
#define CMD2_FILE_V(key, slot)       CMD2_A_FUNCTION(key, command_base_call<torrent::File*>, object_convert_void(slot), "i:", "")
#define CMD2_FILE_VALUE_V(key, slot) CMD2_A_FUNCTION(key, command_base_call_value<torrent::File*>, object_convert_void(slot), "i:i", "")

#define CMD2_FILEITR(key, slot)         CMD2_A_FUNCTION(key, command_base_call<torrent::FileListIterator*>, slot, "i:", "")

#define CMD2_PEER(key, slot)            CMD2_A_FUNCTION(key, command_base_call<torrent::Peer*>, slot, "i:", "")
#define CMD2_PEER_V(key, slot)          CMD2_A_FUNCTION(key, command_base_call<torrent::Peer*>, object_convert_void(slot), "i:", "")
#define CMD2_PEER_VALUE_V(key, slot)    CMD2_A_FUNCTION(key, command_base_call_value<torrent::Peer*>, object_convert_void(slot), "i:i", "")

#define CMD2_TRACKER(key, slot)         CMD2_A_FUNCTION(key, command_base_call<torrent::Tracker*>, slot, "i:", "")
#define CMD2_TRACKER_V(key, slot)       CMD2_A_FUNCTION(key, command_base_call<torrent::Tracker*>, object_convert_void(slot), "i:", "")
#define CMD2_TRACKER_VALUE_V(key, slot) CMD2_A_FUNCTION(key, command_base_call_value<torrent::Tracker*>, object_convert_void(slot), "i:i", "")

#define CMD2_VAR_BOOL(key, value)                                       \
  rpc::commands.call("method.insert", rpc::create_object_list(key, "bool|const", int64_t(value)));
#define CMD2_VAR_VALUE(key, value)                                      \
  rpc::commands.call("method.insert", rpc::create_object_list(key, "value|const", int64_t(value)));
#define CMD2_VAR_STRING(key, value)                                     \
  rpc::commands.call("method.insert", rpc::create_object_list(key, "string|const", std::string(value)));
#define CMD2_VAR_C_STRING(key, value)                                   \
  rpc::commands.call("method.insert", rpc::create_object_list(key, "string|static|const", std::string(value)));

#define CMD2_FUNC_SINGLE(key, cmds)                                  \
  CMD2_ANY(key, std::tr1::bind(&rpc::command_function_call, torrent::raw_string::from_c_str(cmds), \
                               std::tr1::placeholders::_1, std::tr1::placeholders::_2));

#define CMD2_REDIRECT(from_key, to_key) \
  rpc::commands.create_redirect(from_key, to_key, rpc::CommandMap::flag_public_xmlrpc);

#define CMD2_REDIRECT_GENERIC(from_key, to_key) \
  rpc::commands.create_redirect(from_key, to_key, rpc::CommandMap::flag_public_xmlrpc | rpc::CommandMap::flag_no_target);

//
// Conversion of return types:
//

template <typename Functor, typename Result>
struct object_convert_type;

template <typename Functor>
struct object_convert_type<Functor, void> {

  template <typename Signature> struct result {
    typedef torrent::Object type;
  };

  object_convert_type(Functor s) : m_slot(s) {}

  torrent::Object operator () () { m_slot(); return torrent::Object(); }
  template <typename Arg1>
  torrent::Object operator () (Arg1& arg1) { m_slot(arg1); return torrent::Object(); }
  template <typename Arg1, typename Arg2>
  torrent::Object operator () (Arg1& arg1, Arg2& arg2) { m_slot(arg1, arg2); return torrent::Object(); }

  Functor m_slot;
};

template <typename T>
object_convert_type<T, void>
object_convert_void(T f) { return f; }

#endif
