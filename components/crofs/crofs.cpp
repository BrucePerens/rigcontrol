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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_vfs.h"
#endif

#include "crofs.h"

CroFS::CroFS()
{
    _lock_init(&lock);

    fds = NULL;
    registered = false;
}

CroFS::~CroFS()
{
    term();

    _lock_close(&lock);
}

int CroFS::init(const crofs_config_t *config)
{
    cfg = *config;

    if (cfg.fs_addr == NULL || cfg.fs_size == 0)
    {
        errno = EINVAL;
        return -1;
    }

    fs_start = cfg.fs_addr + cfg.fs_size - 1;
    fs_end = cfg.fs_addr;

    fds = new crofs_file_t *[cfg.open_files];
    if (fds == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    memset(fds, 0, sizeof(crofs_file_t *) * cfg.open_files);

#if defined(ESP_PLATFORM)
    esp_vfs_t vfs = {};

    vfs.flags = ESP_VFS_FLAG_CONTEXT_PTR;
    vfs.lseek_p = &lseek_p;
    vfs.read_p = &read_p;
    vfs.open_p = &open_p;
    vfs.close_p = &close_p;
    vfs.fstat_p = &fstat_p;
    vfs.stat_p = &stat_p;
    vfs.opendir_p = &opendir_p;
    vfs.readdir_p = &readdir_p;
    vfs.readdir_r_p = &readdir_r_p;
    vfs.telldir_p = &telldir_p;
    vfs.seekdir_p = &seekdir_p;
    vfs.closedir_p = &closedir_p;

    esp_err_t esperr = esp_vfs_register(cfg.base_path, &vfs, this);
    if (esperr != ESP_OK)
    {
        errno = esperr;
        return -1;
    }

    registered = true;
#endif

    return 0;
}

void CroFS::term()
{
    if (registered)
    {
        for (int i = 0; i < cfg.open_files; i++)
        {
            if (fds[i])
            {
                close_p(this, i);
                fds[i] = NULL;
            }
        }

#if defined(ESP_PLATFORM)
        esp_vfs_unregister(cfg.base_path);
#endif

        registered = false;
    }

    if (fds)
    {
        free(fds);
    }
}

const uint8_t *CroFS::get_entry(const uint8_t *addr, crofs_entry_t *entry)
{
    const uint8_t *p = addr ? addr : fs_start;
    int flags = *p;

    entry->addr = p;
    entry->level = (flags & CROFS_LEVEL);
    entry->isfile = (flags & CROFS_FILE);
    entry->iscomp = (flags & CROFS_COMP);

    p -= 1;
    entry->nsize = *p;

    p -= entry->nsize;
    entry->name = p;

    if (entry->isfile)
    {
        p -= 2;
        entry->usize = p[1] << 8 | p[0];
        if (flags & CROFS_USIZE4)
        {
            p -= 2;
            entry->usize = entry->usize << 16 | p[1] << 8 | p[0];
        }

        if (entry->iscomp)
        {
            p -= 2;
            entry->csize = p[1] << 8 | p[0];
            if (flags & CROFS_CSIZE4)
            {
                p -= 2;
                entry->csize = entry->csize << 16 | p[1] << 8 | p[0];
            }
        }
        else
        {
            entry->csize = entry->usize;
        }
    }
    else
    {
        entry->csize = 0;
        entry->usize = 0;
    }

    p -= entry->csize;
    entry->data = p;

#if 0
    printf("next = %p "
           "addr = %p "
           "data = %p "
           "usize = %d "
           "csize = %d "
           "nsize = %d "
           "level = %d "
           "isfile = %d "
           "iscomp = %d "
           "name = %p %*.*s\n",
           p - 1,
           entry->addr,
           entry->data,
           entry->usize,
           entry->csize,
           entry->nsize,
           entry->level,
           entry->isfile,
           entry->iscomp,
           entry->name,
           entry->nsize,
           entry->nsize,
           entry->name); 
#endif

    p -= 1;

    return p;
}

const uint8_t *CroFS::locate(const uint8_t *name, crofs_entry_t *entry)
{
    const uint8_t *parents[16] = {fs_start};
    const uint8_t *addr = get_entry(fs_start, entry);
    const uint8_t *p = name;
    int level = 1;

    name = NULL;
    while (addr > fs_end)
    {
        if (*p == '/' || *p == '\0')
        {
            if (name)
            {
                int nsize = p - name;

                if (nsize == 2 && name[0] == '.' && name[1] == '.')
                {
                    if (level >= 2)
                    {
                        level -= 1;
                    }

                    if (level >= 1)
                    {
                        level -= 1;
                    }

                    addr = get_entry(parents[level], entry);
                }
                else if (nsize == 1 && name[0] == '.')
                {
                    // Just ignore it
                }
                else if (nsize > 0)
                {
                    const uint8_t *next = NULL;

                    while (addr > fs_end)
                    {
                        next = get_entry(addr, entry);
                        if (entry->level == level &&
                            entry->nsize == nsize &&
                            strncmp((const char *) entry->name, (const char *) name, nsize) == 0)
                        {
                            break;
                        }
                        addr = next;
                        next = NULL;

                        if (entry->level < level)
                        {
                            break;
                        }
                    }

                    if (next == NULL)
                    {
                        errno = ENOENT;
                        return NULL;
                    }

                    if (*p == '/')
                    {
                        if (entry->isfile)
                        {
                            errno = ENOTDIR;
                            return NULL;
                        }

                        parents[level] = addr; 
                    }
                    addr = next;

                    // Found a subdirectory, so increase level.
                    // (addr will be pointing to first entry within this subdirectory)
                    level += 1;
                }

                name = NULL;
            }

            if (*p == '\0')
            {
                break;
            }
        }
        else if (name == NULL)
        {
            name = p;
        }

        p += 1;
    }

    if (name)
    {
        errno = ENOENT;
        return NULL;
    }

    return addr;
}

int CroFS::get_free_fd()
{
    for (int i = 0; i < 10; i++)
    {
        if (fds[i] == NULL)
        {
            return i;
        }
    }

    return -1;
}

off_t CroFS::lseek_p(void *ctx, int fd, off_t offset, int whence)
{
    CroFS *that = (CroFS *) ctx;

    _lock_acquire(&that->lock);

    crofs_file_t *file = that->fds[fd];

    _lock_release(&that->lock);

    if (file == NULL)
    {
        errno = EBADF;
        return -1;
    }

    if (whence == SEEK_SET)
    {
        // nothing to do
    }
    else if (whence == SEEK_CUR)
    {
        offset += file->offset;
    }
    else if (whence == SEEK_END)
    {
        offset += file->entry.usize;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    if (offset < 0 || offset > (off_t) file->entry.usize)
    {
        errno = EINVAL;
        return -1;
    } 

    if (file->dc)
    {
        crofs_decomp_t *dc = file->dc;

        if (offset < file->offset)
        {
            file->offset = 0;

            tinfl_init(&dc->ctx);
            dc->ipos = 0;
            dc->opos = 0;
            dc->osize = 0;
        }

        size_t size = offset - file->offset;
        while (size > 0)
        {
            if (dc->osize == 0)
            {
                size_t isize = file->entry.csize - dc->ipos;
                if (isize == 0)
                {
                    break;
                }

                dc->osize = TINFL_LZ_DICT_SIZE - dc->opos;
                tinfl_status status = tinfl_decompress(&dc->ctx,
                                                       &file->entry.data[dc->ipos],
                                                       &isize,
                                                       &dc->dict[0],
                                                       &dc->dict[dc->opos],
                                                       &dc->osize,
                                                       0);
                if (status < 0)
                {
                    errno = EIO;
                    return -1;
                }

                dc->ipos += isize;
            }

            if (dc->osize > 0)
            {
                size_t osize = dc->osize > size ? size : dc->osize;

                dc->opos = (dc->opos + osize) & (TINFL_LZ_DICT_SIZE - 1);

                dc->osize -= osize;
                file->offset += osize;
                size -= osize;
            }
        }
    }
    else
    {
        file->offset = offset;
    }

    return file->offset;
}

ssize_t CroFS::read_p(void *ctx, int fd, void *dst, size_t size)
{
    CroFS *that = (CroFS *) ctx;
    ssize_t read = 0;
    uint8_t *optr = (uint8_t *) dst;
    
    _lock_acquire(&that->lock);

    crofs_file_t *file = that->fds[fd];

    _lock_release(&that->lock);

    if (file == NULL)
    {
        errno = EBADF;
        return -1;
    }

    if (file->dc)
    {
        crofs_decomp_t *dc = file->dc;

        while (size > 0)
        {
            if (dc->osize == 0)
            {
                size_t isize = file->entry.csize - dc->ipos;
                if (isize == 0)
                {
                    break;
                }

                dc->osize = TINFL_LZ_DICT_SIZE - dc->opos;
                tinfl_status status = tinfl_decompress(&dc->ctx,
                                                       &file->entry.data[dc->ipos],
                                                       &isize,
                                                       &dc->dict[0],
                                                       &dc->dict[dc->opos],
                                                       &dc->osize,
                                                       0);
                if (status < 0)
                {
                    errno = EIO;
                    return -1;
                }

                dc->ipos += isize;
            }

            if (dc->osize > 0)
            {
                size_t osize = dc->osize > size ? size : dc->osize;

                memcpy(optr, &dc->dict[dc->opos], osize);

                dc->opos = (dc->opos + osize) & (TINFL_LZ_DICT_SIZE - 1);

                dc->osize -= osize;
                file->offset += osize;
                optr += osize;
                read += osize;
                size -= osize;
            }
        }
    }
    else
    {
        size_t isize = file->entry.usize - file->offset;
        read = isize > size ? size : isize;

        memcpy(optr, &file->entry.data[file->offset], read);

        file->offset += read;
    }

    return read;
}

int CroFS::open_p(void *ctx, const char *path, int flags, int mode)
{
    CroFS *that = (CroFS *) ctx;

    if ((flags & O_ACCMODE) != O_RDONLY)
    {
        errno = EROFS;
        return -1;
    }

    crofs_file_t *file = (crofs_file_t *) malloc(sizeof(crofs_file_t));
    if (file == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    *file = {};

    if (that->locate((const uint8_t *) path, &file->entry) == NULL)
    {
        free(file);
        return -1;    
    }

    if (!file->entry.isfile)
    {
        free(file);
        errno = EISDIR;
        return -1;
    }

    if (file->entry.iscomp)
    {
        file->dc = (crofs_decomp_t *) malloc(sizeof(crofs_decomp_t));
        if (file->dc == NULL)
        {
            free(file);
            errno = ENOMEM;
            return -1;
        }

        tinfl_init(&file->dc->ctx);
        file->dc->ipos = 0;
        file->dc->opos = 0;
        file->dc->osize = 0;
    }

    _lock_acquire(&that->lock);

    int fd = that->get_free_fd();
    if (fd == -1)
    {
        _lock_release(&that->lock);
        if (file->dc)
        {
            free(file->dc);
        }
        free(file);
        errno = ENFILE;
        return -1;
    }

    that->fds[fd] = file;

    _lock_release(&that->lock);

    return fd;
}

int CroFS::close_p(void *ctx, int fd)
{
    CroFS *that = (CroFS *) ctx;

    _lock_acquire(&that->lock);

    crofs_file_t *file = that->fds[fd];
    if (file == NULL)
    {
        _lock_release(&that->lock);
        errno = EBADF;
        return -1;
    }

    if (file->dc)
    {
        free(file->dc);
    }

    free(file);

    that->fds[fd] = NULL;

    _lock_release(&that->lock);

    return 0;
}

int CroFS::fstat_p(void *ctx, int fd, struct stat *st)
{
    CroFS *that = (CroFS *) ctx;

    _lock_acquire(&that->lock);

    crofs_file_t *file = that->fds[fd];

    _lock_release(&that->lock);

    *st = {};
    st->st_mode = S_IRWXU | S_IRWXG | S_IRWXO | file->entry.isfile ? S_IFREG : S_IFDIR;
    st->st_size = file->entry.usize;

    // Let's fib a little and provide the compressed size...
    st->st_blocks = file->entry.csize;

    return 0;
}

int CroFS::stat_p(void *ctx, const char *path, struct stat *st)
{
    CroFS *that = (CroFS *) ctx;
    crofs_entry_t entry;

    if (that->locate((const uint8_t *) path, &entry) == NULL)
    {
        return -1;
    }
    
    *st = {};
    st->st_mode = S_IRWXU | S_IRWXG | S_IRWXO | entry.isfile ? S_IFREG : S_IFDIR;
    st->st_size = entry.usize;

    // Let's fib a little and provide the compressed size...
    st->st_blocks = entry.csize;

    return 0; 
}

DIR *CroFS::opendir_p(void *ctx, const char *name)
{
    CroFS *that = (CroFS *) ctx;
    crofs_dir_t *dir;
    crofs_entry_t entry;

    dir = (crofs_dir_t *) malloc(sizeof(crofs_dir_t));
    if (dir == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    *dir = {};

    dir->next = that->locate((const uint8_t *) name, &entry);
    if (dir->next == NULL)
    {
        free(dir);
        return NULL;
    }
    dir->addr = entry.addr;
    dir->level = entry.level + 1;
    dir->pos = 0;

    return (DIR *) dir;
}

struct dirent *CroFS::readdir_p(void *ctx, DIR *pdir)
{
    crofs_dir_t *dir = (crofs_dir_t *) pdir;
    struct dirent *out_dirent = NULL;

    if (dir == NULL)
    {
        errno = EBADF;
        return NULL;
    }

    int err = readdir_r_p(ctx, pdir, &dir->dirent, &out_dirent);
    if (err != 0)
    {
        errno = err;
    }

    return out_dirent;
}

int CroFS::readdir_r_p(void *ctx, DIR *pdir, struct dirent *dirent, struct dirent **out_dirent)
{
    CroFS *that = (CroFS *) ctx;
    crofs_dir_t *dir = (crofs_dir_t *) pdir;
    crofs_entry_t entry;

    if (dir == NULL || dirent == NULL || out_dirent == NULL)
    {
        return EBADF;
    }

    if (dir->next == NULL)
    {
        *out_dirent = NULL;
        return 0;
    }

    while (dir->next > that->fs_end)
    {
        dir->next = that->get_entry(dir->next, &entry);
        if (entry.level == dir->level)
        {
            dir->pos++;

            *dirent = {};
            dirent->d_type = entry.isfile ? DT_REG : DT_DIR;
            strncpy(dirent->d_name, (const char *) entry.name, entry.nsize); 
            dirent->d_name[entry.nsize] = '\0';

            *out_dirent = dirent;
            return 0;
        }

        if (entry.level > dir->level)
        {
            continue;
        }

        if (entry.level < dir->level)
        {
            break;
        }
    }

    dir->next = NULL;

    *out_dirent = NULL;
    return 0;
}

long CroFS::telldir_p(void *ctx, DIR *pdir)
{
    crofs_dir_t *dir = (crofs_dir_t *) pdir;

    if (dir == NULL)
    {
        errno = EBADF;
        return -1;
    }

    return dir->pos;
}

void CroFS::seekdir_p(void *ctx, DIR *pdir, long offset)
{
    CroFS *that = (CroFS *) ctx;
    crofs_dir_t *dir = (crofs_dir_t *) pdir;
    crofs_entry_t entry;

    if (dir == NULL)
    {
        return;
    }

    // Start from the owning directory
    dir->next = that->get_entry(dir->addr, &entry);
    dir->pos = 0;

    while (dir->next > that->fs_end)
    {
        if (dir->pos == offset)
        {
            break;
        }

        dir->next = that->get_entry(dir->next, &entry);
        if (entry.level == dir->level)
        {
            dir->pos++;
        }

        if (entry.level > dir->level)
        {
            continue;
        }

        if (entry.level < dir->level)
        {
            break;
        }
    }

    return;
}

int CroFS::closedir_p(void *ctx, DIR *pdir)
{
    crofs_dir_t *dir = (crofs_dir_t *) pdir;

    if (dir == NULL)
    {
        errno = EBADF;
        return -1;
    }

    free(pdir);

    return 0;
}

#if !defined(ESP_PLATFORM)

// ============================================================================
// mkcrofs host code
// ============================================================================

static const int probes[] =
{
    0,
    1 | TDEFL_GREEDY_PARSING_FLAG,
    6 | TDEFL_GREEDY_PARSING_FLAG,
    32 | TDEFL_GREEDY_PARSING_FLAG,
    16,
    32,
    128,
    256,
    512,
    768,
    1500
};

bool CroFS::compress(FILE *out, const char *path, size_t *usize, size_t *csize)
{
    tdefl_compressor compr;
    tdefl_status status;

    *usize = 0;
    *csize = 0;

    if (clevel > 0)
    {
        status = tdefl_init(&compr, NULL, NULL, probes[clevel]);
        if (status != TDEFL_STATUS_OKAY)
        {
            printf("Failed to initialize compressor\n");
            abort();
        }
    }

    FILE *in = fopen(path, "rb");
    if (in == NULL)
    {
        printf("Failed to open %s: %s\n", path, strerror(errno));
        abort();
    }

    char ibuf[4096];
    size_t isize;

    char obuf[TDEFL_OUT_BUF_SIZE];
    size_t osize;
    bool copy = (clevel == 0);

    if (!copy)
    {
        off_t mark = ftell(out);

        do
        {
            isize = fread(ibuf, 1, sizeof(ibuf), in);
            if (ferror(in))
            {
                printf("Read failed on %s: %s\n", path, strerror(errno));
                abort();
            }
            
            if (isize == 0)
            {
                continue;
            }

            *usize += isize;

            osize = sizeof(obuf);

            status = tdefl_compress(&compr, ibuf, &isize, obuf, &osize, TDEFL_NO_FLUSH);
            if (status != TDEFL_STATUS_OKAY)
            {
                printf("Compression failed for %s\n", path);
                abort();
            }

            if (osize)
            {
                *csize += fwrite(obuf, 1, osize, out);
            }

            if (ferror(out))
            {
                printf("Write failed on imagepath: %s\n", strerror(errno));
                abort();
            }
        } while (!feof(in));

        osize = sizeof(obuf);

        status = tdefl_compress(&compr, NULL, 0, obuf, &osize, TDEFL_FINISH);
        if (status != TDEFL_STATUS_DONE)
        {
            printf("Compression failed for %s\n", path);
            abort();
        }

        if (osize)
        {
            *csize += fwrite(obuf, 1, osize, out);
            if (ferror(out))
            {
                printf("Write failed on imagepath: %s\n", strerror(errno));
                abort();
            }
        }

        if (*csize >= *usize)
        {
            if (ftruncate(fileno(out), mark) == -1)
            {
                printf("Unable to truncate imagepath %s: %s\n", path, strerror(errno));
                abort();
            }

            if (fseek(out, mark, SEEK_SET) == -1)
            {
                printf("Unable to rewind imagepath: %s\n", strerror(errno));
                abort();
            }

            if (fseek(in, 0, SEEK_SET) == -1)
            {
                printf("Unable to rewind %s: %s\n", path, strerror(errno));
                abort();
            }

            *usize = 0;
            *csize = 0;
            copy = true;
        }
    }

    if (copy)
    {
        do
        {
            isize = fread(ibuf, 1, sizeof(ibuf), in);
            if (ferror(in))
            {
                printf("Read failed on %s: %s\n", path, strerror(errno));
                abort();
            }
            
            if (isize == 0)
            {
                continue;
            }

            *usize += isize;

            *csize += fwrite(ibuf, 1, isize, out);
            if (ferror(out))
            {
                printf("Write failed on imagepath: %s\n", strerror(errno));
                abort();
            }
        } while (!feof(in));
    }

    if (fclose(in) == EOF)
    {
        printf("Failed to close %s: %s\n", path, strerror(errno));
        abort();
    }

    return copy;
}

void CroFS::copy(FILE *out, const char *parent, int level)
{
    if (level == 16)
    {
        printf("Number of directory levels exceeds 16\n");
        abort();
    }

    DIR *dir = opendir(parent);
    if (dir == NULL)
    {
        printf("Couldn't open directory %s: %s\n", parent, strerror(errno));
        abort();
    }

    while (true)
    {
        errno = 0;
        struct dirent *de = readdir(dir);
        if (de == NULL)
        {
            if (errno != 0)
            {
                printf("Couldn't read directory %s: %s\n", parent, strerror(errno));
                abort();
            }
            break;
        }

        int nsize = strlen(de->d_name);
        if (nsize > 255)
        {
            printf("File name longer than 255 bytes: %s\n", de->d_name);
            abort();
        }

        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
        {
            continue;
        }

        int len = strlen(parent) + 1 + nsize + 1;
        char *path = (char *) malloc(len);
        if (path == NULL)
        {
            printf("Insufficient memory for path buffer\n");
            abort();
        }
        snprintf(path, len, "%s/%s", parent, de->d_name);

        int flags = level;
        if (de->d_type == DT_DIR)
        {
            copy(out, path, level + 1);
        }
        else if (de->d_type == DT_REG)
        {
            size_t usize = 0;
            size_t csize = 0;

            flags |= CROFS_FILE;

            flags |= compress(out, path, &usize, &csize) ? 0 : CROFS_COMP;

            if (flags & CROFS_COMP)
            {
                if (fputc(csize & 0xff, out) == EOF ||
                    fputc((csize >> 8) & 0xff, out) == EOF)
                {
                    printf("Write failed on imagepath: %s\n", strerror(errno));
                    abort();
                }

                if (csize >= 65536)
                {
                    if (fputc((csize >> 16) & 0xff, out) == EOF ||
                        fputc((csize >> 24) & 0xff, out) == EOF)
                    {
                        printf("Write failed on imagepath: %s\n", strerror(errno));
                        abort();
                    }

                    flags |= CROFS_CSIZE4;
                }
            }

            if (fputc(usize & 0xff, out) == EOF ||
                fputc((usize >> 8) & 0xff, out) == EOF)
            {
                printf("Write failed on imagepath: %s\n", strerror(errno));
                abort();
            }

            if (usize >= 65536)
            {
                if (fputc((usize >> 16) & 0xff, out) == EOF ||
                    fputc((usize >> 24) & 0xff, out) == EOF)
                {
                    printf("Write failed on imagepath: %s\n", strerror(errno));
                    abort();
                }

                flags |= CROFS_USIZE4;
            }


            if (verbose)
            {
                printf("Wrote %s: %zd bytes ", path, csize);

                if (flags & CROFS_COMP)
                {
                    printf("(uncompressed bytes = %zd)\n", usize);
                }
                else
                {
                    printf("(not compressed)\n");
                }
            }
        }

        fwrite(de->d_name, 1, nsize, out);
        if (ferror(out))
        {
            printf("Write failed on imagepath: %s\n", strerror(errno));
            abort();
        }

        if (fputc(nsize, out) == EOF || fputc(flags, out) == EOF)
        {
            printf("Write failed on imagepath: %s\n", strerror(errno));
            abort();
        }

        free(path);
    }

    if (closedir(dir) == -1)
    {
        printf("Failed to close directory %s: %s\n", parent, strerror(errno));
        abort();
    }

    return;
}

#define TELL(ctx, fileno) lseek_p(ctx, fileno, 0, SEEK_CUR)

void CroFS::walk(const char *parent, const char *name, int level)
{
    DIR *dir = opendir_p(this, parent);
    if (dir == NULL)
    {
        printf("Couldn't open directory %s: %s\n", parent, strerror(errno));
        abort();
    }

    if (level == 0)
    {
        printf("/\n");
    }

    while (true)
    {
        long dpos = telldir_p(this, dir);
        if (dpos == -1)
        {
            printf("telldir() failed %s: %s\n", parent, strerror(errno));
            abort();
        }

        errno = 0;
        struct dirent *de = readdir_p(this, dir);
        if (de == NULL)
        {
            if (errno != 0)
            {
                printf("Couldn't read directory %s: %s\n", parent, strerror(errno));
                abort();
            }
            break;
        }

        seekdir_p(this, dir, dpos);

        long dpos2 = telldir_p(this, dir);
        if (dpos2 != dpos)
        {
            printf("seekdir() position should be %ld but is %ld for %s\n", dpos, dpos2, parent);
            abort();
        }
        
        errno = 0;
        struct dirent *de2 = readdir_p(this, dir);
        if (de2 == NULL || errno != 0)
        {
            printf("Couldn't read directory after seek %s: %s\n", parent, strerror(errno));
            abort();
        }

        if (de2->d_type != de->d_type || strcmp(de2->d_name, de->d_name) != 0)
        {
            printf("Reread of directory entry after seek doesn't match for %s:\n", parent);
            printf("  before seekdir: type=%d name =%s\n", de->d_type, de->d_name);
            printf("  after seekdir: type=%d name =%s\n", de2->d_type, de2->d_name);
            abort();
        }

        for (int i = 0; i < level; i++)
        {
            printf("|   ");
        }
        printf("|-- %s%s", de->d_name, de->d_type == DT_DIR ? "/\n" : "");

        int len = strlen(parent) + 1 +  strlen(de->d_name)+ 1;
        char *path = (char *) malloc(len);
        if (path == NULL)
        {
            printf("Insufficient memory for path buffer\n");
            abort();
        }
        snprintf(path, len, "%s/%s", parent, de->d_name);

        if (de->d_type == DT_DIR)
        {

            walk(path, de->d_name, level + 1);
        }
        else if (de->d_type == DT_REG)
        {
            int fd = open_p(this, path, O_RDONLY, 0);
            if (fd == -1)
            {
                printf("Unable to open %s: %s", path, strerror(errno));
                abort();
            }

            size_t bytes = 0;
            ssize_t read;
            do
            {
                char buf[256];
                read = read_p(this, fd, buf, sizeof(buf));
                if (read == -1)
                {
                    printf("Error reading %s: %s", path, strerror(errno));
                    abort();
                }
                bytes += read;
            } while (read != 0);

            struct stat st;
            if (fstat_p(this, fd, &st) != 0)
            {
                printf("Unable to fstat() %s: %s\n", path, strerror(errno));
                abort();
            }

            printf(" (usize=%ld, csize=%ld, bytes read=%zd, pos=%ld)\n", st.st_size, st.st_blocks, bytes, TELL(this, fd));

            struct stat st2;
            if (stat_p(this, path, &st2) != 0)
            {
                printf("Unable to stat() %s: %s\n", path, strerror(errno));
                abort();
            } 

            if (st.st_size != st2.st_size || st.st_blocks != st2.st_blocks)
            {
                printf("fstat() and stat() results differ for %s:\n", path);
                printf("  fstat(): size=%ld, blocks=%ld\n", st.st_size, st.st_blocks);
                printf("  stat(): size=%ld blocks=%ld\n", st2.st_size, st2.st_blocks);
                abort();
            } 

            if (st.st_size > 4)
            {
                off_t pos;

                // Seek to half of the file
                pos = lseek_p(this, fd, st.st_size / 2, SEEK_SET);

                if (pos == -1 || pos != st.st_size / 2)
                {
                    printf("SEEK_SET(size / 2) to %ld got %ld failed %s: %s\n", st.st_size / 2, pos, path, strerror(errno));
                    abort();
                } 

                // Seek to 1 byte from end of file
                pos = lseek_p(this, fd, st.st_size - 1, SEEK_SET);
                if (pos == -1 || pos != st.st_size - 1 || TELL(this, fd) != pos)
                {
                    printf("SEEK_SET(size - 1) to %ld got %ld failed %s: %s\n", st.st_size - 1, pos, path, strerror(errno));
                    abort();
                } 

                // Seek to 3 bytes from end of file
                pos = lseek_p(this, fd, -2, SEEK_CUR);
                if (pos == -1 || pos != st.st_size - 3 || TELL(this, fd) != pos)
                {
                    printf("SEEK_CUR(-2) to %ld got %ld failed %s: %s\n", st.st_size - 3, pos, path, strerror(errno));
                    abort();
                } 

                // Seek to 2 bytes from end of file
                pos = lseek_p(this, fd, -2, SEEK_END);
                if (pos == -1 || pos != st.st_size - 2 || TELL(this, fd) != pos)
                {
                    printf("SEEK_END(-2) to %ld got %ld failed %s: %s\n", st.st_size - 2, pos, path, strerror(errno));
                    abort();
                } 

                // Seek to 2 bytes from end of file
                pos = lseek_p(this, fd, -2, SEEK_END);
                if (pos == -1 || pos != st.st_size - 2 || TELL(this, fd) != pos)
                {
                    printf("SEEK_END(-2) to %ld got %ld failed %s: %s\n", st.st_size - 2, pos, path, strerror(errno));
                    abort();
                } 

                // Seek to start of file
                pos = lseek_p(this, fd, -st.st_size, SEEK_END);
                if (pos == -1 || pos != 0 || TELL(this, fd) != pos)
                {
                    printf("SEEK_END(-size) to %d got %ld failed %s: %s\n", 0, pos, path, strerror(errno));
                    abort();
                } 

                // Seek to 1 byte from start of file
                pos = lseek_p(this, fd, 1, SEEK_CUR);
                if (pos == -1 || pos != 1 || TELL(this, fd) != pos)
                {
                    printf("SEEK_CUR(1) to %d got %ld failed %s: %s\n", 1, pos, path, strerror(errno));
                    abort();
                } 

                // Seek to end of file
                pos = lseek_p(this, fd, st.st_size - 1, SEEK_CUR);
                if (pos == -1 || pos != st.st_size || TELL(this, fd) != pos)
                {
                    printf("SEEK_CUR(size - 1) to %ld got %ld failed %s: %s\n", st.st_size, pos, path, strerror(errno));
                    abort();
                } 

                // Seek past end of file
                pos = lseek_p(this, fd, 1, SEEK_END);
                if (pos != -1)
                {
                    printf("SEEK_END(+1) did not fail got %ld %s\n", pos, path);
                    abort();
                } 

                // Seek before start of file
                pos = lseek_p(this, fd, -1, SEEK_SET);
                if (pos != -1)
                {
                    printf("SEEK_SET(-1) did not fail got %ld %s\n", pos, path);
                    abort();
                } 
            }

            if (close_p(this, fd) != 0)
            {
                printf("Unable to close %s: %s", path, strerror(errno));
                abort();
            }

        }

        free(path);
    }

    if (closedir_p(this, dir) == -1)
    {
        printf("Failed to close directory %s: %s\n", parent, strerror(errno));
        abort();
    }
}

int CroFS::mkcrofs(int argc, char *argv[])
{
    int opt;

    // Parse the command line arguments
    while ((opt = getopt(argc, argv, "l:tv")) != -1)
    {
        switch (opt)
        {
            case -1:
            break;

            case 'l':
                clevel = atoi(optarg);
                if (clevel < 0 || clevel > 10)
                {
                    printf("Compression level must be 0 (no compression) or 1 to 10\n");
                    return EXIT_FAILURE;
                }
            break;

            case 't':
                test = true;
            break;

            case 'v':
                verbose = true;
            break;

            default:
                printf("Usage: %s [-l level] [-t] [-v] imagepath sourcepath ...\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Must have at least the image path and 1 source path
    if (argc - optind < 2)
    {
        printf("Usage: %s [-l level] [-t] [-v] imagepath sourcepath ...\n", argv[0]);
        return EXIT_FAILURE;
    } 

    // Create the image path
    FILE *imagefile = fopen(argv[optind], "wb");
    if (imagefile == NULL)
    {
        printf("Unable to create imagepath: %s\n", strerror(errno));
        abort();
    }

    // And add files within the source pages
    for (int i = optind + 1; i < argc; i++)
    {
        copy(imagefile, argv[i], 1);
    }

    // Write "root" entry
    if (fputc(0, imagefile) == EOF || fputc(0, imagefile) == EOF)
    {
        printf("Unable to write root entry to imagepath: %s\n", strerror(errno));
        abort();
    }

    // Retrieve number of bytes written
    long imagesize = ftell(imagefile);
    if (imagesize == -1)
    {
        printf("Unable to retrieve image size: %s\n", strerror(errno));
        abort();
    }
    
    // Done with the imagefile
    if (fclose(imagefile) == EOF)
    {
        printf("Failed to close imagepath: %s\n", strerror(errno));
        abort();
    }

    // If we're not testing, then we're done
    if (!test)
    {
        return EXIT_SUCCESS;
    }

    // Get memory for the entire filesystem
    uint8_t *fs = (uint8_t *) malloc(imagesize);
    if (fs == NULL)
    {
        printf("%ld bytes of memory unavailable for filesystem image\n", imagesize);
        abort();
    }

    // Reopen the image path 
    imagefile = fopen(argv[optind], "rb");
    if (imagefile == NULL)
    {
        printf("Unable to open imagepath: %s\n", strerror(errno));
        abort();
    }

    // Read the entire filesystem into buffer
    if (fread(fs, 1, imagesize, imagefile) != (size_t) imagesize)
    {
        printf("Unable to load imagepath: %s\n", strerror(errno));
        abort();
    }

    // Done with the imagefile
    if (fclose(imagefile) == EOF)
    {
        printf("Failed to close imagepath: %s\n", strerror(errno));
        abort();
    }

    // "Mount" the filesystem
    crofs_config_t cfg = {fs, (size_t) imagesize, "/", 10};
    init(&cfg);

    // And test all of the filesystem functions
    walk("/", "", 0);

    return 0;
}

extern "C" int main(int argc, char *argv[])
{
    CroFS cfs;

    return cfs.mkcrofs(argc, argv);
}
#endif
