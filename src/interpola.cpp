/* ------------------------------------------------------------------------- */
/* Copyright 2013 Esri                                                       */
/*                                                                           */
/* Licensed under the Apache License, Version 2.0 (the "License");           */
/* you may not use this file except in compliance with the License.          */
/* You may obtain a copy of the License at                                   */
/*                                                                           */
/*     http://www.apache.org/licenses/LICENSE-2.0                            */
/*                                                                           */
/* Unless required by applicable law or agreed to in writing, software       */
/* distributed under the License is distributed on an "AS IS" BASIS,         */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  */
/* See the License for the specific language governing permissions and       */
/* limitations under the License.                                            */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Routines to copy/dump/validate NTv2 files and transform points            */
/* ------------------------------------------------------------------------- */

#ifdef _WIN32
#  pragma warning (disable: 4996) /* same as "-D _CRT_SECURE_NO_WARNINGS" */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <locale.h>
#include "interpola.h"


/* ------------------------------------------------------------------------- */
/* Copyright 2013 Esri                                                       */
/*                                                                           */
/* Licensed under the Apache License, Version 2.0 (the "License");           */
/* you may not use this file except in compliance with the License.          */
/* You may obtain a copy of the License at                                   */
/*                                                                           */
/*     http://www.apache.org/licenses/LICENSE-2.0                            */
/*                                                                           */
/* Unless required by applicable law or agreed to in writing, software       */
/* distributed under the License is distributed on an "AS IS" BASIS,         */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  */
/* See the License for the specific language governing permissions and       */
/* limitations under the License.                                            */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* These routines are separated out so they may be replaced by the user.     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Memory routines                                                           */
/* ------------------------------------------------------------------------- */



/* ------------------------------------------------------------------------- */
/* Mutex routines                                                            */
/* ------------------------------------------------------------------------- */

typedef struct gri_critsect GRI_CRITSECT;

#if defined(GRI_NO_MUTEXES)

struct gri_critsect
{
  int foo;
};

#  define GRI_CS_CREATE(cs)
#  define GRI_CS_DELETE(cs)
#  define GRI_CS_ENTER(cs)
#  define GRI_CS_LEAVE(cs)

#elif defined(_WIN32)

#  define  WIN32_LEAN_AND_MEAN   /* Exclude rarely-used stuff */
#  include <windows.h>
/* Note that Windows mutexes are always recursive.
*/
struct gri_critsect
{
  CRITICAL_SECTION   crit;
};

# if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#  define GRI_CS_CREATE(cs) \
      InitializeCriticalSectionEx (&cs.crit, 4000, 0)
# elif defined(WINCE)
#  define GRI_CS_CREATE(cs) \
      InitializeCriticalSection   (&cs.crit)
# else
#  define GRI_CS_CREATE(cs) \
      InitializeCriticalSectionAndSpinCount(&cs.crit, 4000)
# endif
#  define GRI_CS_DELETE(cs) \
      DeleteCriticalSection       (&cs.crit)
#  define GRI_CS_ENTER(cs)  \
      EnterCriticalSection        (&cs.crit)
#  define GRI_CS_LEAVE(cs)  \
      LeaveCriticalSection        (&cs.crit)

#else

#  include <pthread.h>
/* In Unix we always use POSIX threads & mutexes.
   We also make sure that our mutexes are recursive
   (by default, POSIX mutexes are not recursive).
*/
struct gri_critsect
{
  pthread_mutex_t       crit;
  pthread_mutexattr_t   attr;
};

#  if linux
#    define MUTEX_RECURSIVE   PTHREAD_MUTEX_RECURSIVE_NP
#  else
#    define MUTEX_RECURSIVE   PTHREAD_MUTEX_RECURSIVE_NP
#  endif

#  define GRI_CS_CREATE(cs)   \
      pthread_mutexattr_init    (&cs.attr);                  \
      pthread_mutexattr_settype (&cs.attr, MUTEX_RECURSIVE); \
      pthread_mutex_init        (&cs.crit, &cs.attr)

#  define GRI_CS_DELETE(cs)   \
      pthread_mutex_destroy     (&cs.crit); \
      pthread_mutexattr_destroy (&cs.attr)

#  define GRI_CS_ENTER(cs)   \
      pthread_mutex_lock        (&cs.crit)

#  define GRI_CS_LEAVE(cs)   \
      pthread_mutex_unlock      (&cs.crit)

#endif /* OS-specific stuff */

typedef struct gri_mutex_t GRI_MUTEX_T;
struct gri_mutex_t
{
  GRI_CRITSECT  cs;
  int            count;
};

static void* gri_mutex_create()
{
  GRI_MUTEX_T* m =  new GRI_MUTEX_T[sizeof(*m)];

  if (m != GRI_NULL)
  {
    m->count = 0;
    GRI_CS_CREATE(m->cs);
  }

  return (void*)m;
}

static void gri_mutex_enter(void* mp)
{
  GRI_MUTEX_T* m = (GRI_MUTEX_T*)mp;

  if (m != GRI_NULL)
  {
    GRI_CS_ENTER(m->cs);
    m->count++;
  }
}

static void gri_mutex_leave(void* mp)
{
  GRI_MUTEX_T* m = (GRI_MUTEX_T*)mp;

  if (m != GRI_NULL)
  {
    m->count--;
    GRI_CS_LEAVE(m->cs);
  }
}

static void gri_mutex_delete(void* mp)
{
  GRI_MUTEX_T* m = (GRI_MUTEX_T*)mp;

  if (m != GRI_NULL)
  {
    while (m->count > 0)
    {
      gri_mutex_leave(m);
    }
    GRI_CS_DELETE(m->cs);

    delete(m);
  }
}


/* ------------------------------------------------------------------------- */
/* internal defines and macros                                               */
/* ------------------------------------------------------------------------- */

#define GRI_SIZE_INT        4     /* size of an int    */
#define GRI_SIZE_FLT        4     /* size of a  float  */
#define GRI_SIZE_DBL        8     /* size of a  double */

#define GRI_NO_PARENT_NAME  "NONE    "
#define GRI_ALL_BLANKS      "        "
#define GRI_ALL_ZEROS       "\0\0\0\0\0\0\0\0"
#define GRI_END_NAME        "END     "

#define GRI_SWAPI(x,n)      if ( hdr->swap_data ) gri_swap_int(x, n)
#define GRI_SWAPF(x,n)      if ( hdr->swap_data ) gri_swap_flt(x, n)
#define GRI_SWAPD(x,n)      if ( hdr->swap_data ) gri_swap_dbl(x, n)

#define GRI_SHOW_PATH()     if ( rc == GRI_ERR_OK ) \
                                fprintf(fp, "%s:\n", hdr->path)

#define GRI_OFFSET_OF(t,m)       ((int)( (size_t)&(((t *)0)->m) ))
#define GRI_UNUSED_PARAMETER(p)  (void)(p)

/* ------------------------------------------------------------------------- */
/* Floating-point comparison macros                                          */
/* ------------------------------------------------------------------------- */

#define GRI_EPS_48          3.55271367880050092935562e-15 /* 2^(-48) */
#define GRI_EPS_49          1.77635683940025046467781e-15 /* 2^(-49) */
#define GRI_EPS_50          8.88178419700125232338905e-16 /* 2^(-50) */
#define GRI_EPS_51          4.44089209850062616169453e-16 /* 2^(-51) */
#define GRI_EPS_52          2.22044604925031308084726e-16 /* 2^(-52) */
#define GRI_EPS_53          1.11022302462515654042363e-16 /* 2^(-53) */

#define GRI_EPS             GRI_EPS_50   /* best compromise between */
                                           /* speed and accuracy      */

#define GRI_ABS(a)          ( ((a) < 0) ? -(a) : (a) )

#define GRI_EQ_EPS(a,b,e)   ( ((a) == (b)) || GRI_ABS((a)-(b)) <= \
                               (e)*(1+(GRI_ABS(a)+GRI_ABS(b))/2) )
#define GRI_EQ(a,b)         ( GRI_EQ_EPS(a, b, GRI_EPS) )

#define GRI_NE_EPS(a,b,e)   ( !GRI_EQ_EPS(a, b, e) )
#define GRI_NE(a,b)         ( !GRI_EQ    (a, b   ) )

#define GRI_LE_EPS(a,b,e)   ( ((a) < (b))  || GRI_EQ_EPS(a,b,e) )
#define GRI_LE(a,b)         ( GRI_LE_EPS(a, b, GRI_EPS) )

#define GRI_LT_EPS(a,b,e)   ( !GRI_GE_EPS(a, b, e) )
#define GRI_LT(a,b)         ( !GRI_GE    (a, b   ) )

#define GRI_GE_EPS(a,b,e)   ( ((a) > (b))  || GRI_EQ_EPS(a,b,e) )
#define GRI_GE(a,b)         ( GRI_GE_EPS(a, b, GRI_EPS) )

#define GRI_GT_EPS(a,b,e)   ( !GRI_LE_EPS(a, b, e) )
#define GRI_GT(a,b)         ( !GRI_LE    (a, b   ) )

#define GRI_ZERO_EPS(a,e)   ( ((a) == 0.0) || GRI_ABS(a) <= (e) )
#define GRI_ZERO(a)         ( GRI_ZERO_EPS(a, GRI_EPS) )

/* ------------------------------------------------------------------------- */
/* NTv2 error messages                                                       */
/* ------------------------------------------------------------------------- */

typedef struct gri_errs GRI_ERRS;
struct gri_errs
{
  int         err_num;
  const char* err_msg;
};

static const GRI_ERRS gri_errlist[] =
{
   { GRI_ERR_OK,                      "No error"               },

   /* generic errors */

   { GRI_ERR_NO_MEMORY,               "No memory"              },
   { GRI_ERR_IOERR,                   "I/O error"              },
   { GRI_ERR_NULL_HDR,                "NULL header"            },

   /* warnings */

   { GRI_ERR_FILE_NEEDS_FIXING,       "File needs fixing"      },

   /* read errors that may be ignored */

   { GRI_ERR_INVALID_LAT_MIN_MAX,     "LAT_MIN >= LAT_MAX"     },
   { GRI_ERR_INVALID_LON_MIN_MAX,     "LON_MIN >= LON_MAX"     },
   { GRI_ERR_INVALID_LAT_MIN,         "Invalid LAT_MIN"        },
   { GRI_ERR_INVALID_LAT_MAX,         "Invalid LAT_MAX"        },
   { GRI_ERR_INVALID_LAT_INC,         "Invalid LAT_INC"        },
   { GRI_ERR_INVALID_LON_MIN,         "Invalid LON_MIN"        },
   { GRI_ERR_INVALID_LON_MAX,         "Invalid LON_MAX"        },
   { GRI_ERR_INVALID_LON_INC,         "Invalid LON_INC"        },

   /* unrecoverable errors */

   { GRI_ERR_INVALID_NUM_OREC,        "Invalid NUM_OREC"       },
   { GRI_ERR_INVALID_NUM_SREC,        "Invalid NUM_SREC"       },
   { GRI_ERR_INVALID_NUM_FILE,        "Invalid NUM_FILE"       },
   { GRI_ERR_INVALID_GS_TYPE,         "Invalid GS_TYPE"        },
   { GRI_ERR_INVALID_GS_COUNT,        "Invalid GS_COUNT"       },
   { GRI_ERR_INVALID_DELTA,           "Invalid lat/lon delta"  },
   { GRI_ERR_INVALID_PARENT_NAME,     "Invalid parent name"    },
   { GRI_ERR_PARENT_NOT_FOUND,        "Parent not found"       },
   { GRI_ERR_NO_TOP_LEVEL_PARENT,     "No top-level parent"    },
   { GRI_ERR_PARENT_LOOP,             "Parent loop"            },
   { GRI_ERR_PARENT_OVERLAP,          "Parent overlap"         },
   { GRI_ERR_SUBFILE_OVERLAP,         "Subfile overlap"        },
   { GRI_ERR_INVALID_EXTENT,          "Invalid extent"         },
   { GRI_ERR_HDRS_NOT_READ,           "Headers not read yet"   },
   { GRI_ERR_UNKNOWN_FILE_TYPE,       "Unknown file type"      },
   { GRI_ERR_FILE_NOT_BINARY,         "File not binary"        },
   { GRI_ERR_FILE_NOT_ASCII,          "File not ascii"         },
   { GRI_ERR_NULL_PATH,               "NULL pathname"          },
   { GRI_ERR_ORIG_DATA_NOT_KEPT,      "Original data not kept" },
   { GRI_ERR_DATA_NOT_READ,           "Data not read yet"      },
   { GRI_ERR_CANNOT_OPEN_FILE,        "Cannot open file"       },
   { GRI_ERR_UNEXPECTED_EOF,          "Unexpected EOF"         },
   { GRI_ERR_INVALID_LINE,            "Invalid line"           },

   { -1, NULL }
};
char int_b[GRI_SIZE_INT];
char float_b[GRI_SIZE_FLT];
char doubl_b[GRI_SIZE_DBL];

#define conv_int(a)  *(int*)&(*a);
#define conv_flt(a)  *(float*)&(*a);
#define conv_dbl(a)  *(double*)&(*a);
const char* gri_errmsg(int err_num, char msg_buf[])
{
  const GRI_ERRS* e;

  for (e = gri_errlist; e->err_num >= 0; e++)
  {
    if (e->err_num == err_num)
    {
      if (msg_buf == GRI_NULL)
      {
        return e->err_msg;
      }
      else
      {
        strcpy(msg_buf, e->err_msg);
        return msg_buf;
      }
    }
  }

  if (msg_buf == GRI_NULL)
  {
    return "?";
  }
  else
  {
    sprintf(msg_buf, "%d", err_num);
    return msg_buf;
  }
}

/* ------------------------------------------------------------------------- */
/* String routines                                                           */
/* ------------------------------------------------------------------------- */

static char* gri_strip(char* str)
{
  char* s;
  char* e = GRI_NULL;

  for (; isspace(*str); str++);

  for (s = str; *s; s++)
  {
    if (!isspace(*s))
      e = s;
  }

  if (e != GRI_NULL)
    e[1] = 0;
  else
    *str = 0;

  return str;
}

static char* gri_strip_buf(char* str)
{
  char* s = gri_strip(str);

  if (s > str)
    strcpy(str, s);
  return str;
}

static int gri_strcmp_i(const char* s1, const char* s2)
{
  for (;;)
  {
    int c1 = toupper(*(const unsigned char*)s1);
    int c2 = toupper(*(const unsigned char*)s2);
    int rc;

    rc = (c1 - c2);
    if (rc != 0 || c1 == 0 || c2 == 0)
      return (rc);

    s1++;
    s2++;
  }
}

/* Like strncpy(), but guarantees a null-terminated string
 * and returns number of chars copied.
 */
static int gri_strncpy(char* buf, const char* str, int n)
{
  char* b = buf;
  const char* s;

  for (s = str; --n && *s; s++)
    *b++ = *s;
  *b = 0;

  return (int)(b - buf);
}

/* ------------------------------------------------------------------------- */
/* String tokenizing                                                         */
/* ------------------------------------------------------------------------- */

#define GRI_TOKENS_MAX     64
#define GRI_TOKENS_BUFLEN  256

typedef struct gri_token GRI_TOKEN;
struct gri_token
{
  char   buf[GRI_TOKENS_BUFLEN];
  char* toks[GRI_TOKENS_MAX];
  int    num;
};

/* tokenize a buffer
 *
 * This routine splits a line into "tokens", based on the delimiter
 * string.  Each token will have all leading/trailing whitespace
 * and embedding quotes removed from it.
 *
 * If the delimiter string is NULL or empty, then tokenizing will just
 * depend on whitespace.  In this case, multiple whitespace chars will
 * count as a single delimiter.
 *
 * Up to "maxtoks" tokens will be processed.  Any left-over chars will
 * be left in the last token. If there are less tokens than requested,
 * then the remaining token entries will point to an empty string.
 *
 * Return value is the number of tokens found.
 *
 * Note that this routine does not yet support "escaped" chars (\x).
 */
static int gri_str_tokenize(
  GRI_TOKEN* ptoks,
  const char* line,
  const char* delims,
  int         maxtoks)
{
  char* p;
  int ntoks = 1;
  int i;

  /* sanity checks */

  if (ptoks == GRI_NULL)
    return 0;

  if (maxtoks <= 0 || ntoks > GRI_TOKENS_MAX)
    maxtoks = GRI_TOKENS_MAX;

  if (line == GRI_NULL)  line = "";
  if (delims == GRI_NULL)  delims = "";

  /* copy the line, removing any leading/trailing whitespace */

  gri_strncpy(ptoks->buf, line, sizeof(ptoks->buf));
  gri_strip_buf(ptoks->buf);
  ptoks->num = 0;

  if (*ptoks->buf == 0)
    return 0;

  /* now do the tokenizing */

  p = ptoks->buf;
  ptoks->toks[0] = p;

  while (ntoks < maxtoks)
  {
    char* s;
    GRI_BOOL quotes = FALSE;

    for (s = p; *s; s++)
    {
      if (quotes)
      {
        if (*s == '"')
          quotes = FALSE;
        continue;
      }
      else if (*s == '"')
      {
        quotes = TRUE;
        continue;
      }

      if (*delims == 0)
      {
        if (!quotes && isspace(*s))
          break;
      }
      else
      {
        if (!quotes && *s == *delims)
          break;
      }
    }
    if (*s == 0)
      break;

    *s++ = 0;
    gri_strip(ptoks->toks[ntoks - 1]);

    for (p = s; isspace(*p); p++);
    ptoks->toks[ntoks++] = p;
  }

  /* now strip any enbedding quotes from each token */

  for (i = 0; i < ntoks; i++)
  {
    char* str = ptoks->toks[i];
    int len = (int)strlen(str);
    int c = *str;

    if ((c == '\'' || c == '"') && str[len - 1] == c)
    {
      str[len - 1] = 0;
      ptoks->toks[i] = ++str;
      gri_strip_buf(str);
    }
  }

  /* set rest of requested tokens to empty string */
  for (i = ntoks; i < maxtoks; i++)
    ptoks->toks[i] = NULL;

  ptoks->num = ntoks;
  return ntoks;
}

/* ------------------------------------------------------------------------- */
/* Byte swapping routines                                                    */
/* ------------------------------------------------------------------------- */

static GRI_BOOL gri_is_big_endian(void)
{
  int one = 1;

  return (*((char*)&one) == 0);
}

static GRI_BOOL gri_is_ltl_endian(void)
{
  return !gri_is_big_endian();
}

#define SWAP4(a) \
   ( (((a) & 0x000000ff) << 24) | \
     (((a) & 0x0000ff00) <<  8) | \
     (((a) & 0x00ff0000) >>  8) | \
     (((a) & 0xff000000) >> 24) )

static void gri_swap_int(int in[], int ntimes)
{
  int i;

  for (i = 0; i < ntimes; i++)
    in[i] = SWAP4((unsigned int)in[i]);
}

static void gri_swap_flt(float in[], int ntimes)
{
  gri_swap_int((int*)in, ntimes);
}

static void gri_swap_dbl(double in[], int ntimes)
{
  int  i;
  int* p_int, tmpint;

  for (i = 0; i < ntimes; i++)
  {
    p_int = (int*)(&in[i]);
    gri_swap_int(p_int, 2);

    tmpint = p_int[0];
    p_int[0] = p_int[1];
    p_int[1] = tmpint;
  }
}

