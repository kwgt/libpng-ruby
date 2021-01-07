﻿/*
 * PNG encode/decode library for Ruby
 *
 *  Copyright (C) 2016 Hiroshi Kuwagata <kgt9221@gmail.com>
 */

/*
 * $Id: png.c 67 2016-06-07 06:10:47Z pi $
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>
#include <math.h>

#include <png.h>
#include <zlib.h>

#include "ruby.h"
#include "ruby/version.h"

#define N(x)                        (sizeof(x)/sizeof(*x))

#define RUNTIME_ERROR(msg)          rb_raise(rb_eRuntimeError, (msg))
#define ARGUMENT_ERROR(msg)         rb_raise(rb_eArgError, (msg))
#define TYPE_ERROR(msg)             rb_raise(rb_eTypeError, (msg))
#define NOMEMORY_ERROR(msg)         rb_raise(rb_eNoMemError, (msg))

#define API_SIMPLIFIED              1
#define API_CLASSIC                 2

#define EQ_STR(val,str)             (rb_to_id(val) == rb_intern(str))
#define EQ_INT(val,n)               (FIX2INT(val) == n)

static VALUE module;
static VALUE encoder_klass;
static VALUE decoder_klass;
static VALUE meta_klass;

static ID id_meta;
static ID id_stride;
static ID id_format;
static ID id_pixfmt;
static ID id_ncompo;

typedef struct {
  uint8_t* ptr;
  size_t size;
  int pos;
} mem_io_t;

typedef struct {
  /*
   * for raw level chunk access
   */
	png_structp ctx;   // as 'context'
	png_infop info; 

  png_uint_32 width;
  png_uint_32 height;

  png_byte** rows;

  int stride;
  int num_comp;
  int pixels;
  int with_time;

  int c_type;   // as 'color type'
  int i_meth;   // as 'interlace method'
  int c_level;  // as 'compression level'
  int f_type;   // as 'filter type'

  png_text* text;
  int num_text;

  VALUE ibuf;
  VALUE obuf;

  double gamma;
} png_encoder_t;

typedef union {
  struct {
    int api_type;
    int format;
    int need_meta;
    double display_gamma;
  } common;

  struct {
    /*
     * common field
     */
    int api_type;
    int format;
    int need_meta;
    double display_gamma;

    /*
     * classic api context
     */
    png_structp ctx;   // as 'context'
    png_infop fsi;     // as 'front side infomation'
    png_infop bsi;     // as 'back side infomation'

    png_uint_32 width;
    png_uint_32 height;

    png_byte** rows;

    int depth;
    int c_type;        // as 'color type'
    int i_meth;        // as 'interlace method'
    int c_meth;        // as 'compression method'
    int f_meth;        // as 'filter method'

    mem_io_t io;

    png_text* text;
    int num_text;
    png_time* time;
    double file_gamma;
  } classic;

  struct {
    /*
     * common field
     */
    int api_type;
    int format;
    int need_meta;
    double display_gamma;

    /*
     * simplified api context
     */
    png_image* ctx;
  } simplified;
} png_decoder_t;

static const char* decoder_opt_keys[] ={
  "pixel_format",    // alias of "color_type"
  "without_meta",    // bool (default: false)
  "api_type",        // string ("simplified" or "classic")
  "display_gamma",   // float
};

static ID decoder_opt_ids[N(decoder_opt_keys)];

static const char* encoder_opt_keys[] ={
  "pixel_format",    // alias of "color_type"
  "interlace",       // bool (default: false)
  "compression",     // int 0~9 
  "text",            // hash<String,String>
  "time",            // bool (default: true)
  "gamma",           // float
};

static ID encoder_opt_ids[N(encoder_opt_keys)];

static void
rb_encoder_mark(void* _ptr)
{
  png_encoder_t* ptr;

  ptr = (png_encoder_t*)_ptr;

  if (ptr->ibuf != Qnil) {
    rb_gc_mark(ptr->ibuf);
  }

  if (ptr->obuf != Qnil) {
    rb_gc_mark(ptr->obuf);
  }
}

static void
rb_encoder_free(void* _ptr)
{
  png_encoder_t* ptr;
  int i;

  ptr = (png_encoder_t*)_ptr;

  if (ptr->ctx != NULL) {
    if (ptr->rows != NULL) {
      png_free(ptr->ctx, ptr->rows);
    }

    png_destroy_write_struct(&ptr->ctx, &ptr->info);
  }

  if (ptr->text != NULL) {
    for (i = 0; i < ptr->num_text; i++) {
      if (ptr->text[i].key != NULL) {
        xfree(ptr->text[i].key);
      }

      if (ptr->text[i].text != NULL) {
        xfree(ptr->text[i].text);
      }
    }
  }

  free(ptr);
}

static size_t
rb_encoder_size(const void* _ptr)
{
  size_t ret;
  png_encoder_t* ptr;
  int i;

  ptr = (png_encoder_t*)_ptr;

  ret  = sizeof(png_encoder_t);
  // ret += sizeof(png_struct);
  // ret += sizeof(png_info);
  ret += (sizeof(png_byte*) * ptr->width);

  ret += sizeof(png_text) * ptr->num_text;

  for (i = 0; i < ptr->num_text; i++) {
    ret += (strlen(ptr->text[i].key) + ptr->text[i].text_length);
  }

  return ret;
}

