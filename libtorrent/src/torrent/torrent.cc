#include "config.h"

#include <iostream>
#include <algo/algo.h>
#include <sigc++/bind.h>

#include "net/listen.h"
#include "net/handshake_manager.h"
#include "parse/parse.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "download/download_manager.h"

#include "torrent.h"
#include "exceptions.h"
#include "download/download_main.h"
#include "throttle_control.h"
#include "timer.h"
#include "general.h"

using namespace algo;

namespace torrent {

int64_t Timer::m_cache;
std::list<std::string> caughtExceptions;

Listen* listen = NULL;
HandshakeManager handshakes;
DownloadManager downloadManager;

HashQueue hashQueue;
HashTorrent hashTorrent(&hashQueue);

struct add_socket {
  add_socket(fd_set* s) : fd(0), fds(s) {}

  void operator () (SocketBase* s) {
    if (s->fd() < 0)
      throw internal_error("Tried to poll a negative file descriptor");

    if (fd < s->fd())
      fd = s->fd();

    FD_SET(s->fd(), fds);
  }

  int fd;
  fd_set* fds;
};

struct check_socket_isset {
  check_socket_isset(fd_set* s) : fds(s) {}

  bool operator () (SocketBase* socket) {
    if (socket == NULL)
      throw internal_error("Polled socket is NULL");

    return FD_ISSET(socket->fd(), fds);
  }

  fd_set* fds;
};

// Find some better way of doing this.
std::string
download_id(const std::string& hash) {
  DownloadMain* d = downloadManager.find(hash);

  return d && d->is_active() && d->is_checked() ? d->get_me().get_id() : "";
}

void
receive_connection(int fd, const std::string& hash, const PeerInfo& peer) {
  DownloadMain* d = downloadManager.find(hash);
  
  if (!d || !d->is_active() || !d->is_checked() || !d->get_net().add_connection(fd, peer))
    SocketBase::close_socket(fd);
}

// Make sure srandom is properly initialized by the client.
void
initialize() {
  if (listen == NULL) {
    listen = new Listen;

    listen->slot_incoming(sigc::mem_fun(handshakes, &HandshakeManager::add_incoming));
  }

  srandom(Timer::current().usec());

  ThrottleControl::global().insert_service(Timer::current(), 0);

  handshakes.slot_connected(sigc::ptr_fun3(&receive_connection));
  handshakes.slot_download_id(sigc::ptr_fun1(download_id));
}

// Clean up and close stuff. Stopping all torrents and waiting for
// them to finish is not required, but recommended.
void
cleanup() {
  ThrottleControl::global().remove_service();

  handshakes.clear();
  downloadManager.clear();
}

bool
listen_open(uint16_t begin, uint16_t end) {
  if (listen == NULL)
    throw client_error("listen_open called but the library has not been initialized");

  if (!listen->open(begin, end))
    return false;

  std::for_each(downloadManager.get_list().begin(), downloadManager.get_list().end(),
		call_member(&DownloadMain::set_port, value(listen->get_port())));

  return true;
}

void
listen_close() {
  listen->close();
}

// Set the file descriptors we want to pool for R/W/E events. All
// fd_set's must be valid pointers. Returns the highest fd.
void
mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int* maxFd) {
  *maxFd = 0;

  if (readSet == NULL || writeSet == NULL || exceptSet == NULL || maxFd == NULL)
    throw client_error("torrent::mark(...) received a NULL pointer");

  *maxFd = std::max(*maxFd, std::for_each(SocketBase::read_sockets().begin(),
                                          SocketBase::read_sockets().end(),
                                          add_socket(readSet)).fd);

  *maxFd = std::max(*maxFd, std::for_each(SocketBase::write_sockets().begin(),
                                          SocketBase::write_sockets().end(),
                                          add_socket(writeSet)).fd);
  
  *maxFd = std::max(*maxFd, std::for_each(SocketBase::except_sockets().begin(),
                                          SocketBase::except_sockets().end(),
                                          add_socket(exceptSet)).fd);
}

// Do work on the polled file descriptors.
void
work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int maxFd) {
  // Update the cached time.
  Timer::update();

  if (readSet == NULL || writeSet == NULL || exceptSet == NULL)
    throw client_error("Torrent::work(...) received a NULL pointer");

  // Make sure we don't do read/write on fd's that are in except. This should
  // not be a problem as any except call should remove it from the m_*Set's.

  caughtExceptions.clear();

  // If except is called, make sure you correctly remove us from the poll.
  for_each<true>(SocketBase::except_sockets().begin(), SocketBase::except_sockets().end(),
		 if_on(check_socket_isset(exceptSet),
		       call_member(&SocketBase::except)));

  for_each<true>(SocketBase::read_sockets().begin(), SocketBase::read_sockets().end(),
		 if_on(check_socket_isset(readSet),
		       call_member(&SocketBase::read)));

  for_each<true>(SocketBase::write_sockets().begin(), SocketBase::write_sockets().end(),
		 if_on(check_socket_isset(writeSet),
		       call_member(&SocketBase::write)));

  // TODO: Consider moving before the r/w/e. libsic++ should remove the use of
  // zero timeout stuff to send signal. Better yet, use on both sides, it's cheap.
  Service::perform_service();
}

