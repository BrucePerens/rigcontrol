#include <stdint.h>

enum compression_method {
  NONE,	// The file is not compressed, to prevent re-compressing image files, etc.
  ZLIB	// The file was compressed with zlib compress2(..., Z_BEST_COMPRESSION).
};

struct compressed_fs_header {
  char		magic[64];
  uint32_t	number_of_files;
  uint32_t	table_offset;
};

struct compressed_fs_entry {
  uint32_t			name_offset;
  uint32_t			data_offset;
  uint32_t			compressed_size;
  uint32_t			size;
  enum compression_method	method;
};
