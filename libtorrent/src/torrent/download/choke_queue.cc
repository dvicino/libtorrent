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

#include <algorithm>
#include <functional>
#include <numeric>
#include <cstdlib>
#include <tr1/functional>

#include "protocol/peer_connection_base.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/choke_status.h"
#include "torrent/utils/log_files.h"

#include "choke_queue.h"

namespace torrent {

void
choke_manager_erase(choke_queue::container_type* container, PeerConnectionBase* pc) {
  choke_queue::container_type::iterator itr = std::find_if(container->begin(), container->end(),
                                                            rak::equal(pc, rak::mem_ref(&choke_queue::value_type::first)));

  if (itr == container->end())
    throw internal_error("choke_manager_remove(...) itr == m_unchoked.end().");

  *itr = container->back();
  container->pop_back();
}

choke_queue::~choke_queue() {
  if (m_unchoked.size() != 0)
    throw internal_error("choke_queue::~choke_queue() called but m_currentlyUnchoked != 0.");

  if (m_queued.size() != 0)
    throw internal_error("choke_queue::~choke_queue() called but m_currentlyQueued != 0.");
}

// 1  > 1
// 9  > 2
// 17 > 3 < 21
// 25 > 4 < 31
// 33 > 5 < 41
// 65 > 9 < 81

inline uint32_t
choke_queue::max_alternate() const {
  if (m_unchoked.size() < 31)
    return (m_unchoked.size() + 7) / 8;
  else
    return (m_unchoked.size() + 9) / 10;
}

// Would propably replace maxUnchoked with whatever max the global
// manager gives us. When trying unchoke, we always ask the global
// manager for permission to unchoke, and when we did a choke.
//
// Don't notify the global manager anything when we keep the balance.

void
choke_queue::balance() {
  // Return if no balancing is needed. Don't return if is_unlimited()
  // as we might have just changed the value and have interested that
  // can be unchoked.
  if (m_unchoked.size() == m_maxUnchoked)
    return;

  int adjust = m_maxUnchoked - m_unchoked.size();

  if (log_files[LOG_CHOKE_CHANGES].is_open())
    log_choke_changes_func_new(this, "balance", m_maxUnchoked, adjust);

  if (adjust > 0) {
    adjust = adjust_choke_range(m_queued.begin(), m_queued.end(),
                                std::min((uint32_t)adjust, m_slotCanUnchoke()), false);

    m_slotUnchoke(adjust);

  } else if (adjust < 0)  {
    // We can do the choking before the slot is called as this
    // choke_queue won't be unchoking the same peers due to the
    // call-back.
    adjust = adjust_choke_range(m_unchoked.begin(), m_unchoked.end(), -adjust, true);

    m_slotUnchoke(-adjust);
  }
}

int
choke_queue::cycle(uint32_t quota) {
  quota = std::min(quota, m_maxUnchoked);

  uint32_t adjust = std::max<uint32_t>(m_unchoked.size() < quota ? quota - m_unchoked.size() : 0,
                                       std::min(quota, max_alternate()));

  if (log_files[LOG_CHOKE_CHANGES].is_open())
    log_choke_changes_func_new(this, "cycle", quota, adjust);

  // Does this properly handle 'unlimited' quota?
  uint32_t oldSize  = m_unchoked.size();
  uint32_t unchoked = adjust_choke_range(m_queued.begin(), m_queued.end(), adjust, false);

  if (m_unchoked.size() > quota)
    adjust_choke_range(m_unchoked.begin(), m_unchoked.end() - unchoked, m_unchoked.size() - quota, true);

  if (m_unchoked.size() > quota)
    throw internal_error("choke_queue::cycle() m_unchoked.size() > quota.");

  return m_unchoked.size() - oldSize;
}

void
choke_queue::set_queued(PeerConnectionBase* pc, choke_status* base) {
  if (base->queued() || base->unchoked())
    return;

  base->set_queued(true);

  if (base->snubbed())
    return;

  if (!is_full() && (m_flags & flag_unchoke_all_new || m_slotCanUnchoke()) &&
      rak::timer(base->time_last_choke()) + rak::timer::from_seconds(10) < cachedTime) {
    m_unchoked.push_back(value_type(pc, 0));
    m_slotConnection(pc, false);

    m_slotUnchoke(1);

  } else {
    m_queued.push_back(value_type(pc, 0));
  }
}

void
choke_queue::set_not_queued(PeerConnectionBase* pc, choke_status* base) {
  if (!base->queued())
    return;

  base->set_queued(false);

  if (base->snubbed())
    return;

  if (base->unchoked()) {
    choke_manager_erase(&m_unchoked, pc);
    m_slotConnection(pc, true);
    m_slotUnchoke(-1);

  } else {
    choke_manager_erase(&m_queued, pc);
  }
}

void
choke_queue::set_snubbed(PeerConnectionBase* pc, choke_status* base) {
  if (base->snubbed())
    return;

  base->set_snubbed(true);

  if (base->unchoked()) {
    choke_manager_erase(&m_unchoked, pc);
    m_slotConnection(pc, true);
    m_slotUnchoke(-1);

  } else if (base->queued()) {
    choke_manager_erase(&m_queued, pc);
  }

  base->set_queued(false);
}

void
choke_queue::set_not_snubbed(PeerConnectionBase* pc, choke_status* base) {
  if (!base->snubbed())
    return;

  base->set_snubbed(false);

  if (!base->queued())
    return;

  if (base->unchoked())
    throw internal_error("choke_queue::set_not_snubbed(...) base->unchoked().");
  
  if (!is_full() && (m_flags & flag_unchoke_all_new || m_slotCanUnchoke()) &&
      rak::timer(base->time_last_choke()) + rak::timer::from_seconds(10) < cachedTime) {
    m_unchoked.push_back(value_type(pc, 0));
    m_slotConnection(pc, false);

    m_slotUnchoke(1);

  } else {
    m_queued.push_back(value_type(pc, 0));
  }
}

// We are no longer in m_connectionList.
void
choke_queue::disconnected(PeerConnectionBase* pc, choke_status* base) {
  if (base->snubbed()) {
    // Do nothing.

  } else if (base->unchoked()) {
    choke_manager_erase(&m_unchoked, pc);
    m_slotUnchoke(-1);

  } else if (base->queued()) {
    choke_manager_erase(&m_queued, pc);
  }

  base->set_queued(false);
}

// No need to do any choking as the next choke balancing will take
// care of things.
void
choke_queue::move_connections(choke_queue* src, choke_queue* dest, DownloadMain* download) {
  iterator itr = src->m_queued.begin();

  while (itr != src->m_queued.end()) {
    if (itr->first->download() != download) {
      itr++;
      continue;
    }

    dest->m_queued.push_back(*itr);

    *itr = src->m_queued.back();
    src->m_queued.pop_back();
  }

  itr = src->m_unchoked.begin();

  while (itr != src->m_unchoked.end()) {
    if (itr->first->download() != download) {
      itr++;
      continue;
    }

    dest->m_unchoked.push_back(*itr);

    *itr = src->m_unchoked.back();
    src->m_unchoked.pop_back();
  }
}

//
// Heuristics:
//

struct choke_manager_less {
  bool operator () (choke_queue::value_type v1, choke_queue::value_type v2) const { return v1.second < v2.second; }
};

void
choke_manager_allocate_slots(choke_queue::iterator first, choke_queue::iterator last,
                             uint32_t max, uint32_t* weights, choke_queue::target_type* target) {
  // Sorting the connections from the lowest to highest value.
  std::sort(first, last, choke_manager_less());

  // 'weightTotal' only contains the weight of targets that have
  // connections to unchoke. When all connections are in a group are
  // to be unchoked, then the group's weight is removed.
  uint32_t weightTotal = 0;
  uint32_t unchoke = max;

  target[0].second = first;

  for (uint32_t i = 0; i < choke_queue::order_max_size; i++) {
    target[i].first = 0;
    target[i + 1].second = std::find_if(target[i].second, last,
                                        rak::less(i * choke_queue::order_base + (choke_queue::order_base - 1),
                                                  rak::mem_ref(&choke_queue::value_type::second)));

    if (std::distance(target[i].second, target[i + 1].second) != 0)
      weightTotal += weights[i];
  }

  // Spread available unchoke slots as long as we can give everyone an
  // equal share.
  while (weightTotal != 0 && unchoke / weightTotal > 0) {
    uint32_t base = unchoke / weightTotal;

    for (uint32_t itr = 0; itr < choke_queue::order_max_size; itr++) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (weights[itr] == 0 || target[itr].first >= s)
        continue;
      
      uint32_t u = std::min(s - target[itr].first, base * weights[itr]);

      unchoke -= u;
      target[itr].first += u;

      if (target[itr].first >= s)
        weightTotal -= weights[itr];
    }
  }

