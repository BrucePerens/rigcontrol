#include <stdint.h>
struct compressed_fs_header {
  uint32_t	number_of_files;
};

struct compressed_fs_entry {
  uint32_t	name_offset;
  uint32_t	data_offset;
  uint32_t	length;
  uint32_t	compressed_length;
};
