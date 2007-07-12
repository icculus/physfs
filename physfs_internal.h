/*
 * Internal function/structure declaration. Do NOT include in your
 *  application.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#ifndef _INCLUDE_PHYSFS_INTERNAL_H_
#define _INCLUDE_PHYSFS_INTERNAL_H_

#ifndef __PHYSICSFS_INTERNAL__
#error Do not include this header from your applications.
#endif

#include "physfs.h"

#ifdef HAVE_ASSERT_H
#include <assert.h>
#elif (!defined assert)
#define assert(x)
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* The LANG section. */
/*  please send questions/translations to Ryan: icculus@icculus.org. */

#if (!defined PHYSFS_LANG)
#  define PHYSFS_LANG PHYSFS_LANG_ENGLISH
#endif

#define PHYSFS_LANG_ENGLISH            1  /* English by Ryan C. Gordon  */
#define PHYSFS_LANG_RUSSIAN_KOI8_R     2  /* Russian by Ed Sinjiashvili */
#define PHYSFS_LANG_RUSSIAN_CP1251     3  /* Russian by Ed Sinjiashvili */
#define PHYSFS_LANG_RUSSIAN_CP866      4  /* Russian by Ed Sinjiashvili */
#define PHYSFS_LANG_RUSSIAN_ISO_8859_5 5  /* Russian by Ed Sinjiashvili */
#define PHYSFS_LANG_SPANISH            6  /* Spanish by Pedro J. Pérez  */
#define PHYSFS_LANG_FRENCH             7  /*  French by Stéphane Peter  */
#define PHYSFS_LANG_GERMAN             8  /*  German by Michael Renner  */
#define PHYSFS_LANG_PORTUGUESE_BR      9  /* pt-br by Danny Angelo Carminati Grein  */

#if (PHYSFS_LANG == PHYSFS_LANG_ENGLISH)
 #define DIR_ARCHIVE_DESCRIPTION  "Non-archive, direct filesystem I/O"
 #define GRP_ARCHIVE_DESCRIPTION  "Build engine Groupfile format"
 #define HOG_ARCHIVE_DESCRIPTION  "Descent I/II HOG file format"
 #define MVL_ARCHIVE_DESCRIPTION  "Descent II Movielib format"
 #define QPAK_ARCHIVE_DESCRIPTION "Quake I/II format"
 #define ZIP_ARCHIVE_DESCRIPTION  "PkZip/WinZip/Info-Zip compatible"
 #define WAD_ARCHIVE_DESCRIPTION  "DOOM engine format"

 #define ERR_IS_INITIALIZED       "Already initialized"
 #define ERR_NOT_INITIALIZED      "Not initialized"
 #define ERR_INVALID_ARGUMENT     "Invalid argument"
 #define ERR_FILES_STILL_OPEN     "Files still open"
 #define ERR_NO_DIR_CREATE        "Failed to create directories"
 #define ERR_OUT_OF_MEMORY        "Out of memory"
 #define ERR_NOT_IN_SEARCH_PATH   "No such entry in search path"
 #define ERR_NOT_SUPPORTED        "Operation not supported"
 #define ERR_UNSUPPORTED_ARCHIVE  "Archive type unsupported"
 #define ERR_NOT_A_HANDLE         "Not a file handle"
 #define ERR_INSECURE_FNAME       "Insecure filename"
 #define ERR_SYMLINK_DISALLOWED   "Symbolic links are disabled"
 #define ERR_NO_WRITE_DIR         "Write directory is not set"
 #define ERR_NO_SUCH_FILE         "File not found"
 #define ERR_NO_SUCH_PATH         "Path not found"
 #define ERR_NO_SUCH_VOLUME       "Volume not found"
 #define ERR_PAST_EOF             "Past end of file"
 #define ERR_ARC_IS_READ_ONLY     "Archive is read-only"
 #define ERR_IO_ERROR             "I/O error"
 #define ERR_CANT_SET_WRITE_DIR   "Can't set write directory"
 #define ERR_SYMLINK_LOOP         "Infinite symbolic link loop"
 #define ERR_COMPRESSION          "(De)compression error"
 #define ERR_NOT_IMPLEMENTED      "Not implemented"
 #define ERR_OS_ERROR             "Operating system reported error"
 #define ERR_FILE_EXISTS          "File already exists"
 #define ERR_NOT_A_FILE           "Not a file"
 #define ERR_NOT_A_DIR            "Not a directory"
 #define ERR_NOT_AN_ARCHIVE       "Not an archive"
 #define ERR_CORRUPTED            "Corrupted archive"
 #define ERR_SEEK_OUT_OF_RANGE    "Seek out of range"
 #define ERR_BAD_FILENAME         "Bad filename"
 #define ERR_PHYSFS_BAD_OS_CALL   "(BUG) PhysicsFS made a bad system call"
 #define ERR_ARGV0_IS_NULL        "argv0 is NULL"
 #define ERR_ZLIB_NEED_DICT       "zlib: need dictionary"
 #define ERR_ZLIB_DATA_ERROR      "zlib: data error"
 #define ERR_ZLIB_MEMORY_ERROR    "zlib: memory error"
 #define ERR_ZLIB_BUFFER_ERROR    "zlib: buffer error"
 #define ERR_ZLIB_VERSION_ERROR   "zlib: version error"
 #define ERR_ZLIB_UNKNOWN_ERROR   "zlib: unknown error"
 #define ERR_SEARCHPATH_TRUNC     "Search path was truncated"
 #define ERR_GETMODFN_TRUNC       "GetModuleFileName() was truncated"
 #define ERR_GETMODFN_NO_DIR      "GetModuleFileName() had no dir"
 #define ERR_DISK_FULL            "Disk is full"
 #define ERR_DIRECTORY_FULL       "Directory full"
 #define ERR_MACOS_GENERIC        "MacOS reported error (%d)"
 #define ERR_OS2_GENERIC          "OS/2 reported error (%d)"
 #define ERR_VOL_LOCKED_HW        "Volume is locked through hardware"
 #define ERR_VOL_LOCKED_SW        "Volume is locked through software"
 #define ERR_FILE_LOCKED          "File is locked"
 #define ERR_FILE_OR_DIR_BUSY     "File/directory is busy"
 #define ERR_FILE_ALREADY_OPEN_W  "File already open for writing"
 #define ERR_FILE_ALREADY_OPEN_R  "File already open for reading"
 #define ERR_INVALID_REFNUM       "Invalid reference number"
 #define ERR_GETTING_FILE_POS     "Error getting file position"
 #define ERR_VOLUME_OFFLINE       "Volume is offline"
 #define ERR_PERMISSION_DENIED    "Permission denied"
 #define ERR_VOL_ALREADY_ONLINE   "Volume already online"
 #define ERR_NO_SUCH_DRIVE        "No such drive"
 #define ERR_NOT_MAC_DISK         "Not a Macintosh disk"
 #define ERR_VOL_EXTERNAL_FS      "Volume belongs to an external filesystem"
 #define ERR_PROBLEM_RENAME       "Problem during rename"
 #define ERR_BAD_MASTER_BLOCK     "Bad master directory block"
 #define ERR_CANT_MOVE_FORBIDDEN  "Attempt to move forbidden"
 #define ERR_WRONG_VOL_TYPE       "Wrong volume type"
 #define ERR_SERVER_VOL_LOST      "Server volume has been disconnected"
 #define ERR_FILE_ID_NOT_FOUND    "File ID not found"
 #define ERR_FILE_ID_EXISTS       "File ID already exists"
 #define ERR_SERVER_NO_RESPOND    "Server not responding"
 #define ERR_USER_AUTH_FAILED     "User authentication failed"
 #define ERR_PWORD_EXPIRED        "Password has expired on server"
 #define ERR_ACCESS_DENIED        "Access denied"
 #define ERR_NOT_A_DOS_DISK       "Not a DOS disk"
 #define ERR_SHARING_VIOLATION    "Sharing violation"
 #define ERR_CANNOT_MAKE          "Cannot make"
 #define ERR_DEV_IN_USE           "Device already in use"
 #define ERR_OPEN_FAILED          "Open failed"
 #define ERR_PIPE_BUSY            "Pipe is busy"
 #define ERR_SHARING_BUF_EXCEEDED "Sharing buffer exceeded"
 #define ERR_TOO_MANY_HANDLES     "Too many open handles"
 #define ERR_SEEK_ERROR           "Seek error"
 #define ERR_DEL_CWD              "Trying to delete current working directory"
 #define ERR_WRITE_PROTECT_ERROR  "Write protect error"
 #define ERR_WRITE_FAULT          "Write fault"
 #define ERR_LOCK_VIOLATION       "Lock violation"
 #define ERR_GEN_FAILURE          "General failure"
 #define ERR_UNCERTAIN_MEDIA      "Uncertain media"
 #define ERR_PROT_VIOLATION       "Protection violation"
 #define ERR_BROKEN_PIPE          "Broken pipe"

#elif (PHYSFS_LANG == PHYSFS_LANG_GERMAN)
 #define DIR_ARCHIVE_DESCRIPTION  "Kein Archiv, direkte Ein/Ausgabe in das Dateisystem"
 #define GRP_ARCHIVE_DESCRIPTION  "Build engine Groupfile format"
 #define HOG_ARCHIVE_DESCRIPTION  "Descent I/II HOG file format"
 #define MVL_ARCHIVE_DESCRIPTION  "Descent II Movielib format"
 #define QPAK_ARCHIVE_DESCRIPTION "Quake I/II format"
 #define ZIP_ARCHIVE_DESCRIPTION  "PkZip/WinZip/Info-Zip kompatibel"
 #define WAD_ARCHIVE_DESCRIPTION  "DOOM engine format"

 #define ERR_IS_INITIALIZED       "Bereits initialisiert"
 #define ERR_NOT_INITIALIZED      "Nicht initialisiert"
 #define ERR_INVALID_ARGUMENT     "Ungültiges Argument"
 #define ERR_FILES_STILL_OPEN     "Dateien noch immer geöffnet"
 #define ERR_NO_DIR_CREATE        "Fehler beim Erzeugen der Verzeichnisse"
 #define ERR_OUT_OF_MEMORY        "Kein Speicher mehr frei"
 #define ERR_NOT_IN_SEARCH_PATH   "Eintrag nicht im Suchpfad enthalten"
 #define ERR_NOT_SUPPORTED        "Befehl nicht unterstützt"
 #define ERR_UNSUPPORTED_ARCHIVE  "Archiv-Typ nicht unterstützt"
 #define ERR_NOT_A_HANDLE         "Ist kein Dateideskriptor"
 #define ERR_INSECURE_FNAME       "Unsicherer Dateiname"
 #define ERR_SYMLINK_DISALLOWED   "Symbolische Verweise deaktiviert"
 #define ERR_NO_WRITE_DIR         "Schreibverzeichnis ist nicht gesetzt"
 #define ERR_NO_SUCH_FILE         "Datei nicht gefunden"
 #define ERR_NO_SUCH_PATH         "Pfad nicht gefunden"
 #define ERR_NO_SUCH_VOLUME       "Datencontainer nicht gefunden"
 #define ERR_PAST_EOF             "Hinter dem Ende der Datei"
 #define ERR_ARC_IS_READ_ONLY     "Archiv ist schreibgeschützt"
 #define ERR_IO_ERROR             "Ein/Ausgabe Fehler"
 #define ERR_CANT_SET_WRITE_DIR   "Kann Schreibverzeichnis nicht setzen"
 #define ERR_SYMLINK_LOOP         "Endlosschleife durch symbolische Verweise"
 #define ERR_COMPRESSION          "(De)Kompressionsfehler"
 #define ERR_NOT_IMPLEMENTED      "Nicht implementiert"
 #define ERR_OS_ERROR             "Betriebssystem meldete Fehler"
 #define ERR_FILE_EXISTS          "Datei existiert bereits"
 #define ERR_NOT_A_FILE           "Ist keine Datei"
 #define ERR_NOT_A_DIR            "Ist kein Verzeichnis"
 #define ERR_NOT_AN_ARCHIVE       "Ist kein Archiv"
 #define ERR_CORRUPTED            "Beschädigtes Archiv"
 #define ERR_SEEK_OUT_OF_RANGE    "Suche war ausserhalb der Reichweite"
 #define ERR_BAD_FILENAME         "Unzulässiger Dateiname"
 #define ERR_PHYSFS_BAD_OS_CALL   "(BUG) PhysicsFS verursachte einen ungültigen Systemaufruf"
 #define ERR_ARGV0_IS_NULL        "argv0 ist NULL"
 #define ERR_ZLIB_NEED_DICT       "zlib: brauche Wörterbuch"
 #define ERR_ZLIB_DATA_ERROR      "zlib: Datenfehler"
 #define ERR_ZLIB_MEMORY_ERROR    "zlib: Speicherfehler"
 #define ERR_ZLIB_BUFFER_ERROR    "zlib: Bufferfehler"
 #define ERR_ZLIB_VERSION_ERROR   "zlib: Versionskonflikt"
 #define ERR_ZLIB_UNKNOWN_ERROR   "zlib: Unbekannter Fehler"
 #define ERR_SEARCHPATH_TRUNC     "Suchpfad war abgeschnitten"
 #define ERR_GETMODFN_TRUNC       "GetModuleFileName() war abgeschnitten"
 #define ERR_GETMODFN_NO_DIR      "GetModuleFileName() bekam kein Verzeichnis"
 #define ERR_DISK_FULL            "Laufwerk ist voll"
 #define ERR_DIRECTORY_FULL       "Verzeichnis ist voll"
 #define ERR_MACOS_GENERIC        "MacOS meldete Fehler (%d)"
 #define ERR_OS2_GENERIC          "OS/2 meldete Fehler (%d)"
 #define ERR_VOL_LOCKED_HW        "Datencontainer ist durch Hardware gesperrt"
 #define ERR_VOL_LOCKED_SW        "Datencontainer ist durch Software gesperrt"
 #define ERR_FILE_LOCKED          "Datei ist gesperrt"
 #define ERR_FILE_OR_DIR_BUSY     "Datei/Verzeichnis ist beschäftigt"
 #define ERR_FILE_ALREADY_OPEN_W  "Datei schon im Schreibmodus geöffnet"
 #define ERR_FILE_ALREADY_OPEN_R  "Datei schon im Lesemodus geöffnet"
 #define ERR_INVALID_REFNUM       "Ungültige Referenznummer"
 #define ERR_GETTING_FILE_POS     "Fehler beim Finden der Dateiposition"
 #define ERR_VOLUME_OFFLINE       "Datencontainer ist offline"
 #define ERR_PERMISSION_DENIED    "Zugriff verweigert"
 #define ERR_VOL_ALREADY_ONLINE   "Datencontainer ist bereits online"
 #define ERR_NO_SUCH_DRIVE        "Laufwerk nicht vorhanden"
 #define ERR_NOT_MAC_DISK         "Ist kein Macintosh Laufwerk"
 #define ERR_VOL_EXTERNAL_FS      "Datencontainer liegt auf einem externen Dateisystem"
 #define ERR_PROBLEM_RENAME       "Fehler beim Umbenennen"
 #define ERR_BAD_MASTER_BLOCK     "Beschädigter Hauptverzeichnisblock"
 #define ERR_CANT_MOVE_FORBIDDEN  "Verschieben nicht erlaubt"
 #define ERR_WRONG_VOL_TYPE       "Falscher Datencontainer-Typ"
 #define ERR_SERVER_VOL_LOST      "Datencontainer am Server wurde getrennt"
 #define ERR_FILE_ID_NOT_FOUND    "Dateikennung nicht gefunden"
 #define ERR_FILE_ID_EXISTS       "Dateikennung existiert bereits"
 #define ERR_SERVER_NO_RESPOND    "Server antwortet nicht"
 #define ERR_USER_AUTH_FAILED     "Benutzerauthentifizierung fehlgeschlagen"
 #define ERR_PWORD_EXPIRED        "Passwort am Server ist abgelaufen"
 #define ERR_ACCESS_DENIED        "Zugriff verweigert"
 #define ERR_NOT_A_DOS_DISK       "Ist kein DOS-Laufwerk"
 #define ERR_SHARING_VIOLATION    "Zugriffsverletzung"
 #define ERR_CANNOT_MAKE          "Kann nicht erzeugen"
 #define ERR_DEV_IN_USE           "Gerät wird bereits benutzt"
 #define ERR_OPEN_FAILED          "Öffnen fehlgeschlagen"
 #define ERR_PIPE_BUSY            "Pipeverbindung ist belegt"
 #define ERR_SHARING_BUF_EXCEEDED "Zugriffsbuffer überschritten"
 #define ERR_TOO_MANY_HANDLES     "Zu viele offene Dateien"
 #define ERR_SEEK_ERROR           "Fehler beim Suchen"
 #define ERR_DEL_CWD              "Aktuelles Arbeitsverzeichnis darf nicht gelöscht werden"
 #define ERR_WRITE_PROTECT_ERROR  "Schreibschutzfehler"
 #define ERR_WRITE_FAULT          "Schreibfehler"
 #define ERR_LOCK_VIOLATION       "Sperrverletzung"
 #define ERR_GEN_FAILURE          "Allgemeiner Fehler"
 #define ERR_UNCERTAIN_MEDIA      "Unsicheres Medium"
 #define ERR_PROT_VIOLATION       "Schutzverletzung"
 #define ERR_BROKEN_PIPE          "Pipeverbindung unterbrochen"

