#include "config.h"

#include <stdexcept>
#include <sigc++/bind.h>
#include <torrent/torrent.h>

#include "core/download.h"
#include "input/bindings.h"
#include "display/window_title.h"
#include "display/window_peer_list.h"
#include "display/window_peer_info.h"
#include "display/window_statusbar.h"

#include "control.h"
#include "download.h"

namespace ui {

Download::Download(DPtr d, Control* c) :
  m_download(d),
  m_state(DISPLAY_NONE),
  m_window(c->get_display().end()),
  m_title(new WTitle(d->get_download().get_name())),
  m_status(new WStatus),
  m_control(c),
  m_bindings(new input::Bindings) {

  m_focus = m_peers.end();

  bind_keys(m_bindings);

  m_download->get_download().peer_list(m_peers);

  m_connPeerConnected = m_download->get_download().signal_peer_connected(sigc::mem_fun(*this, &Download::receive_peer_connected));
  m_connPeerDisconnected = m_download->get_download().signal_peer_disconnected(sigc::mem_fun(*this, &Download::receive_peer_disconnected));
}

Download::~Download() {
  m_connPeerConnected.disconnect();
  m_connPeerDisconnected.disconnect();

  delete m_title;
  delete m_status;
  delete m_bindings;
}

void
Download::activate() {
  if (m_window != m_control->get_display().end())
    throw std::logic_error("ui::Download::activate() called on an already activated object");

  m_window = m_control->get_display().insert(m_control->get_display().begin(), NULL);

  m_control->get_display().push_front(m_title);
  m_control->get_display().push_back(m_status);

  m_control->get_input().push_front(m_bindings);

  activate_display(DISPLAY_MAIN);
}

void
Download::disable() {
  if (m_window == m_control->get_display().end())
    throw std::logic_error("ui::Download::disable() called on an already disabled object");

  disable_display();

  m_control->get_display().erase(m_window);
  m_control->get_display().erase(m_title);
  m_control->get_display().erase(m_status);

  m_control->get_input().erase(m_bindings);

  m_window = m_control->get_display().end();
}

void
Download::activate_display(Display d) {
  WPeerList* wpl;
  WPeerInfo* wpi;

  if (m_window == m_control->get_display().end())
    throw std::logic_error("ui::Download::activate_display(...) could not find previous display iterator");

  switch (d) {
  case DISPLAY_MAIN:
    *m_window = wpl = new WPeerList(&m_peers, &m_focus);

    wpl->slot_chunks_total(sigc::mem_fun(m_download->get_download(), &torrent::Download::get_chunks_total));

    (*m_bindings)[KEY_RIGHT] = sigc::bind(sigc::mem_fun(*this, &Download::receive_change), DISPLAY_PEER);
    break;

  case DISPLAY_PEER:
    *m_window = wpi = new WPeerInfo(m_download, &m_peers, &m_focus);

    wpi->slot_chunks_total(sigc::mem_fun(m_download->get_download(), &torrent::Download::get_chunks_total));

    (*m_bindings)[' '] = sigc::bind(sigc::mem_fun(*this, &Download::receive_change), DISPLAY_MAIN);
    break;

  default:
    throw std::logic_error("ui::Download::activate_display(...) got wrong state");
  }

  m_state = d;

  m_control->get_display().adjust_layout();
}

// Does not delete disabled window.
void
Download::disable_display() {
  switch (m_state) {
  case DISPLAY_MAIN:
    m_bindings->erase(KEY_RIGHT);

    break;

  case DISPLAY_PEER:
    m_bindings->erase(' ');

    break;

  default:
    break;
  }

  delete *m_window;

  *m_window = NULL;
}

void
Download::receive_next() {
  if (m_focus != m_peers.end())
    ++m_focus;
  else
    m_focus = m_peers.begin();

  mark_dirty();
}

void
Download::receive_prev() {
  if (m_focus != m_peers.begin())
    --m_focus;
  else
    m_focus = m_peers.end();

  mark_dirty();
}

void
Download::receive_peer_connected(torrent::Peer p) {
  m_peers.push_back(p);
}

void
Download::receive_peer_disconnected(torrent::Peer p) {
  PList::iterator itr = std::find(m_peers.begin(), m_peers.end(), p);

  if (itr == m_peers.end())
    throw std::logic_error("Download::receive_peer_disconnected(...) received a peer we don't have in our list");

  if (itr == m_focus)
    m_focus = m_peers.erase(itr);
  else
    m_peers.erase(itr);
}

void
Download::receive_throttle(int t) {
  m_status->mark_dirty();

  torrent::set(torrent::THROTTLE_ROOT_CONST_RATE, torrent::get(torrent::THROTTLE_ROOT_CONST_RATE) + t * 1024);
}

void
Download::receive_change(Display d) {
  if (d == m_state)
    return;

  disable_display();

  activate_display(d);
}

void
Download::bind_keys(input::Bindings* b) {
  (*b)['a'] = sigc::bind(sigc::mem_fun(*this, &Download::receive_throttle), 1);
  (*b)['z'] = sigc::bind(sigc::mem_fun(*this, &Download::receive_throttle), -1);
  (*b)['s'] = sigc::bind(sigc::mem_fun(*this, &Download::receive_throttle), 5);
  (*b)['x'] = sigc::bind(sigc::mem_fun(*this, &Download::receive_throttle), -5);
  (*b)['d'] = sigc::bind(sigc::mem_fun(*this, &Download::receive_throttle), 50);
  (*b)['c'] = sigc::bind(sigc::mem_fun(*this, &Download::receive_throttle), -50);

  (*b)[KEY_UP]   = sigc::mem_fun(*this, &Download::receive_prev);
  (*b)[KEY_DOWN] = sigc::mem_fun(*this, &Download::receive_next);
}

void
Download::mark_dirty() {
  (*m_window)->mark_dirty();
}

}
