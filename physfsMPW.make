#   File:       PhysicsFS.make
#   Target:     PhysicsFS
#   Created:    Saturday, November 20, 2004 09:37:58 PM


MAKEFILE        = PhysicsFS.make
¥MondoBuild¥    = {MAKEFILE}  # Make blank to avoid rebuilds when makefile is modified

ObjDir          = :obj:
Includes        = -i : ¶
				  -i ":zlib123:"

Sym-PPC         = -sym on

PPCCOptions     = {Includes} {Sym-PPC} -prefix ":Mac Classic Support:codewarrior_predefs.h" -enum int -alloca


### Source Files ###

SrcFiles        =  ¶
				  :archivers:dir.c ¶
				  :archivers:grp.c ¶
				  :archivers:hog.c ¶
				  :archivers:mvl.c ¶
				  :archivers:qpak.c ¶
				  :archivers:wad.c ¶
				  :archivers:zip.c ¶
				  physfs.c ¶
				  physfs_unicode.c ¶
				  physfs_byteorder.c ¶
				  :platform:macclassic.c ¶
				  :zlib123:adler32.c ¶
				  :zlib123:compress.c ¶
				  :zlib123:crc32.c ¶
				  :zlib123:deflate.c ¶
				  :zlib123:gzio.c ¶
				  :zlib123:infback.c ¶
				  :zlib123:inffast.c ¶
				  :zlib123:inflate.c ¶
				  :zlib123:inftrees.c ¶
				  :zlib123:trees.c ¶
				  :zlib123:uncompr.c ¶
				  :zlib123:zutil.c


### Object Files ###

ObjFiles-PPC    =  ¶
				  "{ObjDir}dir.c.x" ¶
				  "{ObjDir}grp.c.x" ¶
				  "{ObjDir}hog.c.x" ¶
				  "{ObjDir}mvl.c.x" ¶
				  "{ObjDir}qpak.c.x" ¶
				  "{ObjDir}wad.c.x" ¶
				  "{ObjDir}zip.c.x" ¶
				  "{ObjDir}physfs.c.x" ¶
				  "{ObjDir}physfs_byteorder.c.x" ¶
				  "{ObjDir}physfs_unicode.c.x" ¶
				  "{ObjDir}macclassic.c.x" ¶
				  "{ObjDir}adler32.c.x" ¶
				  "{ObjDir}compress.c.x" ¶
				  "{ObjDir}crc32.c.x" ¶
				  "{ObjDir}deflate.c.x" ¶
				  "{ObjDir}gzio.c.x" ¶
				  "{ObjDir}infback.c.x" ¶
				  "{ObjDir}inffast.c.x" ¶
				  "{ObjDir}inflate.c.x" ¶
				  "{ObjDir}inftrees.c.x" ¶
				  "{ObjDir}trees.c.x" ¶
				  "{ObjDir}uncompr.c.x" ¶
				  "{ObjDir}zutil.c.x"


### Libraries ###

LibFiles-PPC    =  ¶
				  "{SharedLibraries}InterfaceLib" ¶
				  "{SharedLibraries}StdCLib" ¶
				  "{SharedLibraries}MathLib" ¶
				  "{PPCLibraries}StdCRuntime.o" ¶
				  "{PPCLibraries}PPCCRuntime.o" ¶
				  "{PPCLibraries}PPCToolLibs.o"


### Default Rules ###

.c.x  Ä  .c  {¥MondoBuild¥}
	{PPCC} {depDir}{default}.c -o {targDir}{default}.c.x {PPCCOptions}


### Build Rules ###

PhysicsFS  ÄÄ  {ObjFiles-PPC} {LibFiles-PPC} {¥MondoBuild¥}
	PPCLink ¶
		-o {Targ} ¶
		{ObjFiles-PPC} ¶
		{LibFiles-PPC} ¶
		{Sym-PPC} ¶
		-@export ":Mac Classic Support:physfs.exp" ¶
		-t 'shlb' ¶
		-c '????' ¶
		-xm s



### Required Dependencies ###

"{ObjDir}dir.c.x"  Ä  :archivers:dir.c
"{ObjDir}grp.c.x"  Ä  :archivers:grp.c
"{ObjDir}hog.c.x"  Ä  :archivers:hog.c
"{ObjDir}mvl.c.x"  Ä  :archivers:mvl.c
"{ObjDir}qpak.c.x"  Ä  :archivers:qpak.c
"{ObjDir}wad.c.x"  Ä  :archivers:wad.c
"{ObjDir}zip.c.x"  Ä  :archivers:zip.c
"{ObjDir}physfs.c.x"  Ä  physfs.c
"{ObjDir}physfs_byteorder.c.x"  Ä  physfs_byteorder.c
"{ObjDir}physfs_unicode.c.x"  Ä  physfs_unicode.c
"{ObjDir}macclassic.c.x"  Ä  :platform:macclassic.c
"{ObjDir}adler32.c.x"  Ä  :zlib123:adler32.c
"{ObjDir}compress.c.x"  Ä  :zlib123:compress.c
"{ObjDir}crc32.c.x"  Ä  :zlib123:crc32.c
"{ObjDir}deflate.c.x"  Ä  :zlib123:deflate.c
"{ObjDir}gzio.c.x"  Ä  :zlib123:gzio.c
"{ObjDir}infback.c.x"  Ä  :zlib123:infback.c
"{ObjDir}inffast.c.x"  Ä  :zlib123:inffast.c
"{ObjDir}inflate.c.x"  Ä  :zlib123:inflate.c
"{ObjDir}inftrees.c.x"  Ä  :zlib123:inftrees.c
"{ObjDir}trees.c.x"  Ä  :zlib123:trees.c
"{ObjDir}uncompr.c.x"  Ä  :zlib123:uncompr.c
"{ObjDir}zutil.c.x"  Ä  :zlib123:zutil.c


### Optional Dependencies ###
### Build this target to generate "include file" dependencies. ###

Dependencies  Ä  $OutOfDate
	MakeDepend ¶
		-append {MAKEFILE} ¶
		-ignore "{CIncludes}" ¶
		-objdir "{ObjDir}" ¶
		-objext .x ¶
		{Includes} ¶
		{SrcFiles}


