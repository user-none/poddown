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

#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>

#include <curl/curl.h>

#include "cast.h"
#include "downloader.h"
#include "settings.h"
#include "str_builder.h"
#include "str_helpers.h"
#include "rw_files.h"
#include "xml_helpers.h"
#include "xmem.h"

/* - - - - */

#define PD_USERAGENT "PodDown 1.0.0"

tpool_t *feed_pool    = NULL;
tpool_t *dlep_pool    = NULL;
time_t   lastdl       = 0;
bool     was_dl_error = false;

/* - - - - */

static CURL *generic_curl_base(const char *url)
{
    CURL *curl;

    curl = curl_easy_init();
    if (curl == NULL)
        return NULL;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
    /* Empty string means enable all encoding (compression) CURL supports. */
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 25);
    /* Ignore SIGPIPE signals when remote closes the connection unexpectedly. 
     * This is necessary for multi-thread applications like this one. */
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PD_USERAGENT);

    return curl;
}

static CURLcode do_download(const char *url, curl_write_callback wcb, void *thunk, int64_t resumesize, char *error, size_t errlen)
{
    CURL     *curl;
    CURLcode  res;
    /* CURL requires the error buffer to be at least it's
     * specified size. Instead of putting that requirement
     * on the caller we store the error in this buffer then
     * if an error variable was passed into the function we'll
     * fill it with the contents on this buffer on error. */
    char      myerror[CURL_ERROR_SIZE] = { 0 };

    curl = generic_curl_base(url);
    if (curl == NULL) {
        if (error != NULL && errlen > 0) {
            snprintf(error, errlen, "Failed to initialize CURL");
        }
        return CURLE_FAILED_INIT;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, wcb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, thunk);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, myerror);

    if (resumesize > 0)
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM, resumesize);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK && error != NULL && errlen > 0)
        snprintf(error, errlen, "%s", myerror);

    curl_easy_cleanup(curl);
    return res;
}

/* Save some bandwidth by checking if the file's changed.
 * If it hasn't there isn't any need to download it again. */
static bool url_has_changed(const char *url)
{
    CURL     *curl;
    CURLcode  res;
    /* Negative filetime is used to indicate an error in pulling the file time
     * which will be treated as the url has changed since last time. */
    long      filetime = -1;

    /* First run the lastdl time is 0. There is not need
     * to check if the url has changed because it's new to us. */
    if (lastdl == 0)
        return true;

    if (settings->ignore_last_modified)
        return true;

    curl = generic_curl_base(url);
    if (curl == NULL)
        return true;

    /* Check 304 last modified time to determine if we need to
     * download the file. */
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
    curl_easy_setopt(curl, CURLOPT_FILETIME, 1);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        return true;
    }

    res = curl_easy_getinfo(curl, CURLINFO_FILETIME, &filetime);
    if (res != CURLE_OK)
        filetime = -1;

    curl_easy_cleanup(curl);

    if (filetime > 0 && filetime < lastdl)
        return false;
    return true;
}

static int64_t remote_filesize(const char *url)
{
    CURL     *curl;
    CURLcode  res;
    long      filesize = -1;

    curl = generic_curl_base(url);
    if (curl == NULL)
        return -1;

    /* Disable accepting encoding (compression) because we want the real file
     * size. If this is set then the server *should* respond with the size of
     * the compressed data. We want the size of the uncompressed data. */
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, NULL); 
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        return -1;
    }

    res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
    if (res != CURLE_OK)
        filesize = -1;

    curl_easy_cleanup(curl);
    return filesize;
}

/* Callback for writing downloaded cast feed (XML) data to a buffer. */
static size_t feed_dl_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    str_builder_t *sb = userdata;
    str_builder_add_sub_str(sb, ptr, size*nmemb);
    return size*nmemb;
}

/* Callback for writing cast episode data to a file. */
static size_t episode_dl_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    FILE   *f = userdata;
    size_t  r;
    r = fwrite(ptr, size, nmemb, f);
    if (r == 0)
        return 0;
    return r;
}

/* Files will be downloaded with a ".part" extension and renamed
 * after a successful download. This way we always know what was
 * a partial download and what was a finished one. */
