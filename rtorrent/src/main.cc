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

#include <cstdlib>
#include <iostream>
#include <string>
#include <sigc++/adaptors/bind.h>
#include <torrent/http.h>
#include <torrent/torrent.h>
#include <torrent/exceptions.h>
#include <rak/functional.h>

#ifdef USE_EXECINFO
#include <execinfo.h>
#endif

#include "core/dht_manager.h"
#include "core/download.h"
#include "core/download_factory.h"
#include "core/download_store.h"
#include "core/manager.h"
#include "display/canvas.h"
#include "display/window.h"
#include "display/manager.h"
#include "input/bindings.h"

#include "rpc/command_scheduler.h"
#include "rpc/command_scheduler_item.h"
#include "rpc/parse_commands.h"
#include "utils/directory.h"

#include "control.h"
#include "globals.h"
#include "signal_handler.h"
#include "option_parser.h"

#include "thread_main.h"
#include "thread_worker.h"

void do_panic(int signum);
void print_help();
void initialize_commands();

void do_nothing() {}

int
parse_options(Control* c, int argc, char** argv) {
  try {
    OptionParser optionParser;

    // Converted.
    optionParser.insert_flag('h', sigc::ptr_fun(&print_help));
    optionParser.insert_flag('n', OptionParser::Slot());
    optionParser.insert_flag('D', OptionParser::Slot());

    optionParser.insert_option('b', sigc::bind<0>(sigc::ptr_fun(&rpc::call_command_set_string), "network.bind_address.set"));
    optionParser.insert_option('d', sigc::bind<0>(sigc::ptr_fun(&rpc::call_command_set_string), "directory"));
    optionParser.insert_option('i', sigc::bind<0>(sigc::ptr_fun(&rpc::call_command_set_string), "ip"));
    optionParser.insert_option('p', sigc::bind<0>(sigc::ptr_fun(&rpc::call_command_set_string), "port_range"));
    optionParser.insert_option('s', sigc::bind<0>(sigc::ptr_fun(&rpc::call_command_set_string), "session"));

    optionParser.insert_option('O', sigc::ptr_fun(&rpc::parse_command_single_std));
    optionParser.insert_option_list('o', sigc::ptr_fun(&rpc::call_command_set_std_string));

    return optionParser.process(argc, argv);

  } catch (torrent::input_error& e) {
    throw torrent::input_error("Failed to parse command line option: " + std::string(e.what()));
  }
}

void
load_session_torrents(Control* c) {
  utils::Directory entries = c->core()->download_store()->get_formated_entries();

  for (utils::Directory::const_iterator first = entries.begin(), last = entries.end(); first != last; ++first) {
    // We don't really support session torrents that are links. These
    // would be overwritten anyway on exit, and thus not really be
    // useful.
    if (!first->is_file())
      continue;

    core::DownloadFactory* f = new core::DownloadFactory(c->core());

    // Replace with session torrent flag.
    f->set_session(true);
    f->slot_finished(sigc::bind(sigc::ptr_fun(&rak::call_delete_func<core::DownloadFactory>), f));
    f->load(entries.path() + first->d_name);
    f->commit();
  }
}

void
load_arg_torrents(Control* c, char** first, char** last) {
  //std::for_each(begin, end, std::bind1st(std::mem_fun(&core::Manager::insert), &c->get_core()));
  for (; first != last; ++first) {
    core::DownloadFactory* f = new core::DownloadFactory(c->core());

    // Replace with session torrent flag.
    f->set_start(true);
    f->slot_finished(sigc::bind(sigc::ptr_fun(&rak::call_delete_func<core::DownloadFactory>), f));
    f->load(*first);
    f->commit();
  }
}

static inline rak::timer
client_next_timeout(Control* c) {
  if (taskScheduler.empty())
    return c->is_shutdown_started() ? rak::timer::from_milliseconds(100) : rak::timer::from_seconds(60);
  else if (taskScheduler.top()->time() <= cachedTime)
    return 0;
  else
    return taskScheduler.top()->time() - cachedTime;
}

