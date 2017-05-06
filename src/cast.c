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

#include <stdlib.h>
#include <string.h>

#include "cast.h"
#include "rw_files.h"
#include "str_helpers.h"
#include "xmem.h"

/* - - - - */

struct cast_s {
    char *name;
    char *category;
    char *url;
    char *prefix_path;
    bool  allow_explicit;
};

struct cast_ep_s {
    char   *url;
    char   *castname;
    char   *path_prefix;
    size_t  len;
};

/* - - - - */

static void cast_update_prefix_path(cast_t *cast)
{
    if (cast == NULL)
        return;

    xfree(cast->prefix_path);
    cast->prefix_path = NULL;

    if (str_isempty(cast->name) && str_isempty(cast->category))
        return;

    if (str_isempty(cast->name)) {
        cast->prefix_path = str_strdup_safe(cast->category);
    } else if (str_isempty(cast->category)) {
        cast->prefix_path = str_strdup_safe(cast->name);
    } else {
        cast->prefix_path = rw_join_path(2, cast->category, cast->name);
    }
}

/* - - - - */

cast_t *cast_create(const char *url)
{
    cast_t *cast;

    if (str_isempty(url))
        return NULL;

    cast                 = xcalloc(1, sizeof(*cast));
    cast->url            = str_strdup_safe(url);
    cast->allow_explicit = true;

    return cast;
}

void cast_destroy(cast_t *cast)
{
    if (cast == NULL)
        return;

    xfree(cast->name);
    xfree(cast->category);
    xfree(cast->url);
    xfree(cast->prefix_path);

    free(cast);
}

void cast_set_name(cast_t *cast, const char *name)
{
    if (cast == NULL)
        return;

    xfree(cast->name);
    cast->name = NULL;
   
    if (!str_isempty(name))
        cast->name = str_strdup_safe(name);

    cast_update_prefix_path(cast);
}

void cast_set_category(cast_t *cast, const char *category)
{
    if (cast == NULL)
        return;

    xfree(cast->category);
    cast->category = NULL;
   
    if (!str_isempty(category))
        cast->category = str_strdup_safe(category);

    cast_update_prefix_path(cast);
}

void cast_set_allow_explicit(cast_t *cast, bool allow)
{
    if (cast == NULL)
        return;
    cast->allow_explicit = allow;
}

const char *cast_url(const cast_t *cast)
{
    if (cast == NULL)
        return NULL;
    return cast->url;
}

const char *cast_name(const cast_t *cast)
{
    if (cast == NULL)
        return NULL;
    return cast->name;
}

const char *cast_category(const cast_t *cast)
{
    if (cast == NULL)
        return NULL;
    return cast->category;
}

bool cast_allow_explicit(const cast_t *cast)
{
    if (cast == NULL)
        return NULL;
    return cast->allow_explicit;
}

const char *cast_prefix_path(const cast_t *cast)
{
    if (cast == NULL)
        return NULL;
    return cast->prefix_path;
}

/* - - - - */

cast_ep_t *cast_ep_create(const char *url, const char *castname, const char *path_prefix)
{
    cast_ep_t *castep;

    if (str_isempty(url))
        return NULL;

    castep      = xcalloc(1, sizeof(*castep));
    castep->url = str_strdup_safe(url);

    if (!str_isempty(castname))
        castep->castname = strdup(castname);

    if (!str_isempty(path_prefix))
        castep->path_prefix = strdup(path_prefix);

    return castep;
}

void cast_ep_destory(cast_ep_t *castep)
{
    if (castep == NULL)
        return;

    xfree(castep->url);
    xfree(castep->castname);
    xfree(castep->path_prefix);

    free(castep);
}

void cast_ep_set_castname(cast_ep_t *castep, const char *castname)
{
    if (castep == NULL || str_isempty(castname))
        return;

    xfree(castep->castname);
    castep->castname = strdup(castname);
}

void cast_ep_set_size(cast_ep_t *castep, size_t len)
{
    if (castep == NULL || len == 0)
        return;
    castep->len = len;
}

const char *cast_ep_url(const cast_ep_t *castep)
{
    if (castep == NULL)
        return NULL;
    return castep->url;
}

const char *cast_ep_castname(const cast_ep_t *castep)
{
    if (castep == NULL)
        return NULL;
    return castep->castname;
}

const char *cast_ep_prefix_path(const cast_ep_t *castep)
{
    if (castep == NULL)
        return NULL;
    return castep->path_prefix;
}

size_t cast_ep_size(const cast_ep_t *castep)
{
    if (castep == NULL)
        return 0;
    return castep->len;
}