#elif (PHYSFS_LANG == PHYSFS_LANG_RUSSIAN_KOI8_R)
 #define DIR_ARCHIVE_DESCRIPTION  "îÅ ÁÒÈÉ×, ÎÅÐÏÓÒÅÄÓÔ×ÅÎÎÙÊ ××ÏÄ/×Ù×ÏÄ ÆÁÊÌÏ×ÏÊ ÓÉÓÔÅÍÙ"
 #define GRP_ARCHIVE_DESCRIPTION  "æÏÒÍÁÔ ÇÒÕÐÐÏ×ÏÇÏ ÆÁÊÌÁ Build engine"
 #define HOG_ARCHIVE_DESCRIPTION  "Descent I/II HOG file format"
 #define MVL_ARCHIVE_DESCRIPTION  "Descent II Movielib format"
 #define ZIP_ARCHIVE_DESCRIPTION  "PkZip/WinZip/Info-Zip ÓÏ×ÍÅÓÔÉÍÙÊ"
 #define WAD_ARCHIVE_DESCRIPTION  "DOOM engine format" // !!! FIXME: translate this line if needed

 #define ERR_IS_INITIALIZED       "õÖÅ ÉÎÉÃÉÁÌÉÚÉÒÏ×ÁÎ"
 #define ERR_NOT_INITIALIZED      "îÅ ÉÎÉÃÉÁÌÉÚÉÒÏ×ÁÎ"
 #define ERR_INVALID_ARGUMENT     "îÅ×ÅÒÎÙÊ ÁÒÇÕÍÅÎÔ"
 #define ERR_FILES_STILL_OPEN     "æÁÊÌÙ ÅÝÅ ÏÔËÒÙÔÙ"
 #define ERR_NO_DIR_CREATE        "îÅ ÍÏÇÕ ÓÏÚÄÁÔØ ËÁÔÁÌÏÇÉ"
 #define ERR_OUT_OF_MEMORY        "ëÏÎÞÉÌÁÓØ ÐÁÍÑÔØ"
 #define ERR_NOT_IN_SEARCH_PATH   "îÅÔ ÔÁËÏÇÏ ÜÌÅÍÅÎÔÁ × ÐÕÔÉ ÐÏÉÓËÁ"
 #define ERR_NOT_SUPPORTED        "ïÐÅÒÁÃÉÑ ÎÅ ÐÏÄÄÅÒÖÉ×ÁÅÔÓÑ"
 #define ERR_UNSUPPORTED_ARCHIVE  "áÒÈÉ×Ù ÔÁËÏÇÏ ÔÉÐÁ ÎÅ ÐÏÄÄÅÒÖÉ×ÁÀÔÓÑ"
 #define ERR_NOT_A_HANDLE         "îÅ ÆÁÊÌÏ×ÙÊ ÄÅÓËÒÉÐÔÏÒ"
 #define ERR_INSECURE_FNAME       "îÅÂÅÚÏÐÁÓÎÏÅ ÉÍÑ ÆÁÊÌÁ"
 #define ERR_SYMLINK_DISALLOWED   "óÉÍ×ÏÌØÎÙÅ ÓÓÙÌËÉ ÏÔËÌÀÞÅÎÙ"
 #define ERR_NO_WRITE_DIR         "ëÁÔÁÌÏÇ ÄÌÑ ÚÁÐÉÓÉ ÎÅ ÕÓÔÁÎÏ×ÌÅÎ"
 #define ERR_NO_SUCH_FILE         "æÁÊÌ ÎÅ ÎÁÊÄÅÎ"
 #define ERR_NO_SUCH_PATH         "ðÕÔØ ÎÅ ÎÁÊÄÅÎ"
 #define ERR_NO_SUCH_VOLUME       "ôÏÍ ÎÅ ÎÁÊÄÅÎ"
 #define ERR_PAST_EOF             "úÁ ËÏÎÃÏÍ ÆÁÊÌÁ"
 #define ERR_ARC_IS_READ_ONLY     "áÒÈÉ× ÔÏÌØËÏ ÄÌÑ ÞÔÅÎÉÑ"
 #define ERR_IO_ERROR             "ïÛÉÂËÁ ××ÏÄÁ/×Ù×ÏÄÁ"
 #define ERR_CANT_SET_WRITE_DIR   "îÅ ÍÏÇÕ ÕÓÔÁÎÏ×ÉÔØ ËÁÔÁÌÏÇ ÄÌÑ ÚÁÐÉÓÉ"
 #define ERR_SYMLINK_LOOP         "âÅÓËÏÎÅÞÎÙÊ ÃÉËÌ ÓÉÍ×ÏÌØÎÏÊ ÓÓÙÌËÉ"
 #define ERR_COMPRESSION          "ïÛÉÂËÁ (òÁÓ)ÐÁËÏ×ËÉ"
 #define ERR_NOT_IMPLEMENTED      "îÅ ÒÅÁÌÉÚÏ×ÁÎÏ"
 #define ERR_OS_ERROR             "ïÐÅÒÁÃÉÏÎÎÁÑ ÓÉÓÔÅÍÁ ÓÏÏÂÝÉÌÁ ÏÛÉÂËÕ"
 #define ERR_FILE_EXISTS          "æÁÊÌ ÕÖÅ ÓÕÝÅÓÔ×ÕÅÔ"
 #define ERR_NOT_A_FILE           "îÅ ÆÁÊÌ"
 #define ERR_NOT_A_DIR            "îÅ ËÁÔÁÌÏÇ"
 #define ERR_NOT_AN_ARCHIVE       "îÅ ÁÒÈÉ×"
 #define ERR_CORRUPTED            "ðÏ×ÒÅÖÄÅÎÎÙÊ ÁÒÈÉ×"
 #define ERR_SEEK_OUT_OF_RANGE    "ðÏÚÉÃÉÏÎÉÒÏ×ÁÎÉÅ ÚÁ ÐÒÅÄÅÌÙ"
 #define ERR_BAD_FILENAME         "îÅ×ÅÒÎÏÅ ÉÍÑ ÆÁÊÌÁ"
 #define ERR_PHYSFS_BAD_OS_CALL   "(BUG) PhysicsFS ×ÙÐÏÌÎÉÌÁ ÎÅ×ÅÒÎÙÊ ÓÉÓÔÅÍÎÙÊ ×ÙÚÏ×"
 #define ERR_ARGV0_IS_NULL        "argv0 is NULL"
 #define ERR_ZLIB_NEED_DICT       "zlib: ÎÕÖÅÎ ÓÌÏ×ÁÒØ"
 #define ERR_ZLIB_DATA_ERROR      "zlib: ÏÛÉÂËÁ ÄÁÎÎÙÈ"
 #define ERR_ZLIB_MEMORY_ERROR    "zlib: ÏÛÉÂËÁ ÐÁÍÑÔÉ"
 #define ERR_ZLIB_BUFFER_ERROR    "zlib: ÏÛÉÂËÁ ÂÕÆÅÒÁ"
 #define ERR_ZLIB_VERSION_ERROR   "zlib: ÏÛÉÂËÁ ×ÅÒÓÉÉ"
 #define ERR_ZLIB_UNKNOWN_ERROR   "zlib: ÎÅÉÚ×ÅÓÔÎÁÑ ÏÛÉÂËÁ"
 #define ERR_SEARCHPATH_TRUNC     "ðÕÔØ ÐÏÉÓËÁ ÏÂÒÅÚÁÎ"
 #define ERR_GETMODFN_TRUNC       "GetModuleFileName() ÏÂÒÅÚÁÎ"
 #define ERR_GETMODFN_NO_DIR      "GetModuleFileName() ÎÅ ÐÏÌÕÞÉÌ ËÁÔÁÌÏÇ"
 #define ERR_DISK_FULL            "äÉÓË ÐÏÌÏÎ"
 #define ERR_DIRECTORY_FULL       "ëÁÔÁÌÏÇ ÐÏÌÏÎ"
 #define ERR_MACOS_GENERIC        "MacOS ÓÏÏÂÝÉÌÁ ÏÛÉÂËÕ (%d)"
 #define ERR_OS2_GENERIC          "OS/2 ÓÏÏÂÝÉÌÁ ÏÛÉÂËÕ (%d)"
 #define ERR_VOL_LOCKED_HW        "ôÏÍ ÂÌÏËÉÒÏ×ÁÎ ÁÐÐÁÒÁÔÎÏ"
 #define ERR_VOL_LOCKED_SW        "ôÏÍ ÂÌÏËÉÒÏ×ÁÎ ÐÒÏÇÒÁÍÍÎÏ"
 #define ERR_FILE_LOCKED          "æÁÊÌ ÚÁÂÌÏËÉÒÏ×ÁÎ"
 #define ERR_FILE_OR_DIR_BUSY     "æÁÊÌ/ËÁÔÁÌÏÇ ÚÁÎÑÔ"
 #define ERR_FILE_ALREADY_OPEN_W  "æÁÊÌ ÕÖÅ ÏÔËÒÙÔ ÎÁ ÚÁÐÉÓØ"
 #define ERR_FILE_ALREADY_OPEN_R  "æÁÊÌ ÕÖÅ ÏÔËÒÙÔ ÎÁ ÞÔÅÎÉÅ"
 #define ERR_INVALID_REFNUM       "îÅ×ÅÒÎÏÅ ËÏÌÉÞÅÓÔ×Ï ÓÓÙÌÏË"
 #define ERR_GETTING_FILE_POS     "ïÛÉÂËÁ ÐÒÉ ÐÏÌÕÞÅÎÉÉ ÐÏÚÉÃÉÉ ÆÁÊÌÁ"
 #define ERR_VOLUME_OFFLINE       "ôÏÍ ÏÔÓÏÅÄÉÎÅÎ"
 #define ERR_PERMISSION_DENIED    "ïÔËÁÚÁÎÏ × ÒÁÚÒÅÛÅÎÉÉ"
 #define ERR_VOL_ALREADY_ONLINE   "ôÏÍ ÕÖÅ ÐÏÄÓÏÅÄÉÎÅÎ"
 #define ERR_NO_SUCH_DRIVE        "îÅÔ ÔÁËÏÇÏ ÄÉÓËÁ"
 #define ERR_NOT_MAC_DISK         "îÅ ÄÉÓË Macintosh"
 #define ERR_VOL_EXTERNAL_FS      "ôÏÍ ÐÒÉÎÁÄÌÅÖÉÔ ×ÎÅÛÎÅÊ ÆÁÊÌÏ×ÏÊ ÓÉÓÔÅÍÅ"
 #define ERR_PROBLEM_RENAME       "ðÒÏÂÌÅÍÁ ÐÒÉ ÐÅÒÅÉÍÅÎÏ×ÁÎÉÉ"
 #define ERR_BAD_MASTER_BLOCK     "ðÌÏÈÏÊ ÇÌÁ×ÎÙÊ ÂÌÏË ËÁÔÁÌÏÇÁ"
 #define ERR_CANT_MOVE_FORBIDDEN  "ðÏÐÙÔËÁ ÐÅÒÅÍÅÓÔÉÔØ ÚÁÐÒÅÝÅÎÁ"
 #define ERR_WRONG_VOL_TYPE       "îÅ×ÅÒÎÙÊ ÔÉÐ ÔÏÍÁ"
 #define ERR_SERVER_VOL_LOST      "óÅÒ×ÅÒÎÙÊ ÔÏÍ ÂÙÌ ÏÔÓÏÅÄÉÎÅÎ"
 #define ERR_FILE_ID_NOT_FOUND    "éÄÅÎÔÉÆÉËÁÔÏÒ ÆÁÊÌÁ ÎÅ ÎÁÊÄÅÎ"
 #define ERR_FILE_ID_EXISTS       "éÄÅÎÔÉÆÉËÁÔÏÒ ÆÁÊÌÁ ÕÖÅ ÓÕÝÅÓÔ×ÕÅÔ"
 #define ERR_SERVER_NO_RESPOND    "óÅÒ×ÅÒ ÎÅ ÏÔ×ÅÞÁÅÔ"
 #define ERR_USER_AUTH_FAILED     "éÄÅÎÔÉÆÉËÁÃÉÑ ÐÏÌØÚÏ×ÁÔÅÌÑ ÎÅ ÕÄÁÌÁÓØ"
 #define ERR_PWORD_EXPIRED        "ðÁÒÏÌØ ÎÁ ÓÅÒ×ÅÒÅ ÕÓÔÁÒÅÌ"
 #define ERR_ACCESS_DENIED        "ïÔËÁÚÁÎÏ × ÄÏÓÔÕÐÅ"
 #define ERR_NOT_A_DOS_DISK       "îÅ ÄÉÓË DOS"
 #define ERR_SHARING_VIOLATION    "îÁÒÕÛÅÎÉÅ ÓÏ×ÍÅÓÔÎÏÇÏ ÄÏÓÔÕÐÁ"
 #define ERR_CANNOT_MAKE          "îÅ ÍÏÇÕ ÓÏÂÒÁÔØ"
 #define ERR_DEV_IN_USE           "õÓÔÒÏÊÓÔ×Ï ÕÖÅ ÉÓÐÏÌØÚÕÅÔÓÑ"
 #define ERR_OPEN_FAILED          "ïÔËÒÙÔÉÅ ÎÅ ÕÄÁÌÏÓØ"
 #define ERR_PIPE_BUSY            "ëÏÎ×ÅÊÅÒ ÚÁÎÑÔ"
 #define ERR_SHARING_BUF_EXCEEDED "òÁÚÄÅÌÑÅÍÙÊ ÂÕÆÅÒ ÐÅÒÅÐÏÌÎÅÎ"
 #define ERR_TOO_MANY_HANDLES     "óÌÉÛËÏÍ ÍÎÏÇÏ ÏÔËÒÙÔÙÈ ÄÅÓËÒÉÐÔÏÒÏ×"
 #define ERR_SEEK_ERROR           "ïÛÉÂËÁ ÐÏÚÉÃÉÏÎÉÒÏ×ÁÎÉÑ"
 #define ERR_DEL_CWD              "ðÏÐÙÔËÁ ÕÄÁÌÉÔØ ÔÅËÕÝÉÊ ÒÁÂÏÞÉÊ ËÁÔÁÌÏÇ"
 #define ERR_WRITE_PROTECT_ERROR  "ïÛÉÂËÁ ÚÁÝÉÔÙ ÚÁÐÉÓÉ"
 #define ERR_WRITE_FAULT          "ïÛÉÂËÁ ÚÁÐÉÓÉ"
 #define ERR_LOCK_VIOLATION       "îÁÒÕÛÅÎÉÅ ÂÌÏËÉÒÏ×ËÉ"
 #define ERR_GEN_FAILURE          "ïÂÝÉÊ ÓÂÏÊ"
 #define ERR_UNCERTAIN_MEDIA      "îÅÏÐÒÅÄÅÌÅÎÎÙÊ ÎÏÓÉÔÅÌØ"
 #define ERR_PROT_VIOLATION       "îÁÒÕÛÅÎÉÅ ÚÁÝÉÔÙ"
 #define ERR_BROKEN_PIPE          "óÌÏÍÁÎÎÙÊ ËÏÎ×ÅÊÅÒ"