int
main(int argc, char** argv) {
  try {

    // Temporary.
    setlocale(LC_ALL, "");

    cachedTime = rak::timer::current();

    control = new Control;
    
    main_thread = new ThreadMain();
    main_thread->init_thread();

    worker_thread = new ThreadWorker();
    worker_thread->init_thread();

    srandom(cachedTime.usec());
    srand48(cachedTime.usec());

    SignalHandler::set_ignore(SIGPIPE);
    SignalHandler::set_handler(SIGINT,   sigc::mem_fun(control, &Control::receive_normal_shutdown));
    SignalHandler::set_handler(SIGTERM,  sigc::mem_fun(control, &Control::receive_quick_shutdown));
    SignalHandler::set_handler(SIGWINCH, sigc::mem_fun(control->display(), &display::Manager::force_redraw));
    SignalHandler::set_handler(SIGSEGV,  sigc::bind(sigc::ptr_fun(&do_panic), SIGSEGV));
    SignalHandler::set_handler(SIGBUS,   sigc::bind(sigc::ptr_fun(&do_panic), SIGBUS));
    SignalHandler::set_handler(SIGFPE,   sigc::bind(sigc::ptr_fun(&do_panic), SIGFPE));

    // SIGUSR1 is used for interrupting polling, forcing that thread
    // to process new non-socket events.
    SignalHandler::set_handler(SIGUSR1,  sigc::ptr_fun(&do_nothing));

    torrent::initialize(main_thread->poll());

    // Initialize option handlers after libtorrent to ensure
    // torrent::ConnectionManager* are valid etc.
    initialize_commands();

    rpc::parse_command_multiple
      (rpc::make_target(),
//        "method.insert = test.value,value\n"
//        "method.insert = test.value2,value,6\n"

//        "method.insert = test.string,string,6\n"
//        "method.insert = test.bool,bool,true\n"

       "method.insert = test.method.simple,simple,\"print=simple_test_,$argument.0=\"\n"

       "method.insert = event.download.inserted,multi\n"
       "method.insert = event.download.inserted_new,multi\n"
       "method.insert = event.download.inserted_session,multi\n"
       "method.insert = event.download.erased,multi\n"
       "method.insert = event.download.opened,multi\n"
       "method.insert = event.download.closed,multi\n"
       "method.insert = event.download.resumed,multi\n"
       "method.insert = event.download.paused,multi\n"
       
       "method.insert = event.download.finished,multi\n"
       "method.insert = event.download.hash_done,multi\n"
       "method.insert = event.download.hash_failed,multi\n"
       "method.insert = event.download.hash_final_failed,multi\n"
       "method.insert = event.download.hash_removed,multi\n"
       "method.insert = event.download.hash_queued,multi\n"

       "method.set_key = event.download.inserted,         1_connect_logs, d.initialize_logs=\n"
       "method.set_key = event.download.inserted_new,     1_prepare, \"branch=d.get_state=,view.set_visible=started,view.set_visible=stopped ;d.save_full_session=\"\n"
       "method.set_key = event.download.inserted_session, 1_prepare, \"branch=d.get_state=,view.set_visible=started,view.set_visible=stopped\"\n"

       "method.set_key = event.download.erased, !_download_list, ui.unfocus_download=\n"
       "method.set_key = event.download.erased, ~_delete_tied, d.delete_tied=\n"

       "method.insert = ratio.enable, simple|const,group.seeding.ratio.enable=\n"
       "method.insert = ratio.disable,simple|const,group.seeding.ratio.disable=\n"
       "method.insert = ratio.min,    simple|const,group.seeding.ratio.min=\n"
       "method.insert = ratio.max,    simple|const,group.seeding.ratio.max=\n"
       "method.insert = ratio.upload, simple|const,group.seeding.ratio.upload=\n"
       "method.insert = ratio.min.set,   simple|const,group.seeding.ratio.min.set=$argument.0=\n"
       "method.insert = ratio.max.set,   simple|const,group.seeding.ratio.max.set=$argument.0=\n"
       "method.insert = ratio.upload.set,simple|const,group.seeding.ratio.upload.set=$argument.0=\n"

       "method.insert = group.insert_persistent_view,simple|const,"
       "view_add=$argument.0=,view.persistent=$argument.0=,\"group.insert=$argument.0=,$argument.0=\"\n"

       // Allow setting 'group.view' as constant, so that we can't
       // modify the value. And look into the possibility of making
       // 'const' use non-heap memory, as we know they can't be
       // erased.

       // TODO: Remember to ensure it doesn't get restarted by watch
       // dir, etc. Set ignore commands, or something.

       "group.insert = seeding,seeding\n"

       "system.session_name = \"$cat=$system.hostname=,:,$system.pid=\"\n"

       // Currently not doing any sorting on main.
       "view_add = main\n"
       "view_add = default\n"

       "view_add = name\n"
       "view_sort_new     = name,less=d.name=\n"
       "view_sort_current = name,less=d.name=\n"

       "view_add = active\n"
       "view_filter = active,false=\n"

       "view_add = started\n"
       "view_filter = started,false=\n"
       "view.event_added   = started,\"view.set_not_visible=stopped ;d.set_state=1 ;scheduler.simple.added=\"\n"
       "view.event_removed = started,\"view.set_visible=stopped ;scheduler.simple.removed=\"\n"

       "view_add = stopped\n"
       "view_filter = stopped,false=\n"
       "view.event_added   = stopped,\"view.set_not_visible=started ;d.set_state=0\"\n"
       "view.event_removed = stopped,view.set_visible=started\n"

       "view_add = complete\n"
       "view_filter = complete,d.get_complete=\n"
       "view_filter_on    = complete,event.download.hash_done,event.download.hash_failed,event.download.hash_final_failed,event.download.finished\n"
       "view_sort_new     = complete,less=d.get_state_changed=\n"
       "view_sort_current = complete,less=d.get_state_changed=\n"

       "view_add = incomplete\n"
       "view_filter = incomplete,not=$d.get_complete=\n"
       "view_filter_on    = incomplete,event.download.hash_done,event.download.hash_failed,"
       "event.download.hash_final_failed,event.download.finished\n"
       "view_sort_new     = incomplete,less=d.get_state_changed=\n"
       "view_sort_current = incomplete,less=d.get_state_changed=\n"

       // The hashing view does not include stopped torrents.
       "view_add = hashing\n"
       "view_filter = hashing,d.get_hashing=\n"
       "view_filter_on = hashing,event.download.hash_queued,event.download.hash_removed,"
       "event.download.hash_done,event.download.hash_failed,event.download.hash_final_failed\n"
//        "view_sort_new     = hashing,less=d.get_state_changed=\n"
//        "view_sort_current = hashing,less=d.get_state_changed=\n"

       "view_add = seeding\n"
       "view_filter = seeding,\"and=d.get_state=,d.get_complete=\"\n"
       "view_filter_on    = seeding,event.download.resumed,event.download.paused,event.download.finished\n"
       "view_sort_new     = seeding,less=d.get_state_changed=\n"
       "view_sort_current = seeding,less=d.get_state_changed=\n"

       "schedule = view_main,10,10,\"view_sort=main,20\"\n"
       "schedule = view_name,10,10,\"view_sort=name,20\"\n"

       "schedule = session_save,1200,1200,session_save=\n"
       "schedule = low_diskspace,5,60,close_low_diskspace=500M\n"
       "schedule = prune_file_status,3600,86400,system.file_status_cache.prune=\n"

       "encryption=allow_incoming,prefer_plaintext,enable_retry\n"
    );

    // Deprecated commands. Don't use these anymore.

    if (!OptionParser::has_flag('D', argc, argv)) {

    rpc::parse_command_multiple
      (rpc::make_target(),
       // Deprecated in 0.7.0:
       // 
       // List of cleaned up files:
       // - command_download.cc
       // * command_dynamic.cc
       // / command_events.cc
       // - command_file.cc
       // - command_helpers.cc
       // * command_local.cc
       // - command_network.cc
       // - command_object.cc
       // - command_peer.cc
       // - command_scheduler.cc
       // - command_tracker.cc
       // - command_ui.cc

       "method.insert = system.method.insert,redirect|const,method.insert\n"
       "method.insert = system.method.set,redirect|const,method.set\n"
       "method.insert = system.method.set_key,redirect|const,method.set_key\n"

       "method.insert = get_handshake_log,redirect|const,log.handshake\n"
       "method.insert = set_handshake_log,redirect|const,log.handshake.set\n"
       "method.insert = get_log.tracker,redirect|const,log.tracker\n"
       "method.insert = set_log.tracker,redirect|const,log.tracker.set\n"

       "method.insert = get_name,redirect|const,system.session_name\n"
       "method.insert = set_name,redirect|const,system.session_name.set\n"
       "method.insert = system.file_allocate,redirect|const,system.file.allocate\n"
       "method.insert = system.file_allocate.set,redirect|const,system.file.allocate.set\n"

       "method.insert = get_preload_type,redirect|const,pieces.preload.type\n"
       "method.insert = get_preload_min_size,redirect|const,pieces.preload.min_size\n"
       "method.insert = get_preload_required_rate,redirect|const,pieces.preload.min_rate\n"
       "method.insert = set_preload_type,redirect|const,pieces.preload.type.set\n"
       "method.insert = set_preload_min_size,redirect|const,pieces.preload.min_size.set\n"
       "method.insert = set_preload_required_rate,redirect|const,pieces.preload.min_rate.set\n"
       "method.insert = get_stats_preloaded,redirect|const,pieces.stats_preloaded\n"
       "method.insert = get_stats_not_preloaded,redirect|const,pieces.stats_not_preloaded\n"

       "method.insert = get_memory_usage,redirect|const,pieces.memory.current\n"
       "method.insert = get_max_memory_usage,redirect|const,pieces.memory.max\n"
       "method.insert = set_max_memory_usage,redirect|const,pieces.memory.max.set\n"

       "method.insert = get_send_buffer_size,redirect|const,network.send_buffer.size\n"
       "method.insert = set_send_buffer_size,redirect|const,network.send_buffer.size.set\n"
       "method.insert = get_receive_buffer_size,redirect|const,network.receive_buffer.size\n"
       "method.insert = set_receive_buffer_size,redirect|const,network.receive_buffer.size.set\n"

       "method.insert = bind,    redirect|const,network.bind_address.set\n"
       "method.insert = set_bind,redirect|const,network.bind_address.set\n"
       "method.insert = get_bind,redirect|const,network.bind_address\n"

       "method.insert = ip,    redirect|const,network.local_address.set\n"
       "method.insert = set_ip,redirect|const,network.local_address.set\n"
       "method.insert = get_ip,redirect|const,network.local_address\n"

       "method.insert = proxy_address,    redirect|const,network.proxy_address.set\n"
       "method.insert = set_proxy_address,redirect|const,network.proxy_address.set\n"
       "method.insert = get_proxy_address,redirect|const,network.proxy_address\n"

       "method.insert = scgi_port, redirect|const,network.scgi.open_port\n"
       "method.insert = scgi_local,redirect|const,network.scgi.open_local\n"

       "method.insert = scgi_dont_route,    redirect|const,network.scgi.dont_route.set\n"
       "method.insert = set_scgi_dont_route,redirect|const,network.scgi.dont_route.set\n"
       "method.insert = get_scgi_dont_route,redirect|const,network.scgi.dont_route\n"

       "method.insert = d.get_hash,redirect|const,d.hash\n"
       "method.insert = d.get_local_id,redirect|const,d.local_id\n"
       "method.insert = d.get_local_id_html,redirect|const,d.local_id_html\n"
       "method.insert = d.get_bitfield,redirect|const,d.bitfield\n"
       "method.insert = d.get_base_path,redirect|const,d.base_path\n"

       "method.insert = d.get_name,redirect|const,d.name\n"
       "method.insert = d.get_creation_date,redirect|const,d.creation_date\n"

       "method.insert = d.get_peer_exchange,redirect|const,d.peer_exchange\n"

       "method.insert = d.get_up_rate,redirect|const,d.up.rate\n"
       "method.insert = d.get_up_total,redirect|const,d.up.total\n"
       "method.insert = d.get_down_rate,redirect|const,d.down.rate\n"
       "method.insert = d.get_down_total,redirect|const,d.down.total\n"
       "method.insert = d.get_skip_rate,redirect|const,d.skip.rate\n"
       "method.insert = d.get_skip_total,redirect|const,d.skip.total\n"
    );

    }

    if (OptionParser::has_flag('n', argc, argv))
      control->core()->push_log("Ignoring ~/.rtorrent.rc.");
    else
      rpc::parse_command_single(rpc::make_target(), "try_import = ~/.rtorrent.rc");

    int firstArg = parse_options(control, argc, argv);

    control->initialize();

    // Load session torrents and perform scheduled tasks to ensure
    // session torrents are loaded before arg torrents.
    control->dht_manager()->load_dht_cache();
    load_session_torrents(control);
    rak::priority_queue_perform(&taskScheduler, cachedTime);

    load_arg_torrents(control, argv + firstArg, argv + argc);

    // Make sure we update the display before any scheduled tasks can
    // run, so that loading of torrents doesn't look like it hangs on
    // startup.
    control->display()->adjust_layout();
    control->display()->receive_update();

    worker_thread->start_thread();

    while (!control->is_shutdown_completed()) {
      if (control->is_shutdown_received())
        control->handle_shutdown();

      control->inc_tick();

      cachedTime = rak::timer::current();
      rak::priority_queue_perform(&taskScheduler, cachedTime);

      // Do shutdown check before poll, not after.
      main_thread->poll_manager()->poll(client_next_timeout(control));
    }

    control->core()->download_list()->session_save();
    control->cleanup();

  } catch (std::exception& e) {
    control->cleanup_exception();

    std::cout << "rtorrent: " << e.what() << std::endl;
    return -1;
  }

  delete control;
  delete worker_thread;
  delete main_thread;

  return 0;
}

