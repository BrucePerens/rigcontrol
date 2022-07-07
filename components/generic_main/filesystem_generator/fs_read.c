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
#include <zlib.h>
#include "compressed_fs.h"

int
main(int argc, char * * argv)
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

  for ( int i = 0; i < header->number_of_files; i++ ) {
    const char * const name = (image + entries[i].name_offset);
    if ( strcmp(name, argv[2]) == 0 ) {
      write(1, (image + entries[i].data_offset), entries[i].size);
    }
  }
  
  munmap(image, s.st_size);

  close(fd);
  return 0;
}