#if RUBY_API_VERSION_CODE > 20600
static const rb_data_type_t png_encoder_data_type = {
  "libpng-ruby encoder object",     // wrap_struct_name
  {
    rb_encoder_mark,                 // function.dmark
    rb_encoder_free,                 // function.dfree
    rb_encoder_size,                 // function.dsize
    NULL,                            // function.dcompact
    {NULL},                          // function.reserved
  },
  NULL,                              // parent
  NULL,                              // data
  (VALUE)RUBY_TYPED_FREE_IMMEDIATELY // flags
};
#else /* RUBY_API_VERSION_CODE > 20600 */
static const rb_data_type_t png_encoder_data_type = {
  "libpng-ruby encoder object",     // wrap_struct_name
  {
    rb_encoder_mark,                 // function.dmark
    rb_encoder_free,                 // function.dfree
    rb_encoder_size,                 // function.dsize
    {NULL, NULL},                    // function.reserved
  },
  NULL,                              // parent
  NULL,                              // data
  (VALUE)RUBY_TYPED_FREE_IMMEDIATELY // flags
};
#endif /* RUBY_API_VERSION_CODE > 20600 */

static VALUE
create_runtime_error(const char* fmt, ...)
{
  VALUE ret;
  va_list ap;

  va_start(ap, fmt);
  ret = rb_exc_new_str(rb_eRuntimeError, rb_vsprintf(fmt, ap));
  va_end(ap);

  return ret;
}

static void
mem_io_write_data(png_structp ctx, png_bytep src, png_size_t size)
{
  VALUE buf;

  buf = (VALUE)png_get_io_ptr(ctx);
  rb_str_buf_cat(buf, (const char *)src, size);
}

static void
mem_io_flush(png_structp ctx)
{
  // ignore
}

static VALUE
rb_encoder_alloc(VALUE self)
{
  png_encoder_t* ptr;

  ptr = ALLOC(png_encoder_t);
  memset(ptr, 0, sizeof(*ptr));

  ptr->c_type    = PNG_COLOR_TYPE_RGB;
  ptr->i_meth    = PNG_INTERLACE_NONE;
  ptr->c_level   = Z_DEFAULT_COMPRESSION;
  ptr->f_type    = PNG_FILTER_TYPE_BASE;
  ptr->num_comp  = 3;
  ptr->with_time = !0;
  ptr->gamma     = NAN;

  return TypedData_Wrap_Struct(encoder_klass, &png_encoder_data_type, ptr);
}

static void
eval_encoder_opt_color_type(png_encoder_t* ptr, VALUE opt)
{
  if (opt != Qundef) {
    if (EQ_STR(opt, "GRAY") || EQ_STR(opt, "GRAYSCALE")) {
      ptr->c_type   = PNG_COLOR_TYPE_GRAY;
      ptr->num_comp = 1;

    } else if (EQ_STR(opt, "GA")) {
      ptr->c_type   = PNG_COLOR_TYPE_GA;
      ptr->num_comp = 2;

    } else if (EQ_STR(opt, "RGB")) {
      ptr->c_type   = PNG_COLOR_TYPE_RGB;
      ptr->num_comp = 3;

    } else if (EQ_STR(opt, "RGBA")) {
      ptr->c_type   = PNG_COLOR_TYPE_RGBA;
      ptr->num_comp = 4;

    } else {
      ARGUMENT_ERROR(":color_type is invalid value");
    } 
  }
}

static void
eval_encoder_opt_interlace(png_encoder_t* ptr, VALUE opt)
{
  if (opt != Qundef) {
    ptr->i_meth = (RTEST(opt))? PNG_INTERLACE_ADAM7: PNG_INTERLACE_NONE;
  }
}

static void
eval_encoder_opt_compression(png_encoder_t* ptr, VALUE opt)
{
  int val;

  if (opt != Qundef) {
    switch (TYPE(opt)) {
    case T_FIXNUM:
      val = FIX2INT(opt);
      if (val < 0 || val > 9) {
        ARGUMENT_ERROR(":compress is out of range");
      }
      break;

    case T_STRING:
    case T_SYMBOL:
      if (EQ_STR(opt, "NO_COMPRESSION")) {
        val = Z_NO_COMPRESSION;

      } else if (EQ_STR(opt, "BEST_SPEED")) {
        val = Z_BEST_SPEED;

      } else if (EQ_STR(opt, "BEST_COMPRESSION")) {
        val = Z_BEST_COMPRESSION;

      } else if (EQ_STR(opt, "DEFAULT")) {
        val = Z_DEFAULT_COMPRESSION;

      } else {
        ARGUMENT_ERROR(":interlace is invalid value");
      }
      break;

    default:
      TYPE_ERROR(":interlace is not Symbol and String");
    }

    ptr->c_level = val;
  }
}

static VALUE
capitalize(VALUE str)
{
  VALUE tmp;
  int i;

  tmp = rb_str_split(rb_funcall(str, rb_intern("to_s"), 0), "_");

  for (i = 0; i < RARRAY_LEN(tmp); i++) {
    rb_funcall(RARRAY_AREF(tmp, i), rb_intern("capitalize!"), 0);
  }

  return rb_ary_join(tmp, rb_str_new_cstr(" "));
}

static char*
clone_cstr(VALUE s)
{
  char* ret;
  size_t sz;

  sz  = RSTRING_LEN(s);
  ret = (char*)malloc(sz + 1);

  memcpy(ret, RSTRING_PTR(s), sz);
  ret[sz] = '\0';

  return ret;
}


