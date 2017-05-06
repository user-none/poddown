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

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdbool.h>
#include <time.h>

/* - - - - */

typedef struct {
    char   *casts_xml_file;
    char   *cast_dl_dir;
    char   *last_dl_file;
    bool    allow_explicit;
    bool    keep_partial;
    bool    ignore_last_modified;
    bool    update_lastdl_on_error;
    size_t  recent_num;
    size_t  feed_threads;
    size_t  dlep_threads;
} settings_t;

/* - - - - */

extern settings_t *settings;

/* - - - - */

bool settings_load(char *error, size_t errlen);
void settings_unload(void);

#endif /* __SETTINGS_H__ */
