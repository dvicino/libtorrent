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

#include <functional>
#include <cstdio>
#include <rak/file_stat.h>
#include <rak/path.h>
#include <rak/string_manip.h>
#include <sigc++/adaptors/bind.h>
#include <torrent/rate.h>
#include <torrent/hash_string.h>

#include "core/download.h"
#include "core/download_list.h"
#include "core/manager.h"
#include "core/view_manager.h"
#include "rpc/command_scheduler.h"
#include "rpc/command_variable.h"
#include "rpc/parse.h"
#include "rpc/parse_commands.h"

#include "globals.h"
#include "control.h"
#include "command_helpers.h"

#include "thread_worker.h"

torrent::Object
apply_on_ratio(const torrent::Object& rawArgs) {
  const std::string& groupName = rawArgs.as_string();

  char buffer[32 + groupName.size()];
  sprintf(buffer, "group.%s.view", groupName.c_str());

  core::ViewManager::iterator viewItr = control->view_manager()->find(rpc::commands.call(buffer, rpc::make_target()).as_string());

  if (viewItr == control->view_manager()->end())
    throw torrent::input_error("Could not find view.");

  char* bufferStart = buffer + sprintf(buffer, "group.%s.ratio.", groupName.c_str());

  // first argument:  minimum ratio to reach
  // second argument: minimum upload amount to reach [optional]
  // third argument:  maximum ratio to reach [optional]
  std::strcpy(bufferStart, "min");
  int64_t minRatio  = rpc::commands.call(buffer, rpc::make_target()).as_value();
  std::strcpy(bufferStart, "max");
  int64_t maxRatio  = rpc::commands.call(buffer, rpc::make_target()).as_value();
  std::strcpy(bufferStart, "upload");
  int64_t minUpload = rpc::commands.call(buffer, rpc::make_target()).as_value();

  std::vector<core::Download*> downloads;

  for  (core::View::iterator itr = (*viewItr)->begin_visible(), last = (*viewItr)->end_visible(); itr != last; itr++) {
    if (!(*itr)->is_seeding() || rpc::call_command_value("d.ignore_commands", rpc::make_target(*itr)) != 0)
      continue;

    //    rpc::parse_command_single(rpc::make_target(*itr), "print={Checked ratio of download.}");

    int64_t totalDone   = (*itr)->download()->bytes_done();
    int64_t totalUpload = (*itr)->info()->up_rate()->total();

    if (!(totalUpload >= minUpload && totalUpload * 100 >= totalDone * minRatio) &&
        !(maxRatio > 0 && totalUpload * 100 > totalDone * maxRatio))
      continue;

    downloads.push_back(*itr);
  }

  std::strcpy(bufferStart, "command");

  for (std::vector<core::Download*>::iterator itr = downloads.begin(), last = downloads.end(); itr != last; itr++) {
    //    rpc::commands.call("print", rpc::make_target(*itr), "Calling ratio command.");
    rpc::commands.call_catch(buffer, rpc::make_target(*itr), torrent::Object(), "Ratio reached, but command failed: ");
  }

  return torrent::Object();
}

torrent::Object
apply_start_tied() {
  for (core::DownloadList::iterator itr = control->core()->download_list()->begin(); itr != control->core()->download_list()->end(); ++itr) {
    if (rpc::call_command_value("d.state", rpc::make_target(*itr)) == 1)
      continue;

    rak::file_stat fs;
    const std::string& tiedToFile = rpc::call_command_string("d.tied_to_file", rpc::make_target(*itr));

    if (!tiedToFile.empty() && fs.update(rak::path_expand(tiedToFile)))
      rpc::parse_command_single(rpc::make_target(*itr), "d.try_start=");
  }

  return torrent::Object();
}

torrent::Object
apply_stop_untied() {
  for (core::DownloadList::iterator itr = control->core()->download_list()->begin(); itr != control->core()->download_list()->end(); ++itr) {
    if (rpc::call_command_value("d.state", rpc::make_target(*itr)) == 0)
      continue;

    rak::file_stat fs;
    const std::string& tiedToFile = rpc::call_command_string("d.tied_to_file", rpc::make_target(*itr));

    if (!tiedToFile.empty() && !fs.update(rak::path_expand(tiedToFile)))
      rpc::parse_command_single(rpc::make_target(*itr), "d.try_stop=");
  }

  return torrent::Object();
}

