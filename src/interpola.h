#pragma once
#define linux
#include <cstdio>
#include <string>
#include <iostream>
//#include <filesystem>
#include <cmath>
#include <climits>
#include <cstdio>
#include <iostream>
#include <streambuf>
#include <cstdlib>
#include <vector>
#include <cassert>
#include <fstream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
using namespace std;
#define GRI_NO_MUTEXES

/* ------------------------------------------------------------------------- */
/* public header for the libntv2 library                                     */
/* ------------------------------------------------------------------------- */

#ifndef LIBGRI_INCLUDED
#define LIBGRI_INCLUDED

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

  /* ---------------------------------------------------------------------- */
  /* version info                                                           */
  /* ---------------------------------------------------------------------- */

#define GRI_VERSION_MAJOR     1
#define GRI_VERSION_MINOR     0
#define GRI_VERSION_RELEASE   0
#define GRI_VERSION_STR       "1.0.1"

/*------------------------------------------------------------------------*/
/* external definitions & structs                                         */
/*------------------------------------------------------------------------*/

#define FALSE              0
#define TRUE               1

#define GRI_NULL          0             /*!< NULL pointer         */

#define GRI_MAX_PATH_LEN  1024          /*!< Max pathname length  */
#define GRI_MAX_ERR_LEN   32            /*!< Max err msg  length  */

  typedef int            GRI_BOOL;        /*!< Boolean variable     */
  typedef double         GRI_COORD[2];   /*!< Lon/lat coordinate   */

#define GRI_COORD_LON     0             /*!< GRI_COORD longitude */
#define GRI_COORD_LAT     1             /*!< GRI_COORD latitude  */

#define GRI_COORD_LAM     0             /*!< GRI_COORD longitude */
#define GRI_COORD_PHI     1             /*!< GRI_COORD latitude  */

/*---------------------------------------------------------------------*/
/**
 * NTv2 Extent struct
 *
 * <p>This struct defines the lower-left and the upper-right
 * corners of an extent to use to cut down the area defined by
 * a grid file.
 *
 * <p>Unlike NTv2 files, this struct uses the standard notation of
 * negative-west / positive-east. Values are in degrees.
 *
 * <p>Since shifts are usually very small (on the order of fractions
 * of a second), it doesn't matter which datum the values are on.
 *
 * <p>Note that this struct is used only by this API, and is not part of
 * the NTv2 specification.
 */
  typedef struct gri_extent GRI_EXTENT;
  struct gri_extent
  {
    /* lower-left  corner */
    double  wlon;                 /*!< West  longitude (degrees) */
    double  slat;                 /*!< South latitude  (degrees) */

    /* upper-right corner */
    double  elon;                 /*!< East  longitude (degrees) */
    double  nlat;                 /*!< North latitude  (degrees) */
  };

  /*------------------------------------------------------------------------*/
  /* NTv2 file layout                                                       */
  /*                                                                        */
  /* An NTv2 file (either binary or text) is laid out as follows:           */
  /*                                                                        */
  /*    overview record                                                     */
  /*                                                                        */
  /*    sub-file record 1                                                   */
  /*       gs_count (nrows * ncols) grid-shift records                      */
  /*    ...                                                                 */
  /*    sub-file record n (if present)                                      */
  /*       gs_count (nrows * ncols) grid-shift records                      */
  /*                                                                        */
  /*    end record                                                          */
  /*------------------------------------------------------------------------*/

#define GRI_NAME_LEN            8      /*!< Blank-padded, no null */

#define GRI_FILE_BIN_EXTENSION  "gsb"  /*!< "Grid Shift Binary"   */
#define GRI_FILE_ASC_EXTENSION  "gsa"  /*!< "Grid Shift Ascii"    */

#define GRI_FILE_TYPE_UNK       0      /*!< File type is unknown  */
#define GRI_FILE_TYPE_BIN       1      /*!< File type is binary   */
#define GRI_FILE_TYPE_ASC       2      /*!< File type is ascii    */

