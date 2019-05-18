/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/limits.h>
#include <sys/param.h>

#include "fastboot.h"

static char ERROR[128];

char *fb_get_error(void)
{
    return ERROR;
}


size_t write_to_file(const void *ptr, size_t size, int outfile) {
  ssize_t written = 0;

  while(written < size) {
    written = write(outfile, ptr + written, size - written);
    if(written < 0) {
      return written;
    }
  }
  return 0;
}

static int check_response(usb_handle *usb, long unsigned size, 
                          unsigned data_okay, char *response, char* outfile)
{
    unsigned char status[4096];
    int r;
    int len;
    int is_disp = 0;
    unsigned long progress = 0;
    int outfilefd;
    unsigned long expected_size = 0;
    unsigned long expected_crc32 = 0;
    char outfile_crc32[PATH_MAX+1];

    for(;;) {
        len = 0;
        r = usb_read(usb, status, 4095);
        if(r < 0) {
            sprintf(ERROR, "status read failed (%s)\n", strerror(errno));
            usb_close(usb);
            return -1;
        }
        status[r] = 0;

        printf("Received %d bytes from device\n", r);
        
        if(expected_size > 0) {
          len = MIN(r, expected_size);
          if(write_to_file((void*) status, len, outfilefd)) {
            sprintf(ERROR, "Writing to file failed: %s\n", strerror(errno));
            close(outfilefd);
            return -1;
          }
          expected_size -= len;
          progress += len;

          printf("Progress: %lu of %lu received and written to disk\n", progress,expected_size);

          if(expected_size <= 0) {
            close(outfilefd);
            printf("Upload completed with %lu bytes written to file: %s\n", progress, outfile);

            expected_crc32 = 4;
            len = strnlen(outfile, PATH_MAX-6);
            strncpy(outfile_crc32, outfile, len);
            memcpy(outfile_crc32 + len, ".crc32\0", len + 7);

            printf("Waiting for CRC32\n");
            
            outfilefd = open(outfile_crc32, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
            if(outfilefd < 0) {
              sprintf(ERROR, "Opening crc32 file for writing failed: %s\n", strerror(errno));
              usb_close(usb);
              return -1;
            }
            continue;
          }
          continue;
        }

        if(expected_crc32 > 0) {
          if(write_to_file((void*) status + len, 4, outfilefd)) {
            sprintf(ERROR, "Writing to crc32 file failed: %s\n", strerror(errno));
            close(outfilefd);
            return -1;
          }
          close(outfilefd);
          return 0;
        }

        if(is_disp) {
          strcpy(response + progress, (char*) status);
          progress += r;
          if(r == 0 || status[r-1] == 0) {
            return 0;
          }
          continue;
        }

        if(r < 4) {
            sprintf(ERROR, "status malformed (%d bytes)", r);
            usb_close(usb);
            return -1;
        }

        if(!memcmp(status, "DISP", 4)) {
            strcpy(response, (char*) status + 4);
            is_disp = 1;
            progress = r - 4;
            continue;
        }

        if(!memcmp(status, "INFO", 4)) {
            fprintf(stderr,"%s\n", status + 4);
            continue;
        }

        if(!memcmp(status, "OKAY", 4)) {
            if(response) {
                strcpy(response, (char*) status + 4);
            }
            if(outfile) {
              close(outfilefd);
            }
            return 0;
        }

        if(!memcmp(status, "FAIL", 4)) {
            if(r > 4) {
                sprintf(ERROR, "remote: %s", status + 4);
            } else {
                strcpy(ERROR, "remote failure");
            }
            return -1;
        }

        if(!memcmp(status, "DATA", 4) && data_okay){
            unsigned dsize = strtoul((char*) status + 4, 0, 16);
            if(dsize > size) {
                strcpy(ERROR, "data size too large");
                usb_close(usb);
                return -1;
            }
            return dsize;
        }

        if(!memcmp(status, "ATAD", 4)) {
            outfilefd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
            if(outfilefd < 0) {
              sprintf(ERROR, "Opening file for writing failed: %s\n", strerror(errno));
              usb_close(usb);
              return -1;
            }

            expected_size = strtoul((char*) status + 4, NULL, 16);
            //            len = MIN(r, expected_size);
            
            if(write_to_file((void*) status+38, r-38, outfilefd)) {
              sprintf(ERROR, "Writing to file failed: %s\n", strerror(errno));
              close(outfilefd);
              usb_close(usb);
              return -1;
            }
            expected_size -= r-38;
            progress += r-38;
            continue;
        }

        strcpy(ERROR,"unknown status code");
        usb_close(usb);
        break;
    }

    if(outfile) {
      close(outfilefd);
    }
    
    return -1;
}

static int _command_send(usb_handle *usb, const char *cmd,
                         const void *data, unsigned size,
                         char *response, char* outfile)
{
    int cmdsize = strlen(cmd);
    int r;
    
    if(response) {
        response[0] = 0;
    }

    if(cmdsize > 64) {
        sprintf(ERROR,"command too large");
        return -1;
    }

    if(usb_write(usb, cmd, cmdsize) != cmdsize) {
        sprintf(ERROR,"command write failed (%s)", strerror(errno));
        usb_close(usb);
        return -1;
    }

    if(data == 0) {
      return check_response(usb, size, 0, response, outfile);
    }

    r = check_response(usb, size, 1, 0, outfile);
    if(r < 0) {
        return -1;
    }
    size = r;

    if(size) {
        r = usb_write(usb, data, size);
        if(r < 0) {
            sprintf(ERROR, "data transfer failure (%s)", strerror(errno));
            usb_close(usb);
            return -1;
        }
        if(r != ((int) size)) {
            sprintf(ERROR, "data transfer failure (short transfer)");
            usb_close(usb);
            return -1;
        }
    }

    printf("Waiting for response\n");
    r = check_response(usb, 0, 0, 0, outfile);
    if(r < 0) {
        return -1;
    } else {
        return size;
    }
}

int fb_command_upload(usb_handle *usb, const char *cmd, char* outfile)
{

  return _command_send(usb, cmd, 0, 0, 0, outfile);
}

int fb_command(usb_handle *usb, const char *cmd)
{
  return _command_send(usb, cmd, 0, 0, 0, 0);
}

int fb_command_response(usb_handle *usb, const char *cmd, char *response)
{
  return _command_send(usb, cmd, 0, 0, response, 0);
}

int fb_download_data(usb_handle *usb, const void *data, unsigned size)
{
    char cmd[64];
    int r;
    
    sprintf(cmd, "download:%08x", size);
    r = _command_send(usb, cmd, data, size, 0, 0);
    
    if(r < 0) {
        return -1;
    } else {
        return 0;
    }
}