static void episode_dler(void *arg)
{
    cast_ep_t     *cast_ep                = arg;
    char          *filepath_final;
    char          *filepath_dl;
    char          *filename;
    str_builder_t *sb;
    FILE          *f;
    char           error[CURL_ERROR_SIZE] = { 0 };
    int64_t        filesize               = -1;
    int64_t        expectsize;
    CURLcode       res;
    bool           isresume               = false;
    bool           fail                   = false;

    /* Try to find the filename by pulling it
     * off after the last '/' */
    filename = strrchr(cast_ep_url(cast_ep), '/');
    /* Check if we found a '/'. If the name ends with a '/' then
     * filename+1 will be a NULL terminator. An empty filename
     * does us no good. */
    if (filename == NULL || filename+1 == '\0') {
        cast_ep_destory(cast_ep);
        return;
    }
    filename++;

    /* I've seen some bad casts update cast XML with
     * new times for old episodes. Typically, this happens
     * when the feed provider is changed but I've also seen
     * this with some casts that were just bad. Hopefully,
     * the file hasn't changed and we can use the last modified
     * time to determine if we really need to download it. */
    if (!url_has_changed(cast_ep_url(cast_ep))) {
        cast_ep_destory(cast_ep);
        return;
    }
    
    filepath_final = rw_join_path(3, settings->cast_dl_dir, cast_ep_prefix_path(cast_ep), filename);
    /* If we already have the file then we don't need to download anything. We don't
     * need to do any file size checks because the file will only be renamed to the
     * final name after a successful download and file size checks are performed. */
    if (rw_file_exists(filepath_final)) {
        xfree(filepath_final);
        cast_ep_destory(cast_ep);
        return;
    }

    /* Using a str_builder to add ".part" to the end of the file name isn't the most efficient...
     * but it is safe. I'd rather be safe in case the extension gets updated but the math
     * for the allocation isn't (or isn't updated properly). */
    sb = str_builder_create();
    str_builder_add_str(sb, filepath_final);
    str_builder_add_str(sb, ".part");
    filepath_dl = str_builder_dump(sb, NULL);
    str_builder_destroy(sb);

    /* See if we can get the expected file size so we can verify we have a full download. */
    expectsize = cast_ep_size(cast_ep);
    if (expectsize <= 0) {
        /* We don't know how large the file is because it wasn't in
         * the feed so try and get it from the server. */
        expectsize = remote_filesize(cast_ep_url(cast_ep));
    }

    /* If keep_partial is set enabled we'll try resuming the download if
     * the file exists. */
    if (settings->keep_partial) {
        /* We need the current file size to know where to resume from. */
        filesize = rw_file_size(filepath_dl);
        if (filesize > 0) {
            /* expectsize could be <= 0 because we weren't able to pull it.
             * We've already checked filesize > 0 so this check will always
             * cause a full download if we don't know the expected file size.
             *
             * We do still want to verify the file size and expect size are
             * sane if we have the expected size. Larger than, for example,
             * is a situation we should consider needing a new download. */
            if (filesize >= expectsize) {
                filesize = 0;
            } else {
                /* We have a partial file so let's try to resume downloading it. */
                isresume = true;
            }
        }
    }

    /* If we retry the download and we get a resume download error then, the
     * server doesn't support resuming a download. If this happens we'll try
     * downloading from scratch. */
    do {
        /* If we're resuming and there is a file (filesize will be > 0), then
         * open for append so we can resume. Otherwise, open for writing which
         * will truncate if the file already exists. */
        if (filesize > 0) {
            f        = fopen(filepath_dl, "ab");
        } else {
            f        = fopen(filepath_dl, "wb");
            isresume = false;
            filesize = 0;
        }
        if (f == NULL) {
            fprintf(stderr, "Could not %s file '%s'\n", isresume?"open":"create", filepath_dl);
            was_dl_error = true;
            /* Note: Don't try to delete a partial download file because chances are if the
             * file can't be opened/created the user can't delete it either. */
            xfree(filepath_dl);
            xfree(filepath_final);
            cast_ep_destory(cast_ep);
            return;
        }

        res = do_download(cast_ep_url(cast_ep), episode_dl_cb, f, filesize, error, sizeof(error));
        fclose(f);
    } while (isresume && res == CURLE_BAD_DOWNLOAD_RESUME);

    if (res != CURLE_OK) {
        fprintf(stderr, "Download '%s' Episode '%s' failed: %s\n", str_safe(cast_ep_castname(cast_ep)), filename, error);
        was_dl_error = true;
        fail         = true;
    } else {
        /* Try to verify we got a full download. */
        if (expectsize > 0) {
            filesize = rw_file_size(filepath_dl); 
            /* It's possible the expected file size was reported wrong and this
             * check will prevent the file from ever downloading. However, that
             * means the feed / server is badly broken and we shouldn't trust
             * we have a good file. We can only go off of what we're told by
             * the source. */
            if (filesize != expectsize) {
                /* Success? But we have a mismatch of what was downloaded vs the expected file size. */
                fprintf(stderr, "Download '%s' Episode '%s' failed: filesize (%" PRId64 ") %c expect size (%" PRId64 ")\n",
                        str_safe(cast_ep_castname(cast_ep)),
                        filename,
                        filesize,
                        filesize<expectsize?'<':'>',
                        expectsize);
                was_dl_error = true;
                fail         = true;
            }
        }
    }

    /* Delete the file if nothing was ever downloaded. Or if partial resumption
     * isn't enabled. Otherwise leave it so the next run can possibly retry the
     * download. */
    if (fail && (rw_file_size(filepath_dl) <= 0 || !settings->keep_partial)) {
        rw_file_unlink(filepath_dl);
    } else {
        /* Rename the download file to remove the ".part" extension. */
        rw_rename(filepath_dl, filepath_final, true);
    }

    xfree(filepath_dl);
    xfree(filepath_final);
    cast_ep_destory(cast_ep);
}

