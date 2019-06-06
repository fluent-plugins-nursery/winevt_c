#include <ruby.h>
#include <ruby/encoding.h>

#ifdef __GNUC__
# include <w32api.h>
# define MINIMUM_WINDOWS_VERSION WindowsVista
#else /* __GNUC__ */
# define MINIMUM_WINDOWS_VERSION 0x0600 /* Vista */
#endif /* __GNUC__ */

#ifdef _WIN32_WINNT
#  undef WIN32_WINNT
#endif /* WIN32_WINNT */
#define _WIN32_WINNT MINIMUM_WINDOWS_VERSION

#include <winevt.h>
#include <oleauto.h>
#define EventQuery(object) ((struct WinevtQuery *)DATA_PTR(object))
#define EventBookMark(object) ((struct WinevtBookmark *)DATA_PTR(object))

VALUE rb_mWin32;
VALUE rb_cWinevt;
VALUE rb_cQuery;
VALUE rb_cBookmark;
VALUE rb_eWinevtQueryError;

static void query_free(void *ptr);
static void bookmark_free(void *ptr);
static char* render_event(EVT_HANDLE handle, DWORD flags);

static const rb_data_type_t rb_winevt_query_type = {
  "winevt/query", {
    0, query_free, 0,
  }, NULL, NULL,
  RUBY_TYPED_FREE_IMMEDIATELY
};

struct WinevtQuery {
  EVT_HANDLE query;
  EVT_HANDLE event;
  ULONG      count;
};

struct WinevtBookmark {
  EVT_HANDLE bookmark;
  ULONG      count;
};

static const rb_data_type_t rb_winevt_bookmark_type = {
  "winevt/bookmark", {
    0, bookmark_free, 0,
  }, NULL, NULL,
  RUBY_TYPED_FREE_IMMEDIATELY
};

static void
bookmark_free(void *ptr)
{
  struct WinevtBookmark *winevtBookmark = (struct WinevtBookmark *)ptr;
  EvtClose(winevtBookmark->bookmark);

  xfree(ptr);
}

static VALUE
rb_winevt_bookmark_alloc(VALUE klass)
{
  VALUE obj;
  struct WinevtBookmark *winevtBookmark;
  obj = TypedData_Make_Struct(klass,
                              struct WinevtBookmark,
                              &rb_winevt_bookmark_type,
                              winevtBookmark);
  return obj;
}

static VALUE
rb_winevt_bookmark_initialize(VALUE self)
{
  struct WinevtBookmark *winevtBookmark;

  TypedData_Get_Struct(self, struct WinevtBookmark, &rb_winevt_bookmark_type, winevtBookmark);

  winevtBookmark->bookmark = EvtCreateBookmark(NULL);

  return Qnil;
}

static VALUE
rb_winevt_bookmark_update(VALUE self, VALUE event)
{
  struct WinevtQuery *winevtQuery;
  struct WinevtBookmark *winevtBookmark;

  winevtQuery = EventQuery(event);

  TypedData_Get_Struct(self, struct WinevtBookmark, &rb_winevt_bookmark_type, winevtBookmark);

  if(EvtUpdateBookmark(winevtBookmark->bookmark, winevtQuery->event))
    return Qtrue;

  return Qfalse;
}

static VALUE
rb_winevt_bookmark_render(VALUE self)
{
  char* result;
  struct WinevtBookmark *winevtBookmark;

  TypedData_Get_Struct(self, struct WinevtBookmark, &rb_winevt_bookmark_type, winevtBookmark);

  result = render_event(winevtBookmark->bookmark, EvtRenderBookmark);

  return rb_str_new2(result);
}

static void
query_free(void *ptr)
{
  struct WinevtQuery *winevtQuery = (struct WinevtQuery *)ptr;
  EvtClose(winevtQuery->query);

  xfree(ptr);
}

static VALUE
rb_winevt_query_alloc(VALUE klass)
{
  VALUE obj;
  struct WinevtQuery *winevtQuery;
  obj = TypedData_Make_Struct(klass,
                              struct WinevtQuery,
                              &rb_winevt_query_type,
                              winevtQuery);
  return obj;
}

