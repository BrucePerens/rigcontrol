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

static char *	output = 0;
static long	output_size = 0;

static void
descend(int fd, const char * name, void (*function)(int fd, const char * name, const char * pathname))
{
  DIR *			d;
  struct dirent *	e;

  if ( fd >= 0 )
    d = fdopendir(fd);
  else {
    d = opendir(name);
    if ( d == NULL ) {
      fprintf(stderr, "%s: %s\n", name, strerror(errno));
      exit(1);
    }
    fd = dirfd(d);
  }

  while ( (e = readdir(d)) ) {
    char		pathname[1024];
    struct stat		s;
    int			dir_fd;

    if ( e->d_name[0] == '.' && ( e->d_name[1] == '\0'
     | ( e->d_name[1] == '.' && e->d_name[2] == '\0' ) ) )
      continue;

    if ( name[0] == '/' && name[1] == '\0' )
      snprintf(pathname, sizeof(pathname), "/%s", e->d_name);
    else
      snprintf(pathname, sizeof(pathname), "%s/%s", name, e->d_name);

    switch ( e->d_type ) {
    case DT_DIR:
      dir_fd = openat(fd, e->d_name, O_RDONLY);
      if ( dir_fd < 0 ) {
        fprintf(stderr, "%s: %s\n", pathname, strerror(errno));
        exit(1);
      }
      descend(dir_fd, pathname, function);
      break;
    case DT_REG:
      (*function)(fd, e->d_name, pathname);
      break;
    case DT_UNKNOWN:
      if ( fstatat(dirfd(d), e->d_name, &s, 0) != 0 ) {
        fprintf(stderr, "%s: %s\n", pathname, strerror(errno));
        exit(1);
      }
      switch ( s.st_mode & S_IFMT ) {
      case S_IFDIR:
        dir_fd = openat(fd, e->d_name, O_RDONLY);
        if ( dir_fd < 0 ) {
          fprintf(stderr, "%s: %s\n", pathname, strerror(errno));
          exit(1);
        }
        descend(dir_fd, pathname, function);
        break;
      case S_IFREG:
        (*function)(fd, e->d_name, pathname);
        break;
      default:
        break;
      }
      break;
    default:
      break;
    }
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
  int		suffix_count = sizeof(do_not_compress_suffixes) / sizeof(*do_not_compress_suffixes);
  bool		do_not_compress = false;

  for ( unsigned int i = 0; i < suffix_count; i++ ) {
    if ( strcmp(do_not_compress_suffixes[i], dot) == 0 ) {
      do_not_compress = true;
      break;
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

  // The system should really know not to reserve swap space for a read-only
  // mapping, and MAP_NORESERVE should have no effect here. Unless there is
  // some semantic about the mapping not being shared and the file changing
  // under the mapping.
  // Remove this if it's a problem.
  if ( (input = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED ) {
    fprintf(stderr, "%s: mmap failed: %s\n", pathname, strerror(errno));
    exit(1);
  }

  printf("%s\n", pathname);
  fflush(stdout);

  if ( do_not_compress ) {
    // Sendfile would be faster, but it doesn't matter for this application.
    fprintf(stderr, "copy %d bytes from %lx to %lx\n", s.st_size, input, output);
    memcpy(output, input, s.st_size);
    output += s.st_size;
  }
  else {
    int		result;
    long	size = output_size;

    result = compress2(output, &size, input, s.st_size, Z_BEST_COMPRESSION);

    if ( result != Z_OK ) {
      fprintf(stderr, "%s: compress failed.\n", pathname);
      exit(1);
    }

    output += size;
    if ( size > output_size ) {
      fprintf(stderr, "Maximum output file size was too small.\n");
      exit(1);
    }
    output_size -= size;
  }


  munmap(input, s.st_size);
  close(fd);
}

int
main(int argc, char * * argv)
{
  char *	name = argv[1];
  size_t	length = strlen(name);
  char *	output_base = 0;
  long		output_base_size;
  int		fd;

  if ( argc < 2 ) {
    fprintf(stderr, "Usage: %s source-directory output-file\n", argv[0]);
    exit(1);
  }
  if ( length > 1 && name[length - 1] == '/' )
    name[length - 1] = '\0';

  // Maximum output size. The ESP-32 only has 4 MB RAM, so 2 MB here.
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


  *output = 0;
  descend(-1, name, write_file);
  msync(output_base, output_base_size - output_size, MS_ASYNC);
  munmap(output_base, output_base_size);
  close(fd);
  return 0;
}