static void
eval_encoder_opt_text(png_encoder_t* ptr, VALUE opt)
{
  VALUE keys;
  VALUE key;
  VALUE val;
  int i;
  png_text* text;

  text = NULL;

  if (opt != Qundef) {
    if (TYPE(opt) == T_HASH && RHASH_SIZE(opt) > 0) {
      keys = rb_funcall(opt, rb_intern("keys"), 0);
      text = (png_text*)xmalloc(sizeof(png_text) * RHASH_SIZE(opt));

      /*
       * 途中で例外が発生する可能性があるので rb_encoder_free()で
       * 資源回収できる様に 0クリアとコンテキスト登録を最初に済ま
       * せておく。
       *
       * なお、この処理はinitialize()の過程で呼び出される。このた
       * め例外が発生しコンテキストの状態が中途半端な状態でユーザ
       * から参照されることはない。
       */
      memset(text, 0, sizeof(*text));
      ptr->text     = text;
      ptr->num_text = RARRAY_LEN(keys);

      for (i = 0; i < RARRAY_LEN(keys); i++) {
        key  = RARRAY_AREF(keys, i);
        val  = rb_hash_aref(opt, key);

        if (TYPE(key) != T_STRING && TYPE(key) != T_SYMBOL) {
          ARGUMENT_ERROR(":type is invalid structure");
        }

        if (TYPE(val) != T_STRING && TYPE(val) != T_SYMBOL) {
          ARGUMENT_ERROR(":type is invalid structure");
        }

        key = capitalize(key);
        if (RSTRING_LEN(key) >= 0 && RSTRING_LEN(key) <= 79) {
          text[i].compression = PNG_TEXT_COMPRESSION_NONE;
          text[i].key         = clone_cstr(key);
          text[i].text        = clone_cstr(val);
          text[i].text_length = RSTRING_LEN(val);

        } else {
          ARGUMENT_ERROR("keyword in :text is too long");
        }
      }
    } else {
      ARGUMENT_ERROR(":text is invalid value");
    }
  }
}

static void
eval_encoder_opt_time(png_encoder_t* ptr, VALUE opt)
{
  if (opt != Qundef) {
    ptr->with_time = RTEST(opt);
  }
}

static void
eval_encoder_opt_gamma(png_encoder_t* ptr, VALUE opt)
{
  if (opt != Qundef) {
    ptr->gamma = NUM2DBL(opt);
  }
}

static void
set_encoder_context(png_encoder_t* ptr, int wd, int ht, VALUE opt)
{
  png_structp ctx;
  png_infop info;
  VALUE opts[N(encoder_opt_ids)];

  const char* err;
  png_byte** rows;

  /*
   * parse options
   */
  rb_get_kwargs(opt, encoder_opt_ids, 0, N(encoder_opt_ids), opts);

  /*
   * set context
   */
  eval_encoder_opt_color_type(ptr, opts[0]);
  eval_encoder_opt_interlace(ptr, opts[1]);
  eval_encoder_opt_compression(ptr, opts[2]);
  eval_encoder_opt_text(ptr, opts[3]);
  eval_encoder_opt_time(ptr, opts[4]);
  eval_encoder_opt_gamma(ptr, opts[5]);

  /*
   * create PNG context
   */
  do {
    err  = NULL;
    rows = NULL;
    info = NULL;

    ctx = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (ctx == NULL) {
      err = "png_create_read_struct() failed.";
      break;
    }

    info = png_create_info_struct(ctx);
    if (info == NULL) {
      err = "png_create_info_structt() failed.";
      break;
    }

    rows = png_malloc(ctx, ht * sizeof(png_byte*));
    if (rows == NULL) {
      err = "png_malloc() failed.";
      break;
    }

    memset(rows, 0, ht * sizeof(png_byte*));

    ptr->ctx    = ctx;
    ptr->info   = info;
    ptr->width  = wd;
    ptr->height = ht;
    ptr->stride = ptr->width * ptr->num_comp;
    ptr->pixels = ptr->stride * ptr->height;
    ptr->rows   = rows;
    ptr->ibuf   = Qnil;
    ptr->obuf   = Qnil;
  } while(0);

  /*
   * post process
   */
  if (err != NULL) {
    if (ctx != NULL) {
      png_destroy_write_struct(&ctx, &info);
    }

    if (rows != NULL) {
      png_free(ctx, rows);
    }
  }
}

static VALUE
rb_encoder_initialize(int argc, VALUE* argv, VALUE self)
{
  png_encoder_t* ptr;
  VALUE wd;
  VALUE ht;
  VALUE opts;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, png_encoder_t, &png_encoder_data_type, ptr);

  /*
   * parse argument
   */
  rb_scan_args(argc, argv, "21", &wd, &ht, &opts);

  Check_Type(wd, T_FIXNUM);
  Check_Type(ht, T_FIXNUM);
  if (opts != Qnil) Check_Type(opts, T_HASH);


  /*
   * set context
   */
  set_encoder_context(ptr, FIX2INT(wd), FIX2INT(ht), opts);

  return self;
}