torrent::Object
apply_close_untied() {
  for (core::DownloadList::iterator itr = control->core()->download_list()->begin(); itr != control->core()->download_list()->end(); ++itr) {
    rak::file_stat fs;
    const std::string& tiedToFile = rpc::call_command_string("d.tied_to_file", rpc::make_target(*itr));

    if (rpc::call_command_value("d.ignore_commands", rpc::make_target(*itr)) == 0 && !tiedToFile.empty() && !fs.update(rak::path_expand(tiedToFile)))
      rpc::parse_command_single(rpc::make_target(*itr), "d.try_close=");
  }

  return torrent::Object();
}

torrent::Object
apply_remove_untied() {
  for (core::DownloadList::iterator itr = control->core()->download_list()->begin(); itr != control->core()->download_list()->end(); ) {
    rak::file_stat fs;
    const std::string& tiedToFile = rpc::call_command_string("d.tied_to_file", rpc::make_target(*itr));

    if (!tiedToFile.empty() && !fs.update(rak::path_expand(tiedToFile))) {
      // Need to clear tied_to_file so it doesn't try to delete it.
      rpc::call_command("d.tied_to_file.set", std::string(), rpc::make_target(*itr));

      itr = control->core()->download_list()->erase(itr);

    } else {
      ++itr;
    }
  }

  return torrent::Object();
}

torrent::Object
apply_schedule(const torrent::Object::list_type& args) {
  if (args.size() != 4)
    throw torrent::input_error("Wrong number of arguments.");

  torrent::Object::list_const_iterator itr = args.begin();

  const std::string& arg1 = (itr++)->as_string();
  const std::string& arg2 = (itr++)->as_string();
  const std::string& arg3 = (itr++)->as_string();
  const std::string& arg4 = (itr++)->as_string();

  control->command_scheduler()->parse(arg1, arg2, arg3, arg4);

  return torrent::Object();
}

torrent::Object
apply_load(const torrent::Object::list_type& args, int flags) {
  torrent::Object::list_const_iterator argsItr = args.begin();

  if (argsItr == args.end())
    throw torrent::input_error("Too few arguments.");

  const std::string& filename = argsItr->as_string();
  core::Manager::command_list_type commands;

  while (++argsItr != args.end())
    commands.push_back(argsItr->as_string());

  control->core()->try_create_download_expand(filename, flags, commands);

  return torrent::Object();
}

void apply_import(const std::string& path)     { if (!rpc::parse_command_file(path)) throw torrent::input_error("Could not open option file: " + path); }
void apply_try_import(const std::string& path) { if (!rpc::parse_command_file(path)) control->core()->push_log_std("Could not read resource file: " + path); }

torrent::Object
apply_close_low_diskspace(int64_t arg) {
  core::DownloadList* downloadList = control->core()->download_list();

  bool closed = false;
  core::Manager::DListItr itr = downloadList->begin();

  while ((itr = std::find_if(itr, downloadList->end(), std::mem_fun(&core::Download::is_downloading)))
         != downloadList->end()) {
    if ((*itr)->file_list()->free_diskspace() < (uint64_t)arg) {
      downloadList->close(*itr);

      (*itr)->set_hash_failed(true);
      (*itr)->set_message(std::string("Low diskspace."));

      closed = true;
    }

    ++itr;
  }

  if (closed)
    control->core()->push_log("Closed torrents due to low diskspace.");    

  return torrent::Object();
}

torrent::Object
apply_download_list(const torrent::Object::list_type& args) {
  torrent::Object::list_const_iterator argsItr = args.begin();

  core::ViewManager* viewManager = control->view_manager();
  core::ViewManager::iterator viewItr;

  if (argsItr != args.end() && !argsItr->as_string().empty())
    viewItr = viewManager->find((argsItr++)->as_string());
  else
    viewItr = viewManager->find("default");

  if (viewItr == viewManager->end())
    throw torrent::input_error("Could not find view.");

  torrent::Object result = torrent::Object::create_list();
  torrent::Object::list_type& resultList = result.as_list();

  for (core::View::const_iterator itr = (*viewItr)->begin_visible(), last = (*viewItr)->end_visible(); itr != last; itr++) {
    const torrent::HashString* hashString = &(*itr)->info()->hash();

    resultList.push_back(rak::transform_hex(hashString->begin(), hashString->end()));
  }

  return result;
}

