/*
 * This code provides a glue layer between PhysicsFS and Simple Directmedia
 *  Layer's (SDL) IOStream i/o abstraction.
 *
 * License: this code is public domain. I make no warranty that it is useful,
 *  correct, harmless, or environmentally safe.
 *
 * This particular file may be used however you like, including copying it
 *  verbatim into a closed-source project, exploiting it commercially, and
 *  removing any trace of my name from the source (although I hope you won't
 *  do that). I welcome enhancements and corrections to this file, but I do
 *  not require you to send me patches if you make changes. This code has
 *  NO WARRANTY.
 *
 * Unless otherwise stated, the rest of PhysicsFS falls under the zlib license.
 *  Please see LICENSE.txt in the root of the source tree.
 *
 * SDL 3 is zlib, like PhysicsFS.
 *  You can get SDL at https://www.libsdl.org/
 *
 *  This file was originally written by Ryan C. Gordon. (icculus@icculus.org)
 *  and then ported from SDL1/2's SDL_RWops to SDL3's SDL_IOStream.
 */

#include "physfssdliostream.h"

static Sint64 SDLCALL physfssdliostream_size(void *userdata)
{
    PHYSFS_File *handle = (PHYSFS_File *) userdata;
    return (Sint64) PHYSFS_fileLength(handle);
} /* physfsrwops_size */


static Sint64 SDLCALL physfssdliostream_seek(void *userdata, Sint64 offset, int whence)
{
    PHYSFS_File *handle = (PHYSFS_File *) userdata;
    PHYSFS_sint64 pos = 0;

    if (whence == SDL_IO_SEEK_SET)
        pos = (PHYSFS_sint64) offset;

    else if (whence == SDL_IO_SEEK_CUR)
    {
        const PHYSFS_sint64 current = PHYSFS_tell(handle);
        if (current == -1)
        {
            SDL_SetError("Can't find position in file: %s",
                          PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            return -1;
        } /* if */

        if (offset == 0)  /* this is a "tell" call. We're done. */
        {
            return (Sint64) current;
        } /* if */

        pos = current + ((PHYSFS_sint64) offset);
    } /* else if */

    else if (whence == SDL_IO_SEEK_END)
    {
        const PHYSFS_sint64 len = PHYSFS_fileLength(handle);
        if (len == -1)
        {
            SDL_SetError("Can't find end of file: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
            return -1;
        } /* if */

        pos = len + ((PHYSFS_sint64) offset);
    } /* else if */

    else
    {
        SDL_SetError("Invalid 'whence' parameter.");
        return -1;
    } /* else */

    if ( pos < 0 )
    {
        SDL_SetError("Attempt to seek past start of file.");
        return -1;
    } /* if */

    if (!PHYSFS_seek(handle, (PHYSFS_uint64) pos))
    {
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return -1;
    } /* if */

    return (Sint64) pos;
} /* physfssdliostream_seek */


static size_t SDLCALL physfssdliostream_read(void *userdata, void *ptr,
                                       size_t size, SDL_IOStatus *status)
{
    PHYSFS_File *handle = (PHYSFS_File *) userdata;
    const PHYSFS_uint64 readlen = (PHYSFS_uint64) (size);
    const PHYSFS_sint64 rc = PHYSFS_readBytes(handle, ptr, readlen);
    if (rc != ((PHYSFS_sint64) readlen))
    {
        if (!PHYSFS_eof(handle)) /* not EOF? Must be an error. */
        {
            SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));

            return 0;
        } /* if */
    } /* if */

    return (size_t) rc;
} /* physfssdliostream_read */


static size_t SDLCALL physfssdliostream_write(void *userdata, const void *ptr,
                                        size_t size, SDL_IOStatus *status)
{
    PHYSFS_File *handle = (PHYSFS_File *) userdata;
    const PHYSFS_uint64 writelen = (PHYSFS_uint64) (size);
    const PHYSFS_sint64 rc = PHYSFS_writeBytes(handle, ptr, writelen);
    if (rc != ((PHYSFS_sint64) writelen))
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));

    return (size_t) rc;
} /* physfssdliostream_write */


static int physfssdliostream_close(void *userdata)
{
    PHYSFS_File *handle = (PHYSFS_File *) userdata;
    if (!PHYSFS_close(handle))
    {
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return -1;
    } /* if */

    return 0;
} /* physfssdliostream_close */


static SDL_IOStream *create_sdliostream(PHYSFS_File *handle)
{
    SDL_IOStream *retval = NULL;

    if (handle == NULL)
        SDL_SetError("PhysicsFS error: %s", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    else
    {
        SDL_IOStreamInterface iface;
        SDL_zero(iface);
        iface.size = physfssdliostream_size;
        iface.seek = physfssdliostream_seek;
        iface.read = physfssdliostream_read;
        iface.write = physfssdliostream_write;
        iface.close = physfssdliostream_close;

        retval = SDL_OpenIO(&iface, handle);
    } /* else */

    return retval;
} /* create_sdliostream */


SDL_IOStream *PHYSFSSDLIOSTREAM_makeIOStream(PHYSFS_File *handle)
{
    SDL_IOStream *retval = NULL;
    if (handle == NULL)
        SDL_SetError("NULL pointer passed to PHYSFSSDLIOSTREAM_makeIOStream().");
    else
        retval = create_sdliostream(handle);

    return retval;
} /* PHYSFSSDLIOSTREAM_makeIOStream */


SDL_IOStream *PHYSFSSDLIOSTREAM_openRead(const char *fname)
{
    return create_sdliostream(PHYSFS_openRead(fname));
} /* PHYSFSSDLIOSTREAM_openRead */


SDL_IOStream *PHYSFSSDLIOSTREAM_openWrite(const char *fname)
{
    return create_sdliostream(PHYSFS_openWrite(fname));
} /* PHYSFSSDLIOSTREAM_openWrite */


SDL_IOStream *PHYSFSSDLIOSTREAM_openAppend(const char *fname)
{
    return create_sdliostream(PHYSFS_openAppend(fname));
} /* PHYSFSSDLIOSTREAM_openAppend */


/* end of physfssdliostream.c ... */

