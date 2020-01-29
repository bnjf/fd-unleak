#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <dlfcn.h>

#include "uthash.h"

/*-
 * Copyright (c) 2013 Brad Forschinger
 * All rights reserved.
 */


/* last entry must be NULL */
static char *target_files[] = { "/var/krb5/rcache/HTTP_8080", NULL };

struct fd_info_s {
    int fd;
    char path[FILENAME_MAX];
    UT_hash_handle hh;
};

struct fd_info_s *fd_to_path = NULL, *path_to_fd = NULL;

/* see if we've already got an fd for the path.  if so, close it first. */
int open(const char *path, int oflag, mode_t mode)
{
    static int (*real_open) (const char *, int, mode_t);
    struct fd_info_s *fd_info = NULL;
    char **target_file;

    if (real_open == NULL) {
        fprintf(stderr, "open: init\n");
        real_open = dlsym(RTLD_NEXT, "open");
        if (real_open == NULL) {
            return -1;
        }
    }

    /* look for a match */
    target_file = &target_files[0];
    while (*target_file != NULL) {

        /* match? */
        if (strcmp(*target_file, path) == 0) {
            int fd;

            HASH_FIND_STR(path_to_fd, path, fd_info);   /* have we opened this already? */
            if (fd_info) {

                fd = fd_info->fd;
                fd_info = NULL;
                fprintf(stderr, "open: closing %d before opening %s\n", fd, *target_file);
                close(fd);      /* our hooked close, we'll remove the hash entries there */
            }

            fd = real_open(path, oflag, mode);
            if (fd != -1) {
                /* create fd => path */
                fd_info = malloc(sizeof(struct fd_info_s));
                fd_info->fd = fd;
                strcpy(fd_info->path, path);
                HASH_ADD_INT(fd_to_path, fd, fd_info);

                /* create path => fd */
                fd_info = malloc(sizeof(struct fd_info_s));
                fd_info->fd = fd;
                strcpy(fd_info->path, path);
                HASH_ADD_STR(path_to_fd, path, fd_info);
            }
            return fd;
        }

        /* next. */
        *target_file++;
    }

    return real_open(path, oflag, mode);
}

/* stop tracking the fd */
int close(int filedes)
{
    static int (*real_close) (int);
    struct fd_info_s *fd_info = NULL;
    char path[FILENAME_MAX];

    if (real_close == NULL) {
        fprintf(stderr, "close: init\n");
        real_close = dlsym(RTLD_NEXT, "close");
        if (real_close == NULL) {
            return -1;
        }
    }

    HASH_FIND_INT(fd_to_path, &filedes, fd_info);
    if (fd_info) {
        /* del fd => path */
        strcpy(path, fd_info->path);
        HASH_DEL(fd_to_path, fd_info);
        free(fd_info);

        /* del path => fd */
        HASH_FIND_STR(path_to_fd, path, fd_info);
        HASH_DEL(path_to_fd, fd_info);
        free(fd_info);

        fprintf(stderr, "close: was tracking %d (%s)\n", filedes, path);
    }

    return real_close(filedes);
}

