LUACFLAGS="`pkg-config --cflags lua` -shared -pedantic -std=c99 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE"

#gcc ${LUACFLAGS} -Wall lsignal.c -Wl,-soname,signal.so -o signal.so || exit 1 
gcc ${LUACFLAGS} -Wall lsignal.c -o signal.so || exit 1 
gcc ${LUACFLAGS} -Wall sigtest.c -o sigtestlib.so || exit 1
