if BUILD_LZMA
noinst_LTLIBRARIES = liblzma.la
liblzma_la_CFLAGS = -D_LZMA_IN_CB -D_LZMA_PROB32 -D_LZMA_SYSTEM_SIZE_T -D_SZ_ONE_DIRECTORY
liblzma_la_SOURCES = 7zBuffer.c 7zCrc.c 7zHeader.c 7zIn.c 7zItem.c \
        7zMethodID.c 7zExtract.c 7zDecode.c LzmaDecode.c
endif