/* ------------------------------------------------------------------------- */
/* generic utility routines                                                  */
/* ------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * copy a string and pad with blanks
 */
static void gri_strcpy_pad(char* tgt, const char* src)
{
  int i = 0;

  if (src != NULL)
  {
    for (; i < GRI_NAME_LEN; i++)
    {
      if (src[i] == 0)
        break;
      tgt[i] = src[i];
    }
  }

  for (; i < GRI_NAME_LEN; i++)
  {
    tgt[i] = ' ';
  }
}

/*------------------------------------------------------------------------
 * convert a string to a double
 *
 * Note: Any '.' in the string is first converted to the localized value.
 *       This means the string cannot be const, as it may be modified.
 */
static double gri_atod(char* s)
{
  char   dec_pnt = localeconv()->decimal_point[0];
  double d = 0.0;

  if (s != NULL && *s != 0)
  {
    char* p = strchr(s, '.');
    if (p != NULL)
      *p = dec_pnt;
    d = atof(s);
  }

  return d;
}

/*------------------------------------------------------------------------
 * convert a double to a string
 *
 * The string will always have a '.' as the decimal point character.
 */
static char* gri_dtoa(char* buf, double dnum)
{
  char decimal_point_char = localeconv()->decimal_point[0];
  char tmp[64];
  char* pc;
  int  i, is16, iexp, ilen, idec;
  int  nsigdigits = 16;

  if (buf == NULL)
    return NULL;

  if (nsigdigits <= 0)
    nsigdigits = 1;
  else if (nsigdigits > 16)
    nsigdigits = 16;

  is16 = nsigdigits == 16 ? 1 : 0;
  ilen = nsigdigits + 8 - is16;
  idec = nsigdigits - is16;
  sprintf(tmp, "%*.*e", ilen, idec, dnum);

  pc = &tmp[ilen - 3];
  while (*pc != '+' && *pc != '-')
  {
    pc--;
  }
  iexp = atoi(pc);

  if (is16)
  {
    pc = pc - 4;

    if (iexp < 12)
    {
      /* Check the last couple of sig digits */

      if (!strncmp(pc, "00", 2))
      {
        /* Truncate */
        /* Drop the number of significant digits */
        nsigdigits--;
      }
      else if (!strncmp(pc, "99", 2))
      {
        /* Round up */
        /* Drop the number of significant digits */
        nsigdigits--;
      }
    }
  }

  if (iexp < 0)
  {
    idec = nsigdigits - 1 + (iexp * -1);
    ilen = idec + 3;

    if (ilen > 63)
    {
      idec = nsigdigits;
      ilen = idec + 8;
      sprintf(tmp, "%*.*g", ilen, idec, dnum);
    }
    else
    {
      sprintf(tmp, "%*.*f", ilen, idec, dnum);
      for (i = (int)strlen(tmp) - 1; i >= 0; i--)
      {
        if (tmp[i] != '0')
        {
          break;
        }
        tmp[i] = 0;
      }
      if (tmp[i] == decimal_point_char)
      {
        tmp[i + 1] = '0';
        tmp[i + 2] = 0;
      }

      if (strlen(tmp) > 24 || iexp < -9)
      {
        idec = nsigdigits;
        ilen = idec + 8;
        sprintf(tmp, "%*.*g", ilen, idec, dnum);
      }
    }
    for (pc = tmp; isspace(*pc); pc++);
    strcpy(buf, pc);
  }
  else
  {
    idec = nsigdigits - 1 - iexp;
    if (idec > -1)
    {
      ilen = nsigdigits + 2;
      sprintf(buf, "%*.*f", ilen, idec, dnum);

      if (!strchr(buf, decimal_point_char))
      {
        char buf2[8];
        sprintf(buf2, "%c0", decimal_point_char);
        strcat(buf, buf2);
      }

      for (i = (int)strlen(buf) - 1; i >= 0; i--)
      {
        if (buf[i] != '0')
        {
          break;
        }
        buf[i] = 0;
      }
      if (buf[i] == decimal_point_char)
      {
        buf[i + 1] = '0';
        buf[i + 2] = 0;
      }
    }
    else
    {
      idec = nsigdigits;
      ilen = idec + 8;
      sprintf(buf, "%*.*g", ilen, idec, dnum);
    }
  }

  gri_strip_buf(buf);

  return buf;
}

/*------------------------------------------------------------------------
 * convert a double to an int
 */
static int gri_dtoi(double dbl)
{
  if (dbl == 0.0)
    return 0;

  if (dbl < 0.0)
    return (int)(dbl - 0.5);
  else
    return (int)(dbl + 0.5);
}

/*------------------------------------------------------------------------
 * Read in a line from an ascii stream.
 *
 * This will read in a line, strip all leading and trailing whitespace,
 * and discard any blank lines and comments (anything following a #).
 *
 * Returns NULL at EOF.
 */
static char* gri_read_line(ifstream *fp, char* buf, size_t buflen)
{
  char* bufp;

  for (;;)
  {
    char* p;
    fp->getline(buf, buflen);
  
    if (bufp == NULL)
      break;

    p = strchr(bufp, '#');
    if (p != NULL)
      *p = 0;

    bufp = gri_strip(bufp);
    if (*bufp != 0)
      break;
  }

  return bufp;
}

/*------------------------------------------------------------------------
 * Read in a tokenized line
 *
 * Returns number of tokens or -1 at EOF
 */
static int gri_read_toks(ifstream *fp, GRI_TOKEN* ptok, int maxtoks)
{
  char  buf[GRI_TOKENS_BUFLEN];
  char* bufp;

  bufp = gri_read_line(fp, buf, sizeof(buf));
  if (bufp == NULL)
    return -1;

  return gri_str_tokenize(ptok, bufp, NULL, maxtoks);
}

/*------------------------------------------------------------------------
 * Check if an extent is empty.
 */
static GRI_BOOL gri_extent_is_empty(const GRI_EXTENT* extent)
{
  if (extent == GRI_NULL)
    return TRUE;

  if (GRI_EQ(extent->wlon, extent->elon) ||
    GRI_EQ(extent->slat, extent->nlat))
  {
    return TRUE;
  }

  return FALSE;
}

/* ------------------------------------------------------------------------- */
/* NTv2 validation routines                                                  */
/* ------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Cleanup a name string.
 *
 * All strings in NTv2 headers are supposed to be
 * 8-byte fields that are blank-padded to 8 bytes,
 * with no null at the end.
 *
 * However, we have encountered many files in which this is
 * not the case.  We have seen trailing nulls, null/garbage,
 * and even new-line/null/garbage.  This occurs in both
 * the field names and in the value strings.
 * This can cause problems, especially in identifying which
 * records are parents and which are children.
 *
 * We have also seen lower-case (and mixed-case) chars in the field
 * names, although the spec clearly shows all field names should
 * be upper-case.
 *
 * So, our solution is to "cleanup" these fields.  This also
 * gives us the added benefit that any file that we write will
 * always be in "standard" format.
 *
 * Note we always cleanup the record-name and the parent-name in the
 * gri-rec struct, but we only cleanup the strings in the overview
 * and subfile records when we write out a new file.
 */
static int gri_cleanup_str(
  GRI_HDR* hdr,
  char* tgt,
  char* str,
  GRI_BOOL is_user_data)
{
  GRI_BOOL at_end = FALSE;
  int fixed = 0;
  int i;

  for (i = 0; i < GRI_NAME_LEN; i++)
  {
    int c = ((unsigned char*)str)[i];

    if (at_end)
    {
      tgt[i] = ' ';
      continue;
    }

    if (c < ' ' || c > 0x7f)
    {
      tgt[i] = ' ';
      at_end = TRUE;
      fixed |= GRI_FIX_UNPRINTABLE_CHAR;
      continue;
    }

    if (c == ' ' ||
      c == '_' ||
      isupper(c))
    {
      tgt[i] = (char)c;
      continue;
    }

    if (!is_user_data && islower(c))
    {
      tgt[i] = (char)toupper(c);
      fixed |= GRI_FIX_NAME_LOWERCASE;
      continue;
    }

    if (!is_user_data)
    {
      tgt[i] = ' ';
      at_end = TRUE;
      fixed |= GRI_FIX_NAME_NOT_ALPHA;
    }
    else
    {
      tgt[i] = (char)c;
    }
  }

  hdr->fixed |= fixed;
  return fixed;
}

/*------------------------------------------------------------------------
 * Create a printable string from a name for use in error messages.
 */
static char* gri_printable(
  char* buf,
  const char* str)
{
  char* b = buf;
  int i;

  for (i = 0; i < GRI_NAME_LEN; i++)
  {
    if (str[i] < ' ')
    {
      *b++ = '^';
      *b++ = (str[i] + (char)'@');
    }
    else
    {
      *b++ = str[i];
    }
  }
  *b = 0;

  return buf;
}

/*------------------------------------------------------------------------
 * Validate an overview record.
 *
 * Interestingly, even though the NTv2 spec was developed by the
 * Canadian government, all Canadian files use the keywords
 * "DATUM_F" and "DATUM_T" instead of "SYSTEM_F" and "SYSTEM_T".
 * The Brazilian files also do this.
 */
static int gri_validate_ov_field(
  GRI_HDR* hdr,
  FILE* fp,
  char* str,
  const char* name,
  int         rc)
{
  char temp[GRI_NAME_LEN + 1];

  if (name == GRI_NULL)
  {
    gri_cleanup_str(hdr, temp, str, TRUE);
    temp[GRI_NAME_LEN] = 0;
    name = temp;
  }

  if (strncmp(str, name, GRI_NAME_LEN) != 0)
  {
    if (fp != GRI_NULL)
    {
      char buf[24];

      if (rc == GRI_ERR_OK)
      {
        GRI_SHOW_PATH();
        rc = GRI_ERR_FILE_NEEDS_FIXING;
      }

      fprintf(fp, "  overview record: \"%s\" should be \"%s\"\n",
        gri_printable(buf, str), name);
    }

    strncpy(str, name, GRI_NAME_LEN);
  }

  return rc;
}

static int gri_validate_ov(
  GRI_HDR* hdr,
  FILE* fp,
  int       rc)
{
  if (hdr->overview != GRI_NULL)
  {
    GRI_FILE_OV* ov = hdr->overview;

    /* field names */
    rc = gri_validate_ov_field(hdr, fp, ov->n_num_orec, "NUM_OREC", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_num_srec, "NUM_SREC", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_num_file, "NUM_FILE", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_gs_type, "GS_TYPE ", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_version, "VERSION ", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_system_f, "SYSTEM_F", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_system_t, "SYSTEM_T", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_major_f, "MAJOR_F ", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_major_t, "MAJOR_T ", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_minor_f, "MINOR_F ", rc);
    rc = gri_validate_ov_field(hdr, fp, ov->n_minor_t, "MINOR_T ", rc);

    /* user values */
    rc = gri_validate_ov_field(hdr, fp, ov->s_gs_type, GRI_NULL, rc);
    rc = gri_validate_ov_field(hdr, fp, ov->s_version, GRI_NULL, rc);
    rc = gri_validate_ov_field(hdr, fp, ov->s_system_f, GRI_NULL, rc);
    rc = gri_validate_ov_field(hdr, fp, ov->s_system_t, GRI_NULL, rc);
  }

  return rc;
}

/*------------------------------------------------------------------------
 * Validate a subfile record.
 */
static int gri_validate_sf_field(
  GRI_HDR* hdr,
  FILE* fp,
  int         n,
  char* str,
  const char* name,
  int         rc)
{
  char temp[GRI_NAME_LEN + 1];

  if (name == GRI_NULL)
  {
    gri_cleanup_str(hdr, temp, str, TRUE);
    name = temp;
    temp[GRI_NAME_LEN] = 0;
  }

  if (strncmp(str, name, GRI_NAME_LEN) != 0)
  {
    if (fp != GRI_NULL)
    {
      char buf[24];

      if (rc == GRI_ERR_OK)
      {
        GRI_SHOW_PATH();
        rc = GRI_ERR_FILE_NEEDS_FIXING;
      }

      fprintf(fp, "  subfile rec %3d: \"%s\" should be \"%s\"\n",
        n, gri_printable(buf, str), name);
    }

    strncpy(str, name, GRI_NAME_LEN);
  }

  return rc;
}

static int gri_validate_sf(
  GRI_HDR* hdr,
  FILE* fp,
  int       n,
  int       rc)
{
  if (hdr->subfiles != GRI_NULL)
  {
    GRI_FILE_SF* sf = hdr->subfiles + n;

    /* field names */
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_sub_name, "SUB_NAME", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_parent, "PARENT  ", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_created, "CREATED ", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_updated, "UPDATED ", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_s_lat, "S_LAT   ", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_n_lat, "N_LAT   ", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_e_lon, "E_LONG  ", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_w_lon, "W_LONG  ", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_lat_inc, "LAT_INC ", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_lon_inc, "LONG_INC", rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->n_gs_count, "GS_COUNT", rc);

    /* user values */
    rc = gri_validate_sf_field(hdr, fp, n, sf->s_sub_name, GRI_NULL, rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->s_parent, GRI_NULL, rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->s_created, GRI_NULL, rc);
    rc = gri_validate_sf_field(hdr, fp, n, sf->s_updated, GRI_NULL, rc);
  }

  return rc;
}

/*------------------------------------------------------------------------
 * Check if two sub-file records overlap.
 */
static GRI_BOOL gri_overlap(
  GRI_REC* r1,
  GRI_REC* r2)
{
  if (GRI_GE(r1->lat_min, r2->lat_max) ||
    GRI_LE(r1->lat_max, r2->lat_min) ||
    GRI_GE(r1->lon_min, r2->lon_max) ||
    GRI_LE(r1->lon_max, r2->lon_min))
  {
    return FALSE;
  }

  return TRUE;
}

/*------------------------------------------------------------------------
 * Validate a subfile against its parent.
 */
static int gri_validate_subfile(
  GRI_HDR* hdr,
  GRI_REC* par,
  GRI_REC* sub,
  FILE* fp,
  int       rc)
{
  char   buf[32];
  double t;
  int n;

  /* ------ is parent's lat_inc a multiple of subfile's lat_inc ? */

  n = gri_dtoi(par->lat_inc / sub->lat_inc);
  t = sub->lat_inc * n;
  if (!GRI_EQ(t, par->lat_inc))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d : parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LAT_INC not a multiple of parent LAT_INC");
      fprintf(fp, "    sub lat_inc = %s\n", gri_dtoa(buf, sub->lat_inc));
      fprintf(fp, "    par lat_inc = %s\n", gri_dtoa(buf, par->lat_inc));
      fprintf(fp, "    num cells   = %d\n", n);
      fprintf(fp, "    should be   = %s\n", gri_dtoa(buf, t));
    }
    rc = GRI_ERR_INVALID_LAT_INC;
  }

  /* ------ is subfile's lat_min >= parent's lat_min ? */

  if (!GRI_GE(sub->lat_min, par->lat_min))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d: parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LAT_MIN < parent LAT_MIN");
      fprintf(fp, "    sub lat_min = %s\n", gri_dtoa(buf, sub->lat_min));
      fprintf(fp, "    par lat_min = %s\n", gri_dtoa(buf, par->lat_min));
    }
    rc = GRI_ERR_INVALID_LAT_MIN;
  }

  /* ------ does subfile's lat_min line up with parent's grid line ? */

  n = gri_dtoi((sub->lat_min - par->lat_min) / par->lat_inc);
  t = par->lat_min + (n * par->lat_inc);
  if (!GRI_EQ(t, sub->lat_min))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d: parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LAT_MIN not on parent grid line");
      fprintf(fp, "    par LAT_MIN = %s\n", gri_dtoa(buf, par->lat_min));
      fprintf(fp, "    par LAT_INC = %s\n", gri_dtoa(buf, par->lat_inc));
      fprintf(fp, "    num cells   = %d\n", n);
      fprintf(fp, "    sub LAT_MIN = %s\n", gri_dtoa(buf, sub->lat_min));
      fprintf(fp, "    should be   = %s\n", gri_dtoa(buf, t));
    }
    rc = GRI_ERR_INVALID_LAT_MIN;
  }

  /* ------ is subfile's lat_max <= parent's lat_max ? */

  if (!GRI_LE(sub->lat_max, par->lat_max))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d: parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LAT_MAX > parent LAT_MAX");
      fprintf(fp, "    par lat_max = %s\n", gri_dtoa(buf, par->lat_max));
      fprintf(fp, "    sub lat_max = %s\n", gri_dtoa(buf, sub->lat_max));
    }
    rc = GRI_ERR_INVALID_LAT_MAX;
  }

  /* ------ does subfile's lat_max line up with parent's grid line ? */

  n = gri_dtoi((par->lat_max - sub->lat_max) / par->lat_inc);
  t = par->lat_max - (n * par->lat_inc);
  if (!GRI_EQ(t, sub->lat_max))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d: parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LAT_MAX not on parent grid line");
      fprintf(fp, "    par LAT_MAX = %s\n", gri_dtoa(buf, par->lat_max));
      fprintf(fp, "    par LAT_INC = %s\n", gri_dtoa(buf, par->lat_inc));
      fprintf(fp, "    num cells   = %d\n", n);
      fprintf(fp, "    sub LAT_MAX = %s\n", gri_dtoa(buf, sub->lat_max));
      fprintf(fp, "    should be   = %s\n", gri_dtoa(buf, t));
    }
    rc = GRI_ERR_INVALID_LAT_MAX;
  }

  /* ------ is parent's lon_inc a multiple of subfile's lon_inc ? */

  n = gri_dtoi(par->lon_inc / sub->lon_inc);
  t = sub->lon_inc * n;
  if (!GRI_EQ(t, par->lon_inc))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d: parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LON_INC not a multiple of parent LON_INC");
      fprintf(fp, "    sub lon_inc = %s\n", gri_dtoa(buf, sub->lon_inc));
      fprintf(fp, "    num cells   = %d\n", n);
      fprintf(fp, "    par lon_inc = %s\n", gri_dtoa(buf, par->lon_inc));
      fprintf(fp, "    should be   = %s\n", gri_dtoa(buf, t));
    }
    rc = GRI_ERR_INVALID_LON_INC;
  }

  /* ------ is subfile's lon_min >= parent's lon_min ? */

  if (!GRI_GE(sub->lon_min, par->lon_min))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d: parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LON_MIN < parent LON_MIN");
      fprintf(fp, "    par lon_min = %s\n", gri_dtoa(buf, par->lon_min));
      fprintf(fp, "    sub lon_min = %s\n", gri_dtoa(buf, sub->lon_min));
    }
    rc = GRI_ERR_INVALID_LON_MIN;
  }

  /* ------ does subfile's lon_min line up with parent's grid line ? */

  n = gri_dtoi((sub->lon_min - par->lon_min) / par->lon_inc);
  t = par->lon_min + (n * par->lon_inc);
  if (!GRI_EQ(t, sub->lon_min))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d: parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LON_MIN not on parent grid line");
      fprintf(fp, "    par LON_MIN = %s\n", gri_dtoa(buf, par->lon_min));
      fprintf(fp, "    par LON_INC = %s\n", gri_dtoa(buf, par->lon_inc));
      fprintf(fp, "    num cells   = %d\n", n);
      fprintf(fp, "    sub LON_MIN = %s\n", gri_dtoa(buf, sub->lon_min));
      fprintf(fp, "    should be   = %s\n", gri_dtoa(buf, t));
    }
    rc = GRI_ERR_INVALID_LON_MIN;
  }

  /* ------ is subfile's lon_max <= parent's lon_max ? */

  if (!GRI_LE(sub->lon_max, par->lon_max))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d: parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LON_MAX > parent LON_MAX");
      fprintf(fp, "    par lon_max = %s\n", gri_dtoa(buf, par->lon_max));
      fprintf(fp, "    sub lon_max = %s\n", gri_dtoa(buf, sub->lon_max));
    }
    rc = GRI_ERR_INVALID_LON_MAX;
  }

  /* ------ does subfile's lon_max line up with parent's grid line ? */

  n = gri_dtoi((par->lon_max - sub->lon_max) / par->lon_inc);
  t = par->lon_max - (n * par->lon_inc);
  if (!GRI_EQ(t, sub->lon_max))
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();
      fprintf(fp, "  record %3d: parent %3d: %s\n",
        sub->rec_num, par->rec_num,
        "subfile LON_MAX not on parent grid line");
      fprintf(fp, "    par LON_MAX = %s\n", gri_dtoa(buf, par->lon_max));
      fprintf(fp, "    par LON_INC = %s\n", gri_dtoa(buf, par->lon_inc));
      fprintf(fp, "    num cells   = %d\n", n);
      fprintf(fp, "    sub LON_MAX = %s\n", gri_dtoa(buf, sub->lon_max));
      fprintf(fp, "    should be   = %s\n", gri_dtoa(buf, t));
    }
    rc = GRI_ERR_INVALID_LON_MAX;
  }

  return rc;
}

