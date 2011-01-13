CFLAGS=`pkg-config --cflags libtorrent`
LIBS=`pkg-config --libs libtorrent`

all: torrent_dump

torrent_dump: torrent_dump.cc
	g++ -Wall $(CFLAGS) $(LIBS) -o torrent_dump torrent_dump.cc

clean:
	-rm *.o torrent_dump