/*---------------------------------------------------------------------*/
/**
 * NTv2 binary file overview record
 *
 * Notes:
 *   <ul>
 *     <li>The pad words (p_* fields) may not be present in the file,
 *         although any files created after 2000 should have them.
 *     <li>The strings (n_* and s_* fields) are blank-padded and are not
 *         null-terminated.
 *     <li>The name strings (n_* fields) should appear exactly as in the
 *         comments.
 *   </ul>
 */
  typedef struct gri_file_ov GRI_FILE_OV;
  struct gri_file_ov
  {
    char   n_num_orec[GRI_NAME_LEN];  /*!< "NUM_OREC"                */
    int    i_num_orec;                  /*!< Should be 11              */
    int    p_num_orec;                  /*!< Zero pad ?                */

    char   n_num_srec[GRI_NAME_LEN];  /*!< "NUM_SREC"                */
    int    i_num_srec;                  /*!< Should be 11              */
    int    p_num_srec;                  /*!< Zero pad ?                */

    char   n_num_file[GRI_NAME_LEN];  /*!< "NUM_FILE"                */
    int    i_num_file;                  /*!< Num of sub-grid files     */
    int    p_num_file;                  /*!< Zero pad ?                */

    char   n_gs_type[GRI_NAME_LEN];  /*!< "GS_TYPE "                */
    char   s_gs_type[GRI_NAME_LEN];  /*!< e.g. "SECONDS "           */

    char   n_version[GRI_NAME_LEN];  /*!< "VERSION "                */
    char   s_version[GRI_NAME_LEN];  /*!< ID of distortion model    */

    char   n_system_f[GRI_NAME_LEN];  /*!< "SYSTEM_F"                */
    char   s_system_f[GRI_NAME_LEN];  /*!< Reference system from     */

    char   n_system_t[GRI_NAME_LEN];  /*!< "SYSTEM_T"                */
    char   s_system_t[GRI_NAME_LEN];  /*!< Reference system to       */

    char   n_major_f[GRI_NAME_LEN];  /*!< "MAJOR_F "                */
    double d_major_f;                   /*!< Semi-major axis from      */

    char   n_minor_f[GRI_NAME_LEN];  /*!< "MINOR_F "                */
    double d_minor_f;                   /*!< Semi-minor axis from      */

    char   n_major_t[GRI_NAME_LEN];  /*!< "MAJOR_T "                */
    double d_major_t;                   /*!< Semi-major axis to        */

    char   n_minor_t[GRI_NAME_LEN];  /*!< "MINOR_T "                */
    double d_minor_t;                   /*!< Semi-minor axis to        */
  };

  /*---------------------------------------------------------------------*/
  /**
   * NTv2 binary file subfile record
   *
   * Notes:
   *   <ul>
   *     <li>The pad words (p_* fields) may not be present in the file,
   *         although any files created after 2000 should have them.
   *     <li>The strings (n_* and s_* fields) are blank-padded and are not
   *         null-terminated.
   *     <li>The name strings (n_* fields) should appear exactly as in the
   *         comments.
   *     <li>There will always be at least one subfile record which is a parent.
   *     <li>Parent records should have a parent-name of "NONE".
   *     <li>Longitude values are specified as positive-west / negative-east.
   *   </ul>
   */
  typedef struct gri_file_sf GRI_FILE_SF;
  struct gri_file_sf
  {
    char   n_sub_name[GRI_NAME_LEN];  /*!< "SUB_NAME"                */
    char   s_sub_name[GRI_NAME_LEN];  /*!< Sub-file name             */

    char   n_parent[GRI_NAME_LEN];  /*!< "PARENT  "                */
    char   s_parent[GRI_NAME_LEN];  /*!< Parent name or "NONE"     */

    char   n_created[GRI_NAME_LEN];  /*!< "CREATED "                */
    char   s_created[GRI_NAME_LEN];  /*!< Creation date             */

    char   n_updated[GRI_NAME_LEN];  /*!< "UPDATED "                */
    char   s_updated[GRI_NAME_LEN];  /*!< Last revision date        */

    char   n_s_lat[GRI_NAME_LEN];  /*!< "S_LAT   "                */
    double d_s_lat;                     /*!< South lat (in gs-units)   */

    char   n_n_lat[GRI_NAME_LEN];  /*!< "N_LAT   "                */
    double d_n_lat;                     /*!< North lat (in gs-units)   */

    char   n_e_lon[GRI_NAME_LEN];  /*!< "E_LONG  "                */
    double d_e_lon;                     /*!< East  lon (in gs-units)   */

    char   n_w_lon[GRI_NAME_LEN];  /*!< "W_LONG  "                */
    double d_w_lon;                     /*!< West  lon (in gs-units)   */

    char   n_lat_inc[GRI_NAME_LEN];  /*!< "LAT_INC "                */
    double d_lat_inc;                   /*!< Lat inc   (in gs-units)   */

    char   n_lon_inc[GRI_NAME_LEN];  /*!< "LONG_INC"                */
    double d_lon_inc;                   /*!< Lon inc   (in gs-units)   */

    char   n_gs_count[GRI_NAME_LEN];  /*!< "GS_COUNT"                */
    int    i_gs_count;                  /*!< Num grid-shift records    */
    int    p_gs_count;                  /*!< Zero pad ?                */
  };

  /*---------------------------------------------------------------------*/
  /**
   * NTv2 binary file end record
   *
   * <p>There are files (such as all Canadian files) that have only
   * the first four bytes correct (i.e. "END xxxx"), and there are
   * files (such as italy/rer.gsb) that omit this record completely.
   * Fortunately, we don't rely on the end-record when reading a file,
   * but depend on the num-subfiles field in the overview record.
   *
   * <p>Also, very few files that I've seen actually zero out the filler field.
   * Some files fill the field with spaces.  Most files just contain garbage
   * bytes (which is bogus, IMNSHO, since you can't then compare or checksum
   * files properly).
   */
  typedef struct gri_file_end GRI_FILE_END;
  struct gri_file_end
  {
    char   n_end[GRI_NAME_LEN];  /*!< "END     "                */
    char   filler[GRI_NAME_LEN];  /*!< Totally unused
                                             (should be zeroed out)    */
  };

  /*---------------------------------------------------------------------*/
  /**
   * NTv2 binary file grid-shift record
   *
   * For a particular sub-file, the grid-shift records consist of
   * <i>nrows</i> sets of <i>ncols</i> records, with the latitudes going
   * South to North, and longitudes going (backwards) East to West.
   */
  typedef struct gri_file_gs GRI_FILE_GS;
  struct gri_file_gs
  {
    float  f_lat_shift;                 /*!< Lat shift (in gs-units)   */
    float  f_lon_shift;                 /*!< Lon shift (in gs-units)   */
    float  f_lat_accuracy;              /*!< Lat accuracy (in meters)  */
    float  f_lon_accuracy;              /*!< Lon accuracy (in meters)  */
  };

  /*------------------------------------------------------------------------*/
  /* NTv2 internal structs                                                  */
  /*------------------------------------------------------------------------*/

  /*---------------------------------------------------------------------*/

  typedef float    GRI_SHIFT[2];       /*!< Lat/lon shift/accuracy pair */

  /**
   * NTv2 sub-file record
   *
   * This struct describes one sub-grid in the file. There may be many
   * of these, and they may be (logically) nested, although they are
   * all stored in an array in the order they appear in the file.
   */
  typedef struct gri_rec GRI_REC;
  struct gri_rec
  {
    char  record_name[GRI_NAME_LEN + 4]; /*!< Record name               */
    char  parent_name[GRI_NAME_LEN + 4]; /*!< Parent name or "NONE"     */

    GRI_REC* parent;              /*!< Ptr to parent or NULL     */
    GRI_REC* sub;                 /*!< Ptr to first sub-file     */

    GRI_REC* next;                /*!< If this record is a parent,
                                             then this is a ptr to the
                                             next parent. If it is a
                                             child, then this is a ptr
                                             to the next sibling.      */

    GRI_BOOL      active;              /*!< TRUE if this rec is used  */

    int            num_subs;            /*!< Number of child sub-files */
    int            rec_num;             /*!< Record number             */

    int            num;                 /*!< Number of records         */
    int            nrows;               /*!< Number of rows            */
    int            ncols;               /*!< Number of columns         */

    double         lat_min;             /*!< Latitude  min (degrees)   */
    double         lat_max;             /*!< Latitude  max (degrees)   */
    double         lat_inc;             /*!< Latitude  inc (degrees)   */

    double         lon_min;             /*!< Longitude min (degrees)   */
    double         lon_max;             /*!< Longitude max (degrees)   */
    double         lon_inc;             /*!< Longitude inc (degrees)   */

    /* These fields are only used for binary files. */

    long           offset;              /*!< File offset of shifts     */
    int            sskip;               /*!< Bytes to skip South       */
    int            nskip;               /*!< Bytes to skip North       */
    int            wskip;               /*!< Bytes to skip West        */
    int            eskip;               /*!< Bytes to skip East        */

    /* This may be null if data is to be read on-the-fly. */

    GRI_SHIFT* shifts;              /*!< Lat/lon grid-shift array  */

    /* This may be null if not wanted. Always null if shifts is null. */

    GRI_SHIFT* accurs;              /*!< Lat/lon accuracies array  */
  };

  /*---------------------------------------------------------------------*/
  /**
   * NTv2 top-level struct
   *
   * This struct defines the entire NTv2 file as it is stored in memory.
   */
  
  struct gri_hdr
  {
    char           path[GRI_MAX_PATH_LEN];
    /*!< Pathname of file          */
    int            file_type;           /*!< File type (bin or asc)    */

    int            num_recs;            /*!< Number of subfile records */
    int            num_parents;         /*!< Number of parent  records */

    GRI_BOOL      keep_orig;           /*!< TRUE if orig data is kept */
    GRI_BOOL      pads_present;        /*!< TRUE if pads are in file  */
    GRI_BOOL      swap_data;           /*!< TRUE if must swap data    */
    int            fixed;               /*!< Mask of GRI_FIX_* codes  */

    double         hdr_conv;            /*!< Header conversion factor  */
    double         dat_conv;            /*!< Data   conversion factor  */
    char           gs_type[GRI_NAME_LEN + 4];
    /*!< Conversion type
         (e.g. "DEGREES")          */

         /* These are the mins and maxes across all sub-files */

    double         lat_min;             /*!< Latitude  min (degrees)   */
    double         lat_max;             /*!< Latitude  max (degrees)   */
    double         lon_min;             /*!< Longitude min (degrees)   */
    double         lon_max;             /*!< Longitude max (degrees)   */

    GRI_REC* recs;                /*!< Array of ntv2 records     */
    GRI_REC* first_parent;        /*!< Pointer to first parent   */

    /* This will be null if data is in memory. */

    ifstream *fp;                  /*!< Data stream               */

    /* This should be used if mutex control is needed   */
    /* for multi-threaded access to the file when       */
    /* transforming points and reading data on-the-fly. */
    /* This mutex does not need to be recursive.        */

    void* mutex;               /*!< Ptr to OS-specific mutex  */

    /* These may be null if not wanted. */

    GRI_FILE_OV* overview;            /*!< Overview record           */
    GRI_FILE_SF* subfiles;            /*!< Array of sub-file records */
  };
  typedef struct gri_hdr GRI_HDR;
  /*------------------------------------------------------------------------*/
  /* NTv2 error codes                                                       */
  /*------------------------------------------------------------------------*/