static VALUE
rb_encoder_encode(VALUE self, VALUE data)
{
  VALUE ret;
  VALUE exc;
  png_encoder_t* ptr;
  png_uint_32 i;
  uint8_t* p;

  /*
   * initialize
   */
  ret = rb_str_buf_new(0);
  exc = Qnil;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, png_encoder_t, &png_encoder_data_type, ptr);

  /*
   * argument check
   */
  Check_Type(data, T_STRING);
  if (RSTRING_LEN(data) != ptr->pixels) {
    ARGUMENT_ERROR("invalid data size");
  }


  /*
   * prepare
   */
  ptr->ibuf = data;
  ptr->obuf = ret;

  /*
   * call libpng
   */
  png_set_IHDR(ptr->ctx,
               ptr->info,
               ptr->width,
               ptr->height,
               8,
               ptr->c_type,
               ptr->i_meth,
               PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);

  if (ptr->text) {
    png_set_text(ptr->ctx, ptr->info, ptr->text, ptr->num_text);
  }

  if (ptr->with_time) {
    time_t tm;
    png_time png_time;

    time(&tm);
    png_convert_from_time_t(&png_time, tm);
    png_set_tIME(ptr->ctx, ptr->info, &png_time);
  }

  if (!isnan(ptr->gamma)) {
    png_set_gAMA(ptr->ctx, ptr->info, ptr->gamma);
  }

  png_set_compression_level(ptr->ctx, ptr->c_level);

  png_set_write_fn(ptr->ctx,
                   (png_voidp)ret,
                   (png_rw_ptr)mem_io_write_data,
                   (png_flush_ptr)mem_io_flush);

  p = (png_byte*)RSTRING_PTR(data);
  for (i = 0; i < ptr->height; i++) {
    ptr->rows[i] = p;
    p += ptr->stride;
  }

  png_set_rows(ptr->ctx, ptr->info, ptr->rows);

  if (setjmp(png_jmpbuf(ptr->ctx))) {
    exc = create_runtime_error("encode error");

  } else {
    png_write_png(ptr->ctx, ptr->info, PNG_TRANSFORM_IDENTITY, NULL);
  }

  /*
   * post process
   */
  ptr->ibuf = Qnil;
  ptr->obuf = Qnil;
  
  if (RTEST(exc)) rb_exc_raise(exc);

  return ret;
}

static void
mem_io_read_data(png_structp ctx, png_bytep dst, png_size_t rq_size)
{
  mem_io_t* io;

  io = (mem_io_t*)png_get_io_ptr(ctx);

  if (io->pos + rq_size <= io->size) {
    memcpy(dst, io->ptr + io->pos, rq_size);
    io->pos += rq_size;

  } else {
    png_error(ctx, "data not enough.");
  }
}

static void
rb_decoder_mark(void* _ptr)
{
  // nothing
}

static void
rb_decoder_free(void* _ptr)
{
  png_decoder_t* ptr;

  ptr = (png_decoder_t*)_ptr;

  if (ptr->common.api_type == API_SIMPLIFIED) {
    if (ptr->simplified.ctx) {
      png_image_free(ptr->simplified.ctx);
      xfree(ptr->simplified.ctx);
    }

  } else {
   if (ptr->classic.ctx != NULL) {
      if (ptr->classic.rows != NULL) {
        png_free(ptr->classic.ctx, ptr->classic.rows);
      }

      png_destroy_read_struct(&ptr->classic.ctx,
                              &ptr->classic.fsi,
                              &ptr->classic.bsi);
    }
  }

  free(ptr);
}

static size_t
rb_decoder_size(const void* _ptr)
{
  size_t ret;

  ret = sizeof(png_decoder_t);

  return ret;
}

#if RUBY_API_VERSION_CODE > 20600
static const rb_data_type_t png_decoder_data_type = {
  "libpng-ruby decoder object",      // wrap_struct_name
  {
    rb_decoder_mark,                 // function.dmark
    rb_decoder_free,                 // function.dfree
    rb_decoder_size,                 // function.dsize
    NULL,                            // function.dcompact
    {NULL},                          // function.reserved
  },
  NULL,                              // parent
  NULL,                              // data
  (VALUE)RUBY_TYPED_FREE_IMMEDIATELY // flags
};
#else /* RUBY_API_VERSION_CODE > 20600 */
static const rb_data_type_t png_decoder_data_type = {
  "libpng-ruby decoder object",      // wrap_struct_name
  {
    rb_decoder_mark,                 // function.dmark
    rb_decoder_free,                 // function.dfree
    rb_decoder_size,                 // function.dsize
    {NULL, NULL},                    // function.reserved
  },
  NULL,                              // parent
  NULL,                              // data
  (VALUE)RUBY_TYPED_FREE_IMMEDIATELY // flags
};
#endif /* RUBY_API_VERSION_CODE > 20600 */

static VALUE
rb_decoder_alloc(VALUE self)
{
  png_decoder_t* ptr;

  ptr = ALLOC(png_decoder_t);
  memset(ptr, 0, sizeof(*ptr));

  ptr->common.api_type      = API_SIMPLIFIED;
  ptr->common.format        = PNG_FORMAT_RGB;
  ptr->common.need_meta     = !0;
  ptr->common.display_gamma = NAN;

  return TypedData_Wrap_Struct(decoder_klass, &png_decoder_data_type, ptr);
}

static void
eval_decoder_opt_api_type(png_decoder_t* ptr, VALUE opt)
{
  int api_type;

  if (opt != Qundef) {
    if (EQ_STR(opt, "simplified")) {
      api_type = API_SIMPLIFIED;

    } else if (EQ_STR(opt, "classic")) {
      api_type = API_CLASSIC;

    } else {
      ARGUMENT_ERROR(":api_type is invalid value");
    }

    ptr->common.api_type = api_type;
  }
}

