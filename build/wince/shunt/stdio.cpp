/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code, released
 * Jan 28, 2003.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Garrett Arch Blythe, 28-January-2003
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozce_internal.h"

#include <stdarg.h>

extern "C" {
#if 0
}
#endif


#define MAXFDS 100

typedef struct _fdtab_block
{
    int fd;
    FILE* file;
    
} _fdtab_block;


_fdtab_block _fdtab[MAXFDS];





void
_initfds()
{
    static int fdsinitialized = 0;
    
    if(fdsinitialized)
        return;
    
    for (int i = 0; i < MAXFDS; i++)
        _fdtab[i].fd = -1;
    
    fdsinitialized = 1;
}

int
_getnewfd()
{
    int i;
    
    for(i = 0; i < MAXFDS; i++)
    {
        if(_fdtab[i].fd == -1)
            return i;
    }
    
    return -1;
}


MOZCE_SHUNT_API int _waccess(const wchar_t *path, int mode)
{
    WINCE_LOG_API_CALL("-- _waccess called\n");
    
    return 0;
}


MOZCE_SHUNT_API int access(const char *path, int mode)
{
    WINCE_LOG_API_CALL("-- access called\n");
    
    return 0;
}

MOZCE_SHUNT_API void rewind(FILE* inStream)
{
    WINCE_LOG_API_CALL("rewind called\n");
    
    fseek(inStream, 0, SEEK_SET);
}


MOZCE_SHUNT_API FILE* fdopen(int fd, const char* inMode)
{
    WINCE_LOG_API_CALL("-- fdopen called (mode is ignored!) \n");
    
    if(fd < 0 || fd >= MAXFDS || _fdtab[fd].fd == -1)
        return 0;
    
    return _fdtab[fd].file;
}


MOZCE_SHUNT_API void perror(const char* inString)
{
    WINCE_LOG_API_CALL("perror called\n");
    
    fprintf(stderr, "%s", inString);
}


MOZCE_SHUNT_API int remove(const char* inPath)
{
    WINCE_LOG_API_CALL_1("remove called on %s\n", inPath);
    
    int retval = -1;
    
    if(NULL != inPath)
    {
        unsigned short wPath[MAX_PATH];
        
        if(0 != a2w_buffer(inPath, -1, wPath, sizeof(wPath) / sizeof(unsigned short)))
        {
            if(FALSE != DeleteFileW(wPath))
            {
                retval = 0;
                
            }
        }
    }
    
    return retval;
}

MOZCE_SHUNT_API int setmode(FILE*, int) 
{
    return 0;
}
MOZCE_SHUNT_API int _chdir (const char *dirname)
{
    return 0;
}
MOZCE_SHUNT_API int _wchdir (const wchar_t *dirname)
{
    return 0;
}
MOZCE_SHUNT_API char* getcwd(char* buff, size_t size)
{
    WINCE_LOG_API_CALL("getcwd called.\n");

    int i;
    unsigned short dir[MAX_PATH];
    GetModuleFileName(GetModuleHandle (NULL), dir, MAX_PATH);
    for (i = _tcslen(dir); i && dir[i] != TEXT('\\'); i--) {}
    dir[i + 1] = TCHAR('\0');
    
    w2a_buffer(dir, -1, buff, size);
    
    return buff;
}

MOZCE_SHUNT_API int mozce_printf(const char * format, ...)
{
    //#ifdef API_LOGGING
#define MAX_CHARS_IN_VARIABLE_STRING 1024
    
    char buf[MAX_CHARS_IN_VARIABLE_STRING];
    
    TCHAR tBuf[MAX_CHARS_IN_VARIABLE_STRING];
    
    va_list ptr;
    va_start(ptr,format);
    vsprintf(buf,format,ptr);
    
    mbstowcs(tBuf, buf, MAX_CHARS_IN_VARIABLE_STRING);
    
    OutputDebugString(tBuf);

#ifdef SHUNT_LOG_ENABLED
    mozce_DebugWriteToLog(buf);
#endif
    
    return 1;
    //#endif

    return 0;
}

static void mode2binstr(int mode, char* buffer)
{
    if (mode & O_RDWR || (mode & O_WRONLY))  // write only == read|write
    {
        if (mode & O_CREAT)
        {
            strcpy(buffer, "wb+");
        }
        else if (mode & O_APPEND)
        {
            strcpy(buffer, "ab+");
        }
        else if (mode & O_TRUNC)
        {
            strcpy(buffer, "wb+");
        }
        
        else if (mode == O_RDWR)
        {
            strcpy(buffer, "rb+");
        }
    }
    else if (mode & O_RDONLY)
    {
        strcpy(buffer, "rb");
    }
}

MOZCE_SHUNT_API int open(const char *pathname, int flags, int mode)
{
    WINCE_LOG_API_CALL("open called\n");
    
    _initfds();
    
    
    char modestr[10];
    *modestr = '\0';
    
    mode2binstr(mode, modestr);
    if (*modestr == '\0')
        return -1;
    
    
    FILE* file = fopen(pathname, modestr);
    
    int fd = -1;
    
    if (file)
    {
        fd = _getnewfd();
        
        _fdtab[fd].fd = fd;
        _fdtab[fd].file = file;
        
        fflush(file);
        fread(NULL, 0, 0, file);
    }
    
    return fd;
}


MOZCE_SHUNT_API int close(int fd)
{
    WINCE_LOG_API_CALL("close called\n");
    
    if(fd < 0 || fd >= MAXFDS || _fdtab[fd].fd == -1)
        return -1;
    
    fclose(_fdtab[fd].file);
    _fdtab[fd].fd = -1;
    
    return 0;
}

MOZCE_SHUNT_API size_t read(int fd, void* buffer, size_t count)
{
    WINCE_LOG_API_CALL("read called\n");
    
    if(fd < 0 || fd >= MAXFDS || _fdtab[fd].fd == -1)
        return -1;
    
    size_t num = fread(buffer, 1, count, _fdtab[fd].file);
    
    if (ferror(_fdtab[fd].file))
        return -1;
    
    return num;
}


MOZCE_SHUNT_API size_t write(int fd, const void* buffer, size_t count)
{
    WINCE_LOG_API_CALL("write called\n");
    
    if(fd < 0 || fd >= MAXFDS || _fdtab[fd].fd == -1)
        return -1;
    
    size_t num = fwrite(buffer, 1, count, _fdtab[fd].file);
    if (ferror(_fdtab[fd].file))
        return -1;
    
    return num;
}


MOZCE_SHUNT_API int unlink(const char *pathname)
{
    WINCE_LOG_API_CALL("unlink called\n");
    return remove(pathname);
}


MOZCE_SHUNT_API int lseek(int fd, int offset, int whence)
{
    WINCE_LOG_API_CALL("lseek called\n");
    
    if(fd < 0 || fd >= MAXFDS || _fdtab[fd].fd == -1)
        return -1;
    
    int newpos = -1;
    int error = fseek(_fdtab[fd].file, offset, whence);
    
    if (!error)
        newpos = ftell(_fdtab[fd].file);
    return newpos;
}


#if 0
{
#endif
} /* extern "C" */

