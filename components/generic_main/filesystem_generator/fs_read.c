#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <miniz.h>
#include "compressed_fs.h"

static int
process_decompressed_data(const void * data, int length, void * context)
{
  write(1, data, length);
  return 1; // Success code.
}

static void
decompress_file(const char * const image, const struct compressed_fs_entry * const e)
{
  size_t	size = e->compressed_size;

  const int status = tinfl_decompress_mem_to_callback(image + e->data_offset, &size, process_decompressed_data, 0, TINFL_FLAG_PARSE_ZLIB_HEADER);
}

static void
read_file(const char * const image, const struct compressed_fs_entry * const e)
{
  fflush(stdout);

  switch ( e->method ) {
  case ZERO_LENGTH:
    break;
  case NONE:
    write(1, image + e->data_offset, e->size);
    break;
  case ZLIB:
    decompress_file(image, e);
    break;
  }
}

int
main(const int argc, const char * * const argv)
{
  if ( argc < 2 ) {
    fprintf(stderr, "Usage: %s image file\n", argv[0]);
    exit(1);
  }

  const int fd = open(argv[1], O_RDONLY, 0);
  if ( fd < 0 ) {
    fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
    exit(1);
  }

  struct stat s;
  if ( fstat(fd, &s) != 0 ) {
    fprintf(stderr, "%s: %s\n", argv[1], strerror(errno));
    exit(1);
  }

  // Map the image file.
  char * const image = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if ( image == MAP_FAILED ) {
    fprintf(stderr, "%s: mmap failed: %s\n", argv[1], strerror(errno));
    exit(1);
  }

  // The header is at the start of the output file, and says where everything else is.
  // The filesystem entries are at the end of the filesystem, after all of the file
  // names and file data, because their table can
  // only be written after all files have been processed. File names and file data
  // are interleaved after the header. Position of things does matter somewhat because
  // it's a block-based FLASH device.
  struct compressed_fs_header * header = (struct compressed_fs_header *)image;
  struct compressed_fs_entry * entries = (struct compressed_fs_entry *)(image + header->table_offset);

  bool found = false;

  for ( int i = 0; i < header->number_of_files; i++ ) {
    const char * const name = (image + entries[i].name_offset);
    if ( strcmp(name, argv[2]) == 0 ) {
      read_file(image, &entries[i]);
      found = true;
      break;
    }
  }
  
  munmap(image, s.st_size);
  close(fd);

  if ( !found ) {
    fprintf(stderr, "%s: not found.\n", argv[2]);
    return 1;
  }
  else
    return 0;
}