/*------------------------------------------------------------------------
 * Validate all headers in an NTv2 file.
 */
int gri_validate(
  GRI_HDR* hdr,
  FILE* fp)
{
  GRI_REC* rec;
  GRI_REC* sub;
  GRI_REC* next;
  char buf[32];
  int  i;
  int  rc = GRI_ERR_OK;

  rc = gri_validate_ov(hdr, fp, rc);

  /* ------ sanity checks on each record */

  for (i = 0; i < hdr->num_recs; i++)
  {
    double temp;

    rc = gri_validate_sf(hdr, fp, i, rc);

    rec = &hdr->recs[i];

    /* ------ is lat_inc > 0 ? */

    if (!(rec->lat_inc > 0.0))
    {
      if (fp != GRI_NULL)
      {
        GRI_SHOW_PATH();
        fprintf(fp, "  record %3d: %s\n", rec->rec_num,
          "LAT_INC <= 0");
        fprintf(fp, "    LAT_INC     = %s\n", gri_dtoa(buf, rec->lat_inc));
      }
      rc = GRI_ERR_INVALID_LAT_INC;
    }

    /* ------ is lat_min < lat_max ? */

    if (!(rec->lat_min < rec->lat_max))
    {
      if (fp != GRI_NULL)
      {
        GRI_SHOW_PATH();
        fprintf(fp, "  record %3d: %s\n", rec->rec_num,
          "LAT_MIN >= LAT_MAX");
        fprintf(fp, "    LAT_MIN     = %s\n", gri_dtoa(buf, rec->lat_min));
        fprintf(fp, "    LAT_MAX     = %s\n", gri_dtoa(buf, rec->lat_max));
      }
      rc = GRI_ERR_INVALID_LAT_MIN_MAX;
    }

    /* ------ is lon_inc > 0 ? */

    if (!(rec->lon_inc > 0.0))
    {
      if (fp != GRI_NULL)
      {
        GRI_SHOW_PATH();
        fprintf(fp, "  record %3d: %s\n", rec->rec_num,
          "LON_INC <= 0");
        fprintf(fp, "    LAT_INC     = %s\n", gri_dtoa(buf, rec->lon_inc));
      }
      rc = GRI_ERR_INVALID_LON_INC;
    }

    /* ------ is lon_min < lon_max ? */

    if (!(rec->lon_min < rec->lon_max))
    {
      if (fp != GRI_NULL)
      {
        GRI_SHOW_PATH();
        fprintf(fp, "  record %3d: %s\n", rec->rec_num,
          "LON_MIN >= LON_MAX");
        fprintf(fp, "    LON_MIN     = %s\n", gri_dtoa(buf, rec->lon_min));
        fprintf(fp, "    LON_MAX     = %s\n", gri_dtoa(buf, rec->lon_max));
      }
      rc = GRI_ERR_INVALID_LON_MIN_MAX;
    }

    /* ------ is num_recs > 0 ? */

    if (!(rec->num > 0))
    {
      if (fp != GRI_NULL)
      {
        GRI_SHOW_PATH();
        fprintf(fp, "  record %3d: %s\n", rec->rec_num,
          "GS_COUNT <= 0");
        fprintf(fp, "    num         = %d\n", rec->num);
      }
      rc = GRI_ERR_INVALID_GS_COUNT;
    }

    /* ------ is (lat_max - lat_min) a multiple of lat_inc ? */

    temp = rec->lat_min + ((rec->nrows - 1) * rec->lat_inc);
    if (!GRI_EQ(temp, rec->lat_max))
    {
      if (fp != GRI_NULL)
      {
        GRI_SHOW_PATH();
        fprintf(fp, "  record %3d: %s\n", rec->rec_num,
          "LAT range not a multiple of LAT_INC");
        fprintf(fp, "    LON_MIN     = %s\n", gri_dtoa(buf, rec->lat_min));
        fprintf(fp, "    LON_MAX     = %s\n", gri_dtoa(buf, rec->lat_max));
        fprintf(fp, "    range       = %s\n", gri_dtoa(buf, rec->lat_max - rec->lat_min));
        fprintf(fp, "    LON_INC     = %s\n", gri_dtoa(buf, rec->lat_inc));
        fprintf(fp, "    n           = %d\n", rec->nrows - 1);
        fprintf(fp, "    t           = %s\n", gri_dtoa(buf, temp));
      }
      rc = GRI_ERR_INVALID_LAT_INC;
    }

    /* ------ is (lon_max - lon_min) a multiple of lon_inc ? */

    temp = rec->lon_min + ((rec->ncols - 1) * rec->lon_inc);
    if (!GRI_EQ(temp, rec->lon_max))
    {
      if (fp != GRI_NULL)
      {
        GRI_SHOW_PATH();
        fprintf(fp, "  record %3d: %s\n", rec->rec_num,
          "LON range not a multiple of LON_INC");
        fprintf(fp, "    LON_MIN     = %s\n", gri_dtoa(buf, rec->lon_min));
        fprintf(fp, "    LON_MAX     = %s\n", gri_dtoa(buf, rec->lon_max));
        fprintf(fp, "    range       = %s\n", gri_dtoa(buf, rec->lon_max - rec->lon_min));
        fprintf(fp, "    LON_INC     = %s\n", gri_dtoa(buf, rec->lon_inc));
        fprintf(fp, "    n           = %d\n", rec->ncols - 1);
        fprintf(fp, "    t           = %s\n", gri_dtoa(buf, temp));
      }
      rc = GRI_ERR_INVALID_LON_INC;
    }

    /* ------ is (nrows * ncols) == num ? */

    if (!((rec->nrows * rec->ncols) == rec->num))
    {
      if (fp != GRI_NULL)
      {
        GRI_SHOW_PATH();
        fprintf(fp, "  record %3d: %s\n", rec->rec_num,
          "NROWS * NCOLS != COUNT");
        fprintf(fp, "    nrows       = %d\n", rec->nrows);
        fprintf(fp, "    ncols       = %d\n", rec->ncols);
        fprintf(fp, "    num         = %d\n", rec->num);
      }
      rc = GRI_ERR_INVALID_DELTA;
    }
  }

  if (rc > GRI_ERR_START)
  {
    /* no point in going any further */
    return rc;
  }

  /* ------ check if any parents overlap */

  for (rec = hdr->first_parent; rec != GRI_NULL; rec = rec->next)
  {
    for (next = rec->next; next != GRI_NULL; next = next->next)
    {
      if (gri_overlap(rec, next))
      {
        if (fp != GRI_NULL)
        {
          GRI_SHOW_PATH();
          fprintf(fp, "  record %d: record %3d: %s\n",
            rec->rec_num, next->rec_num,
            "parent overlap");
        }
        rc = GRI_ERR_PARENT_OVERLAP;
      }
    }
  }

  /* ------ for each parent:
               - validate all subfiles against their parent
               - verify that no subfiles of a parent overlap
  */

  for (rec = hdr->first_parent; rec != GRI_NULL; rec = rec->next)
  {
    for (sub = rec->sub; sub != GRI_NULL; sub = sub->next)
    {
      rc = gri_validate_subfile(hdr, rec, sub, fp, rc);

      for (next = sub->next; next != GRI_NULL; next = next->next)
      {
        if (gri_overlap(sub, next))
        {
          if (fp != GRI_NULL)
          {
            GRI_SHOW_PATH();
            fprintf(fp, "  record %d: record %3d: %s\n",
              sub->rec_num, next->rec_num,
              "subfile overlap");
          }
          rc = GRI_ERR_SUBFILE_OVERLAP;
        }
      }
    }
  }

  /* ------ check if the file needs fixing */

  if (hdr->fixed != 0)
  {
    if (fp != GRI_NULL)
    {
      GRI_SHOW_PATH();

      if ((hdr->fixed & GRI_FIX_UNPRINTABLE_CHAR) != 0)
        fprintf(fp, "  fix: name contains unprintable character\n");

      if ((hdr->fixed & GRI_FIX_NAME_LOWERCASE) != 0)
        fprintf(fp, "  fix: name contains lowercase letter\n");

      if ((hdr->fixed & GRI_FIX_NAME_NOT_ALPHA) != 0)
        fprintf(fp, "  fix: name contains non-alphameric character\n");

      if ((hdr->fixed & GRI_FIX_BLANK_PARENT_NAME) != 0)
        fprintf(fp, "  fix: parent name blank\n");

      if ((hdr->fixed & GRI_FIX_BLANK_SUBFILE_NAME) != 0)
        fprintf(fp, "  fix: subfile name blank\n");

      if ((hdr->fixed & GRI_FIX_END_REC_NOT_FOUND) != 0)
        fprintf(fp, "  fix: end record not found\n");

      if ((hdr->fixed & GRI_FIX_END_REC_NAME_NOT_ALPHA) != 0)
        fprintf(fp, "  fix: end record name not all alphameric\n");

      if ((hdr->fixed & GRI_FIX_END_REC_PAD_NOT_ZERO) != 0)
        fprintf(fp, "  fix: end record pad not all zeros\n");
    }

    /* Note that we only set this if there are no other errors,
       since any other error is a lot more important.
    */
    if (rc == GRI_ERR_OK)
      rc = GRI_ERR_FILE_NEEDS_FIXING;
  }

  if (rc != GRI_ERR_OK)
  {
    if (fp != GRI_NULL)
      fprintf(fp, "\n");
  }

  return rc;
}

/*------------------------------------------------------------------------
 * Fix all parent and subfile pointers.
 *
 * This creates the chain of parent pointers  (stored in hdr->first_parent)
 * and the chain of sub-files for each parent (stored in rec->sub).
 */
static int gri_fix_ptrs(
  GRI_HDR* hdr)
{
  GRI_REC* sub = GRI_NULL;
  int i, j;

  hdr->num_parents = 0;
  hdr->first_parent = GRI_NULL;

  /* -------- adjust all parent pointers */

  for (i = 0; i < hdr->num_recs; i++)
  {
    GRI_REC* rec = &hdr->recs[i];

    if (rec->active)
    {
      rec->parent = GRI_NULL;
      rec->sub = GRI_NULL;
      rec->next = GRI_NULL;

      if (gri_strcmp_i(rec->record_name, GRI_ALL_BLANKS) == 0)
        hdr->fixed |= GRI_FIX_BLANK_SUBFILE_NAME;

      if (gri_strcmp_i(rec->parent_name, GRI_NO_PARENT_NAME) == 0)
      {
        /* This record has no parent, so it is a top-level parent.
           Bump our parent count & add it to the top-level parent chain.
        */

        hdr->num_parents++;
        if (sub != GRI_NULL)
          sub->next = rec;
        else
          hdr->first_parent = rec;
        sub = rec;
      }
      else
        if (gri_strcmp_i(rec->record_name, rec->parent_name) == 0)
        {
          /* A record can't be its own parent. */
          return GRI_ERR_INVALID_PARENT_NAME;
        }
        else
        {
          /* This record has a parent.
             Add it to its parent's child chain.
             It is an error if we can't find its parent.
          */

          GRI_REC* p = GRI_NULL;

          /*
             I cannot find anything in the NTv2 spec that states
                definitively that a child must appear later in the file
                than a parent, although that is the case for all files
                that I've seen, with sub-files immediately following
                their parent.

             In fact, the NTv2 Developer's Guide states "the order of
                the sub-files is of no consequence", which could be
                interpreted as meaning that the sub-files for a particular
                grid must follow it but may be in any order, but it doesn't
                explicitly say that.

             Thus, we check all records (other than this one) to see if
                it is the parent of this one.

             If we can establish that children must follow their parents,
                then the for-loop below can go to i instead of hdr->num_recs.

             NEWS AT 11:
                I have just found a published Australian NTv2 file
                (australia/WA_0700.gsb) in which all the sub-files appear
                ahead of their parent!

                It appears that the people who wrote this file just wrote
                all the sub-files in alphabetical order by sub-file name.

                However, that file also doesn't blank pad its sub-file field
                names or its end record properly, so it could be considered
                as a bogusly-created file and they just didn't know what
                they were doing, which it seems they didn't.  Nevertheless,
                it is a published file, so we have to deal with it.
          */

          for (j = 0; j < hdr->num_recs; j++)
          {
            GRI_REC* next = hdr->recs + j;

            if (i != j && next->active)
            {
              if (gri_strcmp_i(rec->parent_name, next->record_name) == 0)
              {
                p = next;
                break;
              }
            }
          }

          if (p == GRI_NULL)
          {
            /*
               This record's parent can't be found.  Check if they
               stoopidly left the parent name blank, instead of using
               "NONE" like they're supposed to do.  If they did, we'll
               just fix it.  Note that the parent name has already been
               "cleaned up".
            */

            if (gri_strcmp_i(rec->parent_name, GRI_ALL_BLANKS) == 0)
            {
              strcpy(rec->parent_name, GRI_NO_PARENT_NAME);
              if (hdr->subfiles != GRI_NULL)
              {
                memcpy(hdr->subfiles[rec->rec_num].s_parent,
                  GRI_NO_PARENT_NAME, GRI_NAME_LEN);
              }
              hdr->fixed |= GRI_FIX_BLANK_PARENT_NAME;
              i--;
              continue;
            }

            return GRI_ERR_PARENT_NOT_FOUND;
          }

          rec->parent = p;
          p->num_subs++;
        }
    }
  }

  /* -------- make sure that we have at least one top-level parent.

     It is possible to have no top-level parents.  For example,
     assume we have only two records (A and B), and A's parent is B
     and B's parent is A.
  */

  if (hdr->first_parent == GRI_NULL)
  {
    return GRI_ERR_NO_TOP_LEVEL_PARENT;
  }

  /* -------- validate all parent chains

     At this point, we have ascertained that all records
     either are a top-level parent (i.e. have no parent)
     or have a valid parent.  We have also counted the top-level
     parents and have created a chain of them to follow.

     Now we want to validate that all sub-files ultimately
     point to a top-level parent.  i.e., there are no parent chain
     loops (eg, subfile A's parent is B and B's parent is A).

     Our logic here is to get the longest possible parent chain
     (# records - # parents + 1), then chase each record's parent
     back.  If we don't find a top-level parent within the
     chain length, we must have a loop.
  */

  {
    int max_chain = (hdr->num_recs - hdr->num_parents) + 1;

    for (i = 0; i < hdr->num_recs; i++)
    {
      GRI_REC* rec = &hdr->recs[i];

      if (rec->active)
      {
        GRI_REC* parent = rec->parent;
        int count = 0;

        for (; parent != GRI_NULL; parent = parent->parent)
        {
          if (++count > max_chain)
            return GRI_ERR_PARENT_LOOP;
        }
      }
    }
  }

  /* -------- adjust all sub-file pointers

     Here we create the chains of all sub-file pointers.
  */

  for (i = 0; i < hdr->num_recs; i++)
  {
    GRI_REC* rec = &hdr->recs[i];

    if (rec->active)
    {
      sub = GRI_NULL;

      /* See the comment above about child records.

         We check all records (other than this one) to see if
         it is a child of this one.  If it is, we add it to
         this record's child chain.

         If we could enforce the rule that children must follow
         their parents (which we can't), then the for-loop below
         could go from i+1 instead of from 0.
      */

      for (j = 0; j < hdr->num_recs; j++)
      {
        GRI_REC* next = hdr->recs + j;

        if (i != j && next->active)
        {
          if (next->parent == rec)
          {
            if (rec->sub == GRI_NULL)
              rec->sub = next;

            if (sub != GRI_NULL)
              sub->next = next;
            sub = next;
          }
        }
      }
    }
  }

  return GRI_ERR_OK;
}

/* ------------------------------------------------------------------------- */
/* NTv2 common routines                                                      */
/* ------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Determine the file type of a name
 *
 * This is done solely by checking the filename extension.
 * No examination of the file contents (if any) is done.
 */
int gri_filetype(const char* grifile)
{
  const char* ext;

  if (grifile == GRI_NULL || *grifile == 0)
    return GRI_FILE_TYPE_UNK;

  ext = strrchr(grifile, '.');
  if (ext != GRI_NULL)
  {
    ext++;

    if (gri_strcmp_i(ext, GRI_FILE_BIN_EXTENSION) == 0)
      return GRI_FILE_TYPE_BIN;

    if (gri_strcmp_i(ext, GRI_FILE_ASC_EXTENSION) == 0)
      return GRI_FILE_TYPE_ASC;
  }

  return GRI_FILE_TYPE_UNK;
}

/*------------------------------------------------------------------------
 * Create an empty NTv2 struct, query the file type, and open the file.
 */
GRI_HDR* gri_create(const char* grifile, int type, int* prc)
{
  GRI_HDR* hdr;

  hdr =  new GRI_HDR[(sizeof(*hdr))];
  if (hdr == GRI_NULL)
  {
    *prc = GRI_ERR_NO_MEMORY;
    return GRI_NULL;
  }

  //memset(hdr, 0, sizeof(*hdr));

  strcpy(hdr->path, grifile);
  hdr->file_type = type;
  hdr->lon_min = 360.0;
  hdr->lat_min = 90.0;
  hdr->lon_max = -360.0;
  hdr->lat_max = -90.0;
  hdr->fp = new ifstream;
  hdr->fp->open(hdr->path, ios_base::in | ios_base::binary);
  if (hdr->fp->fail())
  {
    *prc = GRI_ERR_CANNOT_OPEN_FILE;
    return GRI_NULL;
  }

  if (type == GRI_FILE_TYPE_BIN)
  {
    hdr->mutex = gri_mutex_create();
  }

  *prc = GRI_ERR_OK;
  return hdr;
}

/*------------------------------------------------------------------------
 * Delete a NTv2 object
 */
void gri_delete(GRI_HDR* hdr)
{
  if (hdr != GRI_NULL)
  {
    int i;

    if (hdr->fp->is_open())
    {
      hdr->fp->close();
    }
    delete  hdr->fp;

    if (hdr->mutex != GRI_NULL)
    {
      gri_mutex_delete(hdr->mutex);
    }

    for (i = 0; i < hdr->num_recs; i++)
    {
      delete(hdr->recs[i].shifts);
      delete(hdr->recs[i].accurs);
    }

    delete(hdr->overview);
    delete(hdr->subfiles);

    delete(hdr->recs);
    hdr=NULL;
  }
}