static void
eval_decoder_opt_pixel_format(png_decoder_t* ptr, VALUE opt)
{
  int format;

  if (opt != Qundef) {
    if (EQ_STR(opt, "GRAY") || EQ_STR(opt, "GRAYSCALE")) {
      format = PNG_FORMAT_GRAY;

    } else if (EQ_STR(opt, "GA")) {
      format = PNG_FORMAT_GA;

    } else if (EQ_STR(opt, "AG")) {
      format = PNG_FORMAT_AG;

    } else if (EQ_STR(opt, "RGB")) {
      format = PNG_FORMAT_RGB;

    } else if (EQ_STR(opt, "BGR")) {
      format = PNG_FORMAT_BGR;

    } else if (EQ_STR(opt, "RGBA")) {
      format = PNG_FORMAT_RGBA;

    } else if (EQ_STR(opt, "ARGB")) {
      format = PNG_FORMAT_ARGB;

    } else if (EQ_STR(opt, "BGRA")) {
      format = PNG_FORMAT_BGRA;

    } else if (EQ_STR(opt, "ABGR")) {
      format = PNG_FORMAT_ABGR;

    } else {
      ARGUMENT_ERROR(":color_type is invalid value");
    }

    ptr->common.format = format;
  }
}

static void
eval_decoder_opt_without_meta(png_decoder_t* ptr, VALUE opt)
{
  if (opt != Qundef) {
    ptr->common.need_meta = !RTEST(opt);
  }
}

static void
eval_decoder_opt_display_gamma(png_decoder_t* ptr, VALUE opt)
{
  if (opt != Qundef) {
    ptr->common.display_gamma = NUM2DBL(opt);
  }
}

static void
set_decoder_context(png_decoder_t* ptr, VALUE opt)
{
  VALUE opts[N(decoder_opt_ids)];

  /*
   * parse options
   */
  rb_get_kwargs(opt, decoder_opt_ids, 0, N(decoder_opt_ids), opts);

  /*
   * set context
   */
  eval_decoder_opt_api_type(ptr, opts[2]);
  eval_decoder_opt_pixel_format(ptr, opts[0]);
  eval_decoder_opt_without_meta(ptr, opts[1]);
  eval_decoder_opt_display_gamma(ptr, opts[3]);
}

static VALUE
rb_decoder_initialize(int argc, VALUE* argv, VALUE self)
{
  png_decoder_t* ptr;
  VALUE opt;

  /*
   * strip object
   */
  TypedData_Get_Struct(self, png_decoder_t, &png_decoder_data_type, ptr);

  /*
   * parse argument
   */
  rb_scan_args(argc, argv, "01", &opt);
  if (opt != Qnil) Check_Type(opt, T_HASH);

  /*
   * set context
   */
  set_decoder_context(ptr, opt);

  return Qtrue;
}

static void
set_read_context(png_decoder_t* ptr, VALUE data)
{
  png_structp ctx;
  png_infop fsi;
  png_infop bsi;

  const char* err;

  do {
    err = NULL;
    fsi = NULL;
    bsi = NULL;

    ctx = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (ctx == NULL) {
      err = "png_create_read_struct() failed.";
      break;
    }

    fsi = png_create_info_struct(ctx);
    bsi = png_create_info_struct(ctx);
    if (fsi == NULL || bsi == NULL) {
      err = "png_create_info_struct() failed.";
      break;
    }

    ptr->classic.ctx     = ctx;
    ptr->classic.fsi     = fsi;
    ptr->classic.bsi     = bsi;

    ptr->classic.io.ptr  = (uint8_t*)RSTRING_PTR(data);
    ptr->classic.io.size = RSTRING_LEN(data);
    ptr->classic.io.pos  = 0;

    if (setjmp(png_jmpbuf(ptr->classic.ctx))) {
      RUNTIME_ERROR("decode error");

    } else {
      png_set_read_fn(ptr->classic.ctx,
                      (png_voidp)&ptr->classic.io,
                      (png_rw_ptr)mem_io_read_data);
    }
  } while(0);

  if (err != NULL) {
    if (ctx != NULL) {
      png_destroy_read_struct(&ctx, &fsi, &bsi);
    }

    RUNTIME_ERROR(err);
  }
}

static void
get_header_info(png_decoder_t* ptr)
{
  png_get_IHDR(ptr->classic.ctx,
               ptr->classic.fsi,
               &ptr->classic.width,
               &ptr->classic.height,
               &ptr->classic.depth,
               &ptr->classic.c_type,
               &ptr->classic.i_meth,
               &ptr->classic.c_meth,
               &ptr->classic.f_meth); 

  png_get_tIME(ptr->classic.ctx, ptr->classic.fsi, &ptr->classic.time);

  png_get_text(ptr->classic.ctx,
               ptr->classic.fsi,
               &ptr->classic.text,
               &ptr->classic.num_text);

  if (!png_get_gAMA(ptr->classic.ctx,
                    ptr->classic.fsi, &ptr->classic.file_gamma)) {
    ptr->classic.file_gamma = NAN;
  }
}

static VALUE
get_color_type_str(png_decoder_t* ptr)
{
  const char* cstr;
  char tmp[32];

  switch (ptr->classic.c_type) {
  case PNG_COLOR_TYPE_GRAY:
    cstr = "GRAY";
    break;

  case PNG_COLOR_TYPE_PALETTE:
    cstr = "PALETTE";
    break;

  case PNG_COLOR_TYPE_RGB:
    cstr = "RGB";
    break;

  case PNG_COLOR_TYPE_RGBA:
    cstr = "RGBA";
    break;

  case PNG_COLOR_TYPE_GRAY_ALPHA:
    cstr = "GA";
    break;

  default:
    sprintf(tmp, "UNKNOWN(%d)", ptr->classic.c_type);
    cstr = tmp;
    break;
  }

  return rb_str_new_cstr(cstr);
}

static VALUE
get_interlace_method_str(png_decoder_t* ptr)
{
  const char* cstr;
  char tmp[32];

  switch (ptr->classic.i_meth) {
  case PNG_INTERLACE_NONE:
    cstr = "NONE";
    break;

  case PNG_INTERLACE_ADAM7:
    cstr = "ADAM7";
    break;

  default:
    sprintf(tmp, "UNKNOWN(%d)", ptr->classic.i_meth);
    cstr = tmp;
    break;
  }

  return rb_str_new_cstr(cstr);
}