#elif (PHYSFS_LANG == PHYSFS_LANG_RUSSIAN_CP1251)
 #define DIR_ARCHIVE_DESCRIPTION  "Íå àðõèâ, íåïîñðåäñòâåííûé ââîä/âûâîä ôàéëîâîé ñèñòåìû"
 #define GRP_ARCHIVE_DESCRIPTION  "Ôîðìàò ãðóïïîâîãî ôàéëà Build engine"
 #define HOG_ARCHIVE_DESCRIPTION  "Descent I/II HOG file format"
 #define MVL_ARCHIVE_DESCRIPTION  "Descent II Movielib format"
 #define ZIP_ARCHIVE_DESCRIPTION  "PkZip/WinZip/Info-Zip ñîâìåñòèìûé"
 #define WAD_ARCHIVE_DESCRIPTION  "DOOM engine format" // FIXME: translate this line if needed

 #define ERR_IS_INITIALIZED       "Óæå èíèöèàëèçèðîâàí"
 #define ERR_NOT_INITIALIZED      "Íå èíèöèàëèçèðîâàí"
 #define ERR_INVALID_ARGUMENT     "Íåâåðíûé àðãóìåíò"
 #define ERR_FILES_STILL_OPEN     "Ôàéëû åùå îòêðûòû"
 #define ERR_NO_DIR_CREATE        "Íå ìîãó ñîçäàòü êàòàëîãè"
 #define ERR_OUT_OF_MEMORY        "Êîí÷èëàñü ïàìÿòü"
 #define ERR_NOT_IN_SEARCH_PATH   "Íåò òàêîãî ýëåìåíòà â ïóòè ïîèñêà"
 #define ERR_NOT_SUPPORTED        "Îïåðàöèÿ íå ïîääåðæèâàåòñÿ"
 #define ERR_UNSUPPORTED_ARCHIVE  "Àðõèâû òàêîãî òèïà íå ïîääåðæèâàþòñÿ"
 #define ERR_NOT_A_HANDLE         "Íå ôàéëîâûé äåñêðèïòîð"
 #define ERR_INSECURE_FNAME       "Íåáåçîïàñíîå èìÿ ôàéëà"
 #define ERR_SYMLINK_DISALLOWED   "Ñèìâîëüíûå ññûëêè îòêëþ÷åíû"
 #define ERR_NO_WRITE_DIR         "Êàòàëîã äëÿ çàïèñè íå óñòàíîâëåí"
 #define ERR_NO_SUCH_FILE         "Ôàéë íå íàéäåí"
 #define ERR_NO_SUCH_PATH         "Ïóòü íå íàéäåí"
 #define ERR_NO_SUCH_VOLUME       "Òîì íå íàéäåí"
 #define ERR_PAST_EOF             "Çà êîíöîì ôàéëà"
 #define ERR_ARC_IS_READ_ONLY     "Àðõèâ òîëüêî äëÿ ÷òåíèÿ"
 #define ERR_IO_ERROR             "Îøèáêà ââîäà/âûâîäà"
 #define ERR_CANT_SET_WRITE_DIR   "Íå ìîãó óñòàíîâèòü êàòàëîã äëÿ çàïèñè"
 #define ERR_SYMLINK_LOOP         "Áåñêîíå÷íûé öèêë ñèìâîëüíîé ññûëêè"
 #define ERR_COMPRESSION          "Îøèáêà (Ðàñ)ïàêîâêè"
 #define ERR_NOT_IMPLEMENTED      "Íå ðåàëèçîâàíî"
 #define ERR_OS_ERROR             "Îïåðàöèîííàÿ ñèñòåìà ñîîáùèëà îøèáêó"
 #define ERR_FILE_EXISTS          "Ôàéë óæå ñóùåñòâóåò"
 #define ERR_NOT_A_FILE           "Íå ôàéë"
 #define ERR_NOT_A_DIR            "Íå êàòàëîã"
 #define ERR_NOT_AN_ARCHIVE       "Íå àðõèâ"
 #define ERR_CORRUPTED            "Ïîâðåæäåííûé àðõèâ"
 #define ERR_SEEK_OUT_OF_RANGE    "Ïîçèöèîíèðîâàíèå çà ïðåäåëû"
 #define ERR_BAD_FILENAME         "Íåâåðíîå èìÿ ôàéëà"
 #define ERR_PHYSFS_BAD_OS_CALL   "(BUG) PhysicsFS âûïîëíèëà íåâåðíûé ñèñòåìíûé âûçîâ"
 #define ERR_ARGV0_IS_NULL        "argv0 is NULL"
 #define ERR_ZLIB_NEED_DICT       "zlib: íóæåí ñëîâàðü"
 #define ERR_ZLIB_DATA_ERROR      "zlib: îøèáêà äàííûõ"
 #define ERR_ZLIB_MEMORY_ERROR    "zlib: îøèáêà ïàìÿòè"
 #define ERR_ZLIB_BUFFER_ERROR    "zlib: îøèáêà áóôåðà"
 #define ERR_ZLIB_VERSION_ERROR   "zlib: îøèáêà âåðñèè"
 #define ERR_ZLIB_UNKNOWN_ERROR   "zlib: íåèçâåñòíàÿ îøèáêà"
 #define ERR_SEARCHPATH_TRUNC     "Ïóòü ïîèñêà îáðåçàí"
 #define ERR_GETMODFN_TRUNC       "GetModuleFileName() îáðåçàí"
 #define ERR_GETMODFN_NO_DIR      "GetModuleFileName() íå ïîëó÷èë êàòàëîã"
 #define ERR_DISK_FULL            "Äèñê ïîëîí"
 #define ERR_DIRECTORY_FULL       "Êàòàëîã ïîëîí"
 #define ERR_MACOS_GENERIC        "MacOS ñîîáùèëà îøèáêó (%d)"
 #define ERR_OS2_GENERIC          "OS/2 ñîîáùèëà îøèáêó (%d)"
 #define ERR_VOL_LOCKED_HW        "Òîì áëîêèðîâàí àïïàðàòíî"
 #define ERR_VOL_LOCKED_SW        "Òîì áëîêèðîâàí ïðîãðàììíî"
 #define ERR_FILE_LOCKED          "Ôàéë çàáëîêèðîâàí"
 #define ERR_FILE_OR_DIR_BUSY     "Ôàéë/êàòàëîã çàíÿò"
 #define ERR_FILE_ALREADY_OPEN_W  "Ôàéë óæå îòêðûò íà çàïèñü"
 #define ERR_FILE_ALREADY_OPEN_R  "Ôàéë óæå îòêðûò íà ÷òåíèå"
 #define ERR_INVALID_REFNUM       "Íåâåðíîå êîëè÷åñòâî ññûëîê"
 #define ERR_GETTING_FILE_POS     "Îøèáêà ïðè ïîëó÷åíèè ïîçèöèè ôàéëà"
 #define ERR_VOLUME_OFFLINE       "Òîì îòñîåäèíåí"
 #define ERR_PERMISSION_DENIED    "Îòêàçàíî â ðàçðåøåíèè"
 #define ERR_VOL_ALREADY_ONLINE   "Òîì óæå ïîäñîåäèíåí"
 #define ERR_NO_SUCH_DRIVE        "Íåò òàêîãî äèñêà"
 #define ERR_NOT_MAC_DISK         "Íå äèñê Macintosh"
 #define ERR_VOL_EXTERNAL_FS      "Òîì ïðèíàäëåæèò âíåøíåé ôàéëîâîé ñèñòåìå"
 #define ERR_PROBLEM_RENAME       "Ïðîáëåìà ïðè ïåðåèìåíîâàíèè"
 #define ERR_BAD_MASTER_BLOCK     "Ïëîõîé ãëàâíûé áëîê êàòàëîãà"
 #define ERR_CANT_MOVE_FORBIDDEN  "Ïîïûòêà ïåðåìåñòèòü çàïðåùåíà"
 #define ERR_WRONG_VOL_TYPE       "Íåâåðíûé òèï òîìà"
 #define ERR_SERVER_VOL_LOST      "Ñåðâåðíûé òîì áûë îòñîåäèíåí"
 #define ERR_FILE_ID_NOT_FOUND    "Èäåíòèôèêàòîð ôàéëà íå íàéäåí"
 #define ERR_FILE_ID_EXISTS       "Èäåíòèôèêàòîð ôàéëà óæå ñóùåñòâóåò"
 #define ERR_SERVER_NO_RESPOND    "Ñåðâåð íå îòâå÷àåò"
 #define ERR_USER_AUTH_FAILED     "Èäåíòèôèêàöèÿ ïîëüçîâàòåëÿ íå óäàëàñü"
 #define ERR_PWORD_EXPIRED        "Ïàðîëü íà ñåðâåðå óñòàðåë"
 #define ERR_ACCESS_DENIED        "Îòêàçàíî â äîñòóïå"
 #define ERR_NOT_A_DOS_DISK       "Íå äèñê DOS"
 #define ERR_SHARING_VIOLATION    "Íàðóøåíèå ñîâìåñòíîãî äîñòóïà"
 #define ERR_CANNOT_MAKE          "Íå ìîãó ñîáðàòü"
 #define ERR_DEV_IN_USE           "Óñòðîéñòâî óæå èñïîëüçóåòñÿ"
 #define ERR_OPEN_FAILED          "Îòêðûòèå íå óäàëîñü"
 #define ERR_PIPE_BUSY            "Êîíâåéåð çàíÿò"
 #define ERR_SHARING_BUF_EXCEEDED "Ðàçäåëÿåìûé áóôåð ïåðåïîëíåí"
 #define ERR_TOO_MANY_HANDLES     "Ñëèøêîì ìíîãî îòêðûòûõ äåñêðèïòîðîâ"
 #define ERR_SEEK_ERROR           "Îøèáêà ïîçèöèîíèðîâàíèÿ"
 #define ERR_DEL_CWD              "Ïîïûòêà óäàëèòü òåêóùèé ðàáî÷èé êàòàëîã"
 #define ERR_WRITE_PROTECT_ERROR  "Îøèáêà çàùèòû çàïèñè"
 #define ERR_WRITE_FAULT          "Îøèáêà çàïèñè"
 #define ERR_LOCK_VIOLATION       "Íàðóøåíèå áëîêèðîâêè"
 #define ERR_GEN_FAILURE          "Îáùèé ñáîé"
 #define ERR_UNCERTAIN_MEDIA      "Íåîïðåäåëåííûé íîñèòåëü"
 #define ERR_PROT_VIOLATION       "Íàðóøåíèå çàùèòû"
 #define ERR_BROKEN_PIPE          "Ñëîìàííûé êîíâåéåð"