/* ------------------------------------------------------------------------- */
/* NTv2 binary read routines                                                 */
/* ------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Read a binary overview record.
 *
 * The original Canadian specification for NTv2 files included
 * a 4-byte pad of zeros after each int in the headers, which
 * kept the length of each record (overview field, sub-file field,
 * or grid-shift entry) to be 16 bytes.  This was probably done
 * so they could read the files in FORTRAN or on a main-frame using
 * "DCB=(RECFM=FB,LRECL=16,...)".
 *
 * However, when Australia started using this file format, they
 * misunderstood the reason for the padding and thought it was a compiler
 * issue.  The Canadians had implemented NTv2 using FORTRAN,
 * and information at the time suggested that the use of records in
 * a FORTRAN binary file would create a file that other languages
 * would have a difficult time reading and writing.  Thus, the
 * Australians decided to format the file as specified, but to
 * ignore any "auxiliary record identifiers" that FORTRAN used (i.e.
 * the padding bytes).
 *
 * To make matters worse, New Zealand copied the Australian
 * format.  Blimey!  However, it became apparent that the NTv2 binary
 * format is not compiler dependent, and since 2000 both Australia
 * and New Zealand have been writing new files correctly.
 *
 * But this means that when reading a NTv2 header record we have to
 * determine whether these pad bytes are there, since we may be reading
 * an older Australian or New Zealand NTv2 file.
 *
 * Also, there is nothing in the NTv2 specification that states
 * whether the file should be written big-endian or little-endian,
 * so we also have to determine how it was written, so we can know if
 * we have to byte-swap the numbers.  However, all files we've seen
 * so far are all little-endian.
 *
 * Also, when we cache the overview and subfile records, they are always
 * stored in native-endian format.  They get byte-swapped back, if needed,
 * when written out.  This allows us to choose how they are written out,
 * and makes it easier to dump the data.
 *
 *------------------------------------------------------------------------
 * Implementation note:
 *   In GCC, a function can be declared with the attribute "warn_unused_result",
 *   which results in a warning being issued if the return value from that
 *   function is not used.  This, of course, will cause a compilation to
 *   fail if "-Werror" (treat warnings as errors) is specified on the
 *   command line (which we like to do).  For details, see
 *   "http://gcc.gnu.org/onlinedocs/gcc-3.4.6/gcc/Function-Attributes.html".
 *
 *   Despite flames from the user community, Ubuntu Linux sets this attribute
 *   for the fread() function.  Unfortunately, we don't bother to check the
 *   result from each call to fread(), since that makes the code real ugly
 *   and it is easier just to check ferror() at the end.  But, because of
 *   Ubuntu's insistence on requiring that the return value be used, we store
 *   the result but we don't bother checking it.
 *------------------------------------------------------------------------
 */
static int gri_read_ov_bin(
  GRI_HDR* hdr,
  GRI_FILE_OV* ov)
{
 

  /* -------- NUM_OREC */

  hdr->fp->read(ov->n_num_orec, GRI_NAME_LEN);


  hdr->fp->read((char*)&ov->i_num_orec, GRI_SIZE_INT);


  /* determine if byte-swapping is needed */

  if (ov->i_num_orec != 11)
  {
    hdr->swap_data = TRUE;
    GRI_SWAPI(&ov->i_num_orec, 1);
    if (ov->i_num_orec != 11)
      return GRI_ERR_INVALID_NUM_OREC;
  }

  /* determine if pad-bytes are present */
  hdr->fp->read((char*) &ov->p_num_orec, GRI_SIZE_INT);
  if (ov->p_num_orec == 0)
  {
    hdr->pads_present = TRUE;
  }
  else
  {
    ov->p_num_orec = 0;
    hdr->fp->seekg( -GRI_SIZE_INT, ios_base::cur);
  }

  /* -------- NUM_SREC */

  hdr->fp->read(ov->n_num_srec, GRI_NAME_LEN);
  hdr->fp->read((char*) &ov->i_num_srec, GRI_SIZE_INT);
   ov->p_num_srec = 0;

  GRI_SWAPI(&ov->i_num_srec, 1);
  if (ov->i_num_srec != 11)
    return GRI_ERR_INVALID_NUM_SREC;

  if (hdr->pads_present)
    hdr->fp->seekg(GRI_SIZE_INT, ios_base::cur);

  /* -------- NUM_FILE */

  hdr->fp->read(ov->n_num_file, GRI_NAME_LEN);
  hdr->fp->read((char*)&ov->i_num_file, GRI_SIZE_INT);
  ov->p_num_file = 0;

  GRI_SWAPI(&ov->i_num_file, 1);
  if (ov->i_num_file <= 0)
    return GRI_ERR_INVALID_NUM_FILE;
  hdr->num_recs = ov->i_num_file;

  if (hdr->pads_present)
    hdr->fp->seekg(GRI_SIZE_INT, ios_base::cur);

  /* -------- GS_TYPE */

  hdr->fp->read((char*)ov->n_gs_type, GRI_NAME_LEN);
  hdr->fp->read((char*)ov->s_gs_type, GRI_NAME_LEN);

  /* -------- VERSION */

  hdr->fp->read((char*)ov->n_version, GRI_NAME_LEN);
  hdr->fp->read((char*)ov->s_version, GRI_NAME_LEN);

  /* -------- SYSTEM_F */

  hdr->fp->read((char*)ov->n_system_f, GRI_NAME_LEN);
  hdr->fp->read((char*)ov->s_system_f, GRI_NAME_LEN);

  /* -------- SYSTEM_T */

  hdr->fp->read((char*)ov->n_system_t, GRI_NAME_LEN);
  hdr->fp->read((char*)ov->s_system_t, GRI_NAME_LEN);

  /* -------- MAJOR_F */

  hdr->fp->read((char*)ov->n_major_f, GRI_NAME_LEN);
  hdr->fp->read((char*)&ov->d_major_f, GRI_SIZE_DBL);
 
  
  
  GRI_SWAPD(&ov->d_major_f, 1);

  /* -------- MINOR_F */

  hdr->fp->read((char*)ov->n_minor_f, GRI_NAME_LEN);
  hdr->fp->read((char*)&ov->d_minor_f, GRI_SIZE_DBL);
   GRI_SWAPD(&ov->d_minor_f, 1);

  /* -------- MAJOR_T */

  hdr->fp->read((char*)ov->n_major_t, GRI_NAME_LEN);
  hdr->fp->read((char*)&ov->d_major_t, GRI_SIZE_DBL);
  GRI_SWAPD(&ov->d_major_t, 1);

  /* -------- MINOR_T */

  hdr->fp->read((char*)ov->n_minor_t, GRI_NAME_LEN);
  hdr->fp->read((char*)&ov->d_minor_t, GRI_SIZE_DBL);
 
  GRI_SWAPD(&ov->d_minor_t, 1);

  /* -------- check for I/O error */

  if ((hdr->fp->fail()) || hdr->fp->eof())
  {
    return GRI_ERR_IOERR;
  }

  /* -------- get the conversion

     Note that this conversion factor applies to both the header values
     and the shift values, but in different ways.

     So far, the only data type we've encountered in any published
     NTv2 file is "SECONDS".
  */

  gri_cleanup_str(hdr, hdr->gs_type, ov->s_gs_type, FALSE);

  if (strncmp(hdr->gs_type, "SECONDS ", GRI_NAME_LEN) == 0)
  {
    hdr->hdr_conv = (1.0 / 3600.0);
    hdr->dat_conv = (1.0);
  }
  else if (strncmp(hdr->gs_type, "MINUTES ", GRI_NAME_LEN) == 0)
  {
    hdr->hdr_conv = (1.0 / 60.0);
    hdr->dat_conv = (60.0);
  }
  else if (strncmp(hdr->gs_type, "DEGREES ", GRI_NAME_LEN) == 0)
  {
    hdr->hdr_conv = (1.0);
    hdr->dat_conv = (3600.0);
  }
  else
  {
    return GRI_ERR_INVALID_GS_TYPE;
  }

  return GRI_ERR_OK;
}

/*------------------------------------------------------------------------
 * Read a binary subfile record.
 *
 * See above implementation comment.
 */
static int gri_read_sf_bin(
  GRI_HDR* hdr,
  GRI_FILE_SF* sf)
{
 

  /* -------- SUB_NAME */

  hdr->fp->read((char *)sf->n_sub_name, GRI_NAME_LEN);
  hdr->fp->read((char *)sf->s_sub_name, GRI_NAME_LEN);

  /* -------- PARENT */

  hdr->fp->read((char *)sf->n_parent, GRI_NAME_LEN);
  hdr->fp->read((char *)sf->s_parent, GRI_NAME_LEN);

  /* -------- CREATED */

  hdr->fp->read((char *)sf->n_created, GRI_NAME_LEN);
  hdr->fp->read((char *)sf->s_created, GRI_NAME_LEN);

  /* -------- UPDATED */

  hdr->fp->read((char *)sf->n_updated, GRI_NAME_LEN);
  hdr->fp->read((char *)sf->s_updated, GRI_NAME_LEN);

  /* -------- S_LAT */

  hdr->fp->read((char *)sf->n_s_lat, GRI_NAME_LEN);
  hdr->fp->read((char *)&sf->d_s_lat, GRI_SIZE_DBL);
  GRI_SWAPD(&sf->d_s_lat, 1);

  /* -------- N_LAT */

  hdr->fp->read((char *)sf->n_n_lat, GRI_NAME_LEN);
  hdr->fp->read((char*)&sf->d_n_lat, GRI_SIZE_DBL);
  GRI_SWAPD(&sf->d_n_lat, 1);

  /* -------- E_LONG */

  hdr->fp->read((char *)sf->n_e_lon, GRI_NAME_LEN);
  hdr->fp->read((char*)&sf->d_e_lon, GRI_SIZE_DBL);
 

  /* -------- W_LONG */

  hdr->fp->read((char *)sf->n_w_lon, GRI_NAME_LEN);
  hdr->fp->read((char*)(doubl_b), GRI_SIZE_DBL);
  sf->d_w_lon = conv_dbl(doubl_b);
  GRI_SWAPD(&sf->d_w_lon, 1);

  /* -------- LAT_INC */

  hdr->fp->read((char *)sf->n_lat_inc, GRI_NAME_LEN);
  hdr->fp->read((char*)(doubl_b), GRI_SIZE_DBL);
  sf->d_lat_inc= conv_dbl(doubl_b);
  GRI_SWAPD(&sf->d_lat_inc, 1);

  /* -------- LONG_INC */

  hdr->fp->read((char *)sf->n_lon_inc, GRI_NAME_LEN);
  hdr->fp->read((char*)(doubl_b), GRI_SIZE_DBL);
  sf->d_lon_inc=conv_dbl(doubl_b);
  GRI_SWAPD(&sf->d_lon_inc, 1);

  /* -------- GS_COUNT */

  hdr->fp->read((char *)sf->n_gs_count, GRI_NAME_LEN);
  hdr->fp->read((char*)(int_b), GRI_SIZE_INT);
  sf->i_gs_count = conv_int(int_b);
  sf->p_gs_count = 0;

  GRI_SWAPI(&sf->i_gs_count, 1);
  if (sf->i_gs_count <= 0)
    return GRI_ERR_INVALID_GS_COUNT;

  if (hdr->pads_present)
    hdr->fp->seekg(GRI_SIZE_INT, ios_base::cur);

  /* -------- check for I/O error */

  if (hdr->fp->fail() || hdr->fp->eof() )
  {
    return GRI_ERR_IOERR;
  }

  return GRI_ERR_OK;
}

/*------------------------------------------------------------------------
 * Read a binary end record.
 *
 * The only NTv2 file I've seen that would generate an error here
 * is the file "italy/rer.gsb", which omits the end record completely.
 * Maybe we should make them an offer they can't refuse...
 */
static int gri_read_er_bin(
  GRI_HDR* hdr,
  GRI_FILE_END* er)
{
  char dum[sizeof(GRI_FILE_END)];
  hdr->fp->read((char* )dum, sizeof(GRI_FILE_END));
  *er= *(GRI_FILE_END*)&(*dum);
  return (!hdr->fp->fail()) ? GRI_ERR_OK : GRI_ERR_IOERR;
}

/*------------------------------------------------------------------------
 * Convert a subfile record to a gri-rec.
 *
 * The NTv2 specification uses the convention of positive-west /
 * negative-east (probably since Canada is all in the west).
 * However, this is backwards from the standard negative-west /
 * positive-east notation.
 *
 * Thus, when converting the sub-file records, we flip the sign of
 * the longitude-max (the east value) and the longitude-min
 * (the west value).  This way, the longitude-min (west) is less than
 * the longitude-max (east).  This may have to be rethought if an NTv2
 * file ever spans the dateline (+/-180 degrees).
 *
 * However, we still have to remember that the grid-shift records
 * are written (backwards) east-to-west, and the longitude shift values
 * are positive-west/negative-east.
 */
static int gri_sf_to_rec(
  GRI_HDR* hdr,
  GRI_FILE_SF* sf,
  int           n)
{
  GRI_REC* rec = hdr->recs + n;

  gri_cleanup_str(hdr, rec->record_name, sf->s_sub_name, TRUE);
  gri_cleanup_str(hdr, rec->parent_name, sf->s_parent, TRUE);
  rec->record_name[GRI_NAME_LEN] = 0;
  rec->parent_name[GRI_NAME_LEN] = 0;

  rec->active = TRUE;
  rec->parent = GRI_NULL;
  rec->sub = GRI_NULL;
  rec->next = GRI_NULL;
  rec->num_subs = 0;
  rec->rec_num = n;

  rec->offset = hdr->fp->tellg();
  rec->sskip = 0;
  rec->nskip = 0;
  rec->wskip = 0;
  rec->eskip = 0;

  rec->shifts = GRI_NULL;
  rec->accurs = GRI_NULL;

  rec->lat_min = sf->d_s_lat;
  rec->lat_min *= hdr->hdr_conv;

  rec->lat_max = sf->d_n_lat;
  rec->lat_max *= hdr->hdr_conv;

  rec->lat_inc = sf->d_lat_inc;
  rec->lat_inc *= hdr->hdr_conv;

  rec->lon_max = sf->d_e_lon;
  rec->lon_max *= -hdr->hdr_conv;      /* NOTE: reversing sign! */

  rec->lon_min = sf->d_w_lon;
  rec->lon_min *= -hdr->hdr_conv;      /* NOTE: reversing sign! */

  rec->lon_inc = sf->d_lon_inc;
  rec->lon_inc *= hdr->hdr_conv;

  rec->num = sf->i_gs_count;

  rec->nrows = gri_dtoi((rec->lat_max - rec->lat_min) / rec->lat_inc) + 1;
  rec->ncols = gri_dtoi((rec->lon_max - rec->lon_min) / rec->lon_inc) + 1;

  /* collect the min/max of all the sub-files */

  if (hdr->lon_min > rec->lon_min)  hdr->lon_min = rec->lon_min;
  if (hdr->lat_min > rec->lat_min)  hdr->lat_min = rec->lat_min;
  if (hdr->lon_max < rec->lon_max)  hdr->lon_max = rec->lon_max;
  if (hdr->lat_max < rec->lat_max)  hdr->lat_max = rec->lat_max;

  return GRI_ERR_OK;
}

/*------------------------------------------------------------------------
 * Read binary header data from the file.
 */
static int gri_read_hdrs_bin(
  GRI_HDR* hdr)
{
  GRI_FILE_OV  ov_rec;
  GRI_FILE_SF  sf_rec;
  GRI_FILE_END en_rec;
  int i;
  int rc;

  /* -------- read in the overview record */

  rc = gri_read_ov_bin(hdr, &ov_rec);
  if (rc != GRI_ERR_OK)
  {
    return rc;
  }

  hdr->num_parents = 0;
  hdr->recs = (GRI_REC*)
    new GRI_REC[sizeof(*hdr->recs) * hdr->num_recs];
  if (hdr->recs == GRI_NULL)
  {
    return GRI_ERR_NO_MEMORY;
  }

  memset(hdr->recs, 0, sizeof(*hdr->recs) * hdr->num_recs);

  /* -------- create the file record cache if wanted */

  if (hdr->keep_orig)
  {
    hdr->overview = new GRI_FILE_OV [(sizeof(*hdr->overview))];
    if (hdr->overview == GRI_NULL)
    {
      return GRI_ERR_NO_MEMORY;
    }

    memcpy(hdr->overview, &ov_rec, sizeof(*hdr->overview));

    hdr->subfiles = 
      new GRI_FILE_SF[sizeof(*hdr->subfiles) * hdr->num_recs];
    if (hdr->subfiles == GRI_NULL)
    {
      return GRI_ERR_NO_MEMORY;
    }
  }

  /* -------- read in all subfile records (but not the actual data)

     We don't read in the data now, since we may be doing
     extent processing later, which would affect the reading
     of the data.
  */

  for (i = 0; i < hdr->num_recs; i++)
  {
    long offset;

    rc = gri_read_sf_bin(hdr, &sf_rec);
    if (rc != GRI_ERR_OK)
    {
      return rc;
    }

    if (hdr->subfiles != GRI_NULL)
    {
      memcpy(&hdr->subfiles[i], &sf_rec, sizeof(*hdr->subfiles));
    }

    rc = gri_sf_to_rec(hdr, &sf_rec, i);
    if (rc != GRI_ERR_OK)
    {
      return rc;
    }

    /* skip over grid-shift records */
    offset = hdr->recs[i].num * sizeof(GRI_FILE_GS);
    hdr->fp->seekg( offset,ios_base::cur);
  }

  /* -------- read in the end record

     Note that we don't bail if we can't read the end record,
     since it is certainly possible to use the file without it because
     we know the number of records and thus where the end should be.
     But we do note that the file "needs fixing" if it isn't there,
     or if it isn't in "standard" format.

     Interestingly enough, all the Canadian NTv2 files properly
     zero out the filler bytes in the end record, but the name
     field is "END xxxx" (ie "END " and four trailing garbage bytes).
     And it's their specification!
  */

  rc = gri_read_er_bin(hdr, &en_rec);
  if (rc == GRI_ERR_OK)
  {
    if (strncmp(en_rec.n_end, GRI_END_NAME, GRI_NAME_LEN) != 0)
      hdr->fixed |= GRI_FIX_END_REC_NAME_NOT_ALPHA;

    if (memcmp(en_rec.filler, GRI_ALL_ZEROS, GRI_NAME_LEN) != 0)
      hdr->fixed |= GRI_FIX_END_REC_PAD_NOT_ZERO;
  }
  else
  {
    hdr->fixed |= GRI_FIX_END_REC_NOT_FOUND;
  }

  /* -------- adjust all pointers */

  rc = gri_fix_ptrs(hdr);
  return rc;
}

/*------------------------------------------------------------------------
 * Mark a sub-file and all its children as inactive.
 *
 * This is called if a sub-file is to be skipped due to extent processing.
 */
static int gri_inactivate(
  GRI_HDR* hdr,
  GRI_REC* rec,
  int       nrecs)
{
  if (rec->active)
  {
    GRI_REC* sub;

    nrecs--;
    rec->active = FALSE;

    for (sub = rec->sub; sub != GRI_NULL; sub = sub->next)
    {
      nrecs = gri_inactivate(hdr, sub, nrecs);
    }
  }

  return nrecs;
}