static VALUE
get_compression_method_str(png_decoder_t* ptr)
{
  const char* cstr;
  char tmp[32];

  switch (ptr->classic.c_meth) {
  case PNG_COMPRESSION_TYPE_BASE:
    cstr = "BASE";
    break;

  default:
    sprintf(tmp, "UNKNOWN(%d)", ptr->classic.c_meth);
    cstr = tmp;
    break;
  }

  return rb_str_new_cstr(cstr);
}

static VALUE
get_filter_method_str(png_decoder_t* ptr)
{
  const char* cstr;
  char tmp[32];

  switch (ptr->classic.f_meth) {
  case PNG_FILTER_TYPE_BASE:
    cstr = "BASE";
    break;

  case PNG_INTRAPIXEL_DIFFERENCING:
    cstr = "INTRAPIXEL_DIFFERENCING";
    break;

  default:
    sprintf(tmp, "UNKNOWN(%d)", ptr->classic.f_meth);
    cstr = tmp;
    break;
  }

  return rb_str_new_cstr(cstr);
}

static VALUE
create_text_meta(png_decoder_t* ptr)
{
  VALUE ret;
  VALUE key;
  VALUE val;

  int i;

  ret = rb_hash_new();

  for (i = 0; i < ptr->classic.num_text; i++) {
    key = rb_str_new2(ptr->classic.text[i].key);
    val = rb_str_new(ptr->classic.text[i].text,
                     ptr->classic.text[i].text_length);

    rb_funcall(key, rb_intern("downcase!"), 0);
    rb_funcall(key, rb_intern("gsub!"), 2, rb_str_new2(" "), rb_str_new2("_"));

    rb_hash_aset(ret, rb_to_symbol(key), val);
  }

  return ret;
}

static VALUE
create_time_meta(png_decoder_t* ptr)
{
  VALUE ret;

  ret = rb_funcall(rb_cTime,
                   rb_intern("utc"),
                   6,
                   INT2FIX(ptr->classic.time->year),
                   INT2FIX(ptr->classic.time->month),
                   INT2FIX(ptr->classic.time->day),
                   INT2FIX(ptr->classic.time->hour),
                   INT2FIX(ptr->classic.time->minute),
                   INT2FIX(ptr->classic.time->second));

  rb_funcall(ret, rb_intern("localtime"), 0);

  return ret;
}

static VALUE
create_meta(png_decoder_t* ptr)
{
  VALUE ret;

  ret = rb_obj_alloc(meta_klass);

  rb_ivar_set(ret, rb_intern("@width"), INT2FIX(ptr->classic.width));
  rb_ivar_set(ret, rb_intern("@height"), INT2FIX(ptr->classic.height));
  rb_ivar_set(ret, rb_intern("@bit_depth"), INT2FIX(ptr->classic.depth));
  rb_ivar_set(ret, rb_intern("@color_type"), get_color_type_str(ptr));

  rb_ivar_set(ret, rb_intern("@interlace_method"),
              get_interlace_method_str(ptr));

  rb_ivar_set(ret, rb_intern("@compression_method"),
              get_compression_method_str(ptr));

  rb_ivar_set(ret, rb_intern("@filter_method"), get_filter_method_str(ptr));

  if (ptr->classic.text) {
    rb_ivar_set(ret, rb_intern("@text"), create_text_meta(ptr));
  }

  if (ptr->classic.time) {
    rb_ivar_set(ret, rb_intern("@time"), create_time_meta(ptr));
  }

  if (!isnan(ptr->classic.file_gamma)) {
    rb_ivar_set(ret,
                rb_intern("@file_gamma"),
                DBL2NUM(ptr->classic.file_gamma));
  }

  return ret;
}

static VALUE
create_tiny_meta(png_decoder_t* ptr)
{
  VALUE ret;
  const char* fmt;
  int nc;

  ret = rb_obj_alloc(meta_klass);

  rb_ivar_set(ret, rb_intern("@width"),
              INT2FIX(ptr->simplified.ctx->width));

  rb_ivar_set(ret, id_stride,
              INT2FIX(PNG_IMAGE_ROW_STRIDE(*ptr->simplified.ctx)));

  rb_ivar_set(ret, rb_intern("@height"),
              INT2FIX(ptr->simplified.ctx->height));

  switch (ptr->common.format) {
  case PNG_FORMAT_GRAY:
    fmt = "GRAY";
    nc  = 1;
    break;

  case PNG_FORMAT_GA:
    fmt = "GA";
    nc  = 2;
    break;

  case PNG_FORMAT_AG:
    fmt = "AG";
    nc  = 2;
    break;

  case PNG_FORMAT_RGB:
    fmt = "RGB";
    nc  = 3;
    break;

  case PNG_FORMAT_BGR:
    fmt = "BGR";
    nc  = 3;
    break;

  case PNG_FORMAT_RGBA:
    fmt = "RGBA";
    nc  = 4;
    break;

  case PNG_FORMAT_ARGB:
    fmt = "ARGB";
    nc  = 4;
    break;

  case PNG_FORMAT_BGRA:
    fmt = "BGRA";
    nc  = 4;
    break;

  case PNG_FORMAT_ABGR:
    fmt = "ABGR";
    nc  = 4;
    break;

  default:
    fmt = "unknown";
    nc  = 0;
    break;
  }

  rb_ivar_set(ret, id_pixfmt, rb_str_new_cstr(fmt));
  rb_ivar_set(ret, id_ncompo, INT2FIX(nc));

  return ret;
}