#elif (PHYSFS_LANG == PHYSFS_LANG_RUSSIAN_CP866)
 #define DIR_ARCHIVE_DESCRIPTION  "¥  àå¨¢, ­¥¯®áà¥¤áâ¢¥­­ë© ¢¢®¤/¢ë¢®¤ ä ©«®¢®© á¨áâ¥¬ë"
 #define GRP_ARCHIVE_DESCRIPTION  "”®à¬ â £àã¯¯®¢®£® ä ©«  Build engine"
 #define HOG_ARCHIVE_DESCRIPTION  "Descent I/II HOG file format"
 #define MVL_ARCHIVE_DESCRIPTION  "Descent II Movielib format"
 #define ZIP_ARCHIVE_DESCRIPTION  "PkZip/WinZip/Info-Zip á®¢¬¥áâ¨¬ë©"
 #define WAD_ARCHIVE_DESCRIPTION  "DOOM engine format" // FIXME: Translate this line if needed

 #define ERR_IS_INITIALIZED       "“¦¥ ¨­¨æ¨ «¨§¨à®¢ ­"
 #define ERR_NOT_INITIALIZED      "¥ ¨­¨æ¨ «¨§¨à®¢ ­"
 #define ERR_INVALID_ARGUMENT     "¥¢¥à­ë©  à£ã¬¥­â"
 #define ERR_FILES_STILL_OPEN     "” ©«ë ¥é¥ ®âªàëâë"
 #define ERR_NO_DIR_CREATE        "¥ ¬®£ã á®§¤ âì ª â «®£¨"
 #define ERR_OUT_OF_MEMORY        "Š®­ç¨« áì ¯ ¬ïâì"
 #define ERR_NOT_IN_SEARCH_PATH   "¥â â ª®£® í«¥¬¥­â  ¢ ¯ãâ¨ ¯®¨áª "
 #define ERR_NOT_SUPPORTED        "Ž¯¥à æ¨ï ­¥ ¯®¤¤¥à¦¨¢ ¥âáï"
 #define ERR_UNSUPPORTED_ARCHIVE  "€àå¨¢ë â ª®£® â¨¯  ­¥ ¯®¤¤¥à¦¨¢ îâáï"
 #define ERR_NOT_A_HANDLE         "¥ ä ©«®¢ë© ¤¥áªà¨¯â®à"
 #define ERR_INSECURE_FNAME       "¥¡¥§®¯ á­®¥ ¨¬ï ä ©« "
 #define ERR_SYMLINK_DISALLOWED   "‘¨¬¢®«ì­ë¥ ááë«ª¨ ®âª«îç¥­ë"
 #define ERR_NO_WRITE_DIR         "Š â «®£ ¤«ï § ¯¨á¨ ­¥ ãáâ ­®¢«¥­"
 #define ERR_NO_SUCH_FILE         "” ©« ­¥ ­ ©¤¥­"
 #define ERR_NO_SUCH_PATH         "ãâì ­¥ ­ ©¤¥­"
 #define ERR_NO_SUCH_VOLUME       "’®¬ ­¥ ­ ©¤¥­"
 #define ERR_PAST_EOF             "‡  ª®­æ®¬ ä ©« "
 #define ERR_ARC_IS_READ_ONLY     "€àå¨¢ â®«ìª® ¤«ï çâ¥­¨ï"
 #define ERR_IO_ERROR             "Žè¨¡ª  ¢¢®¤ /¢ë¢®¤ "
 #define ERR_CANT_SET_WRITE_DIR   "¥ ¬®£ã ãáâ ­®¢¨âì ª â «®£ ¤«ï § ¯¨á¨"
 #define ERR_SYMLINK_LOOP         "¥áª®­¥ç­ë© æ¨ª« á¨¬¢®«ì­®© ááë«ª¨"
 #define ERR_COMPRESSION          "Žè¨¡ª  ( á)¯ ª®¢ª¨"
 #define ERR_NOT_IMPLEMENTED      "¥ à¥ «¨§®¢ ­®"
 #define ERR_OS_ERROR             "Ž¯¥à æ¨®­­ ï á¨áâ¥¬  á®®¡é¨«  ®è¨¡ªã"
 #define ERR_FILE_EXISTS          "” ©« ã¦¥ áãé¥áâ¢ã¥â"
 #define ERR_NOT_A_FILE           "¥ ä ©«"
 #define ERR_NOT_A_DIR            "¥ ª â «®£"
 #define ERR_NOT_AN_ARCHIVE       "¥  àå¨¢"
 #define ERR_CORRUPTED            "®¢à¥¦¤¥­­ë©  àå¨¢"
 #define ERR_SEEK_OUT_OF_RANGE    "®§¨æ¨®­¨à®¢ ­¨¥ §  ¯à¥¤¥«ë"
 #define ERR_BAD_FILENAME         "¥¢¥à­®¥ ¨¬ï ä ©« "
 #define ERR_PHYSFS_BAD_OS_CALL   "(BUG) PhysicsFS ¢ë¯®«­¨«  ­¥¢¥à­ë© á¨áâ¥¬­ë© ¢ë§®¢"
 #define ERR_ARGV0_IS_NULL        "argv0 is NULL"
 #define ERR_ZLIB_NEED_DICT       "zlib: ­ã¦¥­ á«®¢ àì"
 #define ERR_ZLIB_DATA_ERROR      "zlib: ®è¨¡ª  ¤ ­­ëå"
 #define ERR_ZLIB_MEMORY_ERROR    "zlib: ®è¨¡ª  ¯ ¬ïâ¨"
 #define ERR_ZLIB_BUFFER_ERROR    "zlib: ®è¨¡ª  ¡ãä¥à "
 #define ERR_ZLIB_VERSION_ERROR   "zlib: ®è¨¡ª  ¢¥àá¨¨"
 #define ERR_ZLIB_UNKNOWN_ERROR   "zlib: ­¥¨§¢¥áâ­ ï ®è¨¡ª "
 #define ERR_SEARCHPATH_TRUNC     "ãâì ¯®¨áª  ®¡à¥§ ­"
 #define ERR_GETMODFN_TRUNC       "GetModuleFileName() ®¡à¥§ ­"
 #define ERR_GETMODFN_NO_DIR      "GetModuleFileName() ­¥ ¯®«ãç¨« ª â «®£"
 #define ERR_DISK_FULL            "„¨áª ¯®«®­"
 #define ERR_DIRECTORY_FULL       "Š â «®£ ¯®«®­"
 #define ERR_MACOS_GENERIC        "MacOS á®®¡é¨«  ®è¨¡ªã (%d)"
 #define ERR_OS2_GENERIC          "OS/2 á®®¡é¨«  ®è¨¡ªã (%d)"
 #define ERR_VOL_LOCKED_HW        "’®¬ ¡«®ª¨à®¢ ­  ¯¯ à â­®"
 #define ERR_VOL_LOCKED_SW        "’®¬ ¡«®ª¨à®¢ ­ ¯à®£à ¬¬­®"
 #define ERR_FILE_LOCKED          "” ©« § ¡«®ª¨à®¢ ­"
 #define ERR_FILE_OR_DIR_BUSY     "” ©«/ª â «®£ § ­ïâ"
 #define ERR_FILE_ALREADY_OPEN_W  "” ©« ã¦¥ ®âªàëâ ­  § ¯¨áì"
 #define ERR_FILE_ALREADY_OPEN_R  "” ©« ã¦¥ ®âªàëâ ­  çâ¥­¨¥"
 #define ERR_INVALID_REFNUM       "¥¢¥à­®¥ ª®«¨ç¥áâ¢® ááë«®ª"
 #define ERR_GETTING_FILE_POS     "Žè¨¡ª  ¯à¨ ¯®«ãç¥­¨¨ ¯®§¨æ¨¨ ä ©« "
 #define ERR_VOLUME_OFFLINE       "’®¬ ®âá®¥¤¨­¥­"
 #define ERR_PERMISSION_DENIED    "Žâª § ­® ¢ à §à¥è¥­¨¨"
 #define ERR_VOL_ALREADY_ONLINE   "’®¬ ã¦¥ ¯®¤á®¥¤¨­¥­"
 #define ERR_NO_SUCH_DRIVE        "¥â â ª®£® ¤¨áª "
 #define ERR_NOT_MAC_DISK         "¥ ¤¨áª Macintosh"
 #define ERR_VOL_EXTERNAL_FS      "’®¬ ¯à¨­ ¤«¥¦¨â ¢­¥è­¥© ä ©«®¢®© á¨áâ¥¬¥"
 #define ERR_PROBLEM_RENAME       "à®¡«¥¬  ¯à¨ ¯¥à¥¨¬¥­®¢ ­¨¨"
 #define ERR_BAD_MASTER_BLOCK     "«®å®© £« ¢­ë© ¡«®ª ª â «®£ "
 #define ERR_CANT_MOVE_FORBIDDEN  "®¯ëâª  ¯¥à¥¬¥áâ¨âì § ¯à¥é¥­ "
 #define ERR_WRONG_VOL_TYPE       "¥¢¥à­ë© â¨¯ â®¬ "
 #define ERR_SERVER_VOL_LOST      "‘¥à¢¥à­ë© â®¬ ¡ë« ®âá®¥¤¨­¥­"
 #define ERR_FILE_ID_NOT_FOUND    "ˆ¤¥­â¨ä¨ª â®à ä ©«  ­¥ ­ ©¤¥­"
 #define ERR_FILE_ID_EXISTS       "ˆ¤¥­â¨ä¨ª â®à ä ©«  ã¦¥ áãé¥áâ¢ã¥â"
 #define ERR_SERVER_NO_RESPOND    "‘¥à¢¥à ­¥ ®â¢¥ç ¥â"
 #define ERR_USER_AUTH_FAILED     "ˆ¤¥­â¨ä¨ª æ¨ï ¯®«ì§®¢ â¥«ï ­¥ ã¤ « áì"
 #define ERR_PWORD_EXPIRED        " à®«ì ­  á¥à¢¥à¥ ãáâ à¥«"
 #define ERR_ACCESS_DENIED        "Žâª § ­® ¢ ¤®áâã¯¥"
 #define ERR_NOT_A_DOS_DISK       "¥ ¤¨áª DOS"
 #define ERR_SHARING_VIOLATION    " àãè¥­¨¥ á®¢¬¥áâ­®£® ¤®áâã¯ "
 #define ERR_CANNOT_MAKE          "¥ ¬®£ã á®¡à âì"
 #define ERR_DEV_IN_USE           "“áâà®©áâ¢® ã¦¥ ¨á¯®«ì§ã¥âáï"
 #define ERR_OPEN_FAILED          "Žâªàëâ¨¥ ­¥ ã¤ «®áì"
 #define ERR_PIPE_BUSY            "Š®­¢¥©¥à § ­ïâ"
 #define ERR_SHARING_BUF_EXCEEDED " §¤¥«ï¥¬ë© ¡ãä¥à ¯¥à¥¯®«­¥­"
 #define ERR_TOO_MANY_HANDLES     "‘«¨èª®¬ ¬­®£® ®âªàëâëå ¤¥áªà¨¯â®à®¢"
 #define ERR_SEEK_ERROR           "Žè¨¡ª  ¯®§¨æ¨®­¨à®¢ ­¨ï"
 #define ERR_DEL_CWD              "®¯ëâª  ã¤ «¨âì â¥ªãé¨© à ¡®ç¨© ª â «®£"
 #define ERR_WRITE_PROTECT_ERROR  "Žè¨¡ª  § é¨âë § ¯¨á¨"
 #define ERR_WRITE_FAULT          "Žè¨¡ª  § ¯¨á¨"
 #define ERR_LOCK_VIOLATION       " àãè¥­¨¥ ¡«®ª¨à®¢ª¨"
 #define ERR_GEN_FAILURE          "Ž¡é¨© á¡®©"
 #define ERR_UNCERTAIN_MEDIA      "¥®¯à¥¤¥«¥­­ë© ­®á¨â¥«ì"
 #define ERR_PROT_VIOLATION       " àãè¥­¨¥ § é¨âë"
 #define ERR_BROKEN_PIPE          "‘«®¬ ­­ë© ª®­¢¥©¥à"