/*------------------------------------------------------------------------
 * Apply an extent to an NTv2 object.
 */
static int gri_process_extent(
  GRI_HDR* hdr,
  GRI_EXTENT* extent)
{
  double wlon;
  double slat;
  double elon;
  double nlat;
  int    nrecs;
  int    i;
  int    rc = GRI_ERR_OK;

  if (gri_extent_is_empty(extent))
  {
    return GRI_ERR_OK;
  }

  nrecs = hdr->num_recs;

  /* get extent values */

  wlon = extent->wlon;
  slat = extent->slat;
  elon = extent->elon;
  nlat = extent->nlat;

  /* apply the extent to all records */

  for (i = 0; i < hdr->num_recs; i++)
  {
    GRI_REC* rec = &hdr->recs[i];
    double d;
    int    nskip = 0;
    int    sskip = 0;
    int    wskip = 0;
    int    eskip = 0;
    int    ocols = rec->ncols;
    int    k;

    /* check if this record has already been marked as out-of-range */

    if (!rec->active)
      continue;

    /* check if this record is out-of-range */

    if (GRI_GE(wlon, rec->lon_max) ||
      GRI_GE(slat, rec->lat_max) ||
      GRI_LE(elon, rec->lon_min) ||
      GRI_LE(nlat, rec->lat_min))
    {
      nrecs = gri_inactivate(hdr, rec, nrecs);
      continue;
    }

    /* check if this extent is a subset of the record */

    if (GRI_GT(wlon, rec->lon_min) ||
      GRI_GT(slat, rec->lat_min) ||
      GRI_LT(elon, rec->lon_max) ||
      GRI_LT(nlat, rec->lat_max))
    {
      /* Force our extent to be a proper multiple of the deltas.
         We move the extent edges out to the next delta
         multiple to do this.

         Note that the deltas we use are the parent's deltas if
         we have a parent.  This is to make sure we stay lined
         up on a parent grid line.
      */
      double lat_inc = rec->lat_inc;
      double lon_inc = rec->lon_inc;
      int    lat_mul = 1;
      int    lon_mul = 1;

      if (rec->parent != GRI_NULL)
      {
        lat_inc = rec->parent->lat_inc;
        lon_inc = rec->parent->lon_inc;
        lat_mul = gri_dtoi(lat_inc / rec->lat_inc);
        lon_mul = gri_dtoi(lon_inc / rec->lon_inc);
      }

      if (GRI_GT(wlon, rec->lon_min))
      {
        d = (wlon - rec->lon_min) / lon_inc;
        k = (int)floor(d) * lon_mul;

        if (k > 0)
        {
          wskip = k;
          rec->lon_min += (k * rec->lon_inc);
          rec->ncols -= k;
        }
      }

      if (GRI_LT(elon, rec->lon_max))
      {
        d = (rec->lon_max - elon) / lon_inc;
        k = (int)floor(d) * lon_mul;

        if (k > 0)
        {
          eskip = k;
          rec->lon_max -= (k * rec->lon_inc);
          rec->ncols -= k;
        }
      }

      if (GRI_GT(slat, rec->lat_min))
      {
        d = (slat - rec->lat_min) / lat_inc;
        k = (int)floor(d) * lat_mul;

        if (k > 0)
        {
          sskip = k;
          rec->lat_min += (k * rec->lat_inc);
          rec->nrows -= k;
        }
      }

      if (GRI_LT(nlat, rec->lat_max))
      {
        d = (rec->lat_max - nlat) / lat_inc;
        k = (int)floor(d) * lat_mul;

        if (k > 0)
        {
          nskip = k;
          rec->lat_max -= (k * rec->lat_inc);
          rec->nrows -= k;
        }
      }

      /* Check if there is any data left.
         I think that this will always be the case, but it is
         easier to do the check than to think out all
         possible scenarios.  Also, better safe than sorry.
      */

      if (rec->ncols <= 0 || rec->nrows <= 0)
      {
        nrecs = gri_inactivate(hdr, rec, nrecs);
        continue;
      }

      /* check if this record has changed */

      if (nskip > 0 || sskip > 0 || wskip > 0 || eskip > 0)
      {
        rec->num = (rec->ncols * rec->nrows);
        rec->sskip = int((sskip * sizeof(GRI_FILE_GS)) * ocols);
        rec->nskip = int((nskip * sizeof(GRI_FILE_GS)) * ocols);
        rec->wskip = int((wskip * sizeof(GRI_FILE_GS)));
        rec->eskip = int((eskip * sizeof(GRI_FILE_GS)));

        /* adjust the subfile record if we cached it */
        if (hdr->subfiles != GRI_NULL)
        {
          GRI_FILE_SF* sf = &hdr->subfiles[i];

          sf->i_gs_count = rec->num;

          sf->d_s_lat = (rec->lat_min / hdr->hdr_conv);
          sf->d_n_lat = (rec->lat_max / hdr->hdr_conv);

          /* Note the sign changes here! */

          sf->d_e_lon = -(rec->lon_max / hdr->hdr_conv);
          sf->d_w_lon = -(rec->lon_min / hdr->hdr_conv);
        }
      }
    }
  }

  /* check if any records have been deleted */

  if (nrecs != hdr->num_recs)
  {
    /* error if no records are left */
    if (nrecs == 0)
      return GRI_ERR_INVALID_EXTENT;

    /* fix number of files in overview record if present */
    if (hdr->overview != GRI_NULL)
      hdr->overview->i_num_file = nrecs;

    /* readjust all pointers */
    rc = gri_fix_ptrs(hdr);
  }

  return rc;
}

/*------------------------------------------------------------------------
 * Read in binary shift data for all sub-files.
 */
static int gri_read_data_bin(
  GRI_HDR* hdr)
{
  int i;

  for (i = 0; i < hdr->num_recs; i++)
  {
    GRI_REC* rec = &hdr->recs[i];
    int row;
    int col;
    int j;

    if (!rec->active)
      continue;

    /* allocate our arrays */

    {
      rec->shifts =  new GRI_SHIFT[(sizeof(*rec->shifts) * rec->num)];
      if (rec->shifts == GRI_NULL)
        return GRI_ERR_NO_MEMORY;
    }

    if (hdr->keep_orig)
    {
      rec->accurs = new GRI_SHIFT [(sizeof(*rec->accurs) * rec->num)];
      if (rec->accurs == GRI_NULL)
        return GRI_ERR_NO_MEMORY;
    }

    /* position to start of data to read */
    hdr->fp->seekg((rec->offset + rec->sskip), hdr->fp->beg);
    if (hdr->fp->fail())
    {
      hdr->fp->close();
      hdr->fp->open(hdr->path, ios_base::in | ios_base::binary);
      hdr->fp->seekg((rec->offset + rec->sskip), hdr->fp->beg);
    }

  
   
   
 

    /* now read the data */
    /* Remember that data in a latitude row goes East to West! */

    j = 0;
    for (row = 0; row < rec->nrows; row++)
    {
      if (rec->eskip > 0)
        hdr->fp->seekg(rec->sskip, ios_base::cur);
      for (col = 0; col < rec->ncols; col++)
      {
        GRI_FILE_GS gs;
        hdr->fp->read(((char *)&gs), sizeof(gs));
        GRI_SWAPF(&gs.f_lat_shift, 1);
        GRI_SWAPF(&gs.f_lon_shift, 1);

        rec->shifts[j][GRI_COORD_LAT] = gs.f_lat_shift;
        rec->shifts[j][GRI_COORD_LON] = gs.f_lon_shift;

        if (rec->accurs != GRI_NULL)
        {
          GRI_SWAPF(&gs.f_lat_accuracy, 1);
          GRI_SWAPF(&gs.f_lon_accuracy, 1);

          rec->accurs[j][GRI_COORD_LAT] = gs.f_lat_accuracy;
          rec->accurs[j][GRI_COORD_LON] = gs.f_lon_accuracy;
        }

        j++;
      }

      if (rec->wskip > 0)
       hdr->fp->seekg( rec->wskip, ios_base::cur);
    }
  }

  return GRI_ERR_OK;
}

/*------------------------------------------------------------------------
 * Load a binary NTv2 file into memory.
 *
 * This routine combines all the lower-level read routines into one.
 */
static GRI_HDR* gri_load_file_bin(
  const char* grifile,
  GRI_BOOL     keep_orig,
  GRI_BOOL     read_data,
  GRI_EXTENT* extent,
  int* prc)
{
  GRI_HDR* hdr = GRI_NULL;
  int rc = GRI_ERR_OK;

  hdr = gri_create(grifile, GRI_FILE_TYPE_BIN, prc);
  if (hdr == GRI_NULL)
  {
    return GRI_NULL;
  }

  hdr->keep_orig = keep_orig;

  if (rc == GRI_ERR_OK)
  {
    rc = gri_read_hdrs_bin(hdr);
  }

  if (rc == GRI_ERR_OK && extent != GRI_NULL)
  {
    rc = gri_process_extent(hdr, extent);
  }

  if (rc != GRI_ERR_OK)
  {
    hdr->fp->close();
 
  }
  else
  {
    if (read_data)
    {
      rc = gri_read_data_bin(hdr);

      /* done with the file whether successful or not */
      hdr->fp->close();
    
    }
    else
    {
      if (hdr->file_type == GRI_FILE_TYPE_BIN)
      {
        hdr->mutex = (void*)gri_mutex_create();
      }
      else
      {
        /* No reading on-the-fly if it's an ascii file. */
        hdr->fp->close();
    
      }
    }

  }

  *prc = rc;
  return hdr;
}

/* ------------------------------------------------------------------------- */
/* NTv2 ascii read routines                                                  */
/* ------------------------------------------------------------------------- */

#define RT(n)    if ( gri_read_toks(hdr->fp, &tok, n) <= 0 ) \
                    return GRI_ERR_UNEXPECTED_EOF

#define TOK(i)   tok.toks[i]

/*------------------------------------------------------------------------
 * Read a ascii overview record.
 */
static int gri_read_ov_asc(
  GRI_HDR* hdr,
  GRI_FILE_OV* ov)
{
  GRI_TOKEN tok;

  /* -------- NUM_OREC */

  RT(2);
  gri_strcpy_pad(ov->n_num_orec, TOK(0));
  ov->i_num_orec = atoi(TOK(1));
  ov->p_num_orec = 0;

  if (ov->i_num_orec != 11)
    return GRI_ERR_INVALID_NUM_OREC;

  /* -------- NUM_SREC */

  RT(2);
  gri_strcpy_pad(ov->n_num_srec, TOK(0));
  ov->i_num_srec = atoi(TOK(1));
  ov->p_num_srec = 0;

  if (ov->i_num_srec != 11)
    return GRI_ERR_INVALID_NUM_SREC;

  /* -------- NUM_FILE */

  RT(2);
  gri_strcpy_pad(ov->n_num_file, TOK(0));
  ov->i_num_file = atoi(TOK(1));
  ov->p_num_file = 0;

  if (ov->i_num_file <= 0)
    return GRI_ERR_INVALID_NUM_FILE;
  hdr->num_recs = ov->i_num_file;

  /* -------- GS_TYPE */

  RT(2);
  gri_strcpy_pad(ov->n_gs_type, TOK(0));
  gri_strcpy_pad(ov->s_gs_type, TOK(1));

  /* -------- VERSION */

  RT(2);
  gri_strcpy_pad(ov->n_version, TOK(0));
  gri_strcpy_pad(ov->s_version, TOK(1));

  /* -------- SYSTEM_F */

  RT(2);
  gri_strcpy_pad(ov->n_system_f, TOK(0));
  gri_strcpy_pad(ov->s_system_f, TOK(1));

  /* -------- SYSTEM_T */

  RT(2);
  gri_strcpy_pad(ov->n_system_t, TOK(0));
  gri_strcpy_pad(ov->s_system_t, TOK(1));

  /* -------- MAJOR_F */

  RT(2);
  gri_strcpy_pad(ov->n_major_f, TOK(0));
  ov->d_major_f = gri_atod(TOK(1));

  /* -------- MINOR_F */

  RT(2);
  gri_strcpy_pad(ov->n_minor_f, TOK(0));
  ov->d_minor_f = gri_atod(TOK(1));

  /* -------- MAJOR_T */

  RT(2);
  gri_strcpy_pad(ov->n_major_t, TOK(0));
  ov->d_major_t = gri_atod(TOK(1));

  /* -------- MINOR_T */

  RT(2);
  gri_strcpy_pad(ov->n_minor_t, TOK(0));
  ov->d_minor_t = gri_atod(TOK(1));

  /* -------- get the conversion */

  gri_cleanup_str(hdr, hdr->gs_type, ov->s_gs_type, FALSE);

  if (strncmp(hdr->gs_type, "SECONDS ", GRI_NAME_LEN) == 0)
  {
    hdr->hdr_conv = (1.0 / 3600.0);
    hdr->dat_conv = (1.0);
  }
  else if (strncmp(hdr->gs_type, "MINUTES ", GRI_NAME_LEN) == 0)
  {
    hdr->hdr_conv = (1.0 / 60.0);
    hdr->dat_conv = (60.0);
  }
  else if (strncmp(hdr->gs_type, "DEGREES ", GRI_NAME_LEN) == 0)
  {
    hdr->hdr_conv = (1.0);
    hdr->dat_conv = (3600.0);
  }
  else
  {
    return GRI_ERR_INVALID_GS_TYPE;
  }

  return GRI_ERR_OK;
}

/*------------------------------------------------------------------------
 * Read a ascii subfile record.
 */
static int gri_read_sf_asc(
  GRI_HDR* hdr,
  GRI_FILE_SF* sf)
{
  GRI_TOKEN tok;

  /* -------- SUB_NAME */

  RT(2);
  gri_strcpy_pad(sf->n_sub_name, TOK(0));
  gri_strcpy_pad(sf->s_sub_name, TOK(1));

  /* -------- PARENT */

  RT(2);
  gri_strcpy_pad(sf->n_parent, TOK(0));
  gri_strcpy_pad(sf->s_parent, TOK(1));

  /* -------- CREATED */

  RT(2);
  gri_strcpy_pad(sf->n_created, TOK(0));
  gri_strcpy_pad(sf->s_created, TOK(1));

  /* -------- UPDATED */

  RT(2);
  gri_strcpy_pad(sf->n_updated, TOK(0));
  gri_strcpy_pad(sf->s_updated, TOK(1));

  /* -------- S_LAT */

  RT(2);
  gri_strcpy_pad(sf->n_s_lat, TOK(0));
  sf->d_s_lat = gri_atod(TOK(1));

  /* -------- N_LAT */

  RT(2);
  gri_strcpy_pad(sf->n_n_lat, TOK(0));
  sf->d_n_lat = gri_atod(TOK(1));

  /* -------- E_LONG */

  RT(2);
  gri_strcpy_pad(sf->n_e_lon, TOK(0));
  sf->d_e_lon = gri_atod(TOK(1));

  /* -------- W_LONG */

  RT(2);
  gri_strcpy_pad(sf->n_w_lon, TOK(0));
  sf->d_w_lon = gri_atod(TOK(1));

  /* -------- LAT_INC */

  RT(2);
  gri_strcpy_pad(sf->n_lat_inc, TOK(0));
  sf->d_lat_inc = gri_atod(TOK(1));

  /* -------- LONG_INC */

  RT(2);
  gri_strcpy_pad(sf->n_lon_inc, TOK(0));
  sf->d_lon_inc = gri_atod(TOK(1));

  /* -------- GS_COUNT */

  RT(2);
  gri_strcpy_pad(sf->n_gs_count, TOK(0));
  sf->i_gs_count = atoi(TOK(1));
  sf->p_gs_count = 0;

  return GRI_ERR_OK;
}

/*------------------------------------------------------------------------
 * Read in ascii shift data
 *
 * Note that it is OK if only shift data is on a line (i.e. no
 * accuracy data).  In that case, the accuracies are set to 0.
 */
static int gri_read_data_asc(
  GRI_HDR* hdr,
  GRI_REC* rec,
  GRI_BOOL read_data)
{
  GRI_TOKEN tok;
  int i;

  /* allocate our arrays */

  if (read_data)
  {
    {
      rec->shifts = new GRI_SHIFT[sizeof(*rec->shifts) * rec->num];
      if (rec->shifts == GRI_NULL)
        return GRI_ERR_NO_MEMORY;
    }

    if (hdr->keep_orig)
    {
      rec->accurs = new GRI_SHIFT[(sizeof(*rec->accurs) * rec->num)];
      if (rec->accurs == GRI_NULL)
        return GRI_ERR_NO_MEMORY;
    }
  }

  /* now read the data */

  for (i = 0; i < rec->num; i++)
  {
    RT(4);

    if (tok.num == 2 || tok.num == 4)
    {
      if (read_data)
      {
        rec->shifts[i][GRI_COORD_LAT] = (float)gri_atod(TOK(0));
        rec->shifts[i][GRI_COORD_LON] = (float)gri_atod(TOK(1));

        if (rec->accurs != GRI_NULL)
        {
          /* Note that if there are only 2 tokens in the line,
             these token pointers will point to empty strings.
          */
          rec->accurs[i][GRI_COORD_LAT] = (float)gri_atod(TOK(2));
          rec->accurs[i][GRI_COORD_LON] = (float)gri_atod(TOK(3));
        }
      }
    }
    else
    {
      return GRI_ERR_INVALID_LINE;
    }
  }

  return GRI_ERR_OK;
}

/*------------------------------------------------------------------------
 * Read a ascii end record.
 */
static int gri_read_er_asc(
  GRI_HDR* hdr)
{
  GRI_TOKEN tok;
  int num;
  int rc = 0;

  num = gri_read_toks(hdr->fp, &tok, 1);
  if (num <= 0)
  {
    hdr->fixed |= GRI_FIX_END_REC_NOT_FOUND;
  }
  else
  {
    if (gri_strcmp_i(tok.toks[0], "END") != 0)
      hdr->fixed |= GRI_FIX_END_REC_NOT_FOUND;
  }

  return rc;
}

/*------------------------------------------------------------------------
 * Load a NTv2 ascii file into memory.
 */
