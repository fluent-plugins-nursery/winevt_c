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
#define EventQuery(object) ((struct WinevtQuery *)DATA_PTR(object))
#define EventBookMark(object) ((struct WinevtBookmark *)DATA_PTR(object))
#define EventChannel(object) ((struct WinevtChannel *)DATA_PTR(object))

VALUE rb_mWinevt;
VALUE rb_cEventLog;
VALUE rb_cSubscribe;
VALUE rb_cChannel;
VALUE rb_cQuery;
VALUE rb_cBookmark;
VALUE rb_eWinevtQueryError;

static ID id_call;
static void channel_free(void *ptr);
static void subscribe_free(void *ptr);
static void query_free(void *ptr);
static void bookmark_free(void *ptr);
static char* render_event(EVT_HANDLE handle, DWORD flags);
static DWORD WINAPI SubscriptionCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent);

static const rb_data_type_t rb_winevt_channel_type = {
  "winevt/channel", {
    0, channel_free, 0,
  }, NULL, NULL,
  RUBY_TYPED_FREE_IMMEDIATELY
};

struct WinevtChannel {
  EVT_HANDLE channels;
};

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
  LONG       offset;
  LONG       timeout;
};

static const rb_data_type_t rb_winevt_bookmark_type = {
  "winevt/bookmark", {
    0, bookmark_free, 0,
  }, NULL, NULL,
  RUBY_TYPED_FREE_IMMEDIATELY
};

struct WinevtBookmark {
  EVT_HANDLE bookmark;
  ULONG      count;
};

static const rb_data_type_t rb_winevt_subscribe_type = {
  "winevt/subscribe", {
    0, subscribe_free, 0,
  }, NULL, NULL,
  RUBY_TYPED_FREE_IMMEDIATELY
};

struct WinevtSubscribe {
  HANDLE     signalEvent;
  EVT_HANDLE subscription;
  EVT_HANDLE bookmark;
  EVT_HANDLE event;
  DWORD      flags;
};

static void
channel_free(void *ptr)
{
  struct WinevtChannel *winevtChannel = (struct WinevtChannel *)ptr;
  if (winevtChannel->channels)
    EvtClose(winevtChannel->channels);

  xfree(ptr);
}

static VALUE
rb_winevt_channel_alloc(VALUE klass)
{
  VALUE obj;
  struct WinevtChannel *winevtChannel;
  obj = TypedData_Make_Struct(klass,
                              struct WinevtChannel,
                              &rb_winevt_channel_type,
                              winevtChannel);
  return obj;
}

static VALUE
rb_winevt_channel_initialize(VALUE klass)
{
  return Qnil;
}

static VALUE
rb_winevt_channel_each(VALUE self)
{
  EVT_HANDLE hChannels;
  struct WinevtChannel *winevtChannel;
  char *errBuf;
  char * result;
  LPWSTR buffer = NULL;
  LPWSTR temp = NULL;
  DWORD bufferSize = 0;
  DWORD bufferUsed = 0;
  DWORD status = ERROR_SUCCESS;
  ULONG len;

  RETURN_ENUMERATOR(self, 0, 0);

  TypedData_Get_Struct(self, struct WinevtChannel, &rb_winevt_channel_type, winevtChannel);

  hChannels = EvtOpenChannelEnum(NULL, 0);

  if (hChannels) {
    winevtChannel->channels = hChannels;
  } else {
    sprintf(errBuf, "Failed to enumerate channels with %s\n", GetLastError());
    rb_raise(rb_eRuntimeError, errBuf);
  }

  while (1) {
    if (!EvtNextChannelPath(winevtChannel->channels, bufferSize, buffer, &bufferUsed)) {
      status = GetLastError();

      if (ERROR_NO_MORE_ITEMS == status) {
        break;
      } else if (ERROR_INSUFFICIENT_BUFFER == status) {
        bufferSize = bufferUsed;
        temp = (LPWSTR)realloc(buffer, bufferSize * sizeof(WCHAR));
        if (temp) {
          buffer = temp;
          temp = NULL;
          EvtNextChannelPath(winevtChannel->channels, bufferSize, buffer, &bufferUsed);
        } else {
          status = ERROR_OUTOFMEMORY;
          rb_raise(rb_eRuntimeError, "realloc failed");
        }
      } else {
        sprintf(errBuf, "EvtNextChannelPath failed with %lu.\n", status);
        rb_raise(rb_eRuntimeError, errBuf);
      }
    }

    len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
    if (!(result = malloc(len))) return "";
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, result, len, NULL, NULL);

    rb_yield(rb_str_new2(result));
  }

  return Qnil;
}