static time_t cast_get_pubdate(xmlDocPtr doc, xmlNodePtr node)
{
    char      *text;
    struct tm  tm;

    memset(&tm, 0, sizeof(tm));
    text = get_xml_text("./pubDate/text()", doc, node);
    if (text == NULL)
        return 0;

    if (strptime(text, "%a, %d %b %Y %H:%M:%S %z", &tm) == NULL) {
        xfree(text); 
        return 0;
    }

    xfree(text);
    return mktime(&tm);
}

static bool cast_parse_feed_cb(xmlDocPtr doc, xmlNodePtr node, void *arg)
{
    cast_t    *cast    = arg;
    cast_ep_t *cast_ep;
    char      *temp;
    time_t     pubdate = 0;

    if (lastdl > 0) {
        /* Check if this is older than our last download time
         * which indicates it was previously downloaded. */
        pubdate = cast_get_pubdate(doc, node);
        if (pubdate <= lastdl) {
            return false;
        }
    }

    /* Check explicit. */
    if (!cast_allow_explicit(cast)) {
        temp = get_xml_text("./*[local-name() = 'explicit']/text()", doc, node);
        if (strncasecmp(temp, "clean", strlen(str_safe(temp))) != 0) {
            xfree(temp);
            return true;
        }
        xfree(temp);
    }

    /* Get the cast url. */
    temp = get_xml_text("./enclosure/@url", doc, node);
    if (str_isempty(temp)) {
        fprintf(stderr, "Cast feed '%s' parse error: Couldn't find URL for episode\n", cast_name(cast));
        was_dl_error = true;
        xfree(temp);
        return true;
    }
    cast_ep = cast_ep_create(temp, cast_name(cast), cast_prefix_path(cast));
    xfree(temp);
    if (cast_ep == NULL) {
        /* Something went wrong, but it shouldn't be possible for something
         * to go wrong here. We'll skip this cast. */
        return true;
    }

    /* See if we can get the file size from the enclosure. */
    temp = get_xml_text("./enclosure/@length", doc, node);
    cast_ep_set_size(cast_ep, strtoll(temp, NULL, 10));
    xfree(temp);

    /* If we couldn't get the size from the enclosure try from the 'media:content' tag. */
    if (cast_ep_size(cast_ep) <= 0) {
        temp = get_xml_text("./*[local-name() = 'content']/@fileSize", doc, node);
        cast_ep_set_size(cast_ep, strtoll(temp, NULL, 10));
        xfree(temp);
    }

    /* Start the download. */
    tpool_add_work(dlep_pool, episode_dler, cast_ep);
    return true;
}