static GRI_HDR* gri_load_file_asc(
  const char* grifile,
  GRI_BOOL     keep_orig,
  GRI_BOOL     read_data,
  GRI_EXTENT* extent,
  int* prc)
{
  GRI_HDR* hdr;
  GRI_FILE_OV  ov_rec;
  GRI_FILE_SF  sf_rec;
  int i;
  int rc;

  GRI_UNUSED_PARAMETER(extent);

  hdr = gri_create(grifile, GRI_FILE_TYPE_ASC, prc);
  if (hdr == GRI_NULL)
  {
    return GRI_NULL;
  }

  hdr->keep_orig = keep_orig;

  /* -------- read in the overview record */

  rc = gri_read_ov_asc(hdr, &ov_rec);
  if (rc != GRI_ERR_OK)
  {
    gri_delete(hdr);
    *prc = rc;
    return GRI_NULL;
  }

  hdr->num_parents = 0;
  hdr->recs = new GRI_REC [(sizeof(*hdr->recs) * hdr->num_recs)];
  if (hdr->recs == GRI_NULL)
  {
    gri_delete(hdr);
    *prc = GRI_ERR_NO_MEMORY;
    return GRI_NULL;
  }

  memset(hdr->recs, 0, sizeof(*hdr->recs) * hdr->num_recs);

  /* -------- create the file record cache if wanted */

  if (hdr->keep_orig)
  {
    hdr->overview = 
      new GRI_FILE_OV[sizeof(*hdr->overview)];
    if (hdr->overview == GRI_NULL)
    {
      gri_delete(hdr);
      *prc = GRI_ERR_NO_MEMORY;
      return GRI_NULL;
    }

    memcpy(hdr->overview, &ov_rec, sizeof(*hdr->overview));

    hdr->subfiles = 
      new GRI_FILE_SF[sizeof(*hdr->subfiles) * hdr->num_recs];
    if (hdr->subfiles == GRI_NULL)
    {
      gri_delete(hdr);
      *prc = GRI_ERR_NO_MEMORY;
      return GRI_NULL;
    }
  }

  /* -------- read in all subfile records */

  for (i = 0; i < hdr->num_recs; i++)
  {
    rc = gri_read_sf_asc(hdr, &sf_rec);
    if (rc == GRI_ERR_OK)
    {
      if (hdr->subfiles != GRI_NULL)
        memcpy(&hdr->subfiles[i], &sf_rec, sizeof(*hdr->subfiles));

      rc = gri_sf_to_rec(hdr, &sf_rec, i);
      if (rc == GRI_ERR_OK)
        rc = gri_read_data_asc(hdr, hdr->recs + i, read_data);
    }

    if (rc != GRI_ERR_OK)
    {
      gri_delete(hdr);
      *prc = rc;
      return GRI_NULL;
    }
  }

  /* -------- read in the end record */

  gri_read_er_asc(hdr);

  /* -------- adjust all pointers */

  rc = gri_fix_ptrs(hdr);
  if (rc != GRI_ERR_OK)
  {
    gri_delete(hdr);
    hdr = GRI_NULL;
  }

  *prc = rc;
  return hdr;
}

/* ------------------------------------------------------------------------- */
/* NTv2 read routines                                                        */
/* ------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Load a NTv2 file into memory.
 */
GRI_HDR* gri_load_file(
  const char* grifile,
  GRI_BOOL     keep_orig,
  GRI_BOOL     read_data,
  GRI_EXTENT* extent,
  int* prc)
{
  GRI_HDR* hdr;
  int        type;
  int        rc;

  if (prc == GRI_NULL)
    prc = &rc;
  *prc = GRI_ERR_OK;

  if (grifile == GRI_NULL || *grifile == 0)
  {
    *prc = GRI_ERR_NULL_PATH;
    return GRI_NULL;
  }

  type = gri_filetype(grifile);
  switch (type)
  {
  case GRI_FILE_TYPE_ASC:
    hdr = gri_load_file_asc(grifile,
      keep_orig,
      read_data,
      extent,
      prc);
    break;

  case GRI_FILE_TYPE_BIN:
    hdr = gri_load_file_bin(grifile,
      keep_orig,
      read_data,
      extent,
      prc);
    break;

  default:
    hdr = GRI_NULL;
    *prc = GRI_ERR_UNKNOWN_FILE_TYPE;
    break;
  }

  return hdr;
}

/* ------------------------------------------------------------------------- */
/* NTv2 binary write routines                                                */
/* ------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Write a binary overview record
 */
static void gri_write_ov_bin(
  FILE* fp,
  GRI_HDR* hdr,
  GRI_BOOL swap_data)
{
  GRI_FILE_OV   ov;
  GRI_FILE_OV* pov;

  gri_validate_ov(hdr, GRI_NULL, 0);

  pov = hdr->overview;
  if (swap_data)
  {
    memcpy(&ov, pov, sizeof(ov));
    gri_swap_int(&ov.i_num_orec, 1);
    gri_swap_int(&ov.i_num_srec, 1);
    gri_swap_int(&ov.i_num_file, 1);
    gri_swap_dbl(&ov.d_major_f, 1);
    gri_swap_dbl(&ov.d_minor_f, 1);
    gri_swap_dbl(&ov.d_major_t, 1);
    gri_swap_dbl(&ov.d_minor_t, 1);
    pov = &ov;
  }

  fwrite(pov, sizeof(*pov), 1, fp);
}

/*------------------------------------------------------------------------
 * Write a binary sub-file record and its children.
 */
static void gri_write_sf_bin(
  FILE* fp,
  GRI_HDR* hdr,
  GRI_REC* rec,
  GRI_BOOL swap_data)
{
  GRI_REC* sub;
  int i;

  /* this shouldn't happen */
  if (!rec->active)
    return;

  gri_validate_sf(hdr, GRI_NULL, rec->rec_num, 0);

  /* write the sub-file header */
  {
    GRI_FILE_SF   sf;
    GRI_FILE_SF* psf;

    psf = &hdr->subfiles[rec->rec_num];
    if (swap_data)
    {
      memcpy(&sf, psf, sizeof(sf));
      gri_swap_dbl(&sf.d_s_lat, 1);
      gri_swap_dbl(&sf.d_n_lat, 1);
      gri_swap_dbl(&sf.d_e_lon, 1);
      gri_swap_dbl(&sf.d_w_lon, 1);
      gri_swap_dbl(&sf.d_lat_inc, 1);
      gri_swap_dbl(&sf.d_lon_inc, 1);
      gri_swap_int(&sf.i_gs_count, 1);
      psf = &sf;
    }
    fwrite(psf, sizeof(*psf), 1, fp);
  }

  /* write all grid-shift records */
  for (i = 0; i < rec->num; i++)
  {
    GRI_FILE_GS gs;

    gs.f_lat_shift = rec->shifts[i][GRI_COORD_LAT];
    gs.f_lon_shift = rec->shifts[i][GRI_COORD_LON];

    if (rec->accurs != GRI_NULL)
    {
      gs.f_lat_accuracy = rec->accurs[i][GRI_COORD_LAT];
      gs.f_lon_accuracy = rec->accurs[i][GRI_COORD_LON];
    }
    else
    {
      gs.f_lat_accuracy = 0.0;
      gs.f_lon_accuracy = 0.0;
    }

    if (swap_data)
    {
      gri_swap_flt((float*)&gs, 4);
    }

    fwrite(&gs, sizeof(gs), 1, fp);
  }

  /* recursively write all its children */
  for (sub = rec->sub; sub != GRI_NULL; sub = sub->next)
  {
    gri_write_sf_bin(fp, hdr, sub, swap_data);
  }
}

/*------------------------------------------------------------------------
 * Write a binary end record
 */
static void gri_write_end_bin(
  FILE* fp)
{
  GRI_FILE_END end_rec;

  memcpy(end_rec.n_end, GRI_END_NAME, GRI_NAME_LEN);
  memcpy(end_rec.filler, GRI_ALL_ZEROS, GRI_NAME_LEN);
  fwrite(&end_rec, sizeof(end_rec), 1, fp);
}

/*------------------------------------------------------------------------
 * Write out the GRI_HDR object to a binary file.
 */
static int gri_write_file_bin(
  GRI_HDR* hdr,
  const char* path,
  int         byte_order)
{
  FILE* fp;
  GRI_BOOL swap_data;
  int       rc;

  swap_data = hdr->swap_data;
  switch (byte_order)
  {
  case GRI_ENDIAN_BIG:    swap_data ^= !gri_is_big_endian(); break;
  case GRI_ENDIAN_LITTLE: swap_data ^= !gri_is_ltl_endian(); break;
  case GRI_ENDIAN_NATIVE: swap_data = FALSE;                 break;
  }

  fp = fopen(path, "wb");
  if (fp == GRI_NULL)
  {
    rc = GRI_ERR_CANNOT_OPEN_FILE;
  }
  else
  {
    GRI_REC* sf;

    gri_write_ov_bin(fp, hdr, swap_data);

    for (sf = hdr->first_parent; sf != GRI_NULL; sf = sf->next)
      gri_write_sf_bin(fp, hdr, sf, swap_data);

    gri_write_end_bin(fp);

    rc = (ferror(fp) == 0) ? GRI_ERR_OK : GRI_ERR_IOERR;
    fclose(fp);
  }

  return rc;
}

/* ------------------------------------------------------------------------- */
/* NTv2 ascii write routines                                                 */
/* ------------------------------------------------------------------------- */

#define T(x)   gri_strip( strncpy(stemp, x, GRI_NAME_LEN) )
#define D(x)   gri_dtoa (         ntemp, x)

/*------------------------------------------------------------------------
 * Write a ascii overview record
 */
static void gri_write_ov_asc(
  FILE* fp,
  GRI_HDR* hdr)
{
  GRI_FILE_OV* pov = hdr->overview;
  char stemp[GRI_NAME_LEN + 1];
  char ntemp[32];

  gri_validate_ov(hdr, GRI_NULL, 0);
  stemp[GRI_NAME_LEN] = 0;

  fprintf(fp, "NUM_OREC %d\n", pov->i_num_orec);
  fprintf(fp, "NUM_SREC %d\n", pov->i_num_srec);
  fprintf(fp, "NUM_FILE %d\n", pov->i_num_file);
  fprintf(fp, "GS_TYPE  %s\n", T(pov->s_gs_type));
  fprintf(fp, "VERSION  %s\n", T(pov->s_version));
  fprintf(fp, "SYSTEM_F %s\n", T(pov->s_system_f));
  fprintf(fp, "SYSTEM_T %s\n", T(pov->s_system_t));
  fprintf(fp, "MAJOR_F  %s\n", D(pov->d_major_f));
  fprintf(fp, "MINOR_F  %s\n", D(pov->d_minor_f));
  fprintf(fp, "MAJOR_T  %s\n", D(pov->d_major_t));
  fprintf(fp, "MINOR_T  %s\n", D(pov->d_minor_t));
}

/*------------------------------------------------------------------------
 * Write a ascii sub-file record and its children.
 */
static void gri_write_sf_asc(
  FILE* fp,
  GRI_HDR* hdr,
  GRI_REC* rec)
{
  GRI_REC* sub;
  char stemp[GRI_NAME_LEN + 1];
  char ntemp[32];
  int i;

  /* this shouldn't happen */
  if (!rec->active)
    return;

  gri_validate_sf(hdr, GRI_NULL, rec->rec_num, 0);
  stemp[GRI_NAME_LEN] = 0;

  /* write the sub-file header */
  {
    GRI_FILE_SF* psf = &hdr->subfiles[rec->rec_num];

    fprintf(fp, "\n");
    fprintf(fp, "SUB_NAME %s\n", T(psf->s_sub_name));
    fprintf(fp, "PARENT   %s\n", T(psf->s_parent));
    fprintf(fp, "CREATED  %s\n", T(psf->s_created));
    fprintf(fp, "UPDATED  %s\n", T(psf->s_updated));
    fprintf(fp, "S_LAT    %s\n", D(psf->d_s_lat));
    fprintf(fp, "N_LAT    %s\n", D(psf->d_n_lat));
    fprintf(fp, "E_LONG   %s\n", D(psf->d_e_lon));
    fprintf(fp, "W_LONG   %s\n", D(psf->d_w_lon));
    fprintf(fp, "LAT_INC  %s\n", D(psf->d_lat_inc));
    fprintf(fp, "LONG_INC %s\n", D(psf->d_lon_inc));
    fprintf(fp, "GS_COUNT %d\n", psf->i_gs_count);
    fprintf(fp, "\n");
  }

  /* write all grid-shift records */
  for (i = 0; i < rec->num; i++)
  {
    GRI_FILE_GS gs;

    gs.f_lat_shift = rec->shifts[i][GRI_COORD_LAT];
    gs.f_lon_shift = rec->shifts[i][GRI_COORD_LON];

    if (rec->accurs != GRI_NULL)
    {
      gs.f_lat_accuracy = rec->accurs[i][GRI_COORD_LAT];
      gs.f_lon_accuracy = rec->accurs[i][GRI_COORD_LON];
    }
    else
    {
      gs.f_lat_accuracy = 0.0;
      gs.f_lon_accuracy = 0.0;
    }

    fprintf(fp, "%-16s", D(gs.f_lat_shift));
    fprintf(fp, "%-16s", D(gs.f_lon_shift));
    fprintf(fp, "%-16s", D(gs.f_lat_accuracy));
    fprintf(fp, "%s\n", D(gs.f_lon_accuracy));
  }

  /* recursively write all its children */
  for (sub = rec->sub; sub != GRI_NULL; sub = sub->next)
  {
    gri_write_sf_asc(fp, hdr, sub);
  }
}

/*------------------------------------------------------------------------
 * Write a ascii end record
 */
static void gri_write_end_asc(
  FILE* fp)
{
  fprintf(fp, "\n");
  fprintf(fp, "END\n");
}

/*------------------------------------------------------------------------
 * Write out the GRI_HDR object to an ascii file.
 */
static int gri_write_file_asc(
  GRI_HDR* hdr,
  const char* path)
{
  FILE* fp;
  int     rc;

  fp = fopen(path, "w");
  if (fp == GRI_NULL)
  {
    rc = GRI_ERR_CANNOT_OPEN_FILE;
  }
  else
  {
    GRI_REC* sf;

    gri_write_ov_asc(fp, hdr);

    for (sf = hdr->first_parent; sf != GRI_NULL; sf = sf->next)
      gri_write_sf_asc(fp, hdr, sf);

    gri_write_end_asc(fp);

    rc = (ferror(fp) == 0) ? GRI_ERR_OK : GRI_ERR_IOERR;
    fclose(fp);
  }

  return rc;
}

/* ------------------------------------------------------------------------- */
/* NTv2 write routines                                                       */
/* ------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Write out the GRI_HDR object to a file.
 */
int gri_write_file(
  GRI_HDR* hdr,
  const char* path,
  GRI_BOOL   byte_order)
{
  int type;
  int rc;

  if (hdr == GRI_NULL)
  {
    return GRI_ERR_NULL_HDR;
  }

  if (path == GRI_NULL || *path == 0)
  {
    return GRI_ERR_NULL_PATH;
  }

  if (hdr->num_recs == 0)
  {
    return GRI_ERR_HDRS_NOT_READ;
  }

  if (!hdr->keep_orig)
  {
    return GRI_ERR_ORIG_DATA_NOT_KEPT;
  }

  if ((hdr->first_parent)->shifts == GRI_NULL)
  {
    return GRI_ERR_DATA_NOT_READ;
  }

  type = gri_filetype(path);
  switch (type)
  {
  case GRI_FILE_TYPE_ASC:
    rc = gri_write_file_asc(hdr, path);
    break;

  case GRI_FILE_TYPE_BIN:
    rc = gri_write_file_bin(hdr, path, byte_order);
    break;

  default:
    rc = GRI_ERR_UNKNOWN_FILE_TYPE;
    break;
  }

  return rc;
}

/* ------------------------------------------------------------------------- */
/* NTv2 dump routines                                                        */
/* ------------------------------------------------------------------------- */

#define T1(x)   gri_strip( strncpy(stemp1, x, GRI_NAME_LEN) )
#define T2(x)   gri_strip( strncpy(stemp2, x, GRI_NAME_LEN) )
#define D1(x)   gri_dtoa (         ntemp1, x)
#define D2(x)   gri_dtoa (         ntemp2, x)
#define CV(x)   D2(x * hdr->hdr_conv)

/*------------------------------------------------------------------------
 * Dump the file overview struct to a stream.
 */
static void gri_dump_ov(
  const GRI_HDR* hdr,
  FILE* fp,
  int             mode)
{
  const GRI_FILE_OV* ov = hdr->overview;
  char   stemp1[GRI_NAME_LEN + 1];
  char   stemp2[GRI_NAME_LEN + 1];
  char   ntemp1[32];

  if (ov == GRI_NULL)
    return;

  if ((mode & GRI_DUMP_HDRS_EXT) == 0)
    return;

  stemp1[GRI_NAME_LEN] = 0;
  stemp2[GRI_NAME_LEN] = 0;

  fprintf(fp, "filename %s\n", hdr->path);
  fprintf(fp, "========  hdr ===================================\n");

  fprintf(fp, "%-8s %d\n", T1(ov->n_num_orec), ov->i_num_orec);
  fprintf(fp, "%-8s %d\n", T1(ov->n_num_srec), ov->i_num_srec);
  fprintf(fp, "%-8s %d\n", T1(ov->n_num_file), ov->i_num_file);
  fprintf(fp, "%-8s %s\n", T1(ov->n_gs_type), T2(ov->s_gs_type));
  fprintf(fp, "%-8s %s\n", T1(ov->n_version), T2(ov->s_version));
  fprintf(fp, "%-8s %s\n", T1(ov->n_system_f), T2(ov->s_system_f));
  fprintf(fp, "%-8s %s\n", T1(ov->n_system_t), T2(ov->s_system_t));
  fprintf(fp, "%-8s %s\n", T1(ov->n_major_f), D1(ov->d_major_f));
  fprintf(fp, "%-8s %s\n", T1(ov->n_minor_f), D1(ov->d_minor_f));
  fprintf(fp, "%-8s %s\n", T1(ov->n_major_t), D1(ov->d_major_t));
  fprintf(fp, "%-8s %s\n", T1(ov->n_minor_t), D1(ov->d_minor_t));

  fprintf(fp, "\n");
}

/*------------------------------------------------------------------------
 * Dump a file sub-file struct to a stream.
 */
static void gri_dump_sf(
  const GRI_HDR* hdr,
  FILE* fp,
  int             n,
  int             mode)
{
  const GRI_FILE_SF* sf;
  char   stemp1[GRI_NAME_LEN + 1];
  char   stemp2[GRI_NAME_LEN + 1];
  char   ntemp1[32];
  char   ntemp2[32];

  if ((mode & GRI_DUMP_HDRS_EXT) == 0)
    return;

  if ((mode & GRI_DUMP_HDRS_SUMMARY) != 0)
    return;

  if (!hdr->recs[n].active)
    return;

  if (hdr->subfiles == GRI_NULL)
    return;

  sf = hdr->subfiles + n;

  stemp1[GRI_NAME_LEN] = 0;
  stemp2[GRI_NAME_LEN] = 0;

  fprintf(fp, "======== %4d ===================================\n", n);

  fprintf(fp, "%-8s %s\n", T1(sf->n_sub_name),
    T2(sf->s_sub_name));

  fprintf(fp, "%-8s %s\n", T1(sf->n_parent),
    T2(sf->s_parent));

  fprintf(fp, "%-8s %s\n", T1(sf->n_created),
    T2(sf->s_created));

  fprintf(fp, "%-8s %s\n", T1(sf->n_updated),
    T2(sf->s_updated));

  fprintf(fp, "%-8s %-14s [ %13s ]\n", T1(sf->n_s_lat),
    D1(sf->d_s_lat),
    CV(sf->d_s_lat));

  fprintf(fp, "%-8s %-14s [ %13s ]\n", T1(sf->n_n_lat),
    D1(sf->d_n_lat),
    CV(sf->d_n_lat));

  fprintf(fp, "%-8s %-14s [ %13s ]\n", T1(sf->n_e_lon),
    D1(sf->d_e_lon),
    CV(sf->d_e_lon));

  fprintf(fp, "%-8s %-14s [ %13s ]\n", T1(sf->n_w_lon),
    D1(sf->d_w_lon),
    CV(sf->d_w_lon));

  fprintf(fp, "%-8s %-14s [ %13s ]\n", T1(sf->n_lat_inc),
    D1(sf->d_lat_inc),
    CV(sf->d_lat_inc));

  fprintf(fp, "%-8s %-14s [ %13s ]\n", T1(sf->n_lon_inc),
    D1(sf->d_lon_inc),
    CV(sf->d_lon_inc));

  fprintf(fp, "%-8s %d\n", T1(sf->n_gs_count), sf->i_gs_count);

  fprintf(fp, "\n");
}

