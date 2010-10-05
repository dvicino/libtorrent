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

#include "config.h"

#include <algorithm>

#include "globals.h"
#include "control.h"
#include "command_helpers.h"
#include "rpc/parse.h"

std::string
system_method_generate_command(torrent::Object::list_const_iterator first, torrent::Object::list_const_iterator last) {
  std::string command;

  while (first != last) {
    if (!command.empty())
      command += " ;";

    command += (first++)->as_string();
  }

  return command;
}

// torrent::Object
// system_method_insert_function(const torrent::Object::list_type& args, int flags) {
  
// }

torrent::Object
system_method_insert_object(const torrent::Object::list_type& args, int flags) {
  if (args.empty())
    throw torrent::input_error("Invalid argument count.");

  torrent::Object::list_const_iterator itrArgs = args.begin();
  const std::string& rawKey = (itrArgs++)->as_string();

  if (rawKey.empty() ||
      control->object_storage()->find_local(torrent::raw_string::from_string(rawKey)) != control->object_storage()->end(0) ||
      rpc::commands.has(rawKey) || rpc::commands.has(rawKey + ".set"))
    throw torrent::input_error("Invalid key.");

  torrent::Object value;

  switch (flags & rpc::object_storage::mask_type) {
  case rpc::object_storage::flag_bool_type:
  case rpc::object_storage::flag_value_type:
    value = itrArgs != args.end() ? rpc::convert_to_value(*itrArgs) : int64_t();
    break;
  case rpc::object_storage::flag_string_type:
    value = itrArgs != args.end() ? rpc::convert_to_string(*itrArgs) : "";
    break;
  case rpc::object_storage::flag_function_type:
    value = itrArgs != args.end() ? system_method_generate_command(itrArgs, args.end()) : "";
    break;
  case rpc::object_storage::flag_multi_type:
    break;
  default:
    throw torrent::input_error("Invalid type.");
  }

  int cmd_flags = 0;

  if (!(flags & rpc::object_storage::flag_static))
    cmd_flags |= rpc::CommandMap::flag_modifiable;
  if (!(flags & rpc::object_storage::flag_private))
    cmd_flags |= rpc::CommandMap::flag_public_xmlrpc;

  rpc::object_storage::iterator obj_itr =  control->object_storage()->insert_str(rawKey, value, flags);

  if ((flags & rpc::object_storage::mask_type) == rpc::object_storage::flag_function_type ||
      (flags & rpc::object_storage::mask_type) == rpc::object_storage::flag_multi_type) {

    rpc::commands.insert_slot<rpc::command_base_is_type<rpc::command_base_call<rpc::target_type> >::type>
      (create_new_key<0>(rawKey, ""),
       std::tr1::bind(&rpc::object_storage::call_function_str, control->object_storage(),
                      rawKey, std::tr1::placeholders::_1, std::tr1::placeholders::_2),
       &rpc::command_base_call<rpc::target_type>,
       cmd_flags, NULL, NULL);

  } else {
    rpc::commands.insert_slot<rpc::command_base_is_type<rpc::command_base_call<rpc::target_type> >::type>
      (create_new_key<0>(rawKey, ""),
       std::tr1::bind(&rpc::object_storage::get_str, control->object_storage(), rawKey),
       &rpc::command_base_call<rpc::target_type>,
       cmd_flags, NULL, NULL);
  }

  // TODO: Next... Make test class for this.

//   // Ehm... no proper handling if these throw.

  if (!(flags & rpc::object_storage::flag_constant)) {
    switch (flags & rpc::object_storage::mask_type) {
    case rpc::object_storage::flag_bool_type:
      rpc::commands.insert_slot<rpc::command_base_is_type<rpc::command_base_call_value<rpc::target_type> >::type>
        (create_new_key<5>(rawKey, ".set"),
         std::tr1::bind(&rpc::object_storage::set_str_bool, control->object_storage(), rawKey, std::tr1::placeholders::_2),
         &rpc::command_base_call_value<rpc::target_type>,
         cmd_flags, NULL, NULL);
      break;
    case rpc::object_storage::flag_value_type:
      rpc::commands.insert_slot<rpc::command_base_is_type<rpc::command_base_call_value<rpc::target_type> >::type>
        (create_new_key<5>(rawKey, ".set"),
         std::tr1::bind(&rpc::object_storage::set_str_value, control->object_storage(), rawKey, std::tr1::placeholders::_2),
         &rpc::command_base_call_value<rpc::target_type>,
         cmd_flags, NULL, NULL);
      break;
    case rpc::object_storage::flag_string_type:
      rpc::commands.insert_slot<rpc::command_base_is_type<rpc::command_base_call_string<rpc::target_type> >::type>
        (create_new_key<5>(rawKey, ".set"),
         std::tr1::bind(&rpc::object_storage::set_str_string, control->object_storage(), rawKey, std::tr1::placeholders::_2),
         &rpc::command_base_call_string<rpc::target_type>,
         cmd_flags, NULL, NULL);
      break;
    case rpc::object_storage::flag_function_type:
    case rpc::object_storage::flag_multi_type:
    default: break;
    }
  }

  return torrent::Object();
}