#define GRI_ERR_OK                         0

/* generic errors */
#define GRI_ERR_NO_MEMORY                  1
#define GRI_ERR_IOERR                      2
#define GRI_ERR_NULL_HDR                   3

/* warnings */
#define GRI_ERR_FILE_NEEDS_FIXING        101

/* read errors that may be ignored */
#define GRI_ERR_START                    200

#define GRI_ERR_INVALID_LAT_MIN_MAX      201
#define GRI_ERR_INVALID_LON_MIN_MAX      202
#define GRI_ERR_INVALID_LAT_MIN          203
#define GRI_ERR_INVALID_LAT_MAX          204
#define GRI_ERR_INVALID_LAT_INC          205
#define GRI_ERR_INVALID_LON_INC          206
#define GRI_ERR_INVALID_LON_MIN          207
#define GRI_ERR_INVALID_LON_MAX          208

/* unrecoverable errors */
#define GRI_ERR_UNRECOVERABLE_START      300

#define GRI_ERR_INVALID_NUM_OREC         301
#define GRI_ERR_INVALID_NUM_SREC         302
#define GRI_ERR_INVALID_NUM_FILE         303
#define GRI_ERR_INVALID_GS_TYPE          304
#define GRI_ERR_INVALID_GS_COUNT         305
#define GRI_ERR_INVALID_DELTA            306
#define GRI_ERR_INVALID_PARENT_NAME      307
#define GRI_ERR_PARENT_NOT_FOUND         308
#define GRI_ERR_NO_TOP_LEVEL_PARENT      309
#define GRI_ERR_PARENT_LOOP              310
#define GRI_ERR_PARENT_OVERLAP           311
#define GRI_ERR_SUBFILE_OVERLAP          312
#define GRI_ERR_INVALID_EXTENT           313
#define GRI_ERR_HDRS_NOT_READ            314
#define GRI_ERR_UNKNOWN_FILE_TYPE        315
#define GRI_ERR_FILE_NOT_BINARY          316
#define GRI_ERR_FILE_NOT_ASCII           317
#define GRI_ERR_NULL_PATH                318
#define GRI_ERR_ORIG_DATA_NOT_KEPT       319
#define GRI_ERR_DATA_NOT_READ            320
#define GRI_ERR_CANNOT_OPEN_FILE         321
#define GRI_ERR_UNEXPECTED_EOF           322
#define GRI_ERR_INVALID_LINE             323