/*------------------------------------------------------------------------
 * Dump the internal overview header struct to a stream.
 */
static void gri_dump_hdr(
  const GRI_HDR* hdr,
  FILE* fp,
  int             mode)
{
  char ntemp1[32];

  if ((mode & GRI_DUMP_HDRS_INT) == 0)
    return;

  if ((mode & GRI_DUMP_HDRS_SUMMARY) != 0)
    return;

  if ((mode & GRI_DUMP_HDRS_EXT) == 0)
  {
    fprintf(fp, "filename    %s\n", hdr->path);
    fprintf(fp, "========  overview ==============\n");
  }

  fprintf(fp, "num-recs    %6d\n", hdr->num_recs);
  fprintf(fp, "num-parents %6d\n", hdr->num_parents);
  if (hdr->num_parents > 0)
  {
    GRI_REC* rec = hdr->first_parent;
    const char* t = "parents  ";

    while (rec != GRI_NULL)
    {
      fprintf(fp, "%-11s %4d [ %s ]\n", t, rec->rec_num, rec->record_name);
      rec = rec->next;
      t = "";
    }
  }

  fprintf(fp, "conversion  %s  [ %s ]\n",
    D1(hdr->hdr_conv), hdr->gs_type);

  fprintf(fp, "\n");
}

/*------------------------------------------------------------------------
 * Dump an internal sub-file record struct to a stream.
 */
static void gri_dump_rec(
  const GRI_HDR* hdr,
  FILE* fp,
  int             n,
  int             mode)
{
  const GRI_REC* rec = hdr->recs + n;
  char ntemp1[32];

  if ((mode & GRI_DUMP_HDRS_INT) == 0)
    return;

  if ((mode & GRI_DUMP_HDRS_SUMMARY) != 0)
    return;

  if (!rec->active)
    return;

  if ((mode & GRI_DUMP_HDRS_EXT) == 0)
  {
    fprintf(fp, "======== sub-file %d ==============\n", n);
  }

  fprintf(fp, "name        %s\n", rec->record_name);
  fprintf(fp, "parent      %s", rec->parent_name);

  if (rec->parent != GRI_NULL)
    fprintf(fp, "  [ %d ]", rec->parent->rec_num);
  fprintf(fp, "\n");

  fprintf(fp, "num subs    %d\n", rec->num_subs);

  if (rec->sub != GRI_NULL)
  {
    const GRI_REC* sub = rec->sub;
    const char* t = "sub files";

    while (sub != GRI_NULL)
    {
      fprintf(fp, "%-11s %4d [ %s ]\n", t, sub->rec_num, sub->record_name);
      sub = sub->next;
      t = "";
    }
  }

  fprintf(fp, "num recs    %d\n", rec->num);
  fprintf(fp, "num cols    %d\n", rec->ncols);
  fprintf(fp, "num rows    %d\n", rec->nrows);

  fprintf(fp, "lat min     %s\n", D1(rec->lat_min));
  fprintf(fp, "lat max     %s\n", D1(rec->lat_max));
  fprintf(fp, "lat inc     %s\n", D1(rec->lat_inc));
  fprintf(fp, "lon min     %s\n", D1(rec->lon_min));
  fprintf(fp, "lon max     %s\n", D1(rec->lon_max));
  fprintf(fp, "lon inc     %s\n", D1(rec->lon_inc));

  fprintf(fp, "\n");
}

/*------------------------------------------------------------------------
 * Dump the data for a subfile.
 */
static void gri_dump_data(
  const GRI_HDR* hdr,
  FILE* fp,
  int             n,
  int             mode)
{
  const GRI_REC* rec = hdr->recs + n;

  if (rec->shifts != GRI_NULL)
  {
    /* GCC needs these casts for some reason... */
    const GRI_SHIFT* shifts = (const GRI_SHIFT*)(rec->shifts);
    const GRI_SHIFT* accurs = (const GRI_SHIFT*)(rec->accurs);
    GRI_BOOL    dump_acc = FALSE;
    int row, col;

    if ((mode & GRI_DUMP_DATA_ACC) == GRI_DUMP_DATA_ACC &&
      accurs != GRI_NULL)
    {
      dump_acc = TRUE;
    }

    for (row = 0; row < rec->nrows; row++)
    {
      if (dump_acc)
      {
        fprintf(fp, "   row     col       lat-shift       lon-shift  "
          "  lat-accuracy    lon-accuracy\n");
        fprintf(fp, "------  ------  --------------  --------------  "
          "--------------  --------------\n");
      }
      else
      {
        fprintf(fp, "   row     col        latitude       longitude  "
          "     lat-shift       lon-shift\n");
        fprintf(fp, "------  ------  --------------  --------------  "
          "--------------  --------------\n");
      }

      for (col = 0; col < rec->ncols; col++)
      {
        if (dump_acc)
        {
          fprintf(fp, "%6d  %6d  %14.8f  %14.8f  %14.8f  %14.8f\n",
            row, col,
            (double)(*shifts)[GRI_COORD_LAT],
            (double)(*shifts)[GRI_COORD_LON],
            (double)(*accurs)[GRI_COORD_LAT],
            (double)(*accurs)[GRI_COORD_LON]);
          shifts++;
          accurs++;
        }
        else
        {
          fprintf(fp, "%6d  %6d  %14.8f  %14.8f  %14.8f  %14.8f\n",
            row, col,
            rec->lat_min + (rec->lat_inc * row),
            rec->lon_max - (rec->lon_inc * col),
            (double)(*shifts)[GRI_COORD_LAT],
            (double)(*shifts)[GRI_COORD_LON]);
          shifts++;
        }
      }

      fprintf(fp, "\n");
    }
  }
}

/*------------------------------------------------------------------------
 * Dump the contents of all headers in an NTv2 file.
 */
void gri_dump(
  const GRI_HDR* hdr,
  FILE* fp,
  int             mode)
{
  if (hdr != GRI_NULL && fp != GRI_NULL)
  {
    int i;

    gri_dump_ov(hdr, fp, mode);
    gri_dump_hdr(hdr, fp, mode);

    for (i = 0; i < hdr->num_recs; i++)
    {
      gri_dump_sf(hdr, fp, i, mode);
      gri_dump_rec(hdr, fp, i, mode);

      if ((mode & GRI_DUMP_DATA) != 0)
        gri_dump_data(hdr, fp, i, mode);
    }
  }
}

/*------------------------------------------------------------------------
 * List the contents of all headers in an NTv2 file.
 */
void gri_list(
  const GRI_HDR* hdr,
  FILE* fp,
  int             mode,
  GRI_BOOL       do_hdr_line)
{
  if (hdr != GRI_NULL && fp != GRI_NULL)
  {
    int i;

    if (do_hdr_line)
    {
      if ((mode & GRI_DUMP_HDRS_SUMMARY) == 0)
      {
        fprintf(fp, "filename\n");
        fprintf(fp, "  num sub-file par  lon-min  lat-min "
          " lon-max  lat-max  d-lon  d-lat nrow ncol\n");
        fprintf(fp, "  --- -------- --- -------- -------- "
          "-------- -------- ------ ------ ---- ----\n");
      }
      else
      {
        fprintf(fp,
          "filename                             num  lon-min"
          "  lat-min  lon-max  lat-max\n");
        fprintf(fp,
          "-----------------------------------  --- --------"
          " -------- -------- --------\n");
      }
    }

    if ((mode & GRI_DUMP_HDRS_SUMMARY) == 0)
      fprintf(fp, "%s\n", hdr->path);

    for (i = 0; i < hdr->num_recs; i++)
    {
      const GRI_REC* rec = &hdr->recs[i];

      if (rec->active)
      {
        char parent[4];

        if (rec->parent == GRI_NULL)
        {
          strcpy(parent, " --");
        }
        else
        {
          sprintf(parent, "%3d", (rec->parent)->rec_num);
        }

        if ((mode & GRI_DUMP_HDRS_SUMMARY) == 0)
        {
          fprintf(fp,
            "  %3d %-8s %3s %8.3f %8.3f %8.3f %8.3f %6.3f %6.3f %4d %4d\n",
            i,
            rec->record_name,
            parent,
            rec->lon_min,
            rec->lat_min,
            rec->lon_max,
            rec->lat_max,
            rec->lon_inc,
            rec->lat_inc,
            rec->nrows,
            rec->ncols);
        }
      }
    }

    if ((mode & GRI_DUMP_HDRS_SUMMARY) == 0)
    {
      if (hdr->num_recs > 1)
      {
        fprintf(fp, "  %3s %-8s %3s %8.3f %8.3f %8.3f %8.3f\n",
          "tot",
          "--------",
          "   ",
          hdr->lon_min,
          hdr->lat_min,
          hdr->lon_max,
          hdr->lat_max);
      }
    }
    else
    {
      fprintf(fp, "%-36s %3d %8.3f %8.3f %8.3f %8.3f\n",
        hdr->path,
        hdr->num_recs,
        hdr->lon_min,
        hdr->lat_min,
        hdr->lon_max,
        hdr->lat_max);
    }

    if ((mode & GRI_DUMP_HDRS_SUMMARY) == 0)
      fprintf(fp, "\n");
  }
}

/* ------------------------------------------------------------------------- */
/* NTv2 forward / inverse routines                                           */
/* ------------------------------------------------------------------------- */

/*------------------------------------------------------------------------
 * Find the best gri record containing a given point.
 *
 * The NTv2 specification states that points on the edge of a
 * grid should be processed using the parent grid or be ignored, and
 * points outside of the top-level grid should be ignored completely.
 * However, this creates a situation where a point inside can be
 * shifted to a point outside or to the edge, and then it cannot
 * be shifted back.  Not good.
 *
 * Thus, we pretend that there is a one-cell zone around each grid
 * that has a shift value of zero.  This allows for a point that was
 * shifted out to be properly shifted back, and also allows for
 * a gradual change rather than an abrupt jump.  Much better!
 *
 * However, this creates a problem when two grids are adjacent, either
 * at a corner or along an edge (or both).  In fact, a point on the
 * corner of a grid could be adjacent to up to three other grids!  In this
 * case, a point in the imaginary outside cell of one grid could then
 * magically appear inside another grid and vice-versa.  So we have a lot
 * of esoteric code to deal with those cases.
 */
const GRI_REC* gri_find_rec(
  const GRI_HDR* hdr,
  double           lon,
  double           lat,
  int* pstatus)
{
  const GRI_REC* ps;
  const GRI_REC* psc = GRI_NULL;
  GRI_BOOL  next_gen = TRUE;
  int n_stat_5_pinged = 0;
  int status_ps = (GRI_STATUS_OUTSIDE_CELL + 1);
  int status_psc = (GRI_STATUS_OUTSIDE_CELL + 1);
  int status_tmp;
  int npings;

  if (pstatus == GRI_NULL)
    pstatus = &status_tmp;

  if (hdr == GRI_NULL)
  {
    *pstatus = GRI_STATUS_NOTFOUND;
    return GRI_NULL;
  }

  /* First, find the top-level parent that contains this point.
     There can only be one, since top-level parents cannot overlap.
     But we have to deal with edge conditions.
  */

  for (ps = hdr->first_parent; ps != GRI_NULL; ps = ps->next)
  {
    if (GRI_LE(lat, ps->lat_max) &&
      GRI_GT(lat, ps->lat_min) &&
      GRI_LE(lon, ps->lon_max) &&
      GRI_GT(lon, ps->lon_min))
    {
      /* Total containment */
      psc = ps;
      status_ps = GRI_STATUS_CONTAINED;
      break;
    }

    if (GRI_LE(lon, ps->lon_max) &&
      GRI_GT(lon, ps->lon_min) &&
      GRI_EQ(lat, ps->lat_max) &&
      status_ps > GRI_STATUS_NORTH)
    {
      /* Border condition - on North limit */
      psc = ps;
      status_ps = GRI_STATUS_NORTH;
    }

    else
      if (GRI_LE(lon, ps->lon_max) &&
        GRI_GT(lon, ps->lon_min) &&
        GRI_EQ(lat, ps->lat_max) &&
        status_ps > GRI_STATUS_WEST)
      {
        /* Border condition - on West limit */
        psc = ps;
        status_ps = GRI_STATUS_WEST;
      }

      else
        if (GRI_EQ(lon, ps->lon_min) &&
          GRI_EQ(lat, ps->lat_max) &&
          status_ps > GRI_STATUS_NORTH_WEST)
        {
          /* Border condition - on North & West limit */
          psc = ps;
          status_ps = GRI_STATUS_NORTH_WEST;
        }

        else
          if (GRI_GT(lon, (ps->lon_min - ps->lon_inc)) &&
            GRI_LT(lon, (ps->lon_max + ps->lon_inc)) &&
            GRI_GT(lat, (ps->lat_min - ps->lat_inc)) &&
            GRI_LT(lat, (ps->lat_max + ps->lat_inc)) &&
            status_ps > GRI_STATUS_OUTSIDE_CELL)
          {
            /* Is it just within one cell outside of the border? */
            status_ps = GRI_STATUS_OUTSIDE_CELL;

            /* Do a parent counter, and figure out if it is a
               one point corner or two point edge situation.
               Always take the two-pointer over a one-pointer.
               Always take the last two-pointer as the parent.
            */

            npings = 1;
            if (GRI_GE(lon, ps->lon_min) && GRI_LE(lon, ps->lon_max)) npings++;
            if (GRI_GE(lat, ps->lat_min) && GRI_LE(lat, ps->lat_max)) npings++;

            if (npings >= n_stat_5_pinged)
            {
              psc = ps;
              n_stat_5_pinged = npings;
            }
          }
  }

  /* If no parent was found, we're done. */

  if (psc == GRI_NULL)
  {
    *pstatus = GRI_STATUS_NOTFOUND;
    return GRI_NULL;
  }

  /* If this parent has no children or the point lies outside the
     parent border, we're done. */

  if (psc->sub == GRI_NULL || status_ps == GRI_STATUS_OUTSIDE_CELL)
  {
    *pstatus = status_ps;
    return psc;
  }

  /* Now run recursively through the child grids to find the
     best one.  Again, there can only be one best record, since
     children cannot overlap but can only nest.
     But here also we have to deal with edge conditions.
  */

  ps = psc;
  psc = GRI_NULL;
  while (ps->sub != GRI_NULL && next_gen)
  {
    next_gen = FALSE;

    for (psc = ps->sub; psc != GRI_NULL; psc = psc->next)
    {
      if (GRI_LE(lon, psc->lon_max) &&
        GRI_GT(lon, psc->lon_min) &&
        GRI_GE(lat, psc->lat_min) &&
        GRI_LT(lat, psc->lat_max) &&
        status_psc >= GRI_STATUS_CONTAINED)
      {
        /* Total containment */
        next_gen = TRUE;
        ps = psc;
        status_psc = GRI_STATUS_CONTAINED;
        status_ps = GRI_STATUS_CONTAINED;
        break;
      }

      else
        if (GRI_LE(lon, psc->lon_max) &&
          GRI_GT(lon, psc->lon_min) &&
          GRI_EQ(lat, psc->lat_max) &&
          status_psc >= GRI_STATUS_NORTH)
        {
          /* Border condition - on North limit */
          next_gen = TRUE;
          ps = psc;
          status_psc = GRI_STATUS_NORTH;
          status_ps = GRI_STATUS_NORTH;
        }

        else
          if (GRI_EQ(lon, psc->lon_min) &&
            GRI_GE(lat, psc->lat_min) &&
            GRI_LT(lat, psc->lat_max) &&
            status_psc >= GRI_STATUS_WEST)
          {
            /* Border condition - on West limit */
            next_gen = TRUE;
            ps = psc;
            status_psc = GRI_STATUS_WEST;
            status_ps = GRI_STATUS_WEST;
          }

          else
            if (GRI_EQ(lon, psc->lon_min) &&
              GRI_EQ(lat, psc->lat_max) &&
              status_psc >= GRI_STATUS_NORTH_WEST)
            {
              /* Border condition - on North & West limit */
              next_gen = TRUE;
              ps = psc;
              status_psc = GRI_STATUS_NORTH_WEST;
              status_ps = GRI_STATUS_NORTH_WEST;
            }
    }
  }

  *pstatus = status_ps;
  return ps;
}

/*------------------------------------------------------------------------
 * Get a lat/lon shift value (either from a file or memory).
 *
 * Note that these routines are called only if a sub-file record
 * was found that contains the point.  Thus we know that the row
 * and column values are valid.  However, it is possible to have
 * not read the data (only the headers) and then closed the file,
 * so we have to check for that.
 */
double gri_get_shift_from_file(
  GRI_HDR* hdr,
  GRI_REC* rec,
  int              icol,
  int              irow,
  int              coord_type)
{
  float shift = 0.0;
  int   offs = int(rec->offset +
    (rec->ncols * irow + icol) * sizeof(GRI_FILE_GS) +
    ((coord_type == GRI_COORD_LAT) ?
      GRI_OFFSET_OF(GRI_FILE_GS, f_lat_shift) :
      GRI_OFFSET_OF(GRI_FILE_GS, f_lon_shift)));
  size_t nr = 0;

  if (hdr->fp->is_open())
  {
    gri_mutex_enter(hdr->mutex);
    {

      hdr->fp->seekg(offs,ios_base::beg);
      hdr->fp->read((char *)&shift, GRI_SIZE_FLT);
    }
    gri_mutex_leave(hdr->mutex);
  }

  if (nr != 1)
  {
    shift = 0.0;
  }
  else
  {
    GRI_SWAPF(&shift, 1);
  }

  return shift;
}

static double gri_get_shift_from_data(
  const GRI_HDR* hdr,
  const GRI_REC* rec,
  int              icol,
  int              irow,
  int              coord_type)
{
  double shift;
  int   offs = (irow * rec->ncols) + icol;

  GRI_UNUSED_PARAMETER(hdr);

  shift = rec->shifts[offs][coord_type];

  return shift;
}

static double gri_get_shift(
  const GRI_HDR* hdr,
  const GRI_REC* rec,
  int              icol,
  int              irow,
  int              coord_type)
{
  if (rec->shifts == GRI_NULL)
    return gri_get_shift_from_file((GRI_HDR *) hdr, (GRI_REC * )rec, irow, icol, coord_type);
  else
    return gri_get_shift_from_data((GRI_HDR*)hdr, (GRI_REC*)rec, irow, icol, coord_type);
}

/*------------------------------------------------------------------------
 * Calculate one shift (lat or lon).
 *
 * In this routine we deal with our idea of a phantom row/col of
 * zero-shift values along each edge of the top-level-grid.
 */