// method.insert <generic> {name, "simple|private|const", ...}
// method.insert <generic> {name, "multi|private|const"}
// method.insert <generic> {name, "value|private|const"}
// method.insert <generic> {name, "value|private|const", value}
// method.insert <generic> {name, "bool|private|const"}
// method.insert <generic> {name, "bool|private|const", bool}
// method.insert <generic> {name, "string|private|const"}
// method.insert <generic> {name, "string|private|const", string}
//
// Add a new user-defined method called 'name' and any number of
// lines.
//
// TODO: Make a static version of this that doesn't need to be called
// as a command, and which takes advantage of static const char
// strings, etc.
torrent::Object
system_method_insert(const torrent::Object::list_type& args) {
  if (args.empty() || ++args.begin() == args.end())
    throw torrent::input_error("Invalid argument count.");

  torrent::Object::list_const_iterator itrArgs = args.begin();
  const std::string& rawKey = (itrArgs++)->as_string();

  if (rawKey.empty() || rpc::commands.has(rawKey))
    throw torrent::input_error("Invalid key.");

  int flags = rpc::CommandMap::flag_delete_key | rpc::CommandMap::flag_modifiable | rpc::CommandMap::flag_public_xmlrpc;

  const std::string& options = itrArgs->as_string();

  if (options.find("private") != std::string::npos)
    flags &= ~rpc::CommandMap::flag_public_xmlrpc;
  if (options.find("const") != std::string::npos)
    flags &= ~rpc::CommandMap::flag_modifiable;

  if (options.find("multi") != std::string::npos) {
    torrent::Object::list_type new_args;
    new_args.push_back(rawKey);
    new_args.push_back(system_method_generate_command(++itrArgs, args.end()));

    int new_flags = rpc::object_storage::flag_multi_type;

    if (options.find("static") != std::string::npos)
      new_flags |= rpc::object_storage::flag_static;
    if (options.find("private") != std::string::npos)
      new_flags |= rpc::object_storage::flag_private;
    if (options.find("const") != std::string::npos)
      new_flags |= rpc::object_storage::flag_constant;

    return system_method_insert_object(new_args, new_flags);

  } else if (options.find("simple") != std::string::npos) {
    torrent::Object::list_type new_args;
    new_args.push_back(rawKey);
    new_args.push_back(system_method_generate_command(++itrArgs, args.end()));

    int new_flags = rpc::object_storage::flag_function_type;

    if (options.find("static") != std::string::npos)
      new_flags |= rpc::object_storage::flag_static;
    if (options.find("private") != std::string::npos)
      new_flags |= rpc::object_storage::flag_private;
    if (options.find("const") != std::string::npos)
      new_flags |= rpc::object_storage::flag_constant;

    return system_method_insert_object(new_args, new_flags);

  } else if (options.find("value") != std::string::npos ||
             options.find("bool") != std::string::npos ||
             options.find("string") != std::string::npos ||
             options.find("list") != std::string::npos ||
             options.find("simple") != std::string::npos) {
    torrent::Object::list_type new_args;
    new_args.push_back(rawKey);

    if (++itrArgs != args.end())
      new_args.insert(new_args.end(), itrArgs, args.end());

    int new_flags;

    if (options.find("value") != std::string::npos)
      new_flags = rpc::object_storage::flag_value_type;
    else if (options.find("bool") != std::string::npos)
      new_flags = rpc::object_storage::flag_bool_type;
    else if (options.find("string") != std::string::npos)
      new_flags = rpc::object_storage::flag_string_type;
    else if (options.find("simple") != std::string::npos)
      new_flags = rpc::object_storage::flag_function_type;
    else 
      throw torrent::input_error("No support for 'list' variable type.");

    if (options.find("static") != std::string::npos)
      new_flags |= rpc::object_storage::flag_static;
    if (options.find("private") != std::string::npos)
      new_flags |= rpc::object_storage::flag_private;
    if (options.find("const") != std::string::npos)
      new_flags |= rpc::object_storage::flag_constant;

    return system_method_insert_object(new_args, new_flags);

  } else {
    // THROW.
  }

  return torrent::Object();
}

// method.erase <> {name}
//
// Erase a modifiable method called 'name. Trying to remove methods
// that aren't modifiable, e.g. defined by rtorrent or set to
// read-only by the user, will result in an error.
torrent::Object
system_method_erase(const torrent::Object::string_type& args) {
  rpc::CommandMap::iterator itr = rpc::commands.find(args.c_str());

  if (itr == rpc::commands.end())
    return torrent::Object();

  if (!rpc::commands.is_modifiable(itr))
    throw torrent::input_error("Command not modifiable.");

  rpc::commands.erase(itr);

  return torrent::Object();
}