/* fix header reasons (bit-mask) */

#define GRI_FIX_UNPRINTABLE_CHAR         0x01
#define GRI_FIX_NAME_LOWERCASE           0x02
#define GRI_FIX_NAME_NOT_ALPHA           0x04
#define GRI_FIX_BLANK_PARENT_NAME        0x08
#define GRI_FIX_BLANK_SUBFILE_NAME       0x10
#define GRI_FIX_END_REC_NOT_FOUND        0x20
#define GRI_FIX_END_REC_NAME_NOT_ALPHA   0x40
#define GRI_FIX_END_REC_PAD_NOT_ZERO     0x80

/**
 * Convert an NTV2 error code to a string.
 *
 * <p>Currently, these messages are in English, but this call is
 * designed so a user could implement messages in other languages.
 *
 * @param err_num  The error code to convert.
 * @param msg_buf  Buffer to store error message in.
 *
 * @return A pointer to a error-message string.
 */
  extern const char* gri_errmsg(
    int  err_num,
    char msg_buf[GRI_MAX_ERR_LEN]);

  /*------------------------------------------------------------------------*/
  /* NTv2 file methods                                                      */
  /*------------------------------------------------------------------------*/

  /*---------------------------------------------------------------------*/
  /**
   * Determine whether a filename is for a binary or a text file.
   *
   * <p>This is done solely by checking the filename extension.
   * No examination of the file contents (if any) is done.
   *
   * @param ntv2file The filename to query.
   *
   * @return One of the following:
   *         <ul>
   *           <li>GRI_FILE_TYPE_UNK  file type is unknown
   *           <li>GRI_FILE_TYPE_BIN  file type is binary
   *           <li>GRI_FILE_TYPE_ASC  file type is ascii
   *         </ul>
   */
  extern int gri_filetype(
    const char* ntv2file);

  /*---------------------------------------------------------------------*/
  /**
   * Load an NTv2 file into memory.
   *
   * @param ntv2file     The name of the NTv2 file to load.
   *
   * @param keep_orig    TRUE to keep copies of all external records.
   *                     A TRUE value will also cause accuracy values to be
   *                     read in when reading shift values.
   *
   * @param read_data    TRUE to read in shift (and optionally accuracy) data.
   *                     A TRUE value will also result in closing the file
   *                     after reading, since there is no need to keep it open.
   *
   * @param extent       A pointer to an GRI_EXTENT struct.
   *                     This pointer may be NULL.
   *                     This is ignored for text files.
   *
   * @param prc          A pointer to a result code.
   *                     This pointer may be NULL.
   *                     <ul>
   *                       <li>If successful,   it will be set to GRI_ERR_OK (0).
   *                       <li>If unsuccessful, it will be set to GRI_ERR_*.
   *                     </ul>
   *
   * @return A pointer to an GRI_HDR object or NULL if unsuccessful.
   */
  extern GRI_HDR* gri_load_file(
    const char* ntv2file,
    GRI_BOOL     keep_orig,
    GRI_BOOL     read_data,
    GRI_EXTENT* extent,
    int* prc);

  /*---------------------------------------------------------------------*/
  /**
   * Delete an NTv2 object
   *
   * <p>This method will also close any open stream (and mutex) in the object.
   *
   * @param hdr A pointer to a GRI_HDR object.
   */
  extern void gri_delete(
    GRI_HDR* hdr);

  /*---------------------------------------------------------------------*/