static double gri_calculate_one_shift(
  const GRI_HDR* hdr,
  const GRI_REC* rec,
  int              status,
  int              icol,
  int              irow,
  int              move_shifts_horz,
  int              move_shifts_vert,
  double           x_cellfrac,
  double           y_cellfrac,
  int              coord_type)
{
  double ll_shift = 0, lr_shift = 0, ul_shift = 0, ur_shift = 0;
  double b, c, d;
  double shift;

#define GET_SHIFT(i,j)   gri_get_shift(hdr, rec, i, j, coord_type)

  /* get the shift values for the "corners" of the cell
     containing the point */

  switch (status)
  {
  case GRI_STATUS_CONTAINED:
    lr_shift = GET_SHIFT(irow, icol);
    ll_shift = GET_SHIFT(irow, icol + 1);
    ur_shift = GET_SHIFT(irow + 1, icol);
    ul_shift = GET_SHIFT(irow + 1, icol + 1);
    break;

  case GRI_STATUS_NORTH:
    lr_shift = GET_SHIFT(irow, icol);
    ll_shift = GET_SHIFT(irow, icol + 1);
    ur_shift = lr_shift;
    ul_shift = ll_shift;
    break;

  case GRI_STATUS_WEST:
    lr_shift = GET_SHIFT(irow, icol);
    ll_shift = lr_shift;
    ur_shift = GET_SHIFT(irow + 1, icol);
    ul_shift = ur_shift;
    break;

  case GRI_STATUS_NORTH_WEST:
    lr_shift = GET_SHIFT(irow, icol);
    ll_shift = lr_shift;
    ur_shift = lr_shift;
    ul_shift = lr_shift;
    break;

  case GRI_STATUS_OUTSIDE_CELL:
    lr_shift = GET_SHIFT(irow, icol);
    ll_shift = GET_SHIFT(irow, icol + 1);
    ur_shift = GET_SHIFT(irow + 1, icol);
    ul_shift = GET_SHIFT(irow + 1, icol + 1);

    if (move_shifts_horz == -1)
    {
      lr_shift = ll_shift;
      ur_shift = ul_shift;
      ll_shift = 0.0;
      ul_shift = 0.0;
    }
    if (move_shifts_horz == +1)
    {
      ll_shift = lr_shift;
      ul_shift = ur_shift;
      ur_shift = 0.0;
      lr_shift = 0.0;
    }
    if (move_shifts_vert == -1)
    {
      ul_shift = ll_shift;
      ur_shift = lr_shift;
      ll_shift = 0.0;
      lr_shift = 0.0;
    }
    if (move_shifts_vert == +1)
    {
      ll_shift = ul_shift;
      lr_shift = ur_shift;
      ul_shift = 0.0;
      ur_shift = 0.0;
    }
    break;

  default:
    return 0.0;
  }

#undef GET_SHIFT

  /* do the bilinear interpolation of the corner shift values */

  b = (ll_shift - lr_shift);
  c = (ur_shift - lr_shift);
  d = (ul_shift - ll_shift) - (ur_shift - lr_shift);

  shift = lr_shift + (b * x_cellfrac)
    + (c * y_cellfrac)
    + (d * x_cellfrac * y_cellfrac);

  /* The shift at this point is in decimal seconds, so convert to degrees. */
  return (shift * hdr->dat_conv) / 3600.0;
}

/*------------------------------------------------------------------------
 * Calculate the lon & lat shifts for a given lon/lat.
 *
 * In this routine we deal with our idea of a phantom row/col of
 * zero-shift values along each edge of the top-level-grid.
 */
static void gri_calculate_shifts(
  const GRI_HDR* hdr,
  const GRI_REC* rec,
  double           lon,
  double           lat,
  int              status,
  double* plon_shift,
  double* plat_shift)
{
  double xgrid_index, ygrid_index, x_cellfrac, y_cellfrac;
  int    horz = 0, vert = 0;
  int    icol, irow;

  /* lat goes S to N, lon goes E to W */
  xgrid_index = (rec->lon_max - lon) / rec->lon_inc;
  ygrid_index = (lat - rec->lat_min) / rec->lat_inc;

  icol = (int)xgrid_index;
  irow = (int)ygrid_index;

  /* If the status is GRI_STATUS_OUTSIDE_CELL, it is within one cell of the
     edge of a parent grid.  Have to do something different.
  */
  if (status == GRI_STATUS_OUTSIDE_CELL)
  {
    icol = (xgrid_index < 0.0) ? -1 : (int)xgrid_index;
    irow = (ygrid_index < 0.0) ? -1 : (int)ygrid_index;
  }

  x_cellfrac = xgrid_index - icol;
  y_cellfrac = ygrid_index - irow;

  if (status == GRI_STATUS_OUTSIDE_CELL)
  {
    horz = (icol < 0) ? +1
      : (icol > rec->ncols - 2) ? -1 : 0;
    vert = (irow < 0) ? -1
      : (irow > rec->nrows - 2) ? +1 : 0;

    icol = (icol < 0) ? 0
      : (icol > rec->ncols - 2) ? rec->ncols - 2 : icol;
    irow = (irow < 0) ? 0
      : (irow > rec->nrows - 2) ? rec->nrows - 2 : irow;
  }

  /* The longitude shifts are built for (+west/-east) longitude values,
     so we flip the sign of the calculated longitude shift to make
     it standard (-west/+east).
  */

  *plon_shift = -gri_calculate_one_shift(hdr, rec, status,
    icol, irow, horz, vert, x_cellfrac, y_cellfrac, GRI_COORD_LON);

  *plat_shift = gri_calculate_one_shift(hdr, rec, status,
    icol, irow, horz, vert, x_cellfrac, y_cellfrac, GRI_COORD_LAT);
}

/*------------------------------------------------------------------------
 * Perform a forward transformation on an array of points.
 *
 * Note that the return value is the number of points successfully
 * transformed, and points that can't be transformed (usually because
 * they are outside of the grid) are left unchanged.  However, there
 * is no indication of which points were changed and which were not.
 */
int gri_forward(
  const GRI_HDR* hdr,
  double          deg_factor,
  int             n,
  GRI_COORD      coord[])
{
  int num = 0;
  int i;

  if (hdr == GRI_NULL || coord == GRI_NULL || n <= 0)
    return 0;

  if (deg_factor <= 0.0)
    deg_factor = 1.0;

  for (i = 0; i < n; i++)
  {
    const GRI_REC* rec;
    double lon, lat;
    int status;

    lon = (coord[i][GRI_COORD_LON] * deg_factor);
    lat = (coord[i][GRI_COORD_LAT] * deg_factor);

    rec = gri_find_rec(hdr, lon, lat, &status);
    if (rec != GRI_NULL)
    {
      double lon_shift, lat_shift;

      gri_calculate_shifts(hdr, rec, lon, lat, status,
        &lon_shift, &lat_shift);

      coord[i][GRI_COORD_LON] = ((lon + lon_shift) / deg_factor);
      coord[i][GRI_COORD_LAT] = ((lat + lat_shift) / deg_factor);
      num++;
    }
  }

  return num;
}

/*------------------------------------------------------------------------
 * Perform a inverse transformation on an array of points.
 *
 * Note that the return value is the number of points successfully
 * transformed, and points that can't be transformed (usually because
 * they are outside of the grid) are left unchanged.  However, there
 * is no indication of which points were changed and which were not.
 */
#ifndef   MAX_ITERATIONS
#  define MAX_ITERATIONS  50
#endif

int gri_inverse(
  const GRI_HDR* hdr,
  double          deg_factor,
  int             n,
  GRI_COORD      coord[])
{
  int max_iterations = MAX_ITERATIONS;
  int num = 0;
  int i;

  if (hdr == GRI_NULL || coord == GRI_NULL || n <= 0)
    return 0;

  if (deg_factor <= 0.0)
    deg_factor = 1.0;

  for (i = 0; i < n; i++)
  {
    double  lon, lat;
    double  lon_next, lat_next;
    int num_iterations;

    lon_next = lon = (coord[i][GRI_COORD_LON] * deg_factor);
    lat_next = lat = (coord[i][GRI_COORD_LAT] * deg_factor);

    /* The inverse is not a simple transformation like the forward.
       We have to iteratively zero in on the answer by successively
       calculating what the forward delta is at the point, and then
       subtracting it instead of adding it.  The assumption here
       is that all the shifts are smooth, which should be the case.

       If we can't get the lat and lon deltas between two steps to be
       both within a given tolerance (GRI_EPS) in max_iterations,
       we just give up and use the last value we calculated.
    */

    for (num_iterations = 0;
      num_iterations < max_iterations;
      num_iterations++)
    {
      const GRI_REC* rec;
      double lon_shift, lat_shift;
      double lon_delta, lat_delta;
      double lon_est, lat_est;
      int status;

      /* It may seem unnecessary to find the subfile this point is
         in each time, since it would seem that it *shouldn't* change.
         However, if the point was outside of a grid but within one cell
         of the edge, a shift may move it inside and then it could
         possibly be found in some subfile.  It is also possible
         that a point inside could shift to be on the edge or outside,
         in which case the record for it would be a higher-up parent or
         even a different sub-grid.
      */

      rec = gri_find_rec(hdr, lon_next, lat_next, &status);
      if (rec == GRI_NULL)
        break;

      gri_calculate_shifts(hdr, rec, lon_next, lat_next, status,
        &lon_shift, &lat_shift);

      lon_est = (lon_next + lon_shift);
      lat_est = (lat_next + lat_shift);

      lon_delta = (lon_est - lon);
      lat_delta = (lat_est - lat);

#if DEBUG_INVERSE
      {
        char buf1[32], buf2[32];
        fprintf(stderr, "iteration %2d: value: %s %s\n",
          num_iterations + 1,
          gri_dtoa(buf1, lon_next - lon_delta),
          gri_dtoa(buf1, lat_next - lat_delta));
        fprintf(stderr, "              delta: %s %s\n",
          gri_dtoa(buf1, lon_delta),
          gri_dtoa(buf2, lat_delta));
      }
#endif

      if (GRI_ZERO(lon_delta) && GRI_ZERO(lat_delta))
        break;

      lon_next = (lon_next - lon_delta);
      lat_next = (lat_next - lat_delta);
    }

#if DEBUG_INVERSE
    {
      char buf1[32], buf2[32];
      fprintf(stderr, "final         value: %s %s\n",
        gri_dtoa(buf1, lon_next),
        gri_dtoa(buf2, lat_next));
    }
#endif

    if (num_iterations > 0)
    {
      coord[i][GRI_COORD_LON] = (lon_next / deg_factor);
      coord[i][GRI_COORD_LAT] = (lat_next / deg_factor);
      num++;
    }
  }

  return num;
}

/*------------------------------------------------------------------------
 * Perform a transformation (forward or inverse) on an array of points.
 */
int gri_transform(
  const GRI_HDR* hdr,
  double          deg_factor,
  int             n,
  GRI_COORD      coord[],
  int             direction)
{
  if (direction == GRI_CVT_INVERSE)
    return gri_inverse(hdr, deg_factor, n, coord);
  else
    return gri_forward(hdr, deg_factor, n, coord);
}
static void swap_int(int in[], int ntimes)
{
  int i;

  for (i = 0; i < ntimes; i++)
    in[i] = SWAP4((unsigned int)in[i]);
}

static void swap_flt(float in[], int ntimes)
{
  swap_int((int*)in, ntimes);
}

static void swap_dbl(double in[], int ntimes)
{
  int  i;
  int* p_int, tmpint;

  for (i = 0; i < ntimes; i++)
  {
    p_int = (int*)(&in[i]);
    swap_int(p_int, 2);

    tmpint = p_int[0];
    p_int[0] = p_int[1];
    p_int[1] = tmpint;
  }
}


/*------------------------------------------------------------------------
 * output usage
 */
void display_usage(int level)
{
  if (level)
  {
    printf("Usage: %s [options] grifile [lat lon] ...\n", pgm);
    printf("Options:\n");
    printf("  -?, -help  Display help\n");
    printf("  -r         Reversed data (lon lat) instead of (lat lon)\n");
    printf("  -d         Read shift data on the fly (no load of data)\n");
    printf("  -i         Inverse transformation\n");
    printf("  -f         Forward transformation       "
      "(default)\n");
    printf("\n");
    printf("  -c val     Conversion: degrees-per-unit "
      "(default is %.17g)\n", deg_factor);
    printf("  -s str     Use str as output separator  "
      "(default is \" \")\n");
    printf("  -p file    Read points from file        "
      "(default is \"-\" or stdin)\n");
    printf("  -e wlon slat elon nlat   Specify extent\n");
    printf("\n");

    printf("If no coordinate pairs are specified on the command line, then\n");
    printf("they are read one per line from the specified data file.\n");
  }
  else
  {
    fprintf(stderr,
      "Usage: %s [-r] [-d] [-i|-f] [-c val] [-s str] [-p file]\n",
      pgm);
    fprintf(stderr,
      "       %*s [-e wlon slat elon nlat]\n",
      (int)strlen(pgm), "");
    fprintf(stderr,
      "       %*s grifile [lat lon] ...\n",
      (int)strlen(pgm), "");
  }
}

/*------------------------------------------------------------------------
 * process all command-line options
 */
int process_options(int argc, const char** argv)
{
  int optcnt;

  if (pgm == GRI_NULL)  pgm = strrchr(argv[0], '/');
  if (pgm == GRI_NULL)  pgm = strrchr(argv[0], '\\');
  if (pgm == GRI_NULL)  pgm = argv[0];
  else                     pgm++;

  for (optcnt = 1; optcnt < argc; optcnt++)
  {
    const char* arg = argv[optcnt];

    if (*arg != '-')
      break;

    while (*arg == '-')
      arg++;
    if (!*arg)
    {
      optcnt++;
      break;
    }

    if (strcmp(arg, "?") == 0 ||
      strcmp(arg, "help") == 0)
    {
      display_usage(1);
      exit(EXIT_SUCCESS);
    }

    else if (strcmp(arg, "f") == 0)  direction = GRI_CVT_FORWARD;
    else if (strcmp(arg, "i") == 0)  direction = GRI_CVT_INVERSE;
    else if (strcmp(arg, "r") == 0)  reversed = TRUE;
    else if (strcmp(arg, "d") == 0)  read_on_fly = TRUE;

    else if (strcmp(arg, "s") == 0)
    {
      if (++optcnt >= argc)
      {
        fprintf(stderr, "%s: Option needs an argument -- -%s\n",
          pgm, "s");
        display_usage(0);
        exit(EXIT_FAILURE);
      }
      separator = argv[optcnt];
    }

    else if (strcmp(arg, "c") == 0)
    {
      if (++optcnt >= argc)
      {
        fprintf(stderr, "%s: Option needs an argument -- -%s\n",
          pgm, "c");
        display_usage(0);
        exit(EXIT_FAILURE);
      }
      deg_factor = atof(argv[optcnt]);
    }

    else if (strcmp(arg, "p") == 0)
    {
      if (++optcnt >= argc)
      {
        fprintf(stderr, "%s: Option needs an argument -- -%s\n",
          pgm, "p");
        display_usage(0);
        exit(EXIT_FAILURE);
      }
      datafile = argv[optcnt];
    }

    else if (strcmp(arg, "e") == 0)
    {
      if ((optcnt + 4) >= argc)
      {
        fprintf(stderr, "%s: Option needs 4 arguments -- -%s\n",
          pgm, "e");
        display_usage(0);
        exit(EXIT_FAILURE);
      }
      s_extent.wlon = atof(argv[++optcnt]);
      s_extent.slat = atof(argv[++optcnt]);
      s_extent.elon = atof(argv[++optcnt]);
      s_extent.nlat = atof(argv[++optcnt]);
      extptr = &s_extent;
    }

    else
    {
      fprintf(stderr, "%s: Invalid option -- %s\n", pgm, argv[optcnt]);
      display_usage(0);
      exit(EXIT_FAILURE);
    }
  }

  return optcnt;
}

/*------------------------------------------------------------------------
 * strip a string of all leading/trailing whitespace
 */
char* strip(char* str)
{
  char* s;
  char* e = GRI_NULL;

  for (; isspace(*str); str++);

  for (s = str; *s; s++)
  {
    if (!isspace(*s))
      e = s;
  }

  if (e != GRI_NULL)
    e[1] = 0;
  else
    *str = 0;

  return str;
}

/*------------------------------------------------------------------------
 * process a lat/lon value
 */
void process_lat_lon(
  GRI_HDR* hdr,
  double     &lat,
  double     &lon)
{
  GRI_COORD coord[1];

  coord[0][GRI_COORD_LAT] = lat;
  coord[0][GRI_COORD_LON] = lon;

  if (gri_transform(hdr, deg_factor, 1, coord, direction) == 1)
  {
    lat = coord[0][GRI_COORD_LAT];
    lon = coord[0][GRI_COORD_LON];
  }
  /*
  if (reversed)
    printf("%.16g%s%.16g\n", lon, separator, lat);
  else
    printf("%.16g%s%.16g\n", lat, separator, lon);
  fflush(stdout);*/
 }

/*------------------------------------------------------------------------
 * process all arguments
 */
  int process_args(
  GRI_HDR* hdr,
  int          optcnt,
  int          argc,
  const char** argv)
{
  while ((argc - optcnt) >= 2)
  {
    double lon, lat;

    if (reversed)
    {
      lon = atof(argv[optcnt++]);
      lat = atof(argv[optcnt++]);
    }
    else
    {
      lat = atof(argv[optcnt++]);
      lon = atof(argv[optcnt++]);
    }

    process_lat_lon(hdr, lat, lon);
  }

  return 0;
}

/*------------------------------------------------------------------------
 * process a stream of lon/lat values
 */
int process_file(
  GRI_HDR* hdr,
  const char* file)
{
  FILE* fp;

  if (strcmp(file, "-") == 0)
  {
    fp = stdin;
  }
  else
  {
    fp = fopen(file, "r");
    if (fp == GRI_NULL)
    {
      fprintf(stderr, "%s: Cannot open data file %s\n", pgm, file);
      return -1;
    }
  }

  for (;;)
  {
    char   line[128];
    char* lp;
    double lon, lat;
    int n;

    if (fgets(line, sizeof(line), fp) == GRI_NULL)
      break;

    /* Strip all whitespace to check for an empty line.
       Lines starting with a # are considered comments.
    */
    lp = strip(line);
    if (*lp == 0 || *lp == '#')
      continue;

    /* parse lat/lon or lon/lat & process it */
    if (reversed)
      n = sscanf(lp, "%lf %lf", &lon, &lat);
    else
      n = sscanf(lp, "%lf %lf", &lat, &lon);

    if (n != 2)
      printf("invalid: %s\n", lp);
    else
      process_lat_lon(hdr, lat, lon);
  }

  fclose(fp);
  return 0;
}

int main_routine(int argc, const char** argv)
{
  GRI_HDR* hdr;
  int optcnt;
  int rc;

  /*---------------------------------------------------------
   * process options
   */
  optcnt = process_options(argc, argv);

  /*---------------------------------------------------------
   * get the filename
   */
  if ((argc - optcnt) == 0)
  {
    fprintf(stderr, "%s: Missing NTv2 filename\n", pgm);
    display_usage(0);
    return EXIT_FAILURE;
  }
  grifile = argv[optcnt++];

  /*---------------------------------------------------------
   * load the file
   */
  hdr = gri_load_file(
    grifile,           /* in:  filename                 */
    FALSE,              /* in:  don't keep original hdrs */
    !read_on_fly,       /* in:  read    shift data?      */
    extptr,             /* in:  extent pointer           */
    &rc);               /* out: result code              */

  if (hdr == GRI_NULL)
  {
    char msg_buf[GRI_MAX_ERR_LEN];

    fprintf(stderr, "%s: %s: %s\n", pgm, grifile, gri_errmsg(rc, msg_buf));
    return EXIT_FAILURE;
  }

  /*---------------------------------------------------------
   * Either process lon/lat pairs from the cmd line or
   * process all points in the input file.
   */
  if (optcnt < argc)
    rc = process_args(hdr, optcnt, argc, argv);
  else
    rc = process_file(hdr, datafile);

  /*---------------------------------------------------------
   * done - close out the gri object and exit
   */
  gri_delete(hdr);

  return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

