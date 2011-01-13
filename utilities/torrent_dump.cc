#include <iostream>
#include <fstream>
#include <string>

#include <torrent/hash_string.h>
#include <torrent/object.h>
#include <torrent/object_stream.h>

int
main(int argc, char** argv) {
  if (argc != 2)
    return -1;

  std::ifstream src_file(argv[1]);

  if (!src_file.good())
    return -1;

  torrent::Object src_object;
  torrent::object_read_bencode(&src_file, &src_object);

  std::cout << torrent::hash_string_to_hex_str(*torrent::HashString::cast_from(torrent::object_sha1(&src_object.get_key("info")))) << std::endl;

  return 0;
}
