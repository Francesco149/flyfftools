#include "flyfflib.c"

#define BUFSIZE 1<<26
#define MAX_HDR_SIZE 1000000000

#include <errno.h>
#include <time.h>
#include <utime.h>

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
  char* s;
  MAlloc(&s, size+1);
  memcpy(s, *data, size);
  s[size] = 0;
  *data += size;
  return s;
}

char* RdStr(U8 **data) {
  U16 len = Rd16(data);
  char* s;
  MAlloc(&s, len+1);
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
  char *version, *dstPath = "./flyff";
  size_t dstPathLen;

  if (argc < 2) {
    fprintf(stderr, "usage: %s file.res [outpath=%s]\n", argv[0], dstPath);
    return 1;
  }
  if (argc >= 3) {
    dstPath = argv[2];
  }
  dstPathLen = strlen(dstPath);
  MkDir(dstPath);

  f = FOpen(argv[1], "rb");

  p = cryptHdr;
  if (fread(cryptHdr, sizeof(cryptHdr), 1, f) != 1) {
    Fatal("read", "encryption header");
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

    MAlloc(&path, dstPathLen + strlen(name) + 2);
    strcpy(path, dstPath);
    strcat(path, "/");
    strcat(path, name);

    printf("[%s] %s", isCrypt ? "E" : " ", path);

    {
      struct stat st;
      if (stat(path, &st) >= 0) {
        if (st.st_mtime > time) {
          printf(" ### ignored, newer file already present ");
          DmpTime(st.st_mtime);
        }
      }
    }

    printf("\n");

    extracted = FOpen(path, "wb");
    FSeek(f, offset, SEEK_SET);
    while (size > 0) {
      U32 n = Min(size, BUFSIZE);
      if (fread(buf, n, 1, f) != 1) {
        Fatal("read", name);
      }
      if (isCrypt) {
        ResDecrypt(cryptKey, buf, n);
      }
      size -= n;
      if (fwrite(buf, n, 1, extracted) != 1) {
        Fatal("write", name);
      }
    }


    fclose(extracted);

    {
      struct utimbuf t;
      t.actime = time;
      t.modtime = time;
      if (utime(path, &t) < 0) {
        perror("utime");
        exit(1);
      }
    }

    free(name);
    free(path);
  }

  free(version);
  free(hdr);
  free(buf);
  fclose(f);

  return 0;
}