#define GRI_ENDIAN_INP_FILE 0    /*!< Input-file    byte-order */
#define GRI_ENDIAN_BIG      1    /*!< Big-endian    byte-order */
#define GRI_ENDIAN_LITTLE   2    /*!< Little-endian byte-order */
#define GRI_ENDIAN_NATIVE   3    /*!< Native        byte-order */

/**
 * Write out a GRI_HDR object to a file.
 *
 * <p>Rules:
 *    <ul>
 *      <li>A binary file is always written with the zero-pads in it.
 *      <li>Parents are written in the order they appeared in the original file.
 *      <li>Sub-files are written recursively just after their parents.
 *    </ul>
 *
 * <p>One advantage of this routine is to be able to write out a file
 * that has been "cut down" by an extent specification.  Presumably,
 * this could help keep down memory usage in mobile environments.
 *
 * <p>This also provides the ability to rewrite a file in order to
 * cleanup all its header info into "standard" form.  This could be
 * useful if the file will be used by a different program that isn't as
 * tolerant and forgiving as we are.
 *
 * <p>This call can also be used to write out a binary file for an object
 * that was read from a text file, and vice-versa.
 *
 * @param hdr         A pointer to a GRI_HDR object.
 *
 * @param path        The pathname of the file to write.
 *                    This can name either a binary or a text NTv2 file.
 *
 * @param byte_order  Byte order of the output file (GRI_ENDIAN_*) if
 *                    binary.  A value of GRI_ENDIAN_INP_FILE means to
 *                    write the file using the same byte-order as the
 *                    input file if binary or native byte-order if the
 *                    input file was a text file.
 *                    This parameter is ignored when writing text files.
 *
 * @return If successful,   GRI_ERR_OK (0).
 *         If unsuccessful, GRI_ERR_*.
 */
  extern int gri_write_file(
    GRI_HDR* hdr,
    const char* path,
    int         byte_order);

  /*---------------------------------------------------------------------*/
  /**
   * Validate all headers in an NTv2 file.
   *
   * <p>Some unrecoverable errors are found when reading the headers,
   * but this method does an in-depth analysis of all headers and
   * their parent-child relationships.
   *
   * <p>Note that all validation messages are in English, and there is
   * presently no mechanism to localize them.
   *
   * <p>Rules:
   *
   *   <ul>
   *     <li>Each grid is rectangular, and its dimensions must be an integral
   *         number of grid intervals.
   *     <li>Densified grid intervals must be an integral division of its
   *         parent grid intervals.
   *     <li>Densified boundaries must be coincident with its parent grid,
   *         enclosing complete cells.
   *     <li>Densified areas may not overlap each other.
   *   </ul>
   *
   * @param hdr A pointer to a GRI_HDR object.
   *
   * @param fp  A stream to write all messages to.
   *            If NULL, no messages will be written, but validation
   *            will still be done. Note that in this case, only the last
   *            error encountered will be returned, although there may
   *            be numerous errors in the file.
   *
   * @return If successful,   GRI_ERR_OK (0).
   *         If unsuccessful, GRI_ERR_*. This will be the error code
   *         for the last error encountered.
   */
  extern int gri_validate(
    GRI_HDR* hdr,
    FILE* fp);

  /*---------------------------------------------------------------------*/