static void cast_parse_feed(cast_t *cast, const char *xml)
{
    char   exp[64];
    size_t pos;

    /* Only attempt to download up to the configured number of recent episodes.
     * Use XPath to select up to the max number of nodes to parse. Since they
     * should be in order from newest to oldest we can limit to only selecting
     * X nodes. Then once we hit a node with an episode older than our limit we
     * can stop.
     *
     * This is much easier than tracking how many episodes per cast have been
     * queued. */
    pos = settings->recent_num;
    /* First run without unlimited new episodes, only download a single episode. */
    if (pos == 0 && lastdl == 0)
        pos = 1;

    if (pos == 0) {
        snprintf(exp, sizeof(exp), "//channel/item");
    } else {
        /* XPath positions are 1 based not 0 based. */
        snprintf(exp, sizeof(exp), "//channel/item[position() <= %zu]", pos);
    }
    parse_nodes_int(xml, exp, cast_parse_feed_cb, cast);
    cast_destroy(cast);
}

static void cast_parse(void *arg)
{
    cast_t        *cast                   = arg;
    str_builder_t *sb;
    char          *xml;
    xmlNodePtr    *items;
    char           error[CURL_ERROR_SIZE] = { 0 };
    CURLcode       res;

    if (cast == NULL)
        return;

    if (!url_has_changed(cast_url(cast))) {
        cast_destroy(cast);
        return;
    }

    /* We might want to rework do_download to take a maximum download size in the future.
     * Right now we're going to store the feed in memory which should only be a few hundred
     * kb at most. However, if the feed URL is pointed to something bad like a 20 GB file
     * then we'll run into problems. Not going to worry about it right now since that's an
     * unlike scenario.
     *
     * Reading the cast.xml file from disk has the same problem but reading from disk vs
     * a remote sever where we don't control what could be there is a bit different. */
    sb = str_builder_create();

    res = do_download(cast_url(cast), feed_dl_cb, sb, -1, error, sizeof(error));
    if (res != CURLE_OK) {
        fprintf(stderr, "Could not download feed for '%s': %s\n", cast_name(cast), error);
        was_dl_error = true;
        str_builder_destroy(sb);
        cast_destroy(cast);
        return;
    }

    xml = str_builder_dump(sb, NULL);
    str_builder_destroy(sb);

    cast_parse_feed(cast, xml);
    xfree(xml);
}

static bool download_casts_cb(xmlDocPtr doc, xmlNodePtr node, void *arg)
{
    char   *text;
    cast_t *cast;
    bool    allow_explicit;

    (void)arg;

    text = get_xml_text("./url//text()", doc, node);
    if (str_isempty(text)) {
        xfree(text);
        text = get_xml_text("./name//text()", doc, node);
        fprintf(stderr, "Could not parse cast entry '%s': Missing URL\n", str_safe(text));
        was_dl_error = true;
        xfree(text);
        return true;
    }
    cast = cast_create(text);
    xfree(text);
    if (cast == NULL) {
        /* Shouldn't happen but be safe. */
        return true;
    }

    text = get_xml_text("./name//text()", doc, node);
    cast_set_name(cast, text);
    xfree(text);

    text = get_xml_text("./category//text()", doc, node);
    cast_set_category(cast, text);
    xfree(text);

    allow_explicit = settings->allow_explicit;
    text           = get_xml_text("./explicit//text()", doc, node);
    if (!str_isempty(text)) {
        allow_explicit = str_istrue(text);
    }
    cast_set_allow_explicit(cast, allow_explicit);
    xfree(text);

    text = rw_join_path(2, settings->cast_dl_dir, cast_prefix_path(cast));
    if (!rw_create_dir(text)) {
        fprintf(stderr, "Could not create or access directory '%s' to save cast '%s'\n", text, cast_name(cast));
        was_dl_error = true;
        xfree(text);
        cast_destroy(cast);
        return true;
    }
    xfree(text);

    tpool_add_work(feed_pool, cast_parse, cast);
    return true;
}

/* - - - - */

void download_casts(void)
{
    char *casts;

    casts = (char *)rw_read_file(settings->casts_xml_file, NULL);
    if (casts == NULL)
        return;

    parse_nodes_int(casts, "/casts//cast", download_casts_cb, NULL);
    xfree(casts);
}
