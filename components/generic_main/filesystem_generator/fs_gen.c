#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <fcntl.h>
#include <zlib.h>
#include "compressed_fs.h"

// Files with these suffixes should not be compressed.
static const char * do_not_compress_suffixes[] = {
  "gif",
  "gz",
  "jpg",
  "jpeg",
  "png",
  "tif",
  "tiff",
  "webp"
};

// Moving pointer to output data.
static char *				output = 0;

// Pointer to the base of the memory mapped output file.
static char *				output_base = 0;

// Maximum size of the output file.
static const long			maximum_size = 4 * 1024 * 1024;

// Filesystem header.
static struct compressed_fs_header *	header = 0;

// Entries for each file in the filesystem.
static struct compressed_fs_entry *	entries = 0;

// Index to the current filesystem entry.
static unsigned int		 	entry_index = 0;

// Number of filesystem entries currently allocated.
static unsigned int			entries_size = 0;

// Descend a directory tree, processing each file with the given coroutine.
//
// fd is called with the value -1 for the top-level directory. Internally it is set for
// each subirectory.
static void
descend(int fd, const char * name, void (*function)(int fd, const char * name, const char * pathname))
{
  DIR *			d;	// Handle on a directory in the source filesystem.
  bool			top = false; // Set if this is the top-level directory.

  // Open the directory. Get an fd upon it to use with openat(). That saves inode
  // references, but this code isn't actually performance-sensitive.
  if ( fd >= 0 )
    d = fdopendir(fd);
  else {
    top = true;
    d = opendir(name);
    if ( d == NULL ) {
      fprintf(stderr, "%s: %s\n", name, strerror(errno));
      exit(1);
    }
    fd = dirfd(d);
  }

  // Read the directory, and process each entry.
  for ( struct dirent * e; e = readdir(d); ) {
    char		pathname[1024];
    struct stat		s;

    // "." and ".." are irrelevant.
    if ( e->d_name[0] == '.' && ( e->d_name[1] == '\0'
     | ( e->d_name[1] == '.' && e->d_name[2] == '\0' ) ) )
      continue;

    // Construct a pathname to put in the filesystem entry.
    if ( top )
      snprintf(pathname, sizeof(pathname), "%s", e->d_name);
    else
      snprintf(pathname, sizeof(pathname), "%s/%s", name, e->d_name);

    if ( e->d_type == DT_UNKNOWN ) {
      // The directory didn't contain the file type. Fall back to fstatat() to get it.
      if ( fstatat(dirfd(d), e->d_name, &s, 0) != 0 ) {
        fprintf(stderr, "%s: %s\n", pathname, strerror(errno));
        exit(1);
      }
      // d_type values are not the same as (s.st_mode & S_IFMT) values.
      switch ( s.st_mode & S_IFMT ) {
      case S_IFREG:
        e->d_type = DT_REG;
        break;
      case S_IFDIR:
        e->d_type = DT_DIR;
        break;
      }
    }

    // Switch on the file type to determine how to process it.
    switch ( e->d_type ) {
    case DT_DIR:
      // It's a directory. Recurse.
      {
        const int dir_fd = openat(fd, e->d_name, O_RDONLY);
        if ( dir_fd < 0 ) {
          fprintf(stderr, "%s: %s\n", pathname, strerror(errno));
          exit(1);
        }
        descend(dir_fd, pathname, function);
        close(dir_fd);
      }
      break;
    case DT_REG:
      // It's a regular file. Call the coroutine to process it.
      (*function)(fd, e->d_name, pathname);
      break;
    default:
      break;
    }
  }
  closedir(d);
}