  // Spread the remainder starting from a random position based on the
  // total weight. This will ensure that aggregated over time we
  // spread the unchokes equally according to the weight table.
  if (weightTotal != 0 && unchoke != 0) {
    uint32_t start = ::random() % weightTotal;
    unsigned int itr = 0;

    for ( ; ; itr++) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (weights[itr] == 0 || target[itr].first >= s)
        continue;

      if (start < weights[itr])
        break;

      start -= weights[itr];
    }

    for ( ; weightTotal != 0 && unchoke != 0; itr = (itr + 1) % choke_queue::order_max_size) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (weights[itr] == 0 || target[itr].first >= s)
        continue;

      uint32_t u = std::min(unchoke, std::min(s - target[itr].first, weights[itr] - start));

      start = 0;
      unchoke -= u;
      target[itr].first += u;

      if (target[itr].first >= s)
        weightTotal -= weights[itr];
    }
  }
}

template <typename Itr>
bool range_is_contained(Itr first, Itr last, Itr lower_bound, Itr upper_bound) {
  return first >= lower_bound && last <= upper_bound && first <= last;
}

uint32_t
choke_queue::adjust_choke_range(iterator first, iterator last, uint32_t max, bool is_choke) {
  target_type target[order_max_size + 1];
  container_type* src_container;
  container_type* dest_container;

  if (is_choke) {
    src_container  = &m_unchoked;
    dest_container = &m_queued;

    m_heuristics_list[m_heuristics].slot_choke_weight(first, last);
    choke_manager_allocate_slots(first, last, max, m_heuristics_list[m_heuristics].choke_weight, target);
  } else {
    src_container  = &m_queued;
    dest_container = &m_unchoked;

    m_heuristics_list[m_heuristics].slot_unchoke_weight(first, last);
    choke_manager_allocate_slots(first, last, max, m_heuristics_list[m_heuristics].unchoke_weight, target);
  }

  if (log_files[LOG_CHOKE_CHANGES].is_open())
    for (uint32_t i = 0; i < choke_queue::order_max_size; i++)
      log_choke_changes_func_allocate(this, "unchoke" + 2*is_choke, i, target[i].first, std::distance(target[i].second, target[i + 1].second));

  // Now do the actual unchoking.
  uint32_t count = 0;

  for (target_type* itr = target + order_max_size; itr != target; itr--) {
    if ((itr - 1)->first > (uint32_t)std::distance((itr - 1)->second, itr->second))
      throw internal_error("choke_queue::adjust_choke_range(...) itr->first > std::distance((itr - 1)->second, itr->second).");

    count += (itr - 1)->first;

    iterator first_adjust = itr->second - (itr - 1)->first;
    iterator last_adjust = itr->second;

    if (!range_is_contained(first_adjust, last_adjust, src_container->begin(), src_container->end()))
      throw internal_error("choke_queue::adjust_choke_range(...) bad iterator range.");

    std::for_each(first_adjust, last_adjust, rak::on(rak::mem_ref(&value_type::first), std::bind2nd(m_slotConnection, is_choke)));

    if (log_files[LOG_CHOKE_CHANGES].is_open())
      std::for_each(first_adjust, last_adjust,
                    std::tr1::bind(&log_choke_changes_func_peer, this, "unchoke" + 2*is_choke, std::tr1::placeholders::_1));

    dest_container->insert(dest_container->end(), itr->second - (itr - 1)->first, itr->second);
    src_container->erase(itr->second - (itr - 1)->first, itr->second);
  }

  if (count > max)
    throw internal_error("choke_queue::adjust_choke_range(...) count > max.");

  return count;
}