#define GRI_DUMP_HDRS_EXT     0x01   /*!< Dump hdrs as they are in file   */
#define GRI_DUMP_HDRS_INT     0x02   /*!< Dump hdrs as they are in memory */
#define GRI_DUMP_HDRS_BOTH    0x03   /*!< Dump hdrs both ways             */
#define GRI_DUMP_HDRS_SUMMARY 0x04   /*!< Dump hdr  summaries             */
#define GRI_DUMP_DATA         0x10   /*!< Dump data for shifts            */
#define GRI_DUMP_DATA_ACC     0x30   /*!< Dump data for shifts & accuracy */

/**
 * Dump the contents of all headers in an NTv2 file.
 *
 * @param hdr   A pointer to a GRI_HDR object.
 *
 * @param fp    The stream to dump it to, typically stdout.
 *              If NULL, no dump will be done.
 *
 * @param mode  The dump mode.
 *              This consists of a bit mask of GRI_DUMP_* values.
 */
  extern void gri_dump(
    const GRI_HDR* hdr,
    FILE* fp,
    int             mode);

  /*---------------------------------------------------------------------*/
  /**
   * List the contents of all headers in an NTv2 file.
   *
   * <p>This method dumps all headers in a concise list format.
   *
   * @param hdr          A pointer to a GRI_HDR object.
   *
   * @param fp           The stream to dump it to, typically stdout.
   *                     If NULL, no dump will be done.
   *
   * @param mode         The dump mode.
   *                     This consists of a bit mask of GRI_DUMP_* values.
   *
   * @param do_hdr_line  TRUE to output a header line.
   */
  extern void gri_list(
    const GRI_HDR* hdr,
    FILE* fp,
    int             mode,
    GRI_BOOL       do_hdr_line);

  /*------------------------------------------------------------------------*/
  /* NTv2 transformation methods                                            */
  /*------------------------------------------------------------------------*/

  /*---------------------------------------------------------------------*/

