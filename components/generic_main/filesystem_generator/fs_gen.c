#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include "compressed_fs.h"

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
print_path(int dir_fd, const char * name, const char * pathname)
{
  int fd = openat(dir_fd, name, O_RDONLY);
  if ( fd < 0 ) {
    fprintf(stderr, "%s: %s\n", name, strerror(errno));
    exit(1);
  }
  printf("%s\n", pathname);
  fflush(stdout);
  close(fd);
}

int
main(int argc, char * * argv)
{
  char *	name = argv[1];
  size_t	length = strlen(name);

  if ( argc < 2 ) {
    fprintf(stderr, "Usage: %s source-directory output-file\n", argv[0]);
    exit(1);
  }
  if ( length > 1 && name[length - 1] == '/' )
    name[length - 1] = '\0';

  descend(-1, name, print_path);
  return 0;
}
