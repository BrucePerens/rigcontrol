#include <stdint.h>
struct compressed_fs_header {
  char		magic[64];
  uint32_t	number_of_files;
  uint32_t	table_offset;
};

struct compressed_fs_entry {
  uint32_t	name_offset;
  uint32_t	data_offset;
  uint32_t	compressed_size;
  uint32_t	uncompressed_size;
};