static VALUE
rb_winevt_query_initialize(VALUE self, VALUE channel, VALUE xpath)
{
  PWSTR evtChannel, evtXPath;
  struct WinevtQuery *winevtQuery;
  DWORD len;

  Check_Type(channel, T_STRING);
  Check_Type(xpath, T_STRING);

  // channel : To wide char
  len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(channel), RSTRING_LEN(channel), NULL, 0);
  evtChannel = ALLOCV_N(WCHAR, evtChannel, len+1);
  MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(channel), RSTRING_LEN(channel), evtChannel, len);
  evtChannel[len] = L'\0';

  // xpath : To wide char
  len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(xpath), RSTRING_LEN(xpath), NULL, 0);
  evtXPath = ALLOCV_N(WCHAR, evtXPath, len+1);
  MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(xpath), RSTRING_LEN(xpath), evtXPath, len);
  evtXPath[len] = L'\0';

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->query = EvtQuery(NULL, evtChannel, evtXPath,
                                EvtQueryChannelPath | EvtQueryTolerateQueryErrors);

  return Qnil;
}

static VALUE
rb_winevt_query_next(VALUE self)
{
  EVT_HANDLE event;
  ULONG      count;
  struct WinevtQuery *winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  if (EvtNext(winevtQuery->query, 1, &event, INFINITE, 0, &count) != FALSE) {
    winevtQuery->event = event;
    winevtQuery->count = count;

    return Qtrue;
  }

  return Qfalse;
}

static char* render_event(EVT_HANDLE handle, DWORD flags)
{
  PWSTR      buffer = NULL;
  ULONG      bufferSize = 0;
  ULONG      bufferSizeNeeded = 0;
  EVT_HANDLE event;
  ULONG      len, status, count;
  char*      errBuf;
  char*      result;
  LPTSTR     msgBuf;

  do {
    if (bufferSizeNeeded > bufferSize) {
      free(buffer);
      bufferSize = bufferSizeNeeded;
      buffer = malloc(bufferSize);
      if (buffer == NULL) {
        status = ERROR_OUTOFMEMORY;
        bufferSize = 0;
        rb_raise(rb_eWinevtQueryError, "Out of memory");
        break;
      }
    }

    if (EvtRender(NULL,
                  handle,
                  flags,
                  bufferSize,
                  buffer,
                  &bufferSizeNeeded,
                  &count) != FALSE) {
      status = ERROR_SUCCESS;
    } else {
      status = GetLastError();
    }
  } while (status == ERROR_INSUFFICIENT_BUFFER);

  if (status != ERROR_SUCCESS) {
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        &msgBuf, 0, NULL);
    len = WideCharToMultiByte(CP_UTF8, 0, msgBuf, -1, NULL, 0, NULL, NULL);
    if (!(result = malloc(len))) return "";
    WideCharToMultiByte(CP_UTF8, 0, msgBuf, -1, result, len, NULL, NULL);

    rb_raise(rb_eWinevtQueryError, "ErrorCode: %d\nError: %s\n", status, result);
  }

  len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
  if (!(result = malloc(len))) return "";
  WideCharToMultiByte(CP_UTF8, 0, buffer, -1, result, len, NULL, NULL);

  return result;
}

static VALUE
rb_winevt_query_render(VALUE self)
{
  char* result;
  struct WinevtQuery *winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);
  result = render_event(winevtQuery->event, EvtRenderEventXml);

  return rb_str_new2(result);
}

void
Init_winevt(void)
{
  rb_mWin32 = rb_define_module("Win32");
  rb_cWinevt = rb_define_class_under(rb_mWin32, "Winevt", rb_cObject);
  rb_cQuery = rb_define_class_under(rb_cWinevt, "Query", rb_cObject);
  rb_cBookmark = rb_define_class_under(rb_cWinevt, "Bookmark", rb_cObject);
  rb_eWinevtQueryError = rb_define_class_under(rb_cQuery, "Error", rb_eStandardError);

  rb_define_alloc_func(rb_cQuery, rb_winevt_query_alloc);
  rb_define_method(rb_cQuery, "initialize", rb_winevt_query_initialize, 2);
  rb_define_method(rb_cQuery, "next", rb_winevt_query_next, 0);
  rb_define_method(rb_cQuery, "render", rb_winevt_query_render, 0);
  rb_define_alloc_func(rb_cBookmark, rb_winevt_bookmark_alloc);
  rb_define_method(rb_cBookmark, "initialize", rb_winevt_bookmark_initialize, 0);
  rb_define_method(rb_cBookmark, "update", rb_winevt_bookmark_update, 1);
  rb_define_method(rb_cBookmark, "render", rb_winevt_bookmark_render, 0);
}