static void
subscribe_free(void *ptr)
{
  struct WinevtSubscribe *winevtSubscribe = (struct WinevtSubscribe *)ptr;
  if (winevtSubscribe->signalEvent)
    CloseHandle(winevtSubscribe->signalEvent);

  if (winevtSubscribe->subscription)
    EvtClose(winevtSubscribe->subscription);

  if (winevtSubscribe->bookmark)
    EvtClose(winevtSubscribe->bookmark);

  if (winevtSubscribe->event)
    EvtClose(winevtSubscribe->event);

  xfree(ptr);
}

static VALUE
rb_winevt_subscribe_alloc(VALUE klass)
{
  VALUE obj;
  struct WinevtSubscribe *winevtSubscribe;
  obj = TypedData_Make_Struct(klass,
                              struct WinevtSubscribe,
                              &rb_winevt_subscribe_type,
                              winevtSubscribe);
  return obj;
}

static VALUE
rb_winevt_subscribe_initialize(VALUE self)
{
  return Qnil;
}

static VALUE
rb_winevt_subscribe_set_tail(VALUE self, VALUE rb_tailing_p)
{
  struct WinevtSubscribe *winevtSubscribe;

  TypedData_Get_Struct(self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  winevtSubscribe->tailing = RTEST(rb_tailing_p);

  return Qnil;
}

static VALUE
rb_winevt_subscribe_tail_p(VALUE self, VALUE rb_flag)
{
  struct WinevtSubscribe *winevtSubscribe;

  TypedData_Get_Struct(self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  return winevtSubscribe->tailing ? Qtrue : Qfalse;
}

static VALUE
rb_winevt_subscribe_subscribe(int argc, VALUE argv, VALUE self)
{
  VALUE rb_path, rb_query, rb_bookmark;
  EVT_HANDLE hSubscription = NULL, hBookmark = NULL;
  HANDLE hSignalEvent;
  DWORD len, flags;
  VALUE wpathBuf, wqueryBuf;
  PWSTR path, query;
  struct WinevtBookmark *winevtBookmark;
  struct WinevtSubscribe *winevtSubscribe;

  hSignalEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  TypedData_Get_Struct(self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  if (argc == 2) {
    rb_scan_args(argc, argv, "20", &rb_path, &rb_query);
    Check_Type(rb_path, T_STRING);
    Check_Type(rb_query, T_STRING);

    // path : To wide char
    len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_path), RSTRING_LEN(rb_path), NULL, 0);
    path = ALLOCV_N(WCHAR, wpathBuf, len+1);
    MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_path), RSTRING_LEN(rb_path), path, len);
    path[len] = L'\0';

    // query : To wide char
    len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_query), RSTRING_LEN(rb_query), NULL, 0);
    query = ALLOCV_N(WCHAR, wqueryBuf, len+1);
    MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_query), RSTRING_LEN(rb_query), query, len);
    query[len] = L'\0';

    if (winevtSubscribe->tailing) {
      flags |= EvtSubscribeToFutureEvents;
    } else {
      flags |= EvtSubscribeStartAtOldestRecord;
    }

    hSubscription = EvtSubscribe(NULL, hSignalEvent, path, query, hBookmark, NULL, NULL, flags);
  } else if (argc == 3) {
    rb_scan_args(argc, argv, "30", &rb_path, &rb_query, &rb_bookmark);
    Check_Type(rb_path, T_STRING);
    Check_Type(rb_query, T_STRING);

    // path : To wide char
    len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_path), RSTRING_LEN(rb_path), NULL, 0);
    path = ALLOCV_N(WCHAR, wpathBuf, len+1);
    MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_path), RSTRING_LEN(rb_path), path, len);
    path[len] = L'\0';

    // query : To wide char
    len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_query), RSTRING_LEN(rb_query), NULL, 0);
    query = ALLOCV_N(WCHAR, wqueryBuf, len+1);
    MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_query), RSTRING_LEN(rb_query), query, len);
    query[len] = L'\0';

    if (rb_obj_is_kind_of(rb_bookmark, rb_cBookmark)) {
      hBookmark = EventBookMark(rb_bookmark)->bookmark;
    }
    flags |= EvtSubscribeStartAfterBookmark;


    hSubscription = EvtSubscribe(NULL, hSignalEvent, path, query, hBookmark, NULL, NULL, flags);
  }

  winevtSubscribe->signalEvent = hSignalEvent;
  winevtSubscribe->subscription = hSubscription;
  if (hBookmark) {
    winevtSubscribe->bookmark = hBookmark;
  } else {
    winevtSubscribe->bookmark = EvtCreateBookmark(NULL);
  }

  return Qnil;
}

