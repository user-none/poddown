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

#ifndef __RW_FILES_H__
#define __RW_FILES_H__

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

/*! \addtogroup rw_files File helpers
 *
 * A variety of helper functions for working with files and file paths.
 *
 * @{
 */

/*! Join parts into a path for the current system.
 *
 * \param[in] n_args The number of parts that are specified as var args.
 * \param[in] ...    Strings of path parts to join.
 *
 * \return Joined path.
 */
char *rw_join_path(size_t n_args, ...);

/*! Read a file.
 *
 * \param[in]  filename Path to and name of a file to read.
 * \param[out] len      Length of the data read.
 *
 * \return Data read from file. If the file exists and is empty
 *         an empty string ("") is returned. On error NULL is returned.
 */
unsigned char *rw_read_file(const char *filename, size_t *len);

/*! Write data to a file.
 *
 * Will create the file if it does not exist.
 *
 * \param[in] filename Path to and name of a file to read.
 * \paran[in] data     Data to write to the file.
 * \param[in] len      Length of data to write.
 * \param[in] append   Should the data be appened to the file if it already exists.
 *
 * \return Number of bytes written to the file.
 */
size_t rw_write_file(const char *filename, const unsigned char *data, size_t len, bool append);

/*! Create a directory on disk.
 *
 * A relative path will create relative to cwd.
 *
 * \param[in] name Directory path and name to create.
 *
 * \return true on success, otherwise false.
 */
bool rw_create_dir(const char *name);

/*! Check if a file exists.
 *
 * \param[in] filename File path and name. Can be relative.
 *
 * \return true on success, otherwise false.
 */
bool rw_file_exists(const char *filename);

/*! Get the size of a file.
 *
 * \param[in] filename File path and name. Can be relative.
 *
 * \return Size of file. -1 if file does not exist.
 */
int64_t rw_file_size(const char *filename);

/*! Delete a file.
 *
 * \param[in] filename File path and name. Can be relative.
 *
 * \return true on success, otherwise false.
 */
bool rw_file_unlink(const char *filename);

/*! Rename a file.
 *
 * \param[in] cur_filename File path and name to rename.
 * \param[in] new_filename New file path and name.
 * \param[in] overwrite    If new_filename exists should it be overwritten.
 *                         If false and new_filename this will fail.
 *
 * \return true on success, otherwise false.
 */
bool rw_rename(const char *cur_filename, const char *new_filename, bool overwrite);

#endif /* __RW_FILES_H__ */
