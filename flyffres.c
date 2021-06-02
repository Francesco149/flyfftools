#define BUFSIZE 1<<26
#define MAX_HDR_SIZE 1000000000

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
#include <sys/stat.h>
#define MkDir(path) mkdir(path, 0777)
#else
#define MkDir _mkdir
#endif

#define Min(a, b) ((a) < (b) ? (a) : (b))

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;

U8 Rd8(U8 **ptr) {
  return *(*ptr)++;
}

U16 Rd16(U8 **p) {
  return (Rd8(p) << 0) | (Rd8(p) << 8);
}

U32 Rd32(U8 **p) {
  return (
    (Rd8(p) <<  0) |
    (Rd8(p) <<  8) |
    (Rd8(p) << 16) |
    (Rd8(p) << 24)
  );
}

#ifdef FLYFFTOOLS_DEBUG
#define Dbg printf("(debug) "); printf
#define DmpInt(var) _DmpInt(#var, var);
#define DmpTime(var) _DmpTime(#var, var);
#define DmpStr(var) _DmpStr(#var, var);
void _DmpInt(char *name, int val) { Dbg("%s = %d\n", name, val); }
void _DmpStr(char *name, char *val) { Dbg("%s = %s\n", name, val); }

void _DmpTime(char *name, U32 val) {
  char buf[20];
  time_t now = val;
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
  Dbg("%s = %s\n", name, buf);
}
#else
#define DmpInt(x) (x)
#define DmpTime(x) (x)
#define DmpStr(x) (x)
#endif

#define MAlloc(var, size) *(var) = _MAlloc(#var, size)

void* _MAlloc(char *name, int size) {
  void *res = malloc(size);
  if (!res) {
    fprintf(stderr, "failed to allocate %s\n", name);
    perror("malloc");
    exit(1);
  }
  return res;
}

static U8 ResDecryptU8(U8 key, U8 data) {
  data = ~data ^ key;
  return (data << 4) | (data >> 4);
}

static void ResDecrypt(U8 key, U8* data, U32 size) {
  U32 i;
  for (i = 0; i < size; ++i) {
    data[i] = ResDecryptU8(key, data[i]);
  }
}

char* RdFixedStr(U8 **data, int size) {
  char* s = _MAlloc("RdFixedStr", size+1);
  memcpy(s, *data, size);
  s[size] = 0;
  *data += size;
  return s;
}

char* RdStr(U8 **data) {
  U16 len = Rd16(data);
  char* s = _MAlloc("RdStr", len+1);
  memcpy(s, *data, len);
  s[len] = 0;
  *data += len;
  return s;
}

int main(int argc, char *argv[]) {
  FILE *f;
  U8 *p, cryptHdr[6], cryptKey, isCrypt, *hdr, *buf;
  U32 hdrSize, i;
  U16 numFiles;
  char *version, *dstPath;
  size_t dstPathLen;

  if (argc != 2) {
    fprintf(stderr, "usage: %s file.res\n", argv[0]);
    return 1;
  }
  if (!(f = fopen(argv[1], "rb"))) {
    perror("opening res file");
    return 1;
  }

  MAlloc(&dstPath, strlen(argv[1])+2);
  strcpy(dstPath, argv[1]);
  strcat(dstPath, "_");
  dstPathLen = strlen(dstPath);

  MkDir(dstPath);

  p = cryptHdr;
  if (fread(cryptHdr, sizeof(cryptHdr), 1, f) != 1) {
    perror("reading encryption header");
    exit(1);
  }
  DmpInt(cryptKey = Rd8(&p));
  DmpInt(isCrypt = Rd8(&p)); /* header itself is "encrypted" even if this is false */
  DmpInt(hdrSize = Rd32(&p));

  if (hdrSize > MAX_HDR_SIZE) {
    fprintf(stderr, "huge header size, probably not a valid file");
    return 1;
  }

  MAlloc(&hdr, hdrSize);
  fread(hdr, hdrSize, 1, f);
  ResDecrypt(cryptKey, hdr, hdrSize);

  p = hdr;
  DmpStr(version = RdFixedStr(&p, 7));
  printf("version: %s\n", version);

  DmpInt(numFiles = Rd16(&p));

  MAlloc(&buf, BUFSIZE);

  for (i = 0; i < numFiles; ++i) {
    char *name, *path;
    U32 size, time, offset;
    FILE *extracted;

    DmpStr(name = RdStr(&p));
    DmpInt(size = Rd32(&p));
    DmpTime(time = Rd32(&p));
    DmpInt(offset = Rd32(&p));

    (void)time; /* suppress warning */

    MAlloc(&path, dstPathLen + strlen(name) + 2);
    strcpy(path, dstPath);
    strcat(path, "/");
    strcat(path, name);

    printf("[%s] %s\n", isCrypt ? "E" : " ", path);
    if (!(extracted = fopen(path, "wb"))) {
      fprintf(stderr, "opening %s\n", path);
      perror("fopen");
      return 1;
    }

    if (fseek(f, offset, SEEK_SET) < 0) {
      perror("fseek");
      return 1;
    }
    while (size > 0) {
      U32 n = Min(size, BUFSIZE);
      if (fread(buf, n, 1, f) != 1) {
        fprintf(stderr, "reading %s\n", name);
        perror("fread");
        return 1;
      }
      if (isCrypt) {
        ResDecrypt(cryptKey, buf, n);
      }
      size -= n;
      if (fwrite(buf, n, 1, extracted) != 1) {
        fprintf(stderr, "writing %s\n", name);
        perror("fwrite");
        return 1;
      }
    }

    free(name);
    free(path);
    fclose(extracted);
  }

  return 0;
}