#define GRI_STATUS_NOTFOUND      0  /*!< Point not found in any record   */
#define GRI_STATUS_CONTAINED     1  /*!< Point is totally contained      */
#define GRI_STATUS_NORTH         2  /*!< Point is on North border        */
#define GRI_STATUS_WEST          3  /*!< Point is on West border         */
#define GRI_STATUS_NORTH_WEST    4  /*!< Point is on North & West border */
#define GRI_STATUS_OUTSIDE_CELL  5  /*!< Point is in cell just outside   */

/**
 * Find the best NTv2 sub-file record containing a given point.
 *
 * <p>This is an internal routine, and is exposed for debugging purposes.
 * Normally, users have no need to call this routine.
 *
 * @param hdr         A pointer to a GRI_HDR object.
 * @param lon         Longitude of point (in degrees).
 * @param lat         Latitude  of point (in degrees).
 * @param pstatus     Returned status (GRI_STATUS_*).
 *
 * @return A pointer to a GRI_REC object or NULL.
 */
  extern const GRI_REC* gri_find_rec(
    const GRI_HDR* hdr,
    double         lon,
    double         lat,
    int* pstatus);

  /*---------------------------------------------------------------------*/
  /**
   * Perform a forward transformation on an array of points.
   *
   * @param hdr         A pointer to a GRI_HDR object.
   *
   * @param deg_factor  The conversion factor to convert the given coordinates
   *                    to decimal degrees.
   *                    The value is degrees-per-unit.
   *
   * @param n           Number of points in the array to be transformed.
   *
   * @param coord       An array of GRI_COORD values to be transformed.
   *
   * @return The number of points successfully transformed.
   *
   * <p>Note that the return value is the number of points successfully
   * transformed, and points that can't be transformed (usually because
   * they are outside of the grid) are left unchanged.  However, there
   * is no indication of which points were changed and which were not.
   * If this information is needed, then this routine should be called
   * with one point at a time. The overhead for doing that is minimal.
   */
  extern int gri_forward(
    const GRI_HDR* hdr,
    double          deg_factor,
    int             n,
    GRI_COORD coord[]);

  /*---------------------------------------------------------------------*/
  /**
   * Perform an inverse transformation on an array of points.
   *
   * @param hdr         A pointer to a GRI_HDR object.
   *
   * @param deg_factor  The conversion factor to convert the given coordinates
   *                    to decimal degrees.
   *                    The value is degrees-per-unit.
   *
   * @param n           Number of points in the array to be transformed.
   *
   * @param coord       An array of GRI_COORD values to be transformed.
   *
   * @return The number of points successfully transformed.
   *
   * <p>Note that the return value is the number of points successfully
   * transformed, and points that can't be transformed (usually because
   * they are outside of the grid) are left unchanged.  However, there
   * is no indication of which points were changed and which were not.
   * If this information is needed, then this routine should be called
   * with one point at a time. The overhead for doing that is minimal.
   */
  extern int gri_inverse(
    const GRI_HDR* hdr,
    double          deg_factor,
    int             n,
    GRI_COORD      coord[]);

  /*---------------------------------------------------------------------*/

