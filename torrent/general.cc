#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>

#include "general.h"
#include "bitfield.h"
#include "torrent/exceptions.h"
#include "peer_handshake.h"
#include "settings.h"
#include "bencode.h"

#include <stdlib.h>
#include <iomanip>
#include <openssl/sha.h>
#include <sys/time.h>

namespace torrent {

std::string generateId() {
  std::string id = Settings::peerName;

  for (int i = id.length(); i < 20; ++i)
    id += random();

  return id;
}

std::string generateKey() {
  std::string id;

  for (int i = 0; i < 8; ++i) {
    unsigned int v = random() % 16;

    if (v < 10)
      id += '0' + v;
    else
      id += 'a' + v - 10;
  }

  return id;
}

std::string calcHash(const bencode& b) {
  std::stringstream str;
  str << b;

  return std::string((const char*)SHA1((const unsigned char*)(str.str().c_str()), str.str().length(), NULL), 20);
}

std::vector<std::string> partitionLine(char*& pos, char* end) {
  std::vector<std::string> l;
  std::string s;

  while (pos != end && *pos != '\n') {

    if ((*pos == ' ' || *pos == '\r' || *pos == '\t') &&
	s.length()) {
      l.push_back(s);
      s = std::string();
    } else {
      s += *pos;
    }

    ++pos;
  }

  return pos != end ? l : std::vector<std::string>();
}

}
