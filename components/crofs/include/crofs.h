// Copyright 2017-2018 Leland Lucius
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if !defined(_CROFS_H_)
#define _CROFS_H_ 1

#include <dirent.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#if defined(ESP_PLATFORM)

#include <sys/lock.h>
#include "rom/miniz.h"

#else

#include "miniz.h"

#define _lock_init(l)
#define _lock_open(l)
#define _lock_acquire(l)
#define _lock_release(l)
#define _lock_close(l)
#define _lock_t bool

#endif

#if defined(CONFIG_CROFS_EMBED)
extern "C"
{
    extern const uint8_t _binary_crofs_start[];
    extern const uint8_t _binary_crofs_size;
}
#endif

typedef struct
{
    const uint8_t *fs_addr;     // address to (in-memory) filesystem
    size_t fs_size;             // size of (in-memory) filesystem
    const char *base_path;      // mount point
    int open_files;             // number of open files to support
} crofs_config_t;

class CroFS
{
public:
    CroFS();
    virtual ~CroFS();

    int init(const crofs_config_t *config);
    void term();

    static const uint8_t *get_embedded_start()
    {
#if defined(CONFIG_CROFS_EMBED)
        return _binary_crofs_start;
#else
        return NULL;
#endif
    };

    static size_t get_embedded_size()
    {
#if defined(CONFIG_CROFS_EMBED)
        return (size_t) &_binary_crofs_size;
#else
        return 0;
#endif
    };

#if !defined(ESP_PLATFORM)
    int mkcrofs(int argc, char *argv[]);
#endif

private:

    static const int CROFS_FILE    =   (1 << 7);
    static const int CROFS_USIZE4  =   (1 << 6);
    static const int CROFS_CSIZE4  =   (1 << 5);
    static const int CROFS_COMP    =   (1 << 4);
    static const int CROFS_LEVEL   =   (0x0f);

    typedef struct
    {
        const uint8_t *addr;
        const uint8_t *data;
        const uint8_t *name;
        uint32_t usize;
        uint32_t csize;
        uint8_t nsize;
        uint8_t level;
        bool isfile;
        bool iscomp;
    } crofs_entry_t;

    typedef struct
    {
#if defined(ESP_PLATFORM)
        DIR dir;                // must be first...ESP32 VFS expects it...
#endif
        struct dirent dirent;
        const uint8_t *addr;
        const uint8_t *next;
        long pos;
        uint8_t level;
    } crofs_dir_t;

    typedef struct
    {
        tinfl_decompressor ctx;
        uint8_t dict[TINFL_LZ_DICT_SIZE];
        size_t ipos;
        size_t opos;
        size_t osize;
    } crofs_decomp_t;

    typedef struct
    {
        crofs_entry_t entry;
        crofs_decomp_t *dc;
        off_t offset;
    } crofs_file_t;

    const uint8_t *get_entry(const uint8_t *addr, crofs_entry_t *entry);
    const uint8_t *locate(const uint8_t *name, crofs_entry_t *entry);
    int get_free_fd();

    static off_t lseek_p(void *ctx, int fd, off_t offset, int whence);
    static ssize_t read_p(void *ctx, int fd, void *dst, size_t size);
    static int open_p(void *ctx, const char *path, int flags, int mode);
    static int close_p(void *ctx, int fd);
    static int fstat_p(void *ctx, int fd, struct stat *st);
    static int stat_p(void *ctx, const char *path, struct stat *st);

    static DIR *opendir_p(void *ctx, const char *name);
    static struct dirent *readdir_p(void *ctx, DIR *pdir);
    static int readdir_r_p(void *ctx, DIR *pdir, struct dirent *dirent, struct dirent **out_dirent);
    static long telldir_p(void *ctx, DIR *pdir);
    static void seekdir_p(void *ctx, DIR *pdir, long offset);
    static int closedir_p(void *ctx, DIR *pdir);

#if !defined(ESP_PLATFORM)
    void copy(FILE *out, const char *parent, int level);
    bool compress(FILE *out, const char *path, size_t *usize, size_t *csize);
    void walk(const char *parent, const char *name, int level);
#endif

private:
    crofs_config_t cfg;
    const uint8_t *fs_start;
    const uint8_t *fs_end;
    crofs_file_t **fds;
    _lock_t lock;
    bool registered;

#if !defined(ESP_PLATFORM)
    int clevel = 6;
    bool test = false;
    bool verbose = false;
#endif
};
#endif