#define GRI_CVT_FORWARD    1        /*!< Convert data forward  */
#define GRI_CVT_INVERSE    0        /*!< Convert data inverse  */
#define GRI_CVT_REVERSE(n) (1 - n)  /*!< Reverse the direction */

/**
 * Perform a transformation on an array of points.
 *
 * @param hdr         A pointer to a GRI_HDR object.
 *
 * @param deg_factor  The conversion factor to convert the given coordinates
 *                    to decimal degrees.
 *                    The value is degrees-per-unit.
 *
 * @param n           Number of points in the array to be transformed.
 *
 * @param coord       An array of GRI_COORD values to be transformed.
 *
 * @param direction   The direction of the transformation
 *                    (GRI_CVT_FORWARD or GRI_CVT_INVERSE).
 *
 * @return The number of points successfully transformed.
 *
 * <p>Note that the return value is the number of points successfully
 * transformed, and points that can't be transformed (usually because
 * they are outside of the grid) are left unchanged.  However, there
 * is no indication of which points were changed and which were not.
 * If this information is needed, then this routine should be called
 * with one point at a time. The overhead for doing that is minimal.
 *
 * <p>Note also that internally this routine simply calls gri_forward()
 * or gri_inverse().
 */
  extern int gri_transform(
    const GRI_HDR* hdr,
    double          deg_factor,
    int             n,
    GRI_COORD      coord[],
    int             direction);

  /*---------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* LIBGRI_INCLUDED */

/*------------------------------------------------------------------------
 * program options and variables
 */
static const char* pgm = GRI_NULL;
static const char* grifile = GRI_NULL;        /* NTv2 filename */
static const char* datafile = "-";              /* -p file       */
static const char* separator = " ";              /* -s str        */

static GRI_BOOL       direction = GRI_CVT_FORWARD; /* -f | -i       */
static GRI_BOOL       reversed = FALSE;            /* -r            */
static GRI_BOOL       read_on_fly = FALSE;            /* -d            */

static GRI_EXTENT     s_extent;            /* -e ...        */
static GRI_EXTENT* extptr = GRI_NULL;        /* -e ...        */

static double          deg_factor = 1.0;              /* -c factor     */

/*------------------------------------------------------------------------
 * output usage
 */
void display_usage(int level);


/*------------------------------------------------------------------------
 * process all command-line options
 */
int process_options(int argc, const char** argv);


/*------------------------------------------------------------------------
 * strip a string of all leading/trailing whitespace
 */
char* strip(char* str);


/*------------------------------------------------------------------------
 * process a lat/lon value
 */
 void process_lat_lon(
  GRI_HDR* hdr,
  double     &lat,
  double     &lon);


/*------------------------------------------------------------------------
 * process all arguments
 */
 int process_args(
  GRI_HDR* hdr,
  int          optcnt,
  int          argc,
  const char** argv);

/*------------------------------------------------------------------------
 * process a stream of lon/lat values
 */
int process_file(
  GRI_HDR* hdr,
  const char* file);
int main_routine(int argc, const char** argv);
GRI_HDR* gri_create(const char* grifile, int type, int* prc);
double gri_get_shift_from_file(
  GRI_HDR* hdr,
  GRI_REC* rec,
  int              icol,
  int              irow,
  int              coord_type);