torrent::Object
d_multicall(const torrent::Object::list_type& args) {
  if (args.empty())
    throw torrent::input_error("Too few arguments.");

  core::ViewManager* viewManager = control->view_manager();
  core::ViewManager::iterator viewItr;

  if (!args.front().as_string().empty())
    viewItr = viewManager->find(args.front().as_string());
  else
    viewItr = viewManager->find("default");

  if (viewItr == viewManager->end())
    throw torrent::input_error("Could not find view.");

  // Add some pre-parsing of the commands, so we don't spend time
  // parsing and searching command map for every single call.
  torrent::Object             resultRaw = torrent::Object::create_list();
  torrent::Object::list_type& result = resultRaw.as_list();

  for (core::View::const_iterator vItr = (*viewItr)->begin_visible(), vLast = (*viewItr)->end_visible(); vItr != vLast; vItr++) {
    torrent::Object::list_type& row = result.insert(result.end(), torrent::Object::create_list())->as_list();

    for (torrent::Object::list_const_iterator cItr = ++args.begin(), cLast = args.end(); cItr != args.end(); cItr++) {
      const std::string& cmd = cItr->as_string();
      row.push_back(rpc::parse_command(rpc::make_target(*vItr), cmd.c_str(), cmd.c_str() + cmd.size()).first);
    }
  }

  return resultRaw;
}

torrent::Object
test_thread_locking() {
  worker_thread->queue_item(&ThreadWorker::start_log_counter);

  return torrent::Object();
}

void
initialize_command_events() {
  CMD2_ANY("test.thread_locking", std::tr1::bind(&test_thread_locking));

  CMD2_VAR_BOOL    ("check_hash", true); // Rename

  CMD2_VAR_BOOL    ("system.session.use_lock", true);
  CMD2_VAR_BOOL    ("system.session.on_completion", true);

  CMD2_ANY_STRING  ("on_ratio",        std::tr1::bind(&apply_on_ratio, std::tr1::placeholders::_2));

  CMD2_ANY         ("start_tied",      std::tr1::bind(&apply_start_tied));
  CMD2_ANY         ("stop_untied",     std::tr1::bind(&apply_stop_untied));
  CMD2_ANY         ("close_untied",    std::tr1::bind(&apply_close_untied));
  CMD2_ANY         ("remove_untied",   std::tr1::bind(&apply_remove_untied));

  CMD2_ANY_LIST    ("schedule",        std::tr1::bind(&apply_schedule, std::tr1::placeholders::_2));
  CMD2_ANY_STRING_V("schedule_remove", std::tr1::bind(&rpc::CommandScheduler::erase_str, control->command_scheduler(), std::tr1::placeholders::_2));

  CMD2_ANY_STRING_V("import",          std::tr1::bind(&apply_import, std::tr1::placeholders::_2));
  CMD2_ANY_STRING_V("try_import",      std::tr1::bind(&apply_try_import, std::tr1::placeholders::_2));

  CMD2_ANY_LIST    ("load",            std::tr1::bind(&apply_load, std::tr1::placeholders::_2, core::Manager::create_quiet | core::Manager::create_tied));
  CMD2_ANY_LIST    ("load_verbose",    std::tr1::bind(&apply_load, std::tr1::placeholders::_2, core::Manager::create_tied));
  CMD2_ANY_LIST    ("load_start",      std::tr1::bind(&apply_load, std::tr1::placeholders::_2,
                                                      core::Manager::create_quiet | core::Manager::create_tied | core::Manager::create_start));
  CMD2_ANY_LIST    ("load_start_verbose", std::tr1::bind(&apply_load, std::tr1::placeholders::_2, core::Manager::create_tied  | core::Manager::create_start));
  CMD2_ANY_LIST    ("load_raw",           std::tr1::bind(&apply_load, std::tr1::placeholders::_2, core::Manager::create_quiet | core::Manager::create_raw_data));
  CMD2_ANY_LIST    ("load_raw_verbose",   std::tr1::bind(&apply_load, std::tr1::placeholders::_2, core::Manager::create_raw_data));
  CMD2_ANY_LIST    ("load_raw_start",     std::tr1::bind(&apply_load, std::tr1::placeholders::_2,
                                                         core::Manager::create_quiet | core::Manager::create_start | core::Manager::create_raw_data));

  CMD2_ANY_VALUE   ("close_low_diskspace", std::tr1::bind(&apply_close_low_diskspace, std::tr1::placeholders::_2));

  CMD2_ANY_LIST    ("download_list",       std::tr1::bind(&apply_download_list, std::tr1::placeholders::_2));
  CMD2_ANY_LIST    ("d.multicall",         std::tr1::bind(&d_multicall, std::tr1::placeholders::_2));
}