void
do_panic(int signum) {
  // Use the default signal handler in the future to avoid infinit
  // loops.
  SignalHandler::set_default(signum);
  display::Canvas::cleanup();

  std::cout << "Caught " << SignalHandler::as_string(signum) << ", dumping stack:" << std::endl;
  
#ifdef USE_EXECINFO
  void* stackPtrs[20];

  // Print the stack and exit.
  int stackSize = backtrace(stackPtrs, 20);
  char** stackStrings = backtrace_symbols(stackPtrs, stackSize);

  for (int i = 0; i < stackSize; ++i)
    std::cout << i << ' ' << stackStrings[i] << std::endl;

#else
  std::cout << "Stack dump not enabled." << std::endl;
#endif
  
  if (signum == SIGBUS)
    std::cout << "A bus error probably means you ran out of diskspace." << std::endl;

  std::abort();
}

void
print_help() {
  std::cout << "Rakshasa's BitTorrent client version " VERSION "." << std::endl;
  std::cout << std::endl;
  std::cout << "All value pairs (f.ex rate and queue size) will be in the UP/DOWN" << std::endl;
  std::cout << "order. Use the up/down/left/right arrow keys to move between screens." << std::endl;
  std::cout << std::endl;
  std::cout << "Usage: rtorrent [OPTIONS]... [FILE]... [URL]..." << std::endl;
  std::cout << "  -h                Display this very helpful text" << std::endl;
  std::cout << "  -n                Don't try to load ~/.rtorrent.rc on startup" << std::endl;
  std::cout << "  -b <a.b.c.d>      Bind the listening socket to this IP" << std::endl;
  std::cout << "  -i <a.b.c.d>      Change the IP that is sent to the tracker" << std::endl;
  std::cout << "  -p <int>-<int>    Set port range for incoming connections" << std::endl;
  std::cout << "  -d <directory>    Save torrents to this directory by default" << std::endl;
  std::cout << "  -s <directory>    Set the session directory" << std::endl;
  std::cout << "  -o key=opt,...    Set options, see 'rtorrent.rc' file" << std::endl;
  std::cout << std::endl;
  std::cout << "Main view keys:" << std::endl;
  std::cout << "  backspace         Add a torrent url or path" << std::endl;
  std::cout << "  ^s                Start torrent" << std::endl;
  std::cout << "  ^d                Stop torrent or delete a stopped torrent" << std::endl;
  std::cout << "  ^r                Manually initiate hash checking" << std::endl;
  std::cout << "  ^q                Initiate shutdown or skip shutdown process" << std::endl;
  std::cout << "  a,s,d,z,x,c       Adjust upload throttle" << std::endl;
  std::cout << "  A,S,D,Z,X,C       Adjust download throttle" << std::endl;
  std::cout << "  I                 Toggle whether torrent ignores ratio settings" << std::endl;
  std::cout << "  right             View torrent" << std::endl;
  std::cout << std::endl;
  std::cout << "Download view keys:" << std::endl;
  std::cout << "  spacebar          Depends on the current view" << std::endl;
  std::cout << "  1,2               Adjust max uploads" << std::endl;
  std::cout << "  3,4,5,6           Adjust min/max connected peers" << std::endl;
  std::cout << "  t/T               Query tracker for more peers / Force query" << std::endl;
  std::cout << "  *                 Snub peer" << std::endl;
  std::cout << "  right             View files" << std::endl;
  std::cout << "  p                 View peer information" << std::endl;
  std::cout << "  o                 View trackers" << std::endl;
  std::cout << std::endl;

  std::cout << "Report bugs to <jaris@ifi.uio.no>." << std::endl;

  exit(0);
}