static VALUE
rb_winevt_subscribe_next(VALUE self)
{
  EVT_HANDLE event;
  ULONG      count;
  struct WinevtSubscribe *winevtSubscribe;

  TypedData_Get_Struct(self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  if (EvtNext(winevtSubscribe->subscription, 1, &event, INFINITE, 0, &count) != FALSE) {
    winevtSubscribe->event = event;
    EvtUpdateBookmark(winevtSubscribe->bookmark, winevtSubscribe->event);

    return Qtrue;
  }

  return Qfalse;
}

static VALUE
rb_winevt_subscribe_render(VALUE self)
{
  char* result;
  struct WinevtSubscribe *winevtSubscribe;

  TypedData_Get_Struct(self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);
  result = render_event(winevtSubscribe->event, EvtRenderEventXml);

  return rb_str_new2(result);
}

static VALUE
rb_winevt_subscribe_each(VALUE self)
{
  struct WinevtSubscribe *winevtSubscribe;

  RETURN_ENUMERATOR(self, 0, 0);

  TypedData_Get_Struct(self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  while (rb_winevt_subscribe_next(self)) {
    rb_yield(rb_winevt_subscribe_render(self));
  }

  return Qnil;
}

static VALUE
rb_winevt_subscribe_get_bookmark(VALUE self)
{
  char* result;
  struct WinevtSubscribe *winevtSubscribe;

  TypedData_Get_Struct(self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  result = render_event(winevtSubscribe->bookmark, EvtRenderBookmark);

  return rb_str_new2(result);
}

static void
bookmark_free(void *ptr)
{
  struct WinevtBookmark *winevtBookmark = (struct WinevtBookmark *)ptr;
  if (winevtBookmark->bookmark)
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
rb_winevt_bookmark_initialize(int argc, VALUE *argv, VALUE self)
{
  PWSTR bookmarkXml;
  VALUE wbookmarkXmlBuf;
  DWORD len;
  struct WinevtBookmark *winevtBookmark;

  TypedData_Get_Struct(self, struct WinevtBookmark, &rb_winevt_bookmark_type, winevtBookmark);

  if (argc == 0) {
    winevtBookmark->bookmark = EvtCreateBookmark(NULL);
  } else if (argc == 1) {
    VALUE rb_bookmarkXml;
    rb_scan_args(argc, argv, "10", &rb_bookmarkXml);
    Check_Type(rb_bookmarkXml, T_STRING);

    // bookmarkXml : To wide char
    len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_bookmarkXml), RSTRING_LEN(rb_bookmarkXml), NULL, 0);
    bookmarkXml = ALLOCV_N(WCHAR, wbookmarkXmlBuf, len+1);
    MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_bookmarkXml), RSTRING_LEN(rb_bookmarkXml), bookmarkXml, len);
    bookmarkXml[len] = L'\0';
    winevtBookmark->bookmark = EvtCreateBookmark(bookmarkXml);
    ALLOCV_END(wbookmarkXmlBuf);
  }

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
  if (winevtQuery->query)
    EvtClose(winevtQuery->query);

  if (winevtQuery->event)
    EvtClose(winevtQuery->event);

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
  VALUE wchannelBuf, wpathBuf;

  Check_Type(channel, T_STRING);
  Check_Type(xpath, T_STRING);

  // channel : To wide char
  len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(channel), RSTRING_LEN(channel), NULL, 0);
  evtChannel = ALLOCV_N(WCHAR, wchannelBuf, len+1);
  MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(channel), RSTRING_LEN(channel), evtChannel, len);
  evtChannel[len] = L'\0';

  // xpath : To wide char
  len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(xpath), RSTRING_LEN(xpath), NULL, 0);
  evtXPath = ALLOCV_N(WCHAR, wpathBuf, len+1);
  MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(xpath), RSTRING_LEN(xpath), evtXPath, len);
  evtXPath[len] = L'\0';

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->query = EvtQuery(NULL, evtChannel, evtXPath,
                                EvtQueryChannelPath | EvtQueryTolerateQueryErrors);
  winevtQuery->offset = 0L;
  winevtQuery->timeout = 0L;

  ALLOCV_END(wchannelBuf);
  ALLOCV_END(wpathBuf);

  return Qnil;
}

