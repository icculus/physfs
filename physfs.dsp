# Microsoft Developer Studio Project File - Name="physfs" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=physfs - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "physfs.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "physfs.mak" CFG="physfs - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "physfs - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "physfs - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "physfs - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /WX /Gm /ZI /Od /I "." /I "zlibwin32" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PHYSFS_EXPORTS" /D "PHYSFS_SUPPORTS_GRP" /D "PHYSFS_SUPPORTS_WAD" /D "PHYSFS_SUPPORTS_ZIP" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /WX /Gm- /GX- /Zi /Od /I "." /I "zlib114" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PHYSFS_EXPORTS" /D "PHYSFS_SUPPORTS_GRP" /D "PHYSFS_SUPPORTS_WAD" /D "PHYSFS_SUPPORTS_ZIP" /D "PHYSFS_SUPPORTS_QPAK" /FR /YX /FD /GZ /c
# SUBTRACT CPP /X
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# SUBTRACT BASE LINK32 /incremental:no
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"../src/Debug/physfs.dll"
# SUBTRACT LINK32 /pdb:none /force

!ELSEIF  "$(CFG)" == "physfs - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /WX /O2 /I "." /I "zlibwin32" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PHYSFS_EXPORTS" /D "PHYSFS_SUPPORTS_GRP" /D "PHYSFS_SUPPORTS_WAD" /D "PHYSFS_SUPPORTS_ZIP" /YX /FD /c
# ADD CPP /nologo /MD /W3 /WX /O2 /I "." /I "zlib114" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PHYSFS_EXPORTS" /D "PHYSFS_SUPPORTS_GRP" /D "PHYSFS_SUPPORTS_WAD" /D "PHYSFS_SUPPORTS_ZIP" /D "PHYSFS_SUPPORTS_QPAK" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 zlibwin32\zlibstat.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386

!ENDIF 

# Begin Target

# Name "physfs - Win32 Debug"
# Name "physfs - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\zlib114\adler32.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\compress.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\crc32.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\deflate.c
# End Source File
# Begin Source File

SOURCE=.\archivers\dir.c
# End Source File
# Begin Source File

SOURCE=.\archivers\grp.c
# End Source File
# Begin Source File

SOURCE=.\archivers\wad.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\infblock.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\infcodes.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\inffast.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\inflate.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\inftrees.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\infutil.c
# End Source File
# Begin Source File

SOURCE=.\physfs.c
# End Source File
# Begin Source File

SOURCE=.\physfs_byteorder.c
# End Source File
# Begin Source File

SOURCE=.\archivers\qpak.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\trees.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\uncompr.c
# End Source File
# Begin Source File

SOURCE=.\platform\win32.c
# End Source File
# Begin Source File

SOURCE=.\archivers\zip.c
# End Source File
# Begin Source File

SOURCE=.\zlib114\zutil.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\zlib114\deflate.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\infblock.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\infcodes.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\inffast.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\inffixed.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\inftrees.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\infutil.h
# End Source File
# Begin Source File

SOURCE=.\physfs.h
# End Source File
# Begin Source File

SOURCE=.\physfs_internal.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\trees.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\zconf.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\zlib.h
# End Source File
# Begin Source File

SOURCE=.\zlib114\zutil.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
