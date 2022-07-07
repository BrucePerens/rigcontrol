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

// Used to keep track of overflow of the maximum size of the output file.
static long				output_size = 0;

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
// each subdirectory.
static void
descend(int fd, const char * name, void (*function)(int fd, const char * name, const char * pathname))
{
  DIR *			d;	// Handle on a directory in the source filesystem.
  struct dirent *	e;	// Entry in a directory in the source filesystem.
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
  while ( (e = readdir(d)) ) {
    char		pathname[1024];
    struct stat		s;
    int			dir_fd;

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
      case ST_REG:
        e->d_type = DT_REG;
        break;
      case ST_DIR:
        e->d_type = DT_DIR;
        break;
      }
    }

    // Switch on the file type to determine how to process it.
    switch ( e->d_type ) {
    case DT_DIR:
      // It's a directory. Recurse.
      dir_fd = openat(fd, e->d_name, O_RDONLY);
      if ( dir_fd < 0 ) {
        fprintf(stderr, "%s: %s\n", pathname, strerror(errno));
        exit(1);
      }
      descend(dir_fd, pathname, function);
      close(dir_fd);
      break;
    case DT_REG:
      // It's a regular file. Call the coroutine to process it.
      (*function)(fd, e->d_name, pathname);
      break;
    default:
      break;
  }
  closedir(d);
}

static void
write_file(int dir_fd, const char * name, const char * pathname)
{
  struct stat	s;	
  char *	input;
  int		fd = openat(dir_fd, name, O_RDONLY);
  const char *	dot = strrchr(name, '.');
  const int	suffix_count = sizeof(do_not_compress_suffixes) / sizeof(*do_not_compress_suffixes);
  bool		do_not_compress = false;
  int		name_length = 1 + strlen(pathname);
  struct compressed_fs_entry * e;

  if ( entry_index >= entries_size ) {
    entries_size *= 2;
    if ( (entries = realloc(entries, entries_size * sizeof(*entries))) == 0 ) {
      fprintf(stderr, "Out of memory.\n");
      exit(1);
    }
  }
  e = &entries[entry_index];
  entry_index++;

  e->name_offset = output - output_base;
  memcpy(output, pathname, name_length);
  output += name_length;
  output_size -= name_length;
  e->data_offset = output - output_base;

  header->number_of_files++;

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
    if ( (input = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED ) {
      fprintf(stderr, "%s: mmap failed: %s\n", pathname, strerror(errno));
      exit(1);
    }

    if ( do_not_compress ) {
      // Sendfile would be faster, but it doesn't matter for this application.
      fprintf(stderr, "copy %d bytes from %lx to %lx\n", s.st_size, input, output);
      memcpy(output, input, s.st_size);
      e->compressed_size = s.st_size;
      e->method = NONE;
      output += s.st_size;
      output_size -= s.st_size;
    }
    else {
      long	size = output_size;
  
      if ( compress2(output, &size, input, s.st_size, Z_BEST_COMPRESSION) != Z_OK) {
        fprintf(stderr, "%s: compress failed.\n", pathname);
        exit(1);
      }
      e->compressed_size = size;
      e->method = ZLIB;
      output += size;
      output_size -= size;
  
      if ( size > output_size ) {
        fprintf(stderr, "Maximum output file size was too small.\n");
        exit(1);
      }
      output_size -= size;
    }
  
    munmap(input, s.st_size);
  }
  close(fd);
}

int
main(int argc, char * * argv)
{
  char *	name = argv[1];
  size_t	length = strlen(name);
  long		output_base_size;
  long		file_size;
  int		fd;
  long		table_offset;

  if ( argc < 2 ) {
    fprintf(stderr, "Usage: %s source-directory output-file\n", argv[0]);
    exit(1);
  }
  if ( length > 1 && name[length - 1] == '/' )
    name[length - 1] = '\0';

  // Maximum output size. The ESP-32 only has 4 MB FLASH.
  output_size = output_base_size = 4 * 1024 * 1024;

  if ( (fd = open(argv[2], O_CREAT|O_TRUNC|O_RDWR, 0666)) < 0 ) {
    fprintf(stderr, "%s: %s\n", argv[2], strerror(errno));
    exit(1);
  }
  // Can't write an mmapped file with size 0.
  // Just write the maximum size, and truncate the file to the correct length later.
  if ( ftruncate(fd, output_base_size) != 0 ) {
    fprintf(stderr, "%s: allocate space failed: %s\n", argv[2], strerror(errno));
    exit(1);
  }

  if ( (output = output_base = mmap(0, output_base_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED ) {
    fprintf(stderr, "%s: mmap failed: %s\n", argv[2], strerror(errno));
    exit(1);
  }

  header = (struct compressed_fs_header *)output;
  output += sizeof(*header);
  memcpy(header->magic, compressed_fs_magic, sizeof(header->magic));

  entries_size = 100;
  entries = malloc(entries_size * sizeof(*entries));
  if ( entries == NULL ) {
    fprintf(stderr, "Out of memory.\n");
    exit(1);
  }

  descend(-1, name, write_file);
  entries_size = sizeof(*entries) * entry_index;

  table_offset = output - output_base;
  if ( table_offset & 3 )
    table_offset += (4 - (table_offset & 3));

  header->table_offset = table_offset;
  memcpy(output, entries, entries_size);
  output += entries_size;

  file_size = output - output_base;
  msync(output_base, file_size, MS_ASYNC);
  munmap(output_base, output_base_size);
  if ( ftruncate(fd, file_size) != 0 ) {
    fprintf(stderr, "%s: final truncate failed: %s\n", argv[2], strerror(errno));
    exit(1);
  }
  close(fd);
  return 0;
}
