/*
Copyright (c) 2017, Intel Corporation
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 
    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "logging.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

static bool prnt_irdr = false, prnt_net = false,
     prnt_jtagMsg = false, prnt_Debug = false;
static bool sg_b_writetosyslog = false;
static ShouldLogFunctionPtr shouldLogCallback = NULL;
static LogFunctionPtr loggingCallback = NULL;

#define CALLBACK_LOG_MESSAGE_LENGTH 256

bool ShouldLog(ASD_LogType log_type)
{
    bool log = false;
    if (log_type == LogType_IRDR && prnt_irdr) {
        log = true;
    } else if (log_type == LogType_NETWORK && prnt_net) {
        log = true;
    } else if (log_type == LogType_JTAG && prnt_jtagMsg) {
        log = true;
    } else if (log_type == LogType_Debug && prnt_Debug) {
        log = true;
    } else if (log_type == LogType_Error) {
        log = true;
    }
    return log;
}

void ASD_log(ASD_LogType log_type, const char *format, ...)
{
    bool no_remote = (log_type & LogType_NoRemote) == LogType_NoRemote;
    log_type &= ~LogType_NoRemote;
    bool local_log = ShouldLog(log_type);
    bool remoteLog = (shouldLogCallback && loggingCallback && !no_remote && shouldLogCallback(log_type));
    if (!local_log && !remoteLog)
        return;

    va_list args;
    va_start(args, format);
    if (local_log) {
        if (sg_b_writetosyslog) {
            vsyslog(LOG_USER, format, args);
        } else {
            vfprintf(stderr, format, args);
            fprintf(stderr, "\n");
        }
    }
    if (remoteLog) {
        char buffer[CALLBACK_LOG_MESSAGE_LENGTH];
        memset(buffer, '\0', CALLBACK_LOG_MESSAGE_LENGTH);
        vsnprintf(buffer, CALLBACK_LOG_MESSAGE_LENGTH, format, args);
        loggingCallback(log_type, buffer);
    }
    va_end(args);
}

void ASD_log_buffer(ASD_LogType log_type,
                    const unsigned char *ptr, size_t len,
                    const char *prefixPtr)
{
    const unsigned char *ubuf = ptr;
    unsigned int i = 0, l = 0;
    unsigned char *h;
    char line[256];
    static const char itoh[] = "0123456789abcdef";

    bool no_remote = (log_type & LogType_NoRemote) == LogType_NoRemote;
    log_type &= ~LogType_NoRemote;
    bool local_log = ShouldLog(log_type);
    bool remoteLog = (shouldLogCallback && loggingCallback && !no_remote && shouldLogCallback(log_type));
    if (!local_log && !remoteLog)
        return;

    /*  0         1         2         3         4         5         6
     *  0123456789012345678901234567890123456789012345678901234567890123456789
     *  PREFIX: 0000000: 0000 0000 0000 0000 0000 0000 0000 0000
     */
    while (i < len) {
        memset(line, '\0', sizeof(line));
        snprintf(line, sizeof(line), "%-6.6s: %07x: ", prefixPtr, i);
        h = (unsigned char *)&line[17];
        for (l = 0; l < 16 && (l + i) < len; l++) {
            *h++ = itoh[(*ubuf) >> 4];
            *h++ = itoh[(*ubuf) & 0xf];
            if (l & 1)
                *h++ = ' ';
            ubuf++;
        }
        if (remoteLog) {
            loggingCallback(log_type, line);
        }
        *h++ = '\n';
        i += l;
        if (local_log) {
            if (sg_b_writetosyslog)
                syslog(LOG_USER, "%s", line);
            else
                fprintf(stderr, "%s", line);
        }
    }
}

void ASD_initialize_log_settings(ASD_LogType type, bool b_writetosyslog,
                                 ShouldLogFunctionPtr should_log_ptr, LogFunctionPtr log_ptr)
{
    sg_b_writetosyslog = b_writetosyslog;
    shouldLogCallback = should_log_ptr;
    loggingCallback = log_ptr;

    prnt_irdr = false;
    prnt_net = false;
    prnt_jtagMsg = false;
    prnt_Debug = false;

    switch (type) {
        case LogType_IRDR: {
            prnt_irdr = true;
            fprintf(stderr, "IR/DR messages are enabled\n");
            break;
        }
        case LogType_NETWORK: {
            prnt_net = true;
            fprintf(stderr, "Network response messages are enabled\n");
            break;
        }
        case LogType_JTAG: {
            prnt_jtagMsg = true;
            fprintf(stderr, "JTAG packet messages are enabled\n");
            break;
        }
        case LogType_All: {
            prnt_jtagMsg = true;
            prnt_irdr = true;
            prnt_net = true;
            prnt_Debug = true;
            fprintf(stderr, "IR/DR messages are enabled\n");
            fprintf(stderr, "Network response messages are enabled\n");
            fprintf(stderr, "JTAG packet messages are enabled\n");
            fprintf(stderr, "Debug messages are enabled\n");
            break;
        }
        case LogType_Debug: {
            prnt_Debug = true;
            fprintf(stderr, "Debug messages are enabled\n");
            break;
        }
        case LogType_Error:
        case LogType_None:
            break;
        case LogType_MIN:
        case LogType_MAX:
        case LogType_NoRemote: {
            fprintf(stderr, "Invalid log setting\n");
            break;
        }
    }
}