#elif (PHYSFS_LANG == PHYSFS_LANG_RUSSIAN_ISO_8859_5)
 #define DIR_ARCHIVE_DESCRIPTION  "½Õ ÐàåØÒ, ÝÕßÞáàÕÔáâÒÕÝÝëÙ ÒÒÞÔ/ÒëÒÞÔ äÐÙÛÞÒÞÙ áØáâÕÜë"
 #define GRP_ARCHIVE_DESCRIPTION  "ÄÞàÜÐâ ÓàãßßÞÒÞÓÞ äÐÙÛÐ Build engine"
 #define HOG_ARCHIVE_DESCRIPTION  "Descent I/II HOG file format"
 #define MVL_ARCHIVE_DESCRIPTION  "Descent II Movielib format"
 #define ZIP_ARCHIVE_DESCRIPTION  "PkZip/WinZip/Info-Zip áÞÒÜÕáâØÜëÙ"
 #define WAD_ARCHIVE_DESCRIPTION  "DOOM engine format"  // FIXME: translate this line if needed

 #define ERR_IS_INITIALIZED       "ÃÖÕ ØÝØæØÐÛØ×ØàÞÒÐÝ"
 #define ERR_NOT_INITIALIZED      "½Õ ØÝØæØÐÛØ×ØàÞÒÐÝ"
 #define ERR_INVALID_ARGUMENT     "½ÕÒÕàÝëÙ ÐàÓãÜÕÝâ"
 #define ERR_FILES_STILL_OPEN     "ÄÐÙÛë ÕéÕ ÞâÚàëâë"
 #define ERR_NO_DIR_CREATE        "½Õ ÜÞÓã áÞ×ÔÐâì ÚÐâÐÛÞÓØ"
 #define ERR_OUT_OF_MEMORY        "ºÞÝçØÛÐáì ßÐÜïâì"
 #define ERR_NOT_IN_SEARCH_PATH   "½Õâ âÐÚÞÓÞ íÛÕÜÕÝâÐ Ò ßãâØ ßÞØáÚÐ"
 #define ERR_NOT_SUPPORTED        "¾ßÕàÐæØï ÝÕ ßÞÔÔÕàÖØÒÐÕâáï"
 #define ERR_UNSUPPORTED_ARCHIVE  "°àåØÒë âÐÚÞÓÞ âØßÐ ÝÕ ßÞÔÔÕàÖØÒÐîâáï"
 #define ERR_NOT_A_HANDLE         "½Õ äÐÙÛÞÒëÙ ÔÕáÚàØßâÞà"
 #define ERR_INSECURE_FNAME       "½ÕÑÕ×ÞßÐáÝÞÕ ØÜï äÐÙÛÐ"
 #define ERR_SYMLINK_DISALLOWED   "ÁØÜÒÞÛìÝëÕ ááëÛÚØ ÞâÚÛîçÕÝë"
 #define ERR_NO_WRITE_DIR         "ºÐâÐÛÞÓ ÔÛï ×ÐßØáØ ÝÕ ãáâÐÝÞÒÛÕÝ"
 #define ERR_NO_SUCH_FILE         "ÄÐÙÛ ÝÕ ÝÐÙÔÕÝ"
 #define ERR_NO_SUCH_PATH         "¿ãâì ÝÕ ÝÐÙÔÕÝ"
 #define ERR_NO_SUCH_VOLUME       "ÂÞÜ ÝÕ ÝÐÙÔÕÝ"
 #define ERR_PAST_EOF             "·Ð ÚÞÝæÞÜ äÐÙÛÐ"
 #define ERR_ARC_IS_READ_ONLY     "°àåØÒ âÞÛìÚÞ ÔÛï çâÕÝØï"
 #define ERR_IO_ERROR             "¾èØÑÚÐ ÒÒÞÔÐ/ÒëÒÞÔÐ"
 #define ERR_CANT_SET_WRITE_DIR   "½Õ ÜÞÓã ãáâÐÝÞÒØâì ÚÐâÐÛÞÓ ÔÛï ×ÐßØáØ"
 #define ERR_SYMLINK_LOOP         "±ÕáÚÞÝÕçÝëÙ æØÚÛ áØÜÒÞÛìÝÞÙ ááëÛÚØ"
 #define ERR_COMPRESSION          "¾èØÑÚÐ (ÀÐá)ßÐÚÞÒÚØ"
 #define ERR_NOT_IMPLEMENTED      "½Õ àÕÐÛØ×ÞÒÐÝÞ"
 #define ERR_OS_ERROR             "¾ßÕàÐæØÞÝÝÐï áØáâÕÜÐ áÞÞÑéØÛÐ ÞèØÑÚã"
 #define ERR_FILE_EXISTS          "ÄÐÙÛ ãÖÕ áãéÕáâÒãÕâ"
 #define ERR_NOT_A_FILE           "½Õ äÐÙÛ"
 #define ERR_NOT_A_DIR            "½Õ ÚÐâÐÛÞÓ"
 #define ERR_NOT_AN_ARCHIVE       "½Õ ÐàåØÒ"
 #define ERR_CORRUPTED            "¿ÞÒàÕÖÔÕÝÝëÙ ÐàåØÒ"
 #define ERR_SEEK_OUT_OF_RANGE    "¿Þ×ØæØÞÝØàÞÒÐÝØÕ ×Ð ßàÕÔÕÛë"
 #define ERR_BAD_FILENAME         "½ÕÒÕàÝÞÕ ØÜï äÐÙÛÐ"
 #define ERR_PHYSFS_BAD_OS_CALL   "(BUG) PhysicsFS ÒëßÞÛÝØÛÐ ÝÕÒÕàÝëÙ áØáâÕÜÝëÙ Òë×ÞÒ"
 #define ERR_ARGV0_IS_NULL        "argv0 is NULL"
 #define ERR_ZLIB_NEED_DICT       "zlib: ÝãÖÕÝ áÛÞÒÐàì"
 #define ERR_ZLIB_DATA_ERROR      "zlib: ÞèØÑÚÐ ÔÐÝÝëå"
 #define ERR_ZLIB_MEMORY_ERROR    "zlib: ÞèØÑÚÐ ßÐÜïâØ"
 #define ERR_ZLIB_BUFFER_ERROR    "zlib: ÞèØÑÚÐ ÑãäÕàÐ"
 #define ERR_ZLIB_VERSION_ERROR   "zlib: ÞèØÑÚÐ ÒÕàáØØ"
 #define ERR_ZLIB_UNKNOWN_ERROR   "zlib: ÝÕØ×ÒÕáâÝÐï ÞèØÑÚÐ"
 #define ERR_SEARCHPATH_TRUNC     "¿ãâì ßÞØáÚÐ ÞÑàÕ×ÐÝ"
 #define ERR_GETMODFN_TRUNC       "GetModuleFileName() ÞÑàÕ×ÐÝ"
 #define ERR_GETMODFN_NO_DIR      "GetModuleFileName() ÝÕ ßÞÛãçØÛ ÚÐâÐÛÞÓ"
 #define ERR_DISK_FULL            "´ØáÚ ßÞÛÞÝ"
 #define ERR_DIRECTORY_FULL       "ºÐâÐÛÞÓ ßÞÛÞÝ"
 #define ERR_MACOS_GENERIC        "MacOS áÞÞÑéØÛÐ ÞèØÑÚã (%d)"
 #define ERR_OS2_GENERIC          "OS/2 áÞÞÑéØÛÐ ÞèØÑÚã (%d)"
 #define ERR_VOL_LOCKED_HW        "ÂÞÜ ÑÛÞÚØàÞÒÐÝ ÐßßÐàÐâÝÞ"
 #define ERR_VOL_LOCKED_SW        "ÂÞÜ ÑÛÞÚØàÞÒÐÝ ßàÞÓàÐÜÜÝÞ"
 #define ERR_FILE_LOCKED          "ÄÐÙÛ ×ÐÑÛÞÚØàÞÒÐÝ"
 #define ERR_FILE_OR_DIR_BUSY     "ÄÐÙÛ/ÚÐâÐÛÞÓ ×ÐÝïâ"
 #define ERR_FILE_ALREADY_OPEN_W  "ÄÐÙÛ ãÖÕ ÞâÚàëâ ÝÐ ×ÐßØáì"
 #define ERR_FILE_ALREADY_OPEN_R  "ÄÐÙÛ ãÖÕ ÞâÚàëâ ÝÐ çâÕÝØÕ"
 #define ERR_INVALID_REFNUM       "½ÕÒÕàÝÞÕ ÚÞÛØçÕáâÒÞ ááëÛÞÚ"
 #define ERR_GETTING_FILE_POS     "¾èØÑÚÐ ßàØ ßÞÛãçÕÝØØ ßÞ×ØæØØ äÐÙÛÐ"
 #define ERR_VOLUME_OFFLINE       "ÂÞÜ ÞâáÞÕÔØÝÕÝ"
 #define ERR_PERMISSION_DENIED    "¾âÚÐ×ÐÝÞ Ò àÐ×àÕèÕÝØØ"
 #define ERR_VOL_ALREADY_ONLINE   "ÂÞÜ ãÖÕ ßÞÔáÞÕÔØÝÕÝ"
 #define ERR_NO_SUCH_DRIVE        "½Õâ âÐÚÞÓÞ ÔØáÚÐ"
 #define ERR_NOT_MAC_DISK         "½Õ ÔØáÚ Macintosh"
 #define ERR_VOL_EXTERNAL_FS      "ÂÞÜ ßàØÝÐÔÛÕÖØâ ÒÝÕèÝÕÙ äÐÙÛÞÒÞÙ áØáâÕÜÕ"
 #define ERR_PROBLEM_RENAME       "¿àÞÑÛÕÜÐ ßàØ ßÕàÕØÜÕÝÞÒÐÝØØ"
 #define ERR_BAD_MASTER_BLOCK     "¿ÛÞåÞÙ ÓÛÐÒÝëÙ ÑÛÞÚ ÚÐâÐÛÞÓÐ"
 #define ERR_CANT_MOVE_FORBIDDEN  "¿ÞßëâÚÐ ßÕàÕÜÕáâØâì ×ÐßàÕéÕÝÐ"
 #define ERR_WRONG_VOL_TYPE       "½ÕÒÕàÝëÙ âØß âÞÜÐ"
 #define ERR_SERVER_VOL_LOST      "ÁÕàÒÕàÝëÙ âÞÜ ÑëÛ ÞâáÞÕÔØÝÕÝ"
 #define ERR_FILE_ID_NOT_FOUND    "¸ÔÕÝâØäØÚÐâÞà äÐÙÛÐ ÝÕ ÝÐÙÔÕÝ"
 #define ERR_FILE_ID_EXISTS       "¸ÔÕÝâØäØÚÐâÞà äÐÙÛÐ ãÖÕ áãéÕáâÒãÕâ"
 #define ERR_SERVER_NO_RESPOND    "ÁÕàÒÕà ÝÕ ÞâÒÕçÐÕâ"
 #define ERR_USER_AUTH_FAILED     "¸ÔÕÝâØäØÚÐæØï ßÞÛì×ÞÒÐâÕÛï ÝÕ ãÔÐÛÐáì"
 #define ERR_PWORD_EXPIRED        "¿ÐàÞÛì ÝÐ áÕàÒÕàÕ ãáâÐàÕÛ"
 #define ERR_ACCESS_DENIED        "¾âÚÐ×ÐÝÞ Ò ÔÞáâãßÕ"
 #define ERR_NOT_A_DOS_DISK       "½Õ ÔØáÚ DOS"
 #define ERR_SHARING_VIOLATION    "½ÐàãèÕÝØÕ áÞÒÜÕáâÝÞÓÞ ÔÞáâãßÐ"
 #define ERR_CANNOT_MAKE          "½Õ ÜÞÓã áÞÑàÐâì"
 #define ERR_DEV_IN_USE           "ÃáâàÞÙáâÒÞ ãÖÕ ØáßÞÛì×ãÕâáï"
 #define ERR_OPEN_FAILED          "¾âÚàëâØÕ ÝÕ ãÔÐÛÞáì"
 #define ERR_PIPE_BUSY            "ºÞÝÒÕÙÕà ×ÐÝïâ"
 #define ERR_SHARING_BUF_EXCEEDED "ÀÐ×ÔÕÛïÕÜëÙ ÑãäÕà ßÕàÕßÞÛÝÕÝ"
 #define ERR_TOO_MANY_HANDLES     "ÁÛØèÚÞÜ ÜÝÞÓÞ ÞâÚàëâëå ÔÕáÚàØßâÞàÞÒ"
 #define ERR_SEEK_ERROR           "¾èØÑÚÐ ßÞ×ØæØÞÝØàÞÒÐÝØï"
 #define ERR_DEL_CWD              "¿ÞßëâÚÐ ãÔÐÛØâì âÕÚãéØÙ àÐÑÞçØÙ ÚÐâÐÛÞÓ"
 #define ERR_WRITE_PROTECT_ERROR  "¾èØÑÚÐ ×ÐéØâë ×ÐßØáØ"
 #define ERR_WRITE_FAULT          "¾èØÑÚÐ ×ÐßØáØ"
 #define ERR_LOCK_VIOLATION       "½ÐàãèÕÝØÕ ÑÛÞÚØàÞÒÚØ"
 #define ERR_GEN_FAILURE          "¾ÑéØÙ áÑÞÙ"
 #define ERR_UNCERTAIN_MEDIA      "½ÕÞßàÕÔÕÛÕÝÝëÙ ÝÞáØâÕÛì"
 #define ERR_PROT_VIOLATION       "½ÐàãèÕÝØÕ ×ÐéØâë"
 #define ERR_BROKEN_PIPE          "ÁÛÞÜÐÝÝëÙ ÚÞÝÒÕÙÕà"


#elif (PHYSFS_LANG == PHYSFS_LANG_FRENCH)
 #define DIR_ARCHIVE_DESCRIPTION  "Pas d'archive, E/S directes sur système de fichiers"
 #define GRP_ARCHIVE_DESCRIPTION  "Format Groupfile du moteur Build"
 #define HOG_ARCHIVE_DESCRIPTION  "Descent I/II HOG file format"
 #define MVL_ARCHIVE_DESCRIPTION  "Descent II Movielib format"
 #define QPAK_ARCHIVE_DESCRIPTION "Quake I/II format"
 #define ZIP_ARCHIVE_DESCRIPTION  "Compatible PkZip/WinZip/Info-Zip"
 #define WAD_ARCHIVE_DESCRIPTION  "Format WAD du moteur DOOM"

 #define ERR_IS_INITIALIZED       "Déjà initialisé"
 #define ERR_NOT_INITIALIZED      "Non initialisé"
 #define ERR_INVALID_ARGUMENT     "Argument invalide"
 #define ERR_FILES_STILL_OPEN     "Fichiers encore ouverts"
 #define ERR_NO_DIR_CREATE        "Echec de la création de répertoires"
 #define ERR_OUT_OF_MEMORY        "A court de mémoire"
 #define ERR_NOT_IN_SEARCH_PATH   "Aucune entrée dans le chemin de recherche"
 #define ERR_NOT_SUPPORTED        "Opération non supportée"
 #define ERR_UNSUPPORTED_ARCHIVE  "Type d'archive non supportée"
 #define ERR_NOT_A_HANDLE         "Pas un descripteur de fichier"
 #define ERR_INSECURE_FNAME       "Nom de fichier dangereux"
 #define ERR_SYMLINK_DISALLOWED   "Les liens symboliques sont désactivés"
 #define ERR_NO_WRITE_DIR         "Le répertoire d'écriture n'est pas spécifié"
 #define ERR_NO_SUCH_FILE         "Fichier non trouvé"
 #define ERR_NO_SUCH_PATH         "Chemin non trouvé"
 #define ERR_NO_SUCH_VOLUME       "Volume non trouvé"
 #define ERR_PAST_EOF             "Au-delà de la fin du fichier"
 #define ERR_ARC_IS_READ_ONLY     "L'archive est en lecture seule"
 #define ERR_IO_ERROR             "Erreur E/S"
 #define ERR_CANT_SET_WRITE_DIR   "Ne peut utiliser le répertoire d'écriture"
 #define ERR_SYMLINK_LOOP         "Boucle infinie dans les liens symboliques"
 #define ERR_COMPRESSION          "Erreur de (dé)compression"
 #define ERR_NOT_IMPLEMENTED      "Non implémenté"
 #define ERR_OS_ERROR             "Erreur rapportée par le système d'exploitation"
 #define ERR_FILE_EXISTS          "Le fichier existe déjà"
 #define ERR_NOT_A_FILE           "Pas un fichier"
 #define ERR_NOT_A_DIR            "Pas un répertoire"
 #define ERR_NOT_AN_ARCHIVE       "Pas une archive"
 #define ERR_CORRUPTED            "Archive corrompue"
 #define ERR_SEEK_OUT_OF_RANGE    "Pointeur de fichier hors de portée"
 #define ERR_BAD_FILENAME         "Mauvais nom de fichier"
 #define ERR_PHYSFS_BAD_OS_CALL   "(BOGUE) PhysicsFS a fait un mauvais appel système, le salaud"
 #define ERR_ARGV0_IS_NULL        "argv0 est NULL"
 #define ERR_ZLIB_NEED_DICT       "zlib: a besoin du dico"
 #define ERR_ZLIB_DATA_ERROR      "zlib: erreur de données"
 #define ERR_ZLIB_MEMORY_ERROR    "zlib: erreur mémoire"
 #define ERR_ZLIB_BUFFER_ERROR    "zlib: erreur tampon"
 #define ERR_ZLIB_VERSION_ERROR   "zlib: erreur de version"
 #define ERR_ZLIB_UNKNOWN_ERROR   "zlib: erreur inconnue"
 #define ERR_SEARCHPATH_TRUNC     "Le chemin de recherche a été tronqué"
 #define ERR_GETMODFN_TRUNC       "GetModuleFileName() a été tronqué"
 #define ERR_GETMODFN_NO_DIR      "GetModuleFileName() n'a pas de répertoire"
 #define ERR_DISK_FULL            "Disque plein"
 #define ERR_DIRECTORY_FULL       "Répertoire plein"
 #define ERR_MACOS_GENERIC        "Erreur rapportée par MacOS (%d)"
 #define ERR_OS2_GENERIC          "Erreur rapportée par OS/2 (%d)"
 #define ERR_VOL_LOCKED_HW        "Le volume est verrouillé matériellement"
 #define ERR_VOL_LOCKED_SW        "Le volume est verrouillé par logiciel"
 #define ERR_FILE_LOCKED          "Fichier verrouillé"
 #define ERR_FILE_OR_DIR_BUSY     "Fichier/répertoire occupé"
 #define ERR_FILE_ALREADY_OPEN_W  "Fichier déjà ouvert en écriture"
 #define ERR_FILE_ALREADY_OPEN_R  "Fichier déjà ouvert en lecture"
 #define ERR_INVALID_REFNUM       "Numéro de référence invalide"
 #define ERR_GETTING_FILE_POS     "Erreur lors de l'obtention de la position du pointeur de fichier"
 #define ERR_VOLUME_OFFLINE       "Le volume n'est pas en ligne"
 #define ERR_PERMISSION_DENIED    "Permission refusée"
 #define ERR_VOL_ALREADY_ONLINE   "Volumé déjà en ligne"
 #define ERR_NO_SUCH_DRIVE        "Lecteur inexistant"
 #define ERR_NOT_MAC_DISK         "Pas un disque Macintosh"
 #define ERR_VOL_EXTERNAL_FS      "Le volume appartient à un système de fichiers externe"
 #define ERR_PROBLEM_RENAME       "Problème lors du renommage"
 #define ERR_BAD_MASTER_BLOCK     "Mauvais block maitre de répertoire"
 #define ERR_CANT_MOVE_FORBIDDEN  "Essai de déplacement interdit"
 #define ERR_WRONG_VOL_TYPE       "Mauvais type de volume"
 #define ERR_SERVER_VOL_LOST      "Le volume serveur a été déconnecté"
 #define ERR_FILE_ID_NOT_FOUND    "Identificateur de fichier non trouvé"
 #define ERR_FILE_ID_EXISTS       "Identificateur de fichier existe déjà"
 #define ERR_SERVER_NO_RESPOND    "Le serveur ne répond pas"
 #define ERR_USER_AUTH_FAILED     "Authentification de l'utilisateur échouée"
 #define ERR_PWORD_EXPIRED        "Le mot de passe a expiré sur le serveur"
 #define ERR_ACCESS_DENIED        "Accès refusé"
 #define ERR_NOT_A_DOS_DISK       "Pas un disque DOS"
 #define ERR_SHARING_VIOLATION    "Violation de partage"
 #define ERR_CANNOT_MAKE          "Ne peut faire"
 #define ERR_DEV_IN_USE           "Périphérique déjà en utilisation"
 #define ERR_OPEN_FAILED          "Ouverture échouée"
 #define ERR_PIPE_BUSY            "Le tube est occupé"
 #define ERR_SHARING_BUF_EXCEEDED "Tampon de partage dépassé"
 #define ERR_TOO_MANY_HANDLES     "Trop de descripteurs ouverts"
 #define ERR_SEEK_ERROR           "Erreur de positionement"
 #define ERR_DEL_CWD              "Essai de supprimer le répertoire courant"
 #define ERR_WRITE_PROTECT_ERROR  "Erreur de protection en écriture"
 #define ERR_WRITE_FAULT          "Erreur d'écriture"
 #define ERR_LOCK_VIOLATION       "Violation de verrou"
 #define ERR_GEN_FAILURE          "Echec général"
 #define ERR_UNCERTAIN_MEDIA      "Média incertain"
 #define ERR_PROT_VIOLATION       "Violation de protection"
 #define ERR_BROKEN_PIPE          "Tube cassé"