static void
clear_read_context(png_decoder_t* ptr)
{
  if (ptr->classic.ctx != NULL) {
    png_destroy_read_struct(&ptr->classic.ctx,
                            &ptr->classic.fsi,
                            &ptr->classic.bsi);

    ptr->classic.ctx  = NULL;
    ptr->classic.fsi  = NULL;
    ptr->classic.bsi  = NULL;

    ptr->classic.text = NULL;
    ptr->classic.time = NULL;
  }

  ptr->classic.io.ptr  = NULL;
  ptr->classic.io.size = 0;
  ptr->classic.io.pos  = 0;
}

typedef struct {
  png_decoder_t* ptr;
  VALUE data;
} read_header_arg_t ;

static VALUE
read_header_body(VALUE _arg)
{
  read_header_arg_t* arg;
  png_decoder_t* ptr;
  VALUE data;

  /*
   * initialize
   */
  arg  = (read_header_arg_t*)_arg;
  ptr  = arg->ptr;
  data = arg->data;

  /*
   * set context
   */
  set_read_context(ptr, data);

  /*
   * read header
   */
  png_read_info(ptr->classic.ctx, ptr->classic.fsi);
  get_header_info(ptr);

  return create_meta(ptr);
}

static VALUE
read_header_ensure(VALUE _arg)
{
  clear_read_context((png_decoder_t*)_arg);

  return Qundef;
}

static VALUE
rb_decoder_read_header(VALUE self, VALUE data)
{
  VALUE ret;
  png_decoder_t* ptr;
  read_header_arg_t arg;

  /*
   * argument check
   */
  Check_Type(data, T_STRING);

  if (RSTRING_LEN(data) < 8) {
    RUNTIME_ERROR("data too short.");
  }

  if (png_sig_cmp((png_const_bytep)RSTRING_PTR(data), 0, 8)) {
    RUNTIME_ERROR("Invalid PNG signature.");
  }

  /*
   * strip object
   */
  TypedData_Get_Struct(self, png_decoder_t, &png_decoder_data_type, ptr);

  /*
   * call read header funciton
   */
  arg.ptr  = ptr;
  arg.data = data;

  ret = rb_ensure(read_header_body, (VALUE)&arg,
                  read_header_ensure, (VALUE)ptr);

  return ret;
}

static VALUE
rb_decode_result_meta(VALUE self)
{
  return rb_ivar_get(self, id_meta);
}

typedef struct {
  png_decoder_t* ptr;
  VALUE data;
} decode_arg_t;

static VALUE
decode_simplified_api_body(VALUE _arg)
{
  VALUE ret;

  decode_arg_t* arg;
  png_decoder_t* ptr;
  VALUE data;

  size_t stride;
  size_t size;

  /*
   * initialize
   */
  arg  = (decode_arg_t*)_arg;
  ptr  = arg->ptr;
  data = arg->data;

  /*
   * call simplified API
   */
  do {
    ptr->simplified.ctx = (png_imagep)xmalloc(sizeof(png_image));
    memset(ptr->simplified.ctx, 0, sizeof(png_image));

    ptr->simplified.ctx->version = PNG_IMAGE_VERSION;

    png_image_begin_read_from_memory(ptr->simplified.ctx,
                                     RSTRING_PTR(data), RSTRING_LEN(data));
    if (PNG_IMAGE_FAILED(*ptr->simplified.ctx)) {
      RUNTIME_ERROR("png_image_begin_read_from_memory() failed");
    }

    ptr->simplified.ctx->format = ptr->common.format;

    stride = PNG_IMAGE_ROW_STRIDE(*ptr->simplified.ctx);
    size   = PNG_IMAGE_BUFFER_SIZE(*ptr->simplified.ctx, stride);
    ret    = rb_str_buf_new(size);
    rb_str_set_len(ret, size);

    png_image_finish_read(ptr->simplified.ctx,
                          NULL, RSTRING_PTR(ret), stride, NULL);
    if (PNG_IMAGE_FAILED(*ptr->simplified.ctx)) {
      RUNTIME_ERROR("png_image_finish_read() failed");
    }

    if (ptr->common.need_meta) {
      rb_ivar_set(ret, id_meta, create_tiny_meta(ptr));
      rb_define_singleton_method(ret, "meta", rb_decode_result_meta, 0);
    }
  } while(0);

  return ret;
}

static VALUE
decode_simplified_api_ensure(VALUE _arg)
{
  png_decoder_t* ptr;

  ptr = (png_decoder_t*)_arg;

  if (ptr->simplified.ctx) {
    png_image_free(ptr->simplified.ctx);
    xfree(ptr->simplified.ctx);
  }

  ptr->simplified.ctx = NULL;

  return Qundef;
}