torrent::Object
system_method_redirect(const torrent::Object::list_type& args) {
  if (args.size() != 2)
    throw torrent::input_error("Invalid argument count.");

  std::string new_key  = torrent::object_create_string(args.front());
  std::string dest_key = torrent::object_create_string(args.back());

  rpc::commands.create_redirect(create_new_key<0>(new_key, ""), create_new_key<0>(dest_key, ""),
                                rpc::CommandMap::flag_public_xmlrpc | rpc::CommandMap::flag_delete_key | rpc::CommandMap::flag_modifiable);

  return torrent::Object();
}

torrent::Object
system_method_set_function(const torrent::Object::list_type& args) {
  if (args.empty())
    throw torrent::input_error("Invalid argument count.");

  rpc::object_storage::local_iterator itr =
    control->object_storage()->find_local(torrent::raw_string::from_string(args.front().as_string()));

  if (itr == control->object_storage()->end(0) || itr->second.flags & rpc::object_storage::flag_constant)
    throw torrent::input_error("Command is not modifiable.");    

  return control->object_storage()->set_str_function(args.front().as_string(),
                                                     system_method_generate_command(++args.begin(), args.end()));
}

torrent::Object
system_method_has_key(const torrent::Object::list_type& args) {
  if (args.empty() || ++args.begin() == args.end())
    throw torrent::input_error("Invalid argument count.");

  torrent::Object::list_const_iterator itrArgs = args.begin();
  const std::string& key = (itrArgs++)->as_string();
  const std::string& cmd_key = (itrArgs++)->as_string();
  
  return control->object_storage()->has_str_multi_key(key, cmd_key);
}

torrent::Object
system_method_set_key(const torrent::Object::list_type& args) {
  if (args.empty() || ++args.begin() == args.end())
    throw torrent::input_error("Invalid argument count.");

  torrent::Object::list_const_iterator itrArgs = args.begin();
  const std::string& key = (itrArgs++)->as_string();
  const std::string& cmd_key = (itrArgs++)->as_string();
  
  if (itrArgs != args.end())
    control->object_storage()->set_str_multi_key(key, cmd_key, system_method_generate_command(itrArgs, args.end()));
  else
    control->object_storage()->erase_str_multi_key(key, cmd_key);

  return torrent::Object();
}

torrent::Object
system_method_list_keys(const torrent::Object::string_type& args) {
  const torrent::Object::map_type& multi_cmd = control->object_storage()->get_str(args).as_map();
  
  torrent::Object rawResult = torrent::Object::create_list();
  torrent::Object::list_type& result = rawResult.as_list();

  for (torrent::Object::map_const_iterator itr = multi_cmd.begin(), last = multi_cmd.end(); itr != last; itr++)
    result.push_back(itr->first);

  return rawResult;
}

#define CMD2_METHOD_INSERT(key, flags) \
  CMD2_ANY_LIST(key, std::tr1::bind(&system_method_insert_object, std::tr1::placeholders::_2, flags));

void
initialize_command_dynamic() {
  CMD2_VAR_BOOL    ("method.use_deprecated", true);
  CMD2_VAR_VALUE   ("method.use_intermediate", 1);

  CMD2_ANY_LIST    ("method.insert",    std::tr1::bind(&system_method_insert, std::tr1::placeholders::_2));

  CMD2_ANY_LIST    ("method.insert.value", std::tr1::bind(&system_method_insert_object, std::tr1::placeholders::_2,
                                                          rpc::object_storage::flag_value_type));

  CMD2_METHOD_INSERT("method.insert.simple",     rpc::object_storage::flag_function_type);
  CMD2_METHOD_INSERT("method.insert.c_simple",   rpc::object_storage::flag_constant | rpc::object_storage::flag_function_type);
  CMD2_METHOD_INSERT("method.insert.s_c_simple", rpc::object_storage::flag_static |
                     rpc::object_storage::flag_constant |rpc::object_storage::flag_function_type);

  CMD2_ANY_STRING  ("method.erase",     std::tr1::bind(&system_method_erase, std::tr1::placeholders::_2));
  CMD2_ANY_LIST    ("method.redirect",  std::tr1::bind(&system_method_redirect, std::tr1::placeholders::_2));
  CMD2_ANY_STRING  ("method.get",       std::tr1::bind(&rpc::object_storage::get_str, control->object_storage(),
                                                       std::tr1::placeholders::_2));
  CMD2_ANY_LIST    ("method.set",       std::tr1::bind(&system_method_set_function, std::tr1::placeholders::_2));

  CMD2_ANY_LIST    ("method.has_key",   std::tr1::bind(&system_method_has_key, std::tr1::placeholders::_2));
  CMD2_ANY_LIST    ("method.set_key",   std::tr1::bind(&system_method_set_key, std::tr1::placeholders::_2));
  CMD2_ANY_STRING  ("method.list_keys", std::tr1::bind(&system_method_list_keys, std::tr1::placeholders::_2));
}
