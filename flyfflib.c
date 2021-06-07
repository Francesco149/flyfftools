#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define Fatal(action, varname) _Fatal(__func__, action, varname)
static void _Fatal(char const *func, char *action, char *varname) {
  fprintf(stderr, "%s: failed to %s %s\n", func, action, varname);
  perror("errno");
  exit(1);
}

#define MAlloc(var, size) ReAlloc(var, 0, size)
#define ReAlloc(var, prev, size) *(var) = _ReAlloc(__func__, #var, prev, size)
void *_ReAlloc(char const *func, char *name, void *prev, size_t size) {
  void *res = realloc(prev, size);
  if (!res) _Fatal(func, "reallocate" , name);
  return res;
}

#define FOpen(path, mode) _FOpen(__func__, #path, path, mode)
FILE *_FOpen(char const *func, char *varname, char *path, char *mode) {
  FILE *f;
  if (!(f = fopen(path, mode))) {
    fprintf(stderr, "error opening '%s'\n", path);
    _Fatal(func, "open", varname);
  }
  return f;
}

#define FSeek(f, off, whence) _FSeek(__func__, #f, f, off, whence)
void _FSeek(char const *func, char *varname, FILE *f, long off, int whence) {
  if (fseek(f, off, whence) < 0) {
    _Fatal(func, "seek", varname);
  }
}

#define FSize(f) _FSize(__func__, #f, f)
long _FSize(char const *func, char *varname, FILE *f) {
  long old, size;
  old = ftell(f);
  _FSeek(func, varname, f, 0, SEEK_END);
  size = ftell(f);
  _FSeek(func, varname, f, old, SEEK_SET);
  return size;
}

char *LdFile(char *path) {
  FILE *f = FOpen(path, "rb");
  long size = FSize(f);
  char *buf;
  MAlloc(&buf, size + 1);
  if (fread(buf, size, 1, f) != 1) {
    Fatal("read", path);
  }
  buf[size] = 0;
  if (buf[0] == 0xff && buf[1] == 0xfe) {
    char *mb;
    size_t mbsize = wcstombs(0, (wchar_t *)buf, 0);
    if (mbsize < 0) {
      Fatal("probe for multi-byte string size", "");
    }
    MAlloc(&mb, mbsize);
    if (wcstombs(mb, (wchar_t *)buf, mbsize) != mbsize) {
      Fatal("convert to multi-byte string", "");
    }
    free(buf);
    buf = mb;
  }
  fclose(f);
  return buf;
}

typedef struct _Flyff {
  char *path;
  int pathLen;
} Flyff;

Flyff *MkFlyff(char *path) {
  Flyff *res;
  MAlloc(&res, sizeof(Flyff));
  res->path = strdup(path);
  res->pathLen = strlen(path);
  return res;
}

void RmFlyff(Flyff *flyff) {
  if (flyff) {
    free(flyff->path);
  }
  free(flyff);
}

static char *FlyffPath(Flyff *flyff, char *path) {
  char *res;
  MAlloc(&res, flyff->pathLen + strlen(path) + 2);
  strcpy(res, flyff->path);
#if defined(_WIN32) || defined(WIN32)
  res[flyff->pathLen] = '\\';
#else
  res[flyff->pathLen] = '/';
#endif
  strcpy(res + flyff->pathLen + 1, path);
  return res;
}

typedef struct _JobProp {
  char *id;
  float aspd;

  float factorMaxHP;
  float factorMaxMP;
  float factorMaxFP;
  float factorDef;
  float factorHPRecovery;
  float factorMPRecovery;
  float factorFPRecovery;

  /* attack factors for each weapon type */
  float meleeSwd;
  float meleeAxe;
  float meleeStaff;
  float meleeStick;
  float meleeKnuckle;
  float meleeWand;
  float meleeYOYO;

  float blocking;
  float critical;
  char *icon;
} JobProp;

static int IsWhite(wint_t c) {
  return (c > 0 && c <= 0x20);
}

/* TODO: full lexer for flyff scripts */
static int IncLex(char **pp, char **buf) {
  #define p (*pp)
  size_t count;

  for (; *p && IsWhite(*p); ++p);

  /* handle comments */
LEX_COMMENTS:
  while (*p == '/') {
    ++p;
    if (*p == '/') {
      ++p;
      for (; *p != '\r' && *p; ++p);
      if (*p == '\r') p += 2;
    } else if (*p == '*') {
      ++p;
      do {
        for (; *p && *p != '*'; ++p);
        if (!*p) return 0; /* TODO: eof delimiter */
        ++p;
      } while (*p != '/');

      ++p;
      while (*p && IsWhite(*p)) ++p;
    } else {
      --p;
      break;
    }
    goto LEX_COMMENTS;
  }

  if (!*p) return 0; /* TODO: eof delimiter */

  for (count = 0; *p && !IsWhite(*p); ++count) {
    ReAlloc(buf, *buf, (count + 2) * sizeof(char));
    (*buf)[count] = *p++;
  }
  if (*buf) (*buf)[count] = 0;
  return count > 0;
  #undef p
}

static float GetFlt(char **p, char **buf) {
  if (!IncLex(p, buf)) Fatal("get float", "expected float, got eof");
  return atof(*buf);
}

static char *GetStr(char **p, char **buf) {
  if (!IncLex(p, buf)) Fatal("get string", "expected string, got eof");
  return strdup(*buf);
}

JobProp *LdJobProps(Flyff *flyff, int *outCount) {
  JobProp *res = 0;
  char *path = FlyffPath(flyff, "propJob.inc");
  char *file = (char *)LdFile(path);
  char *p = file, *buf = 0;
  *outCount = 0;
  /* TODO: proper script parsing */
  while (IncLex(&p, &buf)) {
    JobProp *prop;
    ++*outCount;
    ReAlloc(&res, res, *outCount * sizeof(JobProp));
    prop = &res[*outCount - 1];
    prop->id = strdup(buf);
    prop->aspd = GetFlt(&p, &buf);
    prop->factorMaxHP = GetFlt(&p, &buf);
    prop->factorMaxMP = GetFlt(&p, &buf);
    prop->factorMaxFP = GetFlt(&p, &buf);
    prop->factorDef = GetFlt(&p, &buf);
    prop->factorHPRecovery = GetFlt(&p, &buf);
    prop->factorMPRecovery = GetFlt(&p, &buf);
    prop->factorFPRecovery = GetFlt(&p, &buf);
    prop->meleeSwd = GetFlt(&p, &buf);
    prop->meleeAxe = GetFlt(&p, &buf);
    prop->meleeStaff = GetFlt(&p, &buf);
    prop->meleeStick = GetFlt(&p, &buf);
    prop->meleeKnuckle = GetFlt(&p, &buf);
    prop->meleeWand = GetFlt(&p, &buf);
    prop->blocking = GetFlt(&p, &buf);
    prop->meleeYOYO = GetFlt(&p, &buf);
    prop->critical = GetFlt(&p, &buf);
    prop->icon = GetStr(&p, &buf);
  }
  free(buf);
  free(file);
  free(path);
  return res;
}

void RmJobProps(JobProp *props, int count) {
  int i;
  for (i = 0; i < count; ++i) {
    free(props[i].id);
    free(props[i].icon);
  }
  free(props);
}
