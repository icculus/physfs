# Microsoft Developer Studio Project File - Name="physfs_static" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=physfs_static - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "physfs_static.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "physfs_static.mak" CFG="physfs_static - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "physfs_static - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "physfs_static - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Perforce Project"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "physfs_static - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "physfs_static___Win32_Release"
# PROP BASE Intermediate_Dir "physfs_static___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "physfs_static_release"
# PROP Intermediate_Dir "physfs_static_release"
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /GX /O2 /I "zlib121" /I "../physfs" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PHYSFS_SUPPORTS_QPAK=1 /D PHYSFS_SUPPORTS_ZIP=1 /D PHYSFS_SUPPORTS_HOG=1 /D PHYSFS_SUPPORTS_GRP=1 /D PHYSFS_SUPPORTS_WAD=1 /D PHYSFS_SUPPORTS_MVL=1 /D Z_PREFIX=1 /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "physfs_static - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "physfs_static___Win32_Debug"
# PROP BASE Intermediate_Dir "physfs_static___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "physfs_static_debug"
# PROP Intermediate_Dir "physfs_static_debug"
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MTd /W3 /Gm /GX /ZI /Od /I "zlib121" /I "../physfs" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D PHYSFS_SUPPORTS_QPAK=1 /D PHYSFS_SUPPORTS_ZIP=1 /D PHYSFS_SUPPORTS_HOG=1 /D PHYSFS_SUPPORTS_GRP=1 /D PHYSFS_SUPPORTS_WAD=1 /D PHYSFS_SUPPORTS_MVL=1 /D Z_PREFIX=1 /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "physfs_static - Win32 Release"
# Name "physfs_static - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "zlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\zlib121\adler32.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\compress.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\crc32.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\deflate.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\infback.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\inffast.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\inflate.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\inftrees.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\trees.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\uncompr.c
# End Source File
# Begin Source File

SOURCE=.\zlib121\zutil.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\archivers\dir.c
# End Source File
# Begin Source File

SOURCE=.\archivers\grp.c
# End Source File
# Begin Source File

SOURCE=.\archivers\hog.c
# End Source File
# Begin Source File

SOURCE=.\archivers\mvl.c
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

SOURCE=.\archivers\wad.c
# End Source File
# Begin Source File

SOURCE=.\platform\win32.c
# End Source File
# Begin Source File

SOURCE=.\archivers\zip.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\physfs.h
# End Source File
# Begin Source File

SOURCE=.\physfs_internal.h
# End Source File
# End Group
# End Target
# End Project
