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

#ifndef __CAST_H__
#define __CAST_H__

#include <stdbool.h>

/* - - - - */

struct cast_s;
typedef struct cast_s cast_t;

struct cast_ep_s;
typedef struct cast_ep_s cast_ep_t;

/* - - - - */

cast_t *cast_create(const char *url);
void cast_destroy(cast_t *cast);

void cast_set_name(cast_t *cast, const char *name);
void cast_set_category(cast_t *cast, const char *category);
void cast_set_allow_explicit(cast_t *cast, bool allow);

const char *cast_url(const cast_t *cast);
const char *cast_name(const cast_t *cast);
const char *cast_category(const cast_t *cast);
bool cast_allow_explicit(const cast_t *cast);
const char *cast_prefix_path(const cast_t *cast);

/* - - - - */

cast_ep_t *cast_ep_create(const char *url, const char *castname, const char *path_prefix);
void cast_ep_destory(cast_ep_t *castep);

void cast_ep_set_size(cast_ep_t *castep, size_t len);

const char *cast_ep_url(const cast_ep_t *castep);
const char *cast_ep_castname(const cast_ep_t *castep);
const char *cast_ep_prefix_path(const cast_ep_t *castep);
size_t cast_ep_size(const cast_ep_t *castep);

#endif /* __CAST_H__ */