static VALUE
decode_classic_api_body(VALUE _arg)
{
  VALUE ret;

  decode_arg_t* arg;
  png_decoder_t* ptr;
  VALUE data;

  size_t stride;

  png_uint_32 i;
  png_byte* p;

  double file_gamma;

  /*
   * initialize
   */
  arg  = (decode_arg_t*)_arg;
  ptr  = arg->ptr;
  data = arg->data;

  /*
   * set context
   */
  set_read_context(ptr, data);

  /*
   * read basic info
   */
  png_read_info(ptr->classic.ctx, ptr->classic.fsi);

  /*
   * gamma correction
   */
  if (!isnan(ptr->common.display_gamma)) {
    if (!png_get_gAMA(ptr->classic.ctx, ptr->classic.fsi, &file_gamma)) {
      file_gamma = 0.45;
    }

    png_set_gamma(ptr->classic.ctx, ptr->common.display_gamma, file_gamma);
  }

  png_read_update_info(ptr->classic.ctx, ptr->classic.fsi);

  /*
   * get image size
   */

  ptr->classic.width  = \
      png_get_image_width(ptr->classic.ctx, ptr->classic.fsi);

  ptr->classic.height = \
      png_get_image_height(ptr->classic.ctx, ptr->classic.fsi);

  stride = png_get_rowbytes(ptr->classic.ctx, ptr->classic.fsi);

  /*
   * alloc return memory
   */
  ret  = rb_str_buf_new(stride * ptr->classic.height);
  rb_str_set_len(ret, stride * ptr->classic.height);

  /*
   * alloc rows
   */
  ptr->classic.rows = png_malloc(ptr->classic.ctx,
                                 ptr->classic.height * sizeof(png_byte*));
  if (ptr->classic.rows == NULL) {
    NOMEMORY_ERROR("no memory");
  }

  p = (png_byte*)RSTRING_PTR(ret);
  for (i = 0; i < ptr->classic.height; i++) {
    ptr->classic.rows[i] = p;
    p += stride;
  }

  png_read_image(ptr->classic.ctx, ptr->classic.rows);
  png_read_end(ptr->classic.ctx, ptr->classic.fsi);

  if (ptr->classic.need_meta) {
    get_header_info(ptr);
    rb_ivar_set(ret, id_meta, create_meta(ptr));
    rb_define_singleton_method(ret, "meta", rb_decode_result_meta, 0);
  }

  return ret;
}

static VALUE
decode_classic_api_ensure(VALUE _arg)
{
  png_decoder_t* ptr;

  ptr = (png_decoder_t*)_arg;

  if (ptr->classic.rows) {
    png_free(ptr->classic.ctx, ptr->classic.rows);
    ptr->classic.rows = NULL;
  }

  clear_read_context(ptr);

  return Qundef;
}

static VALUE
rb_decoder_decode(VALUE self, VALUE data)
{
  VALUE ret;
  png_decoder_t* ptr;
  decode_arg_t arg;

  /*
   * argument check
   */
  Check_Type(data, T_STRING);

  /*
   * strip object
   */
  TypedData_Get_Struct(self, png_decoder_t, &png_decoder_data_type, ptr);

  /*
   * call decode funcs
   */
  arg.ptr  = ptr;
  arg.data = data;

  if (ptr->common.api_type == API_SIMPLIFIED) {
    ret = rb_ensure(decode_simplified_api_body, (VALUE)&arg,
                    decode_simplified_api_ensure, (VALUE)ptr);

  } else {
    ret = rb_ensure(decode_classic_api_body, (VALUE)&arg,
                    decode_classic_api_ensure, (VALUE)ptr);
  }

  return ret;
}

#define DEFINE_SYMBOL(name, str)

void
Init_png()
{
  int i;

  module = rb_define_module("PNG");

  encoder_klass = rb_define_class_under(module, "Encoder", rb_cObject);
  rb_define_alloc_func(encoder_klass, rb_encoder_alloc);
  rb_define_method(encoder_klass, "initialize", rb_encoder_initialize, -1);
  rb_define_method(encoder_klass, "encode", rb_encoder_encode, 1);
  rb_define_alias(encoder_klass, "compress", "encode");
  rb_define_alias(encoder_klass, "<<", "encode");

  decoder_klass = rb_define_class_under(module, "Decoder", rb_cObject);
  rb_define_alloc_func(decoder_klass, rb_decoder_alloc);
  rb_define_method(decoder_klass, "initialize", rb_decoder_initialize, -1);
  rb_define_method(decoder_klass, "read_header", rb_decoder_read_header, 1);
  rb_define_method(decoder_klass, "decode", rb_decoder_decode, 1);
  rb_define_alias(decoder_klass, "decompress", "decode");
  rb_define_alias(decoder_klass, "<<", "decode");

  meta_klass = rb_define_class_under(module, "Meta", rb_cObject);
  rb_define_attr(meta_klass, "width", 1, 0);
  rb_define_attr(meta_klass, "height", 1, 0);
  rb_define_attr(meta_klass, "stride", 1, 0);
  rb_define_attr(meta_klass, "bit_depth", 1, 0);
  rb_define_attr(meta_klass, "color_type", 1, 0);
  rb_define_attr(meta_klass, "interlace_method", 1, 0);
  rb_define_attr(meta_klass, "compression_method", 1, 0);
  rb_define_attr(meta_klass, "filter_method", 1, 0);
  rb_define_attr(meta_klass, "pixel_format", 1, 0);
  rb_define_attr(meta_klass, "num_components", 1, 0);
  rb_define_attr(meta_klass, "text", 1, 0);
  rb_define_attr(meta_klass, "time", 1, 0);
  rb_define_attr(meta_klass, "file_gamma", 1, 0);

  for (i = 0; i < (int)N(encoder_opt_keys); i++) {
    encoder_opt_ids[i] = rb_intern_const(encoder_opt_keys[i]);
  }

  for (i = 0; i < (int)N(decoder_opt_keys); i++) {
    decoder_opt_ids[i] = rb_intern_const(decoder_opt_keys[i]);
  }

  id_meta    = rb_intern_const("@meta");
  id_stride  = rb_intern_const("@stride");
  id_format  = rb_intern_const("@format");
  id_pixfmt  = rb_intern_const("@pixel_format");
  id_ncompo  = rb_intern_const("@num_components");
}