Download
download_create(std::istream& s) {
  // TODO: Should we clear failed bits?
  s.clear();

  Bencode b;

  s >> b;

  if (s.fail())
    // Make it configurable whetever we throw or return .end()?
    throw local_error("Could not parse Bencoded torrent");
  
  DownloadMain* d = new DownloadMain();

  try {

    d->get_me().set_id(generateId());
    d->set_port(listen->get_port());

    parse_main(b, *d);
    parse_info(b["info"], d->get_state().get_content());

    // Hash must be calculated before these are connected.
    d->get_net().slot_has_handshake(sigc::mem_fun(handshakes, &HandshakeManager::has_peer));
    d->get_net().slot_start_handshake(sigc::bind(sigc::mem_fun(handshakes, &HandshakeManager::add_outgoing),
					     d->get_hash(), d->get_me().get_id()));
    d->get_net().slot_count_handshakes(sigc::bind(sigc::mem_fun(handshakes, &HandshakeManager::get_size_hash),
					      d->get_hash()));

    d->get_state().slot_hash_check_add(sigc::bind(sigc::mem_fun(hashQueue, &HashQueue::add),
					      sigc::mem_fun(d->get_state(), &DownloadState::receive_hash_done),
					      d->get_hash()));

    d->setup_net();
    d->setup_delegator();
    d->setup_tracker();

    parse_tracker(b, d->get_tracker());

  } catch (...) {
    delete d;
    throw;
  }

  downloadManager.add(d);

  return Download((DownloadWrapper*)d);
}

// Add all downloads to dlist. Make sure it's cleared.
void
download_list(DList& dlist) {
  for (DownloadManager::DownloadList::const_iterator itr = downloadManager.get_list().begin();
       itr != downloadManager.get_list().end(); ++itr)
    dlist.push_back(Download((DownloadWrapper*)*itr));
}

// Make sure you check that it's valid.
Download
download_find(const std::string& id) {
  return (DownloadWrapper*)downloadManager.find(id);
}

void
download_remove(const std::string& id) {
  downloadManager.remove(id);
}

// Throws a local_error of some sort.
int64_t
get(GValue t) {
  switch (t) {
  case LISTEN_PORT:
    return listen->get_port();

  case HANDSHAKES_TOTAL:
    return handshakes.get_size();

  case SHUTDOWN_DONE:
    return std::find_if(downloadManager.get_list().begin(), downloadManager.get_list().end(),
			bool_not(call_member(&DownloadMain::is_stopped)))
      == downloadManager.get_list().end();

  case FILES_CHECK_WAIT:
    return Settings::filesCheckWait;

  case DEFAULT_PEERS_MIN:
    return DownloadSettings::global().minPeers;

  case DEFAULT_PEERS_MAX:
    return DownloadSettings::global().maxPeers;

  case DEFAULT_UPLOADS_MAX:
    return DownloadSettings::global().maxUploads;

  case DEFAULT_CHOKE_CYCLE:
    return DownloadSettings::global().chokeCycle;

  case HAS_EXCEPTION:
    return !caughtExceptions.empty();

  case TIME_CURRENT:
    return Timer::current().usec();

  case TIME_SELECT:
    return Service::next_service().usec();

  case THROTTLE_ROOT_CONST_RATE:
    return std::max(ThrottleControl::global().settings(ThrottleControl::SETTINGS_ROOT)->constantRate, 0);

  default:
    throw internal_error("get(GValue) received invalid type");
  }
}

std::string
get(GString t) {
  std::string s;

  switch (t) {
  case LIBRARY_NAME:
    return std::string("LibTorrent") + " " VERSION;

  case POP_EXCEPTION:
    if (caughtExceptions.empty())
      throw internal_error("get(GString) tried to pop an exception from an empty list");

    s = caughtExceptions.front();
    caughtExceptions.pop_front();

    return s;

  default:
    throw internal_error("get(GString) received invalid type");
  }
}

void
set(GValue t, int64_t v) {
  switch (t) {
  case FILES_CHECK_WAIT:
    if (v >= 0 && v < 60 * 1000000)
      Settings::filesCheckWait = v;
    break;

  case DEFAULT_PEERS_MIN:
    if (v > 0 && v < 1000)
      DownloadSettings::global().minPeers = v;
    break;

  case DEFAULT_PEERS_MAX:
    if (v > 0 && v < 1000)
      DownloadSettings::global().maxPeers = v;
    break;

  case DEFAULT_UPLOADS_MAX:
    if (v >= 0 && v < 1000)
      DownloadSettings::global().maxUploads = v;
    break;

  case DEFAULT_CHOKE_CYCLE:
    if (v > 10 * 1000000 && v < 3600 * 1000000)
      DownloadSettings::global().chokeCycle = v;
    break;

  case THROTTLE_ROOT_CONST_RATE:
    ThrottleControl::global().settings(ThrottleControl::SETTINGS_ROOT)->constantRate =
      v > 0 ? v : Throttle::UNLIMITED;
    break;

  default:
    throw internal_error("set(GValue, int) received invalid type");
  }
}

void
set(GString t, const std::string& s) {
}

}