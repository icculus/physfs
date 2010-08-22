/* Metadata to generate the scripting language bindings. Please ignore. */
%module physfs

%{
#include "physfs.h"
%}

/* I _think_ this is safe for now. */
%warnfilter(451) PHYSFS_ArchiveInfo;

%ignore _INCLUDE_PHYSFS_H_;  /* ignore the include-once blocker. */
%ignore PHYSFS_DECL;  /* ignore the export define. */
%ignore PHYSFS_CALL;  /* ignore the calling conventions define. */
%ignore PHYSFS_DEPRECATED;  /* ignore the deprecation define. */
%ignore PHYSFS_file;  /* legacy type define. */

/* Some bindings put everything in a namespace, so we don't need PHYSFS_ */
#if defined(SWIGPERL) || defined(SWIGRUBY)
%rename(File) PHYSFS_File;
%rename(Version) PHYSFS_Version;
%rename(ArchiveInfo) PHYSFS_ArchiveInfo;
%rename(getLinkedVersion) PHYSFS_getLinkedVersion;
%rename(init) PHYSFS_init;
%rename(deinit) PHYSFS_deinit;
%rename(supportedArchiveTypes) PHYSFS_supportedArchiveTypes;
%rename(freeList) PHYSFS_freeList;
%rename(getLastError) PHYSFS_getLastError;
%rename(getDirSeparator) PHYSFS_getDirSeparator;
%rename(permitSymbolicLinks) PHYSFS_permitSymbolicLinks;
%rename(getCdRomDirs) PHYSFS_getCdRomDirs;
%rename(getBaseDir) PHYSFS_getBaseDir;
%rename(getUserDir) PHYSFS_getUserDir;
%rename(getWriteDir) PHYSFS_getWriteDir;
%rename(setWriteDir) PHYSFS_setWriteDir;
%rename(addToSearchPath) PHYSFS_addToSearchPath;
%rename(removeFromSearchPath) PHYSFS_removeFromSearchPath;
%rename(getSearchPath) PHYSFS_getSearchPath;
%rename(setSaneConfig) PHYSFS_setSaneConfig;
%rename(mkdir) PHYSFS_mkdir;
%rename(delete) PHYSFS_delete;
%rename(getRealDir) PHYSFS_getRealDir;
%rename(enumerateFiles) PHYSFS_enumerateFiles;
%rename(exists) PHYSFS_exists;
%rename(isDirectory) PHYSFS_isDirectory;
%rename(isSymbolicLink) PHYSFS_isSymbolicLink;
%rename(getLastModTime) PHYSFS_getLastModTime;
%rename(openWrite) PHYSFS_openWrite;
%rename(openAppend) PHYSFS_openAppend;
%rename(openRead) PHYSFS_openRead;
%rename(close) PHYSFS_close;
%rename(read) PHYSFS_read;
%rename(write) PHYSFS_write;
%rename(eof) PHYSFS_eof;
%rename(tell) PHYSFS_tell;
%rename(seek) PHYSFS_seek;
%rename(fileLength) PHYSFS_fileLength;
%rename(setBuffer) PHYSFS_setBuffer;
%rename(flush) PHYSFS_flush;
%rename(readSLE16) PHYSFS_readSLE16;
%rename(readULE16) PHYSFS_readULE16;
%rename(readSBE16) PHYSFS_readSBE16;
%rename(readUBE16) PHYSFS_readUBE16;
%rename(readSLE32) PHYSFS_readSLE32;
%rename(readULE32) PHYSFS_readULE32;
%rename(readSBE32) PHYSFS_readSBE32;
%rename(readUBE32) PHYSFS_readUBE32;
%rename(readSLE64) PHYSFS_readSLE64;
%rename(readULE64) PHYSFS_readULE64;
%rename(readSBE64) PHYSFS_readSBE64;
%rename(readUBE64) PHYSFS_readUBE64;
%rename(writeSLE16) PHYSFS_writeSLE16;
%rename(writeULE16) PHYSFS_writeULE16;
%rename(writeSBE16) PHYSFS_writeSBE16;
%rename(writeUBE16) PHYSFS_writeUBE16;
%rename(writeSLE32) PHYSFS_writeSLE32;
%rename(writeULE32) PHYSFS_writeULE32;
%rename(writeSBE32) PHYSFS_writeSBE32;
%rename(writeUBE32) PHYSFS_writeUBE32;
%rename(writeSLE64) PHYSFS_writeSLE64;
%rename(writeULE64) PHYSFS_writeULE64;
%rename(writeSBE64) PHYSFS_writeSBE64;
%rename(writeUBE64) PHYSFS_writeUBE64;
%rename(isInit) PHYSFS_isInit;
%rename(symbolicLinksPermitted) PHYSFS_symbolicLinksPermitted;
%rename(mount) PHYSFS_mount;
%rename(getMountPoint) PHYSFS_getMountPoint;
%rename(Stat) PHYSFS_Stat;   /* !!! FIXME: case insensitive script languages? */
%rename(stat) PHYSFS_stat;
%rename(readBytes) PHYSFS_readBytes;
%rename(writeBytes) PHYSFS_writeBytes;
%rename(unmount) PHYSFS_unmount;
#endif

%include "../src/physfs.h"