// Note that these algorithms fail if the rate >= 2^30.

// Need to add the recently unchoked check here?

void
calculate_upload_choke(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    // Very crude version for now.
    uint32_t downloadRate = first->first->peer_chunks()->download_throttle()->rate()->rate();
    first->second = choke_queue::order_base - 1 - downloadRate;

    first++;
  }
}

void
calculate_upload_unchoke(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    if (first->first->is_down_local_unchoked()) {
      uint32_t downloadRate = first->first->peer_chunks()->download_throttle()->rate()->rate();

      // If the peer transmits at less than 1KB, we should consider it
      // to be a rather stingy peer, and should look for new ones.

      if (downloadRate < 1000)
        first->second = downloadRate;
      else
        first->second = 2 * choke_queue::order_base + downloadRate;

    } else {
      // This will be our optimistic unchoke queue, should be
      // semi-random. Give lower weights to known stingy peers.

      first->second = 1 * choke_queue::order_base + ::random() % (1 << 10);
    }

    first++;
  }
}

// Fix this, but for now just use something simple.

void
calculate_download_choke(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    // Very crude version for now.
    uint32_t downloadRate = first->first->peer_chunks()->download_throttle()->rate()->rate();
    first->second = choke_queue::order_base - 1 - downloadRate;

    first++;
  }
}

void
calculate_download_unchoke(choke_queue::iterator first, choke_queue::iterator last) {
  while (first != last) {
    // Very crude version for now.
    uint32_t downloadRate = first->first->peer_chunks()->download_throttle()->rate()->rate();
    first->second = downloadRate;

    first++;
  }
}

choke_queue::heuristics_type choke_queue::m_heuristics_list[HEURISTICS_MAX_SIZE] = {
  { &calculate_upload_choke,      &calculate_upload_unchoke,      { 1, 1, 1, 1 }, { 1, 3, 9, 0 } },
  { &calculate_download_choke,    &calculate_download_unchoke,    { 1, 1, 1, 1 }, { 1, 1, 1, 1 } },
  //  { &calculate_upload_choke_seed, &calculate_upload_unchoke_seed, { 1, 1, 1, 1 }, { 1, 3, 9, 0 } },
};

}