#elif (PHYSFS_LANG == PHYSFS_LANG_PORTUGUESE_BR)
 #define DIR_ARCHIVE_DESCRIPTION  "Não arquivo, E/S sistema de arquivos direto"
 #define GRP_ARCHIVE_DESCRIPTION  "Formato Groupfile do engine Build"
 #define HOG_ARCHIVE_DESCRIPTION  "Formato Descent I/II HOG file"
 #define MVL_ARCHIVE_DESCRIPTION  "Formato Descent II Movielib"
 #define QPAK_ARCHIVE_DESCRIPTION "Formato Quake I/II"
 #define ZIP_ARCHIVE_DESCRIPTION  "Formato compatível PkZip/WinZip/Info-Zip"
 #define WAD_ARCHIVE_DESCRIPTION  "Formato WAD do engine DOOM"

 #define ERR_IS_INITIALIZED       "Já inicializado"
 #define ERR_NOT_INITIALIZED      "Não inicializado"
 #define ERR_INVALID_ARGUMENT     "Argumento inválido"
 #define ERR_FILES_STILL_OPEN     "Arquivos ainda abertos"
 #define ERR_NO_DIR_CREATE        "Falha na criação de diretórios"
 #define ERR_OUT_OF_MEMORY        "Memória insuficiente"
 #define ERR_NOT_IN_SEARCH_PATH   "Entrada não encontrada no caminho de busca"
 #define ERR_NOT_SUPPORTED        "Operação não suportada"
 #define ERR_UNSUPPORTED_ARCHIVE  "Tipo de arquivo não suportado"
 #define ERR_NOT_A_HANDLE         "Não é um handler de arquivo"
 #define ERR_INSECURE_FNAME       "Nome de arquivo inseguro"
 #define ERR_SYMLINK_DISALLOWED   "Links simbólicos desabilitados"
 #define ERR_NO_WRITE_DIR         "Diretório de escrita não definido"
 #define ERR_NO_SUCH_FILE         "Arquivo não encontrado"
 #define ERR_NO_SUCH_PATH         "Caminho não encontrado"
 #define ERR_NO_SUCH_VOLUME       "Volume não encontrado"
 #define ERR_PAST_EOF             "Passou o fim do arquivo"
 #define ERR_ARC_IS_READ_ONLY     "Arquivo é somente de leitura"
 #define ERR_IO_ERROR             "Erro de E/S"
 #define ERR_CANT_SET_WRITE_DIR   "Não foi possível definir diretório de escrita"
 #define ERR_SYMLINK_LOOP         "Loop infinito de link simbólico"
 #define ERR_COMPRESSION          "Erro de (Des)compressão"
 #define ERR_NOT_IMPLEMENTED      "Não implementado"
 #define ERR_OS_ERROR             "Erro reportado pelo Sistema Operacional"
 #define ERR_FILE_EXISTS          "Arquivo já existente"
 #define ERR_NOT_A_FILE           "Não é um arquivo"
 #define ERR_NOT_A_DIR            "Não é um diretório"
 #define ERR_NOT_AN_ARCHIVE       "Não é um pacote"
 #define ERR_CORRUPTED            "Pacote corrompido"
 #define ERR_SEEK_OUT_OF_RANGE    "Posicionamento além do tamanho"
 #define ERR_BAD_FILENAME         "Nome de arquivo inválido"
 #define ERR_PHYSFS_BAD_OS_CALL   "(BUG) PhysicsFS realizou uma chamada de sistema inválida"
 #define ERR_ARGV0_IS_NULL        "argv0 é NULL"
 #define ERR_ZLIB_NEED_DICT       "zlib: precisa de diretório"
 #define ERR_ZLIB_DATA_ERROR      "zlib: erro nos dados"
 #define ERR_ZLIB_MEMORY_ERROR    "zlib: erro de memória"
 #define ERR_ZLIB_BUFFER_ERROR    "zlib: erro de buffer"
 #define ERR_ZLIB_VERSION_ERROR   "zlib: erro na version"
 #define ERR_ZLIB_UNKNOWN_ERROR   "zlib: erro desconhecido"
 #define ERR_SEARCHPATH_TRUNC     "Caminho de procura quebrado"
 #define ERR_GETMODFN_TRUNC       "GetModuleFileName() foi quebrado"
 #define ERR_GETMODFN_NO_DIR      "GetModuleFileName() nao teve diretório"
 #define ERR_DISK_FULL            "Disco cheio"
 #define ERR_DIRECTORY_FULL       "Diretório cheio"
 #define ERR_MACOS_GENERIC        "MacOS reportou um erro (%d)"
 #define ERR_OS2_GENERIC          "OS/2 reportou um erro (%d)"
 #define ERR_VOL_LOCKED_HW        "Volume travado por hardware"
 #define ERR_VOL_LOCKED_SW        "Volume travado por software"
 #define ERR_FILE_LOCKED          "Arquivo travado"
 #define ERR_FILE_OR_DIR_BUSY     "Arquivo/Diretório está em uso"
 #define ERR_FILE_ALREADY_OPEN_W  "Arquivo já aberto para escrita"
 #define ERR_FILE_ALREADY_OPEN_R  "Arquivo já aberto para leitura"
 #define ERR_INVALID_REFNUM       "Número de referência"
 #define ERR_GETTING_FILE_POS     "Erro ao tentar obter posição do arquivo"
 #define ERR_VOLUME_OFFLINE       "Volume está indisponível"
 #define ERR_PERMISSION_DENIED    "Permissão negada"
 #define ERR_VOL_ALREADY_ONLINE   "Volume disponível"
 #define ERR_NO_SUCH_DRIVE        "Drive inexistente"
 #define ERR_NOT_MAC_DISK         "Não é um disco Macintosh"
 #define ERR_VOL_EXTERNAL_FS      "Volume pertence a um sistema de arquivos externo"
 #define ERR_PROBLEM_RENAME       "Problema durante renomeação"
 #define ERR_BAD_MASTER_BLOCK     "Bloco master do diretório inválido"
 #define ERR_CANT_MOVE_FORBIDDEN  "Tentativa de mover proibida"
 #define ERR_WRONG_VOL_TYPE       "Tipo inválido de volume"
 #define ERR_SERVER_VOL_LOST      "Volume servidor desconectado"
 #define ERR_FILE_ID_NOT_FOUND    "ID de Arquivo não encontrado"
 #define ERR_FILE_ID_EXISTS       "ID de Arquivo já existente"
 #define ERR_SERVER_NO_RESPOND    "Servidor não respondendo"
 #define ERR_USER_AUTH_FAILED     "Autenticação de usuário falhada"
 #define ERR_PWORD_EXPIRED        "Password foi expirada no servidor"
 #define ERR_ACCESS_DENIED        "Accesso negado"
 #define ERR_NOT_A_DOS_DISK       "Não é um disco DOS"
 #define ERR_SHARING_VIOLATION    "Violação de compartilhamento"
 #define ERR_CANNOT_MAKE          "Não pode ser feito"
 #define ERR_DEV_IN_USE           "Device já em uso"
 #define ERR_OPEN_FAILED          "Falaha na abertura"
 #define ERR_PIPE_BUSY            "Fila ocupada"
 #define ERR_SHARING_BUF_EXCEEDED "Buffer de compartilhamento excedeu"
 #define ERR_TOO_MANY_HANDLES     "Muitos handles abertos"
 #define ERR_SEEK_ERROR           "Erro de posicionamento"
 #define ERR_DEL_CWD              "Tentando remover diretório de trabalho atual"
 #define ERR_WRITE_PROTECT_ERROR  "Erro de proteção de escrita"
 #define ERR_WRITE_FAULT          "Erro de escrita"
 #define ERR_LOCK_VIOLATION       "Violação de trava"
 #define ERR_GEN_FAILURE          "Falha geral"
 #define ERR_UNCERTAIN_MEDIA      "Media incerta"
 #define ERR_PROT_VIOLATION       "Violação de proteção"
 #define ERR_BROKEN_PIPE          "Fila quebrada"

