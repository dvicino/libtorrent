// libTorrent - BitTorrent library
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

#include <cerrno>

#include <algorithm>
#include <unistd.h>
#include <rak/error_number.h>
#include <rak/functional.h>
#include <torrent/exceptions.h>
#include <torrent/event.h>

#include "poll_kqueue.h"

#ifdef USE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/select.h>
#include <sys/time.h>
#endif

namespace torrent {

#ifdef USE_KQUEUE

inline uint32_t
PollKQueue::event_mask(Event* e) {
  assert(e->file_descriptor() != -1);

  Table::value_type entry = m_table[e->file_descriptor()];
  return entry.second != e ? 0 : entry.first;
}

inline void
PollKQueue::set_event_mask(Event* e, uint32_t m) {
  assert(e->file_descriptor() != -1);

  m_table[e->file_descriptor()] = std::make_pair(m, e);
}

void
PollKQueue::flush_events() {
  timespec timeout = { 0, 0 };

  int nfds = kevent(m_fd, m_changes, m_changedEvents, m_events + m_waitingEvents, m_maxEvents - m_waitingEvents, &timeout);
  if (nfds == -1)
    throw internal_error("PollKQueue::flush_events() error: " + std::string(rak::error_number::current().c_str()));

  m_changedEvents = 0;
  m_waitingEvents += nfds;
}

void
PollKQueue::modify(Event* event, unsigned short op, short mask) {
  // Flush the changed filters to the kernel if the buffer if full.
  if (m_changedEvents == m_table.size())
    flush_events();

  if (m_changedEvents == m_maxEvents) {
    if (kevent(m_fd, m_changes, m_changedEvents, NULL, 0, NULL) == -1)
      throw internal_error("PollKQueue::modify() error: " + std::string(rak::error_number::current().c_str()));

    m_changedEvents = 0;
  }

  struct kevent* itr = m_changes + (m_changedEvents++);

  EV_SET(itr, event->file_descriptor(), mask, op, 0, 0, event);
}

PollKQueue*
PollKQueue::create(int maxOpenSockets) {
  int fd = kqueue();

  if (fd == -1)
    return NULL;

  return new PollKQueue(fd, 1024, maxOpenSockets);
}

PollKQueue::PollKQueue(int fd, int maxEvents, int maxOpenSockets) :
  m_fd(fd),
  m_maxEvents(maxEvents),
  m_waitingEvents(0),
  m_changedEvents(0),
  m_stdinEvent(NULL) {

  m_events = new struct kevent[m_maxEvents];
  m_changes = new struct kevent[maxOpenSockets];

  m_table.resize(maxOpenSockets);
}

PollKQueue::~PollKQueue() {
  m_table.clear();
  delete [] m_events;
  delete [] m_changes;

  ::close(m_fd);
}

int
PollKQueue::poll(int msec) {
#if KQUEUE_SOCKET_ONLY
  if (m_stdinEvent != NULL) {
    // Flush all changes to the kqueue poll before we start select
    // polling, so that they get included.
    if (m_changedEvents != 0)
      flush_events();

    if (poll_select(msec) == -1)
      return -1;

    // The timeout was already handled in select().
    msec = 0;
  }
#endif

  timespec timeout = { msec / 1000, (msec % 1000) * 1000000 };

  int nfds = kevent(m_fd, m_changes, m_changedEvents, m_events + m_waitingEvents, m_maxEvents - m_waitingEvents, &timeout);

  // Clear the changed events even on fail as we might have received a
  // signal or similar, and the changed events have already been
  // consumed.
  //
  // There's a chance a bad changed event could make kevent return -1,
  // but it won't as long as there is room enough in m_events.
  m_changedEvents = 0;

  if (nfds == -1)
    return -1;

  m_waitingEvents += nfds;

  return nfds;
}

#if KQUEUE_SOCKET_ONLY
int
PollKQueue::poll_select(int msec) {
  if (m_waitingEvents >= m_maxEvents)
    return 0;

  timeval selectTimeout = { msec / 1000, (msec % 1000) * 1000 };

  // If m_fd isn't the first FD opened by the client and has
  // a low number, using ::poll() here would perform better.
  //
  // This kinda assumes fd_set's internal type is int.
  int readBuffer[m_fd + 1];
  fd_set* readSet = (fd_set*)&readBuffer;

  std::memset(readBuffer, 0, m_fd + 1);
  FD_SET(0,    readSet);
  FD_SET(m_fd, readSet);

  int nfds = select(m_fd + 1, readSet, NULL, NULL, &selectTimeout);

  if (nfds == -1)
    return nfds;

  if (FD_ISSET(0, readSet)) {
    m_events[m_waitingEvents].filter = EVFILT_READ;
    m_events[m_waitingEvents].udata = m_stdinEvent;
    m_waitingEvents++;
  }

  return nfds;
}
#endif

void
PollKQueue::perform() {
  for (struct kevent *itr = m_events, *last = m_events + m_waitingEvents; itr != last; ++itr) {
    if ((itr->flags & EV_ERROR) && itr->udata != NULL) {
      if (event_mask((Event*)itr->udata) & flag_error)
        ((Event*)itr->udata)->event_error();
      continue;
    }

    // Also check current mask.

    if (itr->filter == EVFILT_READ && itr->udata != NULL && event_mask((Event*)itr->udata) & flag_read)
      ((Event*)itr->udata)->event_read();

    if (itr->filter == EVFILT_WRITE && itr->udata != NULL && event_mask((Event*)itr->udata) & flag_write)
      ((Event*)itr->udata)->event_write();
  }

  m_waitingEvents = 0;
}

uint32_t
PollKQueue::open_max() const {
  return m_table.size();
}

void
PollKQueue::open(Event* event) {
  if (event_mask(event) != 0)
    throw internal_error("PollKQueue::open(...) called but the file descriptor is active");
}

void
PollKQueue::close(Event* event) {
#if KQUEUE_SOCKET_ONLY
  if (event->file_descriptor() == 0) {
    m_stdinEvent = NULL;
    return;
  }
#endif

  if (event_mask(event) != 0)
    throw internal_error("PollKQueue::close(...) called but the file descriptor is active");

  for (struct kevent *itr = m_events, *last = m_events + m_waitingEvents; itr != last; ++itr)
    if (itr->udata == event)
      itr->udata = NULL;

  m_changedEvents = std::remove_if(m_changes, m_changes + m_changedEvents, rak::equal(event, rak::mem_ref(&kevent::udata))) - m_changes;
}

void
PollKQueue::closed(Event* event) {
#if KQUEUE_SOCKET_ONLY
  if (event->file_descriptor() == 0) {
    m_stdinEvent = NULL;
    return;
  }
#endif

  // Kernel removes closed FDs automatically, so just clear the mask
  // and remove it from pending calls.  Don't touch if the FD was
  // re-used before we received the close notification.
  if (m_table[event->file_descriptor()].second == event)
    set_event_mask(event, 0);

  for (struct kevent *itr = m_events, *last = m_events + m_waitingEvents; itr != last; ++itr) {
    if (itr->udata == event)
      itr->udata = NULL;
  }

  m_changedEvents = std::remove_if(m_changes, m_changes + m_changedEvents, rak::equal(event, rak::mem_ref(&kevent::udata))) - m_changes;
}

// Use custom defines for EPOLL* to make the below code compile with
// and with epoll.
bool
PollKQueue::in_read(Event* event) {
  return event_mask(event) & flag_read;
}

bool
PollKQueue::in_write(Event* event) {
  return event_mask(event) & flag_write;
}

bool
PollKQueue::in_error(Event* event) {
  return event_mask(event) & flag_error;
}

void
PollKQueue::insert_read(Event* event) {
  if (event_mask(event) & flag_read)
    return;

  set_event_mask(event, event_mask(event) | flag_read);

#if KQUEUE_SOCKET_ONLY
  if (event->file_descriptor() == 0) {
    m_stdinEvent = event;
    return;
  }
#endif

  modify(event, EV_ADD, EVFILT_READ);
}

void
PollKQueue::insert_write(Event* event) {
  if (event_mask(event) & flag_write)
    return;

  set_event_mask(event, event_mask(event) | flag_write);
  modify(event, EV_ADD, EVFILT_WRITE);
}

void
PollKQueue::insert_error(Event* event) {
}

void
PollKQueue::remove_read(Event* event) {
  if (!(event_mask(event) & flag_read))
    return;

  set_event_mask(event, event_mask(event) & ~flag_read);

#if KQUEUE_SOCKET_ONLY
  if (event->file_descriptor() == 0) {
    m_stdinEvent = NULL;
    return;
  }
#endif

  modify(event, EV_DELETE, EVFILT_READ);
}

void
PollKQueue::remove_write(Event* event) {
  if (!(event_mask(event) & flag_write))
    return;

  set_event_mask(event, event_mask(event) & ~flag_write);
  modify(event, EV_DELETE, EVFILT_WRITE);
}

void
PollKQueue::remove_error(Event* event) {
}

#else // USE_QUEUE

PollKQueue*
PollKQueue::create(__UNUSED int maxOpenSockets) {
  return NULL;
}

PollKQueue::~PollKQueue() {
}

int
PollKQueue::poll(__UNUSED int msec) {
  throw internal_error("An PollKQueue function was called, but it is disabled.");
}

void
PollKQueue::perform() {
  throw internal_error("An PollKQueue function was called, but it is disabled.");
}

uint32_t
PollKQueue::open_max() const {
  throw internal_error("An PollKQueue function was called, but it is disabled.");
}

void
PollKQueue::open(__UNUSED torrent::Event* event) {
}

void
PollKQueue::close(__UNUSED torrent::Event* event) {
}

void
PollKQueue::closed(__UNUSED torrent::Event* event) {
}

bool
PollKQueue::in_read(__UNUSED torrent::Event* event) {
  throw internal_error("An PollKQueue function was called, but it is disabled.");
}

bool
PollKQueue::in_write(__UNUSED torrent::Event* event) {
  throw internal_error("An PollKQueue function was called, but it is disabled.");
}

bool
PollKQueue::in_error(__UNUSED torrent::Event* event) {
  throw internal_error("An PollKQueue function was called, but it is disabled.");
}

void
PollKQueue::insert_read(__UNUSED torrent::Event* event) {
}

void
PollKQueue::insert_write(__UNUSED torrent::Event* event) {
}

void
PollKQueue::insert_error(__UNUSED torrent::Event* event) {
}

void
PollKQueue::remove_read(__UNUSED torrent::Event* event) {
}

void
PollKQueue::remove_write(__UNUSED torrent::Event* event) {
}

void
PollKQueue::remove_error(__UNUSED torrent::Event* event) {
}

PollKQueue::PollKQueue(__UNUSED int fd, __UNUSED int maxEvents, __UNUSED int maxOpenSockets) {
  throw internal_error("An PollKQueue function was called, but it is disabled.");
}

#endif // USE_KQUEUE

}
