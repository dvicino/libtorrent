#ifndef LIBTORRENT_SETTINGS_H
#define LIBTORRENT_SETTINGS_H

#include <string>
#include <inttypes.h>

namespace torrent {

class Settings {
 public:
  static std::string httpName;
  static std::string peerName;

  static int filesCheckWait;
  static int filesMode;
  static int dirMode;

  static int rateStart;
  static int rateWindow;
  static int rateQuick;
  static int rateSample;

  static int hashForcedWait;
  static int hashMadviceWait;
};

class DownloadSettings {
 public:
  DownloadSettings();

  int minPeers;
  int maxPeers;
  uint32_t maxAvailable;

  int maxUploads;

  int chokeCycle;
  int chokeGracePeriod;

  // How long before we stall a download.
  int stallTimeout;

  // Time to wait after a choke before we consider it final.
  int cancelTimeout;

  int32_t endgameBorder;
  uint32_t endgameRate;

  static DownloadSettings& global() { return *m_global; }

 private:
  static DownloadSettings* m_global;
};

class ThrottleSettings { 
 public:
  ThrottleSettings();

  int constantRate;

  static int minPeriod;
  static int wakeupPoint;

  // If greater than 'starvePoint' bytes was left then limit the amount
  // allocated.
  static int starvePoint;

  // Each turn give spent + starveBuffer bytes if not starved.
  static int starveBuffer;

  // Min size of chunks to send out.
  static int minChunkMask;
};

}

#endif // LIBTORRENT_SETTINGS_H
