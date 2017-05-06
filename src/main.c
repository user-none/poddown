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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>

#include "downloader.h"
#include "rw_files.h"
#include "settings.h"
#include "tpool.h"
#include "xmem.h"

/* We will get the start time when the app starts and use it
 * to update the lastdl time. We don't want a time gap between
 * downloading a feed and finishing. Prevents this situation.
 * - Start
 * - Download feed A
 * - Download eposiodes across all feeds
 * - Feed A has been updated with a new episode
 * - Finish all downloads and update time
 * The next run would not download the new episode that was
 * added to the feed in the middle of the last run. Since we
 * check modified times and pub times at worst we might end up
 * with an extra feed download here or there. */
static time_t start_time = 0;

static void get_last_download(void)
{
    char *out;
    long  lval;

    start_time = time(NULL);

    out = (char *)rw_read_file(settings->last_dl_file, NULL);
    if (out == NULL) {
        lastdl = 0;
        return;
    }

    lval = strtol(out, NULL, 10);
    xfree(out);
    if (lval < 0)
        lval = 0;

    lastdl = lval;
}

static void update_last_download(void)
{
    char   temp[32];
    size_t r;

    if (was_dl_error && !settings->update_lastdl_on_error)
        return;

    snprintf(temp, sizeof(temp), "%ld", start_time);
    r = rw_write_file(settings->last_dl_file, (unsigned char *)temp, strlen(temp), false);
    if (r == 0) {
        rw_file_unlink(settings->last_dl_file);
    }
}

static bool init(char *error, size_t errlen)
{
    if (!settings_load(error, errlen))
        return false;

    get_last_download();

    feed_pool = tpool_create(settings->feed_threads);
    dlep_pool = tpool_create(settings->dlep_threads);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    return true;
}

static void deinit(void)
{
    update_last_download();

    tpool_destroy(dlep_pool);
    tpool_destroy(feed_pool);
    settings_unload();

    curl_global_cleanup();
}

int main(int argc, char **argv)
{
    char error[256];

    if (!init(error, sizeof(error))) {
        fprintf(stderr, "INIT FAILED: %s\n", error);
        return 2;
    }

    download_casts();

    tpool_wait(feed_pool);
    tpool_wait(dlep_pool);

    deinit();

    return 0;
}
