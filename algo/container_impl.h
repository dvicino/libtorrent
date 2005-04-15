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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef ALGO_CONTAINER_IMPL_H
#define ALGO_CONTAINER_IMPL_H

#include <algorithm>

namespace algo {

template <bool PreInc, typename Iterator, typename Ftor>
inline void for_each(Iterator first, Iterator last, Ftor ftor) {
  if (PreInc) {
    for (Iterator tmp; first != last; ) {
      tmp = first++;

      ftor(*tmp);
    }

  } else {
    for (; first != last; ++first)
      ftor(*first);
  }
}

template <bool PreInc, typename Target, typename Call>
struct CallForEach {
  CallForEach(Target target, Call call) : m_target(target), m_call(call) {}

  template <typename Arg1>
  void operator () (Arg1& arg1) {
    for_each<PreInc>(Direct(m_target(arg1)).begin(), Direct(m_target(arg1)).end(), m_call);
  }

  Target m_target;
  Call m_call;
};

template <typename Target, typename Cond>
struct ContainsCond {
  ContainsCond(Target target, Cond cond) : m_target(target), m_cond(cond) {}

  template <typename Arg1>
  bool operator () (Arg1& arg1) {
    return std::find_if(Direct(m_target(arg1)).begin(), Direct(m_target(arg1)).end(), m_cond)
      != Direct(m_target(arg1)).end();
  }

  Target m_target;
  Cond m_cond;
};

template <typename Container, typename Element>
struct ContainedIn {
  ContainedIn(Container c, Element e) : m_container(c), m_element(e) {}

  template <typename Arg1>
  bool operator () (Arg1& arg1) {
    return std::find(Direct(m_container(arg1)).begin(), Direct(m_container(arg1)).end(), m_element(arg1))
      != Direct(m_container(arg1)).end();
  }

  Container m_container;
  Element m_element;
};

template <typename Target, typename Cond, typename Found, typename NotFound>
struct FindIfOn {
  FindIfOn(Target target, Cond cond, Found found, NotFound notFound) :
    m_target(target),
    m_cond(cond),
    m_found(found),
    m_notFound(notFound) {}

  template <typename A>
  bool operator () (A& a) {
    typename Target::BaseType::iterator itr = m_target(a).begin();
    typename Target::BaseType::iterator end = m_target(a).end();

    while (itr != end) {
      if (m_cond(*itr)) {

	m_found(*itr);
	return true;
      }

      ++itr;
    }

    m_notFound(*itr);
    return false;
  }

  Target m_target;
  Cond m_cond;
  Found m_found;
  NotFound m_notFound;
};

}

#endif // ALGO_CONTAINER_IMPL_H