// Write a file to the embedded filesystem. This is used as the coroutine with descend().
// dir_fd is a file descriptor to the directory containing the file, for openat().
// name is the name of the file.
// pathname is relative to the root of the source directory, with no leading slash.
//
static void
write_file(const int dir_fd, const char * const name, const char * const pathname)
{
  // File descriptor of the file.
  int			fd = openat(dir_fd, name, O_RDONLY);

  // Used to scan for suffixes of files that should not be compressed.
  const char * const	dot = strrchr(name, '.');

  // Number of entries in the suffix table.
  const int		suffix_count = sizeof(do_not_compress_suffixes) / sizeof(*do_not_compress_suffixes);

  // If set, this file should not be compressed.
  bool			do_not_compress = false;

  // Length of the pathname with terminating null.
  const int		name_length = 1 + strlen(pathname);

  // Entry for the file in the compressed filesystem.

  // Grow the number of filesystem entries as necessary.
  if ( entry_index >= entries_size ) {
    entries_size *= 2;
    if ( (entries = realloc(entries, entries_size * sizeof(*entries))) == 0 ) {
      fprintf(stderr, "Out of memory.\n");
      exit(1);
    }
  }
  struct compressed_fs_entry * e = &entries[entry_index];
  entry_index++;

  // Store the file name.
  e->name_offset = output - output_base;
  memcpy(output, pathname, name_length);
  output += name_length;
  header->number_of_files++;

  // The file data goes right after the name.
  e->data_offset = output - output_base;

  // Determine if this file should be compressed.
  if ( dot ) {
    for ( unsigned int i = 0; i < suffix_count; i++ ) {
      if ( strcmp(do_not_compress_suffixes[i], dot) == 0 ) {
        do_not_compress = true;
        break;
      }
    }
  }

  if ( fd < 0 ) {
    fprintf(stderr, "%s: %s\n", pathname, strerror(errno));
    exit(1);
  }
  // stat() is used to get the size of the file.
  struct stat	s;	
  if ( fstat(fd, &s) < 0 ) {
    fprintf(stderr, "%s: stat failed: %s\n", pathname, strerror(errno));
    exit(1);
  }

  e->size = s.st_size;

  if ( s.st_size == 0 ) {
    e->compressed_size = 0;
    e->method = ZERO_LENGTH;
    e->data_offset = 0;
  }
  else {
    // Map the input file.
    void * const input = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if ( input == MAP_FAILED ) {
      fprintf(stderr, "%s: mmap failed: %s\n", pathname, strerror(errno));
      exit(1);
    }

    // libz generally doesn't save space for files smaller than 100 bytes.
    if ( s.st_size < 100 )
      do_not_compress = true;

    if ( do_not_compress ) {
      if ( output - output_base + s.st_size >= maximum_size ) {
        fprintf(stderr, "Compressed filesystem too large.\n");
        exit(1);
      }
      // Sendfile would be faster, but it doesn't matter for this application.
      memcpy(output, input, s.st_size);
      e->compressed_size = s.st_size;
      e->method = NONE;
      output += s.st_size;
    }
    else {
      long size = maximum_size - (output - output_base);
  
      if ( compress2(output, &size, input, s.st_size, Z_BEST_COMPRESSION) != Z_OK) {
        fprintf(stderr, "%s: compress failed, probably compressed filesystem too large.\n", pathname);
        exit(1);
      }
      e->compressed_size = size;
      e->method = ZLIB;
      output += size;
    }
  
    munmap(input, s.st_size);
  }
  close(fd);
}

int
main(int argc, char * * argv)
{
  char * const	name = argv[1];
  const size_t	length = strlen(name);

  if ( argc < 2 ) {
    fprintf(stderr, "Usage: %s source-directory output-file\n", argv[0]);
    exit(1);
  }
  if ( length > 1 && name[length - 1] == '/' )
    name[length - 1] = '\0';

  // Maximum output size. The ESP-32 only has 4 MB FLASH.

  const int fd = open(argv[2], O_CREAT|O_TRUNC|O_RDWR, 0666);
  if ( fd < 0 ) {
    fprintf(stderr, "%s: %s\n", argv[2], strerror(errno));
    exit(1);
  }
  // Can't write a memory-mapped file with size 0.
  // ftruncate() sets the file size to our maximum, but doesn't allocate any blocks
  // to the file. msync() does that. At the end of processing, the file is truncated
  // again, to its actual length.
  if ( ftruncate(fd, maximum_size) != 0 ) {
    fprintf(stderr, "%s: allocate space failed: %s\n", argv[2], strerror(errno));
    exit(1);
  }

  // Map the output file.
  if ( (output = output_base = mmap(0, maximum_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED ) {
    fprintf(stderr, "%s: mmap failed: %s\n", argv[2], strerror(errno));
    exit(1);
  }

  // The header is at the start of the output file, and says where everything else is.
  // The filesystem entries are at the end of the filesystem, after all of the file
  // names and file data, because their table can
  // only be written after all files have been processed. File names and file data
  // are interleaved after the header. Position of things does matter somewhat because
  // it's a block-based FLASH device.
  header = (struct compressed_fs_header *)output;
  output += sizeof(*header);
  memcpy(header->magic, compressed_fs_magic, sizeof(header->magic));

  // Allocate the initial file table.
  entries_size = 100;
  entries = malloc(entries_size * sizeof(*entries));
  if ( entries == NULL ) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }

  // Traverse the source directory, calling write_file() for each file.
  descend(-1, name, write_file);

  // Write the file table to the end of the compressed filesystem.
  entries_size = sizeof(*entries) * entry_index;
  const int unaligned_bits = (long)output & 3;
  if ( unaligned_bits )
    output += 4 - unaligned_bits;

  if ( output + entries_size - output_base > maximum_size ) {
    fprintf(stderr, "Compressed filesystem too large.\n");
    exit(1);
  }
  header->table_offset = output - output_base;
  memcpy(output, entries, entries_size);
  output += entries_size;

  // Write the output file blocks, and set the correct file size.
  const long file_size = output - output_base;
  msync(output_base, file_size, MS_ASYNC);
  munmap(output_base, maximum_size);
  if ( ftruncate(fd, file_size) != 0 ) {
    fprintf(stderr, "%s: final truncate failed: %s\n", argv[2], strerror(errno));
    exit(1);
  }

  close(fd);
  return 0;
}