#elif (PHYSFS_LANG == PHYSFS_LANG_SPANISH)
 #define DIR_ARCHIVE_DESCRIPTION  "No es un archivo, E/S directa al sistema de ficheros"
 #define GRP_ARCHIVE_DESCRIPTION  "Formato Build engine Groupfile"
 #define HOG_ARCHIVE_DESCRIPTION  "Formato Descent I/II HOG file"
 #define MVL_ARCHIVE_DESCRIPTION  "Formato Descent II Movielib"
 #define QPAK_ARCHIVE_DESCRIPTION "Formato Quake I/II"
 #define ZIP_ARCHIVE_DESCRIPTION  "Compatible con PkZip/WinZip/Info-Zip"

 #define ERR_IS_INITIALIZED       "Ya estaba inicializado"
 #define ERR_NOT_INITIALIZED      "No está inicializado"
 #define ERR_INVALID_ARGUMENT     "Argumento inválido"
 #define ERR_FILES_STILL_OPEN     "Archivos aún abiertos"
 #define ERR_NO_DIR_CREATE        "Fallo al crear los directorios"
 #define ERR_OUT_OF_MEMORY        "Memoria agotada"
 #define ERR_NOT_IN_SEARCH_PATH   "No existe tal entrada en la ruta de búsqueda"
 #define ERR_NOT_SUPPORTED        "Operación no soportada"
 #define ERR_UNSUPPORTED_ARCHIVE  "Tipo de archivo no soportado"
 #define ERR_NOT_A_HANDLE         "No es un manejador de ficheo (file handle)"
 #define ERR_INSECURE_FNAME       "Nombre de archivo inseguro"
 #define ERR_SYMLINK_DISALLOWED   "Los enlaces simbólicos están desactivados"
 #define ERR_NO_WRITE_DIR         "No has configurado un directorio de escritura"
 #define ERR_NO_SUCH_FILE         "Archivo no encontrado"
 #define ERR_NO_SUCH_PATH         "Ruta no encontrada"
 #define ERR_NO_SUCH_VOLUME       "Volumen no encontrado"
 #define ERR_PAST_EOF             "Te pasaste del final del archivo"
 #define ERR_ARC_IS_READ_ONLY     "El archivo es de sólo lectura"
 #define ERR_IO_ERROR             "Error E/S"
 #define ERR_CANT_SET_WRITE_DIR   "No puedo configurar el directorio de escritura"
 #define ERR_SYMLINK_LOOP         "Bucle infnito de enlaces simbólicos"
 #define ERR_COMPRESSION          "Error de (des)compresión"
 #define ERR_NOT_IMPLEMENTED      "No implementado"
 #define ERR_OS_ERROR             "El sistema operativo ha devuelto un error"
 #define ERR_FILE_EXISTS          "El archivo ya existe"
 #define ERR_NOT_A_FILE           "No es un archivo"
 #define ERR_NOT_A_DIR            "No es un directorio"
 #define ERR_NOT_AN_ARCHIVE       "No es un archivo"
 #define ERR_CORRUPTED            "Archivo corrupto"
 #define ERR_SEEK_OUT_OF_RANGE    "Búsqueda fuera de rango"
 #define ERR_BAD_FILENAME         "Nombre de archivo incorrecto"
 #define ERR_PHYSFS_BAD_OS_CALL   "(BUG) PhysicsFS ha hecho una llamada incorrecta al sistema"
 #define ERR_ARGV0_IS_NULL        "argv0 es NULL"
 #define ERR_ZLIB_NEED_DICT       "zlib: necesito diccionario"
 #define ERR_ZLIB_DATA_ERROR      "zlib: error de datos"
 #define ERR_ZLIB_MEMORY_ERROR    "zlib: error de memoria"
 #define ERR_ZLIB_BUFFER_ERROR    "zlib: error de buffer"
 #define ERR_ZLIB_VERSION_ERROR   "zlib: error de versión"
 #define ERR_ZLIB_UNKNOWN_ERROR   "zlib: error desconocido"
 #define ERR_SEARCHPATH_TRUNC     "La ruta de búsqueda ha sido truncada"
 #define ERR_GETMODFN_TRUNC       "GetModuleFileName() ha sido truncado"
 #define ERR_GETMODFN_NO_DIR      "GetModuleFileName() no tenia directorio"
 #define ERR_DISK_FULL            "El disco está lleno"
 #define ERR_DIRECTORY_FULL       "El directorio está lleno"
 #define ERR_MACOS_GENERIC        "MacOS ha devuelto un error (%d)"
 #define ERR_OS2_GENERIC          "OS/2 ha devuelto un error (%d)"
 #define ERR_VOL_LOCKED_HW        "El volumen está bloqueado por el hardware"
 #define ERR_VOL_LOCKED_SW        "El volumen está bloqueado por el software"
 #define ERR_FILE_LOCKED          "El archivo está bloqueado"
 #define ERR_FILE_OR_DIR_BUSY     "Fichero o directorio ocupados"
 #define ERR_FILE_ALREADY_OPEN_W  "Fichero ya abierto para escritura"
 #define ERR_FILE_ALREADY_OPEN_R  "Fichero ya abierto para lectura"
 #define ERR_INVALID_REFNUM       "El número de referencia no es válido"
 #define ERR_GETTING_FILE_POS     "Error al tomar la posición del fichero"
 #define ERR_VOLUME_OFFLINE       "El volumen está desconectado"
 #define ERR_PERMISSION_DENIED    "Permiso denegado"
 #define ERR_VOL_ALREADY_ONLINE   "El volumen ya estaba conectado"
 #define ERR_NO_SUCH_DRIVE        "No existe tal unidad"
 #define ERR_NOT_MAC_DISK         "No es un disco Macintosh"
 #define ERR_VOL_EXTERNAL_FS      "El volumen pertence a un sistema de ficheros externo"
 #define ERR_PROBLEM_RENAME       "Problemas al renombrar"
 #define ERR_BAD_MASTER_BLOCK     "Bloque maestro de directorios incorrecto"
 #define ERR_CANT_MOVE_FORBIDDEN  "Intento de mover forbidden"
 #define ERR_WRONG_VOL_TYPE       "Tipo de volumen incorrecto"
 #define ERR_SERVER_VOL_LOST      "El servidor de volúmenes ha sido desconectado"
 #define ERR_FILE_ID_NOT_FOUND    "Identificador de archivo no encontrado"
 #define ERR_FILE_ID_EXISTS       "El identificador de archivo ya existe"
 #define ERR_SERVER_NO_RESPOND    "El servidor no responde"
 #define ERR_USER_AUTH_FAILED     "Fallo al autentificar el usuario"
 #define ERR_PWORD_EXPIRED        "La Password  en el servidor ha caducado"
 #define ERR_ACCESS_DENIED        "Acceso denegado"
 #define ERR_NOT_A_DOS_DISK       "No es un disco de DOS"
 #define ERR_SHARING_VIOLATION    "Violación al compartir"
 #define ERR_CANNOT_MAKE          "No puedo hacer make"
 #define ERR_DEV_IN_USE           "El dispositivo ya estaba en uso"
 #define ERR_OPEN_FAILED          "Fallo al abrir"
 #define ERR_PIPE_BUSY            "Tubería ocupada"
 #define ERR_SHARING_BUF_EXCEEDED "Buffer de compartición sobrepasado"
 #define ERR_TOO_MANY_HANDLES     "Demasiados manejadores (handles)"
 #define ERR_SEEK_ERROR           "Error de búsqueda"
 #define ERR_DEL_CWD              "Intentando borrar el directorio de trabajo actual"
 #define ERR_WRITE_PROTECT_ERROR  "Error de protección contra escritura"
 #define ERR_WRITE_FAULT          "Fallo al escribir"
 #define ERR_LOCK_VIOLATION       "Violación del bloqueo"
 #define ERR_GEN_FAILURE          "Fallo general"
 #define ERR_UNCERTAIN_MEDIA      "Medio incierto"
 #define ERR_PROT_VIOLATION       "Violación de la protección"
 #define ERR_BROKEN_PIPE          "Tubería rota"

#else
 #error Please define PHYSFS_LANG.
#endif

/* end LANG section. */

struct __PHYSFS_DIRHANDLE__;
struct __PHYSFS_FILEFUNCTIONS__;


typedef struct __PHYSFS_LINKEDSTRINGLIST__
{
    char *str;
    struct __PHYSFS_LINKEDSTRINGLIST__ *next;
} LinkedStringList;


typedef struct __PHYSFS_FILEHANDLE__
{
        /*
         * This is reserved for the driver to store information.
         */
    void *opaque;

        /*
         * Non-zero if file opened for reading, zero if write/append.
         */
    PHYSFS_uint8 forReading;

        /*
         * This is the buffer, if one is set (NULL otherwise). Don't touch.
         */
    PHYSFS_uint8 *buffer;

        /*
         * This is the buffer size, if one is set (0 otherwise). Don't touch.
         */
    PHYSFS_uint32 bufsize;

        /*
         * This is the buffer fill size. Don't touch.
         */
    PHYSFS_uint32 buffill;

        /*
         * This is the buffer position. Don't touch.
         */
    PHYSFS_uint32 bufpos;

        /*
         * This should be the DirHandle that created this FileHandle.
         */
    const struct __PHYSFS_DIRHANDLE__ *dirHandle;

        /*
         * Pointer to the file i/o functions for this filehandle.
         */
    const struct __PHYSFS_FILEFUNCTIONS__ *funcs;
} FileHandle;


typedef struct __PHYSFS_FILEFUNCTIONS__
{
        /*
         * Read more from the file.
         * Returns number of objects of (objSize) bytes read from file, -1
         *  if complete failure.
         * On failure, call __PHYSFS_setError().
         */
    PHYSFS_sint64 (*read)(FileHandle *handle, void *buffer,
                          PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);

        /*
         * Write more to the file. Archives don't have to implement this.
         *  (Set it to NULL if not implemented).
         * Returns number of objects of (objSize) bytes written to file, -1
         *  if complete failure.
         * On failure, call __PHYSFS_setError().
         */
    PHYSFS_sint64 (*write)(FileHandle *handle, const void *buffer,
                 PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);

        /*
         * Returns non-zero if at end of file.
         */
    int (*eof)(FileHandle *handle);

        /*
         * Returns byte offset from start of file.
         */
    PHYSFS_sint64 (*tell)(FileHandle *handle);

        /*
         * Move read/write pointer to byte offset from start of file.
         *  Returns non-zero on success, zero on error.
         * On failure, call __PHYSFS_setError().
         */
    int (*seek)(FileHandle *handle, PHYSFS_uint64 offset);

        /*
         * Return number of bytes available in the file, or -1 if you
         *  aren't able to determine.
         * On failure, call __PHYSFS_setError().
         */
    PHYSFS_sint64 (*fileLength)(FileHandle *handle);

        /*
         * Close the file, and free the FileHandle structure (including "opaque").
         *  returns non-zero on success, zero if can't close file.
         * On failure, call __PHYSFS_setError().
         */
    int (*fileClose)(FileHandle *handle);
} FileFunctions;


typedef struct __PHYSFS_DIRHANDLE__
{
        /*
         * This is reserved for the driver to store information.
         */
    void *opaque;

        /*
         * Pointer to the directory i/o functions for this handle.
         */
    const struct __PHYSFS_DIRFUNCTIONS__ *funcs;
} DirHandle;


/*
 * Symlinks should always be followed; PhysicsFS will use
 *  DirFunctions->isSymLink() and make a judgement on whether to
 *  continue to call other methods based on that.
 */
typedef struct __PHYSFS_DIRFUNCTIONS__
{
    const PHYSFS_ArchiveInfo *info;

        /*
         * Returns non-zero if (filename) is a valid archive that this
         *  driver can handle. This filename is in platform-dependent
         *  notation. forWriting is non-zero if this is to be used for
         *  the write directory, and zero if this is to be used for an
         *  element of the search path.
         */
    int (*isArchive)(const char *filename, int forWriting);

        /*
         * Return a DirHandle for dir/archive (name).
         *  This filename is in platform-dependent notation.
         *  forWriting is non-zero if this is to be used for
         *  the write directory, and zero if this is to be used for an
         *  element of the search path.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         */
    DirHandle *(*openArchive)(const char *name, int forWriting);

        /*
         * Returns a list of all files in dirname. Each element of this list
         *  (and its "str" field) will be deallocated with the system's free()
         *  function by the caller, so be sure to explicitly malloc() each
         *  chunk. Omit symlinks if (omitSymLinks) is non-zero.
         * If you have a memory failure, return as much as you can.
         *  This dirname is in platform-independent notation.
         */
    LinkedStringList *(*enumerateFiles)(DirHandle *r,
                                        const char *dirname,
                                        int omitSymLinks);


        /*
         * Returns non-zero if filename can be opened for reading.
         *  This filename is in platform-independent notation.
         *  You should not follow symlinks.
         */
    int (*exists)(DirHandle *r, const char *name);

        /*
         * Returns non-zero if filename is really a directory.
         *  This filename is in platform-independent notation.
         *  Symlinks should be followed; if what the symlink points
         *  to is missing, or isn't a directory, then the retval is zero.
         *
         * Regardless of success or failure, please set *fileExists to
         *  non-zero if the file existed (even if it's a broken symlink!),
         *  zero if it did not.
         */
    int (*isDirectory)(DirHandle *r, const char *name, int *fileExists);

        /*
         * Returns non-zero if filename is really a symlink.
         *  This filename is in platform-independent notation.
         *
         * Regardless of success or failure, please set *fileExists to
         *  non-zero if the file existed (even if it's a broken symlink!),
         *  zero if it did not.
         */
    int (*isSymLink)(DirHandle *r, const char *name, int *fileExists);

        /*
         * Retrieve the last modification time (mtime) of a file.
         *  Returns -1 on failure, or the file's mtime in seconds since
         *  the epoch (Jan 1, 1970) on success.
         *  This filename is in platform-independent notation.
         *
         * Regardless of success or failure, please set *exists to
         *  non-zero if the file existed (even if it's a broken symlink!),
         *  zero if it did not.
         */
    PHYSFS_sint64 (*getLastModTime)(DirHandle *r, const char *fnm, int *exist);

        /*
         * Open file for reading, and return a FileHandle.
         *  This filename is in platform-independent notation.
         * If you can't handle multiple opens of the same file,
         *  you can opt to fail for the second call.
         * Fail if the file does not exist.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         *
         * Regardless of success or failure, please set *fileExists to
         *  non-zero if the file existed (even if it's a broken symlink!),
         *  zero if it did not.
         */
    FileHandle *(*openRead)(DirHandle *r, const char *fname, int *fileExists);

        /*
         * Open file for writing, and return a FileHandle.
         * If the file does not exist, it should be created. If it exists,
         *  it should be truncated to zero bytes. The writing
         *  offset should be the start of the file.
         * This filename is in platform-independent notation.
         *  This method may be NULL.
         * If you can't handle multiple opens of the same file,
         *  you can opt to fail for the second call.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         */
    FileHandle *(*openWrite)(DirHandle *r, const char *filename);

        /*
         * Open file for appending, and return a FileHandle.
         * If the file does not exist, it should be created. The writing
         *  offset should be the end of the file.
         * This filename is in platform-independent notation.
         *  This method may be NULL.
         * If you can't handle multiple opens of the same file,
         *  you can opt to fail for the second call.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         */
    FileHandle *(*openAppend)(DirHandle *r, const char *filename);

        /*
         * Delete a file in the archive/directory.
         *  Return non-zero on success, zero on failure.
         *  This filename is in platform-independent notation.
         *  This method may be NULL.
         * On failure, call __PHYSFS_setError().
         */
    int (*remove)(DirHandle *r, const char *filename);

        /*
         * Create a directory in the archive/directory.
         *  If the application is trying to make multiple dirs, PhysicsFS
         *  will split them up into multiple calls before passing them to
         *  your driver.
         *  Return non-zero on success, zero on failure.
         *  This filename is in platform-independent notation.
         *  This method may be NULL.
         * On failure, call __PHYSFS_setError().
         */
    int (*mkdir)(DirHandle *r, const char *filename);

        /*
         * Close directories/archives, and free the handle, including
         *  the "opaque" entry. This should assume that it won't be called if
         *  there are still files open from this DirHandle.
         */
    void (*dirClose)(DirHandle *r);
} DirFunctions;


/*
 * Call this to set the message returned by PHYSFS_getLastError().
 *  Please only use the ERR_* constants above, or add new constants to the
 *  above group, but I want these all in one place.
 *
 * Calling this with a NULL argument is a safe no-op.
 */
void __PHYSFS_setError(const char *err);


/*
 * Convert (dirName) to platform-dependent notation, then prepend (prepend)
 *  and append (append) to the converted string.
 *
 *  So, on Win32, calling:
 *     __PHYSFS_convertToDependent("C:\", "my/files", NULL);
 *  ...will return the string "C:\my\files".
 *
 * This is a convenience function; you might want to hack something out that
 *  is less generic (and therefore more efficient).
 *
 * Be sure to free() the return value when done with it.
 */
char *__PHYSFS_convertToDependent(const char *prepend,
                                  const char *dirName,
                                  const char *append);

/*
 * Verify that (fname) (in platform-independent notation), in relation
 *  to (h) is secure. That means that each element of fname is checked
 *  for symlinks (if they aren't permitted). Also, elements such as
 *  ".", "..", or ":" are flagged.
 *
 * With some exceptions (like PHYSFS_mkdir(), which builds multiple subdirs
 *  at a time), you should always pass zero for "allowMissing" for efficiency.
 *
 * Returns non-zero if string is safe, zero if there's a security issue.
 *  PHYSFS_getLastError() will specify what was wrong.
 */
int __PHYSFS_verifySecurity(DirHandle *h, const char *fname, int allowMissing);


/*
 * Use this to build the list that your enumerate function should return.
 *  See zip.c for an example of proper use.
 */
LinkedStringList *__PHYSFS_addToLinkedStringList(LinkedStringList *retval,
                                                 LinkedStringList **prev,
                                                 const char *str,
                                                 PHYSFS_sint32 len);


/*
 * When sorting the entries in an archive, we use a modified QuickSort.
 *  When there are less then PHYSFS_QUICKSORT_THRESHOLD entries left to sort,
 *  we switch over to a BubbleSort for the remainder. Tweak to taste.
 *
 * You can override this setting by defining PHYSFS_QUICKSORT_THRESHOLD
 *  before #including "physfs_internal.h".
 */
#ifndef PHYSFS_QUICKSORT_THRESHOLD
#define PHYSFS_QUICKSORT_THRESHOLD 4
#endif