static VALUE
rb_winevt_query_get_offset(VALUE self, VALUE offset)
{
  struct WinevtQuery *winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  return LONG2NUM(winevtQuery->offset);
}

static VALUE
rb_winevt_query_set_offset(VALUE self, VALUE offset)
{
  struct WinevtQuery *winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->offset = NUM2LONG(offset);

  return Qnil;
}

static VALUE
rb_winevt_query_get_timeout(VALUE self, VALUE timeout)
{
  struct WinevtQuery *winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  return LONG2NUM(winevtQuery->timeout);
}

static VALUE
rb_winevt_query_set_timeout(VALUE self, VALUE timeout)
{
  struct WinevtQuery *winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->timeout = NUM2LONG(timeout);

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

static DWORD
get_evt_seek_flag_from_cstr(char* flag_str)
{
  if (strcmp(flag_str, "first") == 0)
    return EvtSeekRelativeToFirst;
  else if (strcmp(flag_str, "last") == 0)
    return EvtSeekRelativeToLast;
  else if (strcmp(flag_str, "current") == 0)
    return EvtSeekRelativeToCurrent;
  else if (strcmp(flag_str, "bookmark") == 0)
    return EvtSeekRelativeToBookmark;
  else if (strcmp(flag_str, "originmask") == 0)
    return EvtSeekOriginMask;
  else if (strcmp(flag_str, "strict") == 0)
    return EvtSeekStrict;
}

static VALUE
rb_winevt_query_seek(VALUE self, VALUE bookmark_or_flag)
{
  struct WinevtQuery *winevtQuery;
  struct WinevtBookmark *winevtBookmark = NULL;
  DWORD status;
  DWORD flag;

  switch (TYPE(bookmark_or_flag)) {
  case T_SYMBOL:
    flag = get_evt_seek_flag_from_cstr(RSTRING_PTR(rb_sym2str(bookmark_or_flag)));
    break;
  case T_STRING:
    flag = get_evt_seek_flag_from_cstr(StringValueCStr(bookmark_or_flag));
    break;
  default:
    if (!rb_obj_is_kind_of(bookmark_or_flag, rb_cBookmark))
      rb_raise(rb_eArgError, "Expected a String or a Symbol or a Bookmark instance");

    winevtBookmark = EventBookMark(bookmark_or_flag);
  }

  if (winevtBookmark) {
    TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);
    if (EvtSeek(winevtQuery->query, winevtQuery->offset, winevtBookmark->bookmark, winevtQuery->timeout, EvtSeekRelativeToBookmark))
      return Qtrue;
  } else {
    TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);
    if (EvtSeek(winevtQuery->query, winevtQuery->offset, NULL, winevtQuery->timeout, flag))
      return Qtrue;
  }

  return Qfalse;
}

