/* The MIT License
 * 
 * Copyright (c) 2017 John Schember <john@nachtimwald.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifndef _WIN32
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include "str_builder.h"
#include "str_helpers.h"
#include "rw_files.h"

/* - - - - */

#ifdef _WIN32
const char SEP = '\\';
#else
const char SEP = '/';
#endif

/* - - - - */

char *rw_join_path(size_t n_args, ...)
{
    const char    *const_temp;
    char          *out;
    str_builder_t *sb;
    size_t         i;
    va_list        ap;

    if (n_args == 0)
        return NULL;

    va_start(ap, n_args);

    sb = str_builder_create();
    for (i=0; i<n_args; i++) {
        const_temp = va_arg(ap, const char *);
        if (str_isempty(const_temp))
            continue;

        str_builder_add_str(sb, const_temp);
        str_builder_add_char(sb, SEP);
    }

    if (str_builder_len(sb) == 0) {
        str_builder_destroy(sb);
        va_end(ap);
        return NULL;
    }

    /* SEP is always added after the last arg. Remove it because
     * it could have been a file. */
    str_builder_truncate(sb, str_builder_len(sb)-1);

    out = str_builder_dump(sb, NULL);
    str_builder_destroy(sb);

    va_end(ap);
    return out;
}

unsigned char *rw_read_file(const char *filename, size_t *len)
{
    FILE          *f;
    str_builder_t *sb;
    char          *out;
    char           temp[256];
    size_t         mylen;
    size_t         r;

    if (len == NULL)
        len = &mylen;
    *len = 0;

    if (str_isempty(filename) || len == NULL)
        return NULL;

    f = fopen(filename, "rb");
    if (f == NULL)
        return NULL;
    
    sb = str_builder_create();
    do {
        r = fread(temp, sizeof(*temp), sizeof(temp), f);
        str_builder_add_sub_str(sb, temp, r);
        *len += r;
    } while (r > 0);
    fclose(f);

    if (str_builder_len(sb) == 0) {
        str_builder_destroy(sb);
        return str_strdup_safe("");
    }

    out = str_builder_dump(sb, NULL);
    str_builder_destroy(sb);
    return (unsigned char *)out;
}

size_t rw_write_file(const char *filename, const unsigned char *data, size_t len, bool append)
{
    FILE          *f;
    const char    *mode  = "wb";
    size_t         wrote = 0;
    size_t         r     = 1;

    if (str_isempty(filename) || data == NULL)
        return 0;

    if (append)
        mode = "ab";

    f = fopen(filename, mode);
    if (f == NULL)
        return 0;

    while (r > 0 && len != 0) {
        r = fwrite(data, sizeof(*data), len, f);
        wrote += r;
        len   -= r;
        data  += r;
    }

    fclose(f);
    return wrote;
}

bool rw_create_dir(const char *name)
{
    str_builder_t  *sb;
    char          **parts;
    size_t          num_parts;
    size_t          i;
    bool            ret = true;

    if (str_isempty(name))
        return false;

    parts = str_split(name, strlen(name), SEP, &num_parts, 0);
    if (parts == NULL || num_parts == 0) {
        str_split_free(parts, num_parts);
        return false;
    }

    sb = str_builder_create();
    i  = 0;
#ifdef _WIN32
    /* If the first part has a ':' it's a drive. E.g 'C:'. We don't
     * want to try creating it because we can't. We'll add it to base
     * and move forward. The next part will be a directory we need
     * to try creating. */
    if (strchr(parts[0], ':')) {
        i++;
        str_builder_add_str(sb, parts[0]);
        str_builder_add_char(sb, SEP);
    }
#else
    if (*name == '/') {
        str_builder_add_char(sb, SEP);
    }
#endif

    for ( ; i<num_parts; i++) {
        if (str_isempty(parts[i])) {
            continue;
        }

        str_builder_add_str(sb, parts[i]);
        str_builder_add_char(sb, SEP);

#ifdef _WIN32
        if (CreateDirectory(str_builder_peek(sb), NULL) == FALSE) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                ret = false;
                goto done;
            }
        }
#else
        if (mkdir(str_builder_peek(sb), 0774) != 0)
            if (errno != EEXIST) {
                ret = false;
                goto done;
            }
#endif
    }

done:
    str_split_free(parts, num_parts);
    str_builder_destroy(sb);
    return ret;
}

bool rw_file_exists(const char *filename)
{
    FILE *f;

    f = fopen(filename, "rb+");
    if (f == NULL)
        return false;

    fclose(f);
    return true;
}

int64_t rw_file_size(const char *filename)
{
    FILE    *f;
    int64_t  size;

    f = fopen(filename, "rb");
    if (f == NULL)
        return -1;

    fseek(f, 0L, SEEK_END);
    size = ftell(f);

    fclose(f);
    return size;
}

bool rw_file_unlink(const char *filename)
{
#ifdef _WIN32
    return DeleteFile(filename)?true:false;
#else
    return unlink(filename)==0?true:false;
#endif
}

bool rw_rename(const char *cur_filename, const char *new_filename, bool overwrite)
{
    if (str_isempty(cur_filename) || str_isempty(new_filename))
        return false;

    if (!overwrite && rw_file_exists(new_filename))
        return false;

#ifdef _WIN32
    /* On Windows new_filename cannot exist. */
    if (rw_file_exists(new_filename)) {
        if (!DeleteFile(new_filename))
            return false;
    }

    if (MoveFile(cur_filename, new_filename)) {
#else
    /* POSIX will remove new_filename if it exists, then move cur_filename. */
    if (rename(cur_filename, new_filename) == 0) {
#endif
        return true;
    }

    return false;
}