/*
 * Sort an array (or whatever) of (max) elements. This uses a mixture of
 *  a QuickSort and BubbleSort internally.
 * (cmpfn) is used to determine ordering, and (swapfn) does the actual
 *  swapping of elements in the list.
 *
 *  See zip.c for an example.
 */
void __PHYSFS_sort(void *entries, PHYSFS_uint32 max,
                   int (*cmpfn)(void *, PHYSFS_uint32, PHYSFS_uint32),
                   void (*swapfn)(void *, PHYSFS_uint32, PHYSFS_uint32));


/* These get used all over for lessening code clutter. */
#define BAIL_MACRO(e, r) { __PHYSFS_setError(e); return r; }
#define BAIL_IF_MACRO(c, e, r) if (c) { __PHYSFS_setError(e); return r; }
#define BAIL_MACRO_MUTEX(e, m, r) { __PHYSFS_setError(e); __PHYSFS_platformReleaseMutex(m); return r; }
#define BAIL_IF_MACRO_MUTEX(c, e, m, r) if (c) { __PHYSFS_setError(e); __PHYSFS_platformReleaseMutex(m); return r; }




/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*------------                                              ----------------*/
/*------------  You MUST implement the following functions  ----------------*/
/*------------        if porting to a new platform.         ----------------*/
/*------------     (see platform/unix.c for an example)     ----------------*/
/*------------                                              ----------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/


/*
 * The dir separator; "/" on unix, "\\" on win32, ":" on MacOS, etc...
 *  Obviously, this isn't a function, but it IS a null-terminated string.
 */
extern const char *__PHYSFS_platformDirSeparator;


/*
 * Initialize the platform. This is called when PHYSFS_init() is called from
 *  the application. You can use this to (for example) determine what version
 *  of Windows you're running.
 *
 * Return zero if there was a catastrophic failure (which prevents you from
 *  functioning at all), and non-zero otherwise.
 */
int __PHYSFS_platformInit(void);


/*
 * Deinitialize the platform. This is called when PHYSFS_deinit() is called
 *  from the application. You can use this to clean up anything you've
 *  allocated in your platform driver.
 *
 * Return zero if there was a catastrophic failure (which prevents you from
 *  functioning at all), and non-zero otherwise.
 */
int __PHYSFS_platformDeinit(void);


/*
 * Open a file for reading. (filename) is in platform-dependent notation. The
 *  file pointer should be positioned on the first byte of the file.
 *
 * The return value will be some platform-specific datatype that is opaque to
 *  the caller; it could be a (FILE *) under Unix, or a (HANDLE *) under win32.
 *
 * The same file can be opened for read multiple times, and each should have
 *  a unique file handle; this is frequently employed to prevent race
 *  conditions in the archivers.
 *
 * Call __PHYSFS_setError() and return (NULL) if the file can't be opened.
 */
void *__PHYSFS_platformOpenRead(const char *filename);


/*
 * Open a file for writing. (filename) is in platform-dependent notation. If
 *  the file exists, it should be truncated to zero bytes, and if it doesn't
 *  exist, it should be created as a zero-byte file. The file pointer should
 *  be positioned on the first byte of the file.
 *
 * The return value will be some platform-specific datatype that is opaque to
 *  the caller; it could be a (FILE *) under Unix, or a (HANDLE *) under win32,
 *  etc.
 *
 * Opening a file for write multiple times has undefined results.
 *
 * Call __PHYSFS_setError() and return (NULL) if the file can't be opened.
 */
void *__PHYSFS_platformOpenWrite(const char *filename);


/*
 * Open a file for appending. (filename) is in platform-dependent notation. If
 *  the file exists, the file pointer should be place just past the end of the
 *  file, so that the first write will be one byte after the current end of
 *  the file. If the file doesn't exist, it should be created as a zero-byte
 *  file. The file pointer should be positioned on the first byte of the file.
 *
 * The return value will be some platform-specific datatype that is opaque to
 *  the caller; it could be a (FILE *) under Unix, or a (HANDLE *) under win32,
 *  etc.
 *
 * Opening a file for append multiple times has undefined results.
 *
 * Call __PHYSFS_setError() and return (NULL) if the file can't be opened.
 */
void *__PHYSFS_platformOpenAppend(const char *filename);


/*
 * Read more data from a platform-specific file handle. (opaque) should be
 *  cast to whatever data type your platform uses. Read a maximum of (count)
 *  objects of (size) 8-bit bytes to the area pointed to by (buffer). If there
 *  isn't enough data available, return the number of full objects read, and
 *  position the file pointer at the start of the first incomplete object.
 *  On success, return (count) and position the file pointer one byte past
 *  the end of the last read object. Return (-1) if there is a catastrophic
 *  error, and call __PHYSFS_setError() to describe the problem; the file
 *  pointer should not move in such a case.
 */
PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count);

/*
 * Write more data to a platform-specific file handle. (opaque) should be
 *  cast to whatever data type your platform uses. Write a maximum of (count)
 *  objects of (size) 8-bit bytes from the area pointed to by (buffer). If
 *  there isn't enough data available, return the number of full objects
 *  written, and position the file pointer at the start of the first
 *  incomplete object. Return (-1) if there is a catastrophic error, and call
 *  __PHYSFS_setError() to describe the problem; the file pointer should not
 *  move in such a case.
 */
PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count);

/*
 * Set the file pointer to a new position. (opaque) should be cast to
 *  whatever data type your platform uses. (pos) specifies the number
 *  of 8-bit bytes to seek to from the start of the file. Seeking past the
 *  end of the file is an error condition, and you should check for it.
 *
 * Not all file types can seek; this is to be expected by the caller.
 *
 * On error, call __PHYSFS_setError() and return zero. On success, return
 *  a non-zero value.
 */
int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos);


/*
 * Get the file pointer's position, in an 8-bit byte offset from the start of
 *  the file. (opaque) should be cast to whatever data type your platform
 *  uses.
 *
 * Not all file types can "tell"; this is to be expected by the caller.
 *
 * On error, call __PHYSFS_setError() and return zero. On success, return
 *  a non-zero value.
 */
PHYSFS_sint64 __PHYSFS_platformTell(void *opaque);


/*
 * Determine the current size of a file, in 8-bit bytes, from an open file.
 *
 * The caller expects that this information may not be available for all
 *  file types on all platforms.
 *
 * Return -1 if you can't do it, and call __PHYSFS_setError(). Otherwise,
 *  return the file length in 8-bit bytes.
 */
PHYSFS_sint64 __PHYSFS_platformFileLength(void *handle);

/*
 * Determine if a file is at EOF. (opaque) should be cast to whatever data
 *  type your platform uses.
 *
 * The caller expects that there was a short read before calling this.
 *
 * Return non-zero if EOF, zero if it is _not_ EOF.
 */
int __PHYSFS_platformEOF(void *opaque);

/*
 * Flush any pending writes to disk. (opaque) should be cast to whatever data
 *  type your platform uses. Be sure to check for errors; the caller expects
 *  that this function can fail if there was a flushing error, etc.
 *
 *  Return zero on failure, non-zero on success.
 */
int __PHYSFS_platformFlush(void *opaque);

/*
 * Flush and close a file. (opaque) should be cast to whatever data type
 *  your platform uses. Be sure to check for errors when closing; the
 *  caller expects that this function can fail if there was a flushing
 *  error, etc.
 *
 * You should clean up all resources associated with (opaque).
 *
 *  Return zero on failure, non-zero on success.
 */
int __PHYSFS_platformClose(void *opaque);

/*
 * Platform implementation of PHYSFS_getCdRomDirs()...
 *  See physfs.h. The retval should be freeable via PHYSFS_freeList().
 */
char **__PHYSFS_platformDetectAvailableCDs(void);

/*
 * Calculate the base dir, if your platform needs special consideration.
 *  Just return NULL if the standard routines will suffice. (see
 *  calculateBaseDir() in physfs.c ...)
 *  Caller will free() the retval if it's not NULL.
 */
char *__PHYSFS_platformCalcBaseDir(const char *argv0);

/*
 * Get the platform-specific user name.
 *  Caller will free() the retval if it's not NULL. If it's NULL, the username
 *  will default to "default".
 */
char *__PHYSFS_platformGetUserName(void);

/*
 * Get the platform-specific user dir.
 *  Caller will free() the retval if it's not NULL. If it's NULL, the userdir
 *  will default to basedir/username.
 */
char *__PHYSFS_platformGetUserDir(void);

/*
 * Return a number that uniquely identifies the current thread.
 *  On a platform without threading, (1) will suffice. These numbers are
 *  arbitrary; the only requirement is that no two threads have the same
 *  number.
 */
PHYSFS_uint64 __PHYSFS_platformGetThreadID(void);

/*
 * This is a pass-through to whatever stricmp() is called on your platform.
 */
int __PHYSFS_platformStricmp(const char *str1, const char *str2);

/*
 * This is a pass-through to whatever strnicmp() is called on your platform.
 */
int __PHYSFS_platformStrnicmp(const char *s1, const char *s2, PHYSFS_uint32 l);

/*
 * Return non-zero if filename (in platform-dependent notation) exists.
 *  Symlinks should NOT be followed; at this stage, we do not care what the
 *  symlink points to. Please call __PHYSFS_SetError() with the details of
 *  why the file does not exist, if it doesn't; you are in a better position
 *  to know (path not found, bogus filename, file itself is missing, etc).
 */
int __PHYSFS_platformExists(const char *fname);

/*
 * Return the last modified time (in seconds since the epoch) of a file.
 *  Returns -1 on failure. (fname) is in platform-dependent notation.
 *  Symlinks should be followed; if what the symlink points to is missing,
 *  then the retval is -1.
 */
PHYSFS_sint64 __PHYSFS_platformGetLastModTime(const char *fname);

/*
 * Return non-zero if filename (in platform-dependent notation) is a symlink.
 */
int __PHYSFS_platformIsSymLink(const char *fname);


/*
 * Return non-zero if filename (in platform-dependent notation) is a symlink.
 *  Symlinks should be followed; if what the symlink points to is missing,
 *  or isn't a directory, then the retval is false.
 */
int __PHYSFS_platformIsDirectory(const char *fname);


/*
 * Convert (dirName) to platform-dependent notation, then prepend (prepend)
 *  and append (append) to the converted string.
 *
 *  So, on Win32, calling:
 *     __PHYSFS_platformCvtToDependent("C:\", "my/files", NULL);
 *  ...will return the string "C:\my\files".
 *
 * This can be implemented in a platform-specific manner, so you can get
 *  get a speed boost that the default implementation can't, since
 *  you can make assumptions about the size of strings, etc..
 *
 * Platforms that choose not to implement this may just call
 *  __PHYSFS_convertToDependent() as a passthrough, which may fit the bill
 *  already.
 *
 * Be sure to free() the return value when done with it.
 */
char *__PHYSFS_platformCvtToDependent(const char *prepend,
                                      const char *dirName,
                                      const char *append);


/*
 * Make the current thread give up a timeslice. This is called in a loop
 *  while waiting for various external forces to get back to us.
 */
void __PHYSFS_platformTimeslice(void);


/*
 * Enumerate a directory of files. This follows the rules for the
 *  DirFunctions->enumerateFiles() method (see above), except that the
 *  (dirName) that is passed to this function is converted to
 *  platform-DEPENDENT notation by the caller. The DirFunctions version
 *  uses platform-independent notation. Note that ".", "..", and other
 *  metaentries should always be ignored.
 */
LinkedStringList *__PHYSFS_platformEnumerateFiles(const char *dirname,
                                                  int omitSymLinks);


/*
 * Get the current working directory. The return value should be an
 *  absolute path in platform-dependent notation. The caller will deallocate
 *  the return value with the standard C runtime free() function when it
 *  is done with it.
 * On error, return NULL and set the error message.
 */
char *__PHYSFS_platformCurrentDir(void);


/*
 * Get the real physical path to a file. (path) is specified in
 *  platform-dependent notation, as should your return value be.
 *  All relative paths should be removed, leaving you with an absolute
 *  path. Symlinks should be resolved, too, so that the returned value is
 *  the most direct path to a file.
 * The return value will be deallocated with the standard C runtime free()
 *  function when the caller is done with it.
 * On error, return NULL and set the error message.
 */
char *__PHYSFS_platformRealPath(const char *path);


/*
 * Make a directory in the actual filesystem. (path) is specified in
 *  platform-dependent notation. On error, return zero and set the error
 *  message. Return non-zero on success.
 */
int __PHYSFS_platformMkDir(const char *path);


/*
 * Remove a file or directory entry in the actual filesystem. (path) is
 *  specified in platform-dependent notation. Note that this deletes files
 *  _and_ directories, so you might need to do some determination.
 *  Non-empty directories should report an error and not delete themselves
 *  or their contents.
 *
 * Deleting a symlink should remove the link, not what it points to.
 *
 * On error, return zero and set the error message. Return non-zero on success.
 */
int __PHYSFS_platformDelete(const char *path);


/*
 * Create a platform-specific mutex. This can be whatever datatype your
 *  platform uses for mutexes, but it is cast to a (void *) for abstractness.
 *
 * Return (NULL) if you couldn't create one. Systems without threads can
 *  return any arbitrary non-NULL value.
 */
void *__PHYSFS_platformCreateMutex(void);

/*
 * Destroy a platform-specific mutex, and clean up any resources associated
 *  with it. (mutex) is a value previously returned by
 *  __PHYSFS_platformCreateMutex(). This can be a no-op on single-threaded
 *  platforms.
 */
void __PHYSFS_platformDestroyMutex(void *mutex);

/*
 * Grab possession of a platform-specific mutex. Mutexes should be recursive;
 *  that is, the same thread should be able to call this function multiple
 *  times in a row without causing a deadlock. This function should block 
 *  until a thread can gain possession of the mutex.
 *
 * Return non-zero if the mutex was grabbed, zero if there was an 
 *  unrecoverable problem grabbing it (this should not be a matter of 
 *  timing out! We're talking major system errors; block until the mutex 
 *  is available otherwise.)
 *
 * _DO NOT_ call __PHYSFS_setError() in here! Since setError calls this
 *  function, you'll cause an infinite recursion. This means you can't
 *  use the BAIL_*MACRO* macros, either.
 */
int __PHYSFS_platformGrabMutex(void *mutex);

/*
 * Relinquish possession of the mutex when this method has been called 
 *  once for each time that platformGrabMutex was called. Once possession has
 *  been released, the next thread in line to grab the mutex (if any) may
 *  proceed.
 *
 * _DO NOT_ call __PHYSFS_setError() in here! Since setError calls this
 *  function, you'll cause an infinite recursion. This means you can't
 *  use the BAIL_*MACRO* macros, either.
 */
void __PHYSFS_platformReleaseMutex(void *mutex);

#ifdef __cplusplus
}
#endif

#endif

/* end of physfs_internal.h ... */