static VALUE
rb_winevt_query_each(VALUE self)
{
  struct WinevtQuery *winevtQuery;

  RETURN_ENUMERATOR(self, 0, 0);

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  while (rb_winevt_query_next(self)) {
    rb_yield(rb_winevt_query_render(self));
  }

  return Qnil;
}

void
Init_winevt(void)
{
  rb_mWinevt = rb_define_module("Winevt");
  rb_cEventLog = rb_define_class_under(rb_mWinevt, "EventLog", rb_cObject);
  rb_cQuery = rb_define_class_under(rb_cEventLog, "Query", rb_cObject);
  rb_cBookmark = rb_define_class_under(rb_cEventLog, "Bookmark", rb_cObject);
  rb_cChannel = rb_define_class_under(rb_cEventLog, "Channel", rb_cObject);
  rb_cSubscribe = rb_define_class_under(rb_cEventLog, "Subscribe", rb_cObject);
  rb_eWinevtQueryError = rb_define_class_under(rb_cQuery, "Error", rb_eStandardError);

  id_call = rb_intern("call");

  rb_define_alloc_func(rb_cSubscribe, rb_winevt_subscribe_alloc);
  rb_define_method(rb_cSubscribe, "initialize", rb_winevt_subscribe_initialize, 0);
  rb_define_method(rb_cSubscribe, "subscribe", rb_winevt_subscribe_subscribe, -1);
  rb_define_method(rb_cSubscribe, "next", rb_winevt_subscribe_next, 0);
  rb_define_method(rb_cSubscribe, "render", rb_winevt_subscribe_render, 0);
  rb_define_method(rb_cSubscribe, "each", rb_winevt_subscribe_each, 0);
  rb_define_method(rb_cSubscribe, "bookmark", rb_winevt_subscribe_get_bookmark, 0);
  rb_define_method(rb_cSubscribe, "tail?", rb_winevt_subscribe_tail_p, 0);
  rb_define_method(rb_cSubscribe, "tail=", rb_winevt_subscribe_set_tail, 1);

  rb_define_alloc_func(rb_cChannel, rb_winevt_channel_alloc);
  rb_define_method(rb_cChannel, "initialize", rb_winevt_channel_initialize, 0);
  rb_define_method(rb_cChannel, "each", rb_winevt_channel_each, 0);

  rb_define_alloc_func(rb_cBookmark, rb_winevt_bookmark_alloc);
  rb_define_method(rb_cBookmark, "initialize", rb_winevt_bookmark_initialize, -1);
  rb_define_method(rb_cBookmark, "update", rb_winevt_bookmark_update, 1);
  rb_define_method(rb_cBookmark, "render", rb_winevt_bookmark_render, 0);

  rb_define_alloc_func(rb_cQuery, rb_winevt_query_alloc);
  rb_define_method(rb_cQuery, "initialize", rb_winevt_query_initialize, 2);
  rb_define_method(rb_cQuery, "next", rb_winevt_query_next, 0);
  rb_define_method(rb_cQuery, "render", rb_winevt_query_render, 0);
  rb_define_method(rb_cQuery, "seek", rb_winevt_query_seek, 1);
  rb_define_method(rb_cQuery, "offset", rb_winevt_query_get_offset, 0);
  rb_define_method(rb_cQuery, "offset=", rb_winevt_query_set_offset, 1);
  rb_define_method(rb_cQuery, "timeout", rb_winevt_query_get_timeout, 0);
  rb_define_method(rb_cQuery, "timeout=", rb_winevt_query_set_timeout, 1);
  rb_define_method(rb_cQuery, "each", rb_winevt_query_each, 0);
}
