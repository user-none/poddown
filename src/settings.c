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

#include "cpthread.h"
#include "rw_files.h"
#include "settings.h"
#include "str_helpers.h"
#include "xmem.h"
#include "xml_helpers.h"

#ifdef __APPLE__
#  include "settings_mac.h"
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#  include <sys/types.h>
#  include <pwd.h>
#  include <uuid/uuid.h>
#  include <unistd.h>
#endif

/* - - - - */

const char *settings_filename = "poddown.xml";
settings_t *settings          = NULL;

/* - - - - */

static char *settings_get_dir(void)
{
    char *path;
    char *d;

#if defined(_WIN32)
    TCHAR home[MAX_PATH];
    size_t len;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, NULL, &home) != S_OK) {
        return NULL;
    }
    wcstombs(NULL, home, &len);
    if (len == -1) {
        CoTaskMemFree(home);
        return NULL;
    }
    path = xcalloc(1, len+1);
    wcstombs(path, home, &len);
    path[len] = '\0';
    CoTaskMemFree(home);
#elif defined(__APPLE__)
    path = settings_get_dir_mac();
#else
    char *home;
    bool  iscfg = true;
    /* Try a variety of different ways the home / user's config dir could be set.
     * Note: 'home' is not allocated so do not free it! */
    home = getenv("XDG_CONFIG_HOME");
    if (str_isempty(home)) {
        iscfg = false;
        home = getenv("HOME");
        if (str_isempty(home)) {
            home = getpwuid(getuid())->pw_dir;
        }
    }
    if (str_isempty(home)) {
        return NULL;
    }
    if (iscfg) {
        path = strdup(home);
    } else {
        path = rw_join_path(2, home, ".config");
    }
#endif

    d = rw_join_path(2, path, "poddown");
    xfree(path);
    return d;
}

bool settings_load(char *error, size_t errlen)
{
    char      *sxml = NULL;
    char      *text;
    char      *path;
    xmlDocPtr  doc  = NULL;
    char       myerror[4];
    bool       ret  = true;
    long       lval;

    if (error == NULL || errlen == 0) {
        error  = myerror;
        errlen = sizeof(myerror);
    }

    settings = xcalloc(1, sizeof(*settings));

    path = settings_get_dir();
    if (str_isempty(path)) {
        snprintf(error, errlen, "Could not determine configuration directory");
        goto error;
    }

    settings->last_dl_file = rw_join_path(2, path, "lastdl");

    text = rw_join_path(2, path, "settings.xml");
    sxml = (char *)rw_read_file(text, NULL);
    if (str_isempty(sxml)) {
        snprintf(error, errlen, "Could not read settings file: '%s'", text);
        xfree(text);
        goto error;
    }
    xfree(text);

    doc = xmlParseMemory(sxml, strlen(sxml));
    if (doc == NULL) {
        snprintf(error, errlen, "Failed to parse settings xml");
        goto error;
    }

    text = get_xml_text("/poddown/location/cast_dir", doc, NULL);
    if (str_isempty(text)) {
        snprintf(error, errlen, "Cast download dir not specified");
        xfree(text);
        goto error;
    }
    settings->cast_dl_dir = text;

    text = get_xml_text("/poddown/location/cast_list", doc, NULL);
    if (str_isempty(text)) {
        snprintf(error, errlen, "Cast xml list not specified");
        xfree(text);
        goto error;
    }
    settings->casts_xml_file = text;

    text = get_xml_text("/poddown/download/recent", doc, NULL);
    lval = strtoll(str_safe(text), NULL, 10);
    xfree(text);
    if (lval < 0)
        lval = 0;
    settings->recent_num = lval;

    text = get_xml_text("/poddown/download/ignore_last_modified", doc, NULL);
    settings->ignore_last_modified = false;
    if (!str_isempty(text))
        settings->ignore_last_modified = str_istrue(text);
    xfree(text);

    text = get_xml_text("/poddown/download/keep_partial", doc, NULL);
    settings->keep_partial = true;
    if (!str_isempty(text))
        settings->keep_partial = str_istrue(text);
    xfree(text);

    text = get_xml_text("/poddown/download/allow_explicit", doc, NULL);
    settings->allow_explicit = true;
    if (!str_isempty(text))
        settings->allow_explicit = str_istrue(text);
    xfree(text);

    text = get_xml_text("/poddown/tuning/feed_threads", doc, NULL);
    lval = strtoll(str_safe(text), NULL, 10);
    xfree(text);
    if (lval <= 0)
        lval = (cpthread_get_num_procs()/2)+1;
    settings->feed_threads = lval;

    text = get_xml_text("/poddown/tuning/download_threads", doc, NULL);
    lval = strtoll(str_safe(text), NULL, 10);
    xfree(text);
    if (lval <= 0)
        lval = cpthread_get_num_procs()+1;
    settings->dlep_threads = lval;

    text = get_xml_text("/poddown/tuning/update_lastdl_on_error", doc, NULL);
    settings->update_lastdl_on_error = true;
    if (!str_isempty(text))
        settings->update_lastdl_on_error = str_istrue(text);
    xfree(text);


    goto done;

error:
    ret = false;
    settings_unload();
done:
    if (doc != NULL)
        xmlFreeDoc(doc);
    xfree(sxml);
    xfree(path);
    return ret;
}

void settings_unload(void)
{
    if (settings == NULL)
        return;

    xfree(settings->casts_xml_file);
    xfree(settings->cast_dl_dir);
    xfree(settings->last_dl_file);

    xfree(settings);
    settings = NULL;
}
