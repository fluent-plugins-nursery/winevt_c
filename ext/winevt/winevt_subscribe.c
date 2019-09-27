#include <winevt_c.h>

static void
subscribe_free(void* ptr);

static const rb_data_type_t rb_winevt_subscribe_type = { "winevt/subscribe",
                                                         {
                                                           0,
                                                           subscribe_free,
                                                           0,
                                                         },
                                                         NULL,
                                                         NULL,
                                                         RUBY_TYPED_FREE_IMMEDIATELY };

static void
subscribe_free(void* ptr)
{
  struct WinevtSubscribe* winevtSubscribe = (struct WinevtSubscribe*)ptr;
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
  struct WinevtSubscribe* winevtSubscribe;
  obj = TypedData_Make_Struct(
    klass, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);
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
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  winevtSubscribe->tailing = RTEST(rb_tailing_p);

  return Qnil;
}

static VALUE
rb_winevt_subscribe_tail_p(VALUE self, VALUE rb_flag)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  return winevtSubscribe->tailing ? Qtrue : Qfalse;
}

static VALUE
rb_winevt_subscribe_subscribe(int argc, VALUE* argv, VALUE self)
{
  VALUE rb_path, rb_query, rb_bookmark;
  EVT_HANDLE hSubscription = NULL, hBookmark = NULL;
  HANDLE hSignalEvent;
  DWORD len, flags = 0L;
  VALUE wpathBuf, wqueryBuf;
  PWSTR path, query;
  DWORD status = ERROR_SUCCESS;
  struct WinevtSubscribe* winevtSubscribe;

  hSignalEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  rb_scan_args(argc, argv, "21", &rb_path, &rb_query, &rb_bookmark);
  Check_Type(rb_path, T_STRING);
  Check_Type(rb_query, T_STRING);

  if (rb_obj_is_kind_of(rb_bookmark, rb_cBookmark)) {
    hBookmark = EventBookMark(rb_bookmark)->bookmark;
  }

  // path : To wide char
  len =
    MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_path), RSTRING_LEN(rb_path), NULL, 0);
  path = ALLOCV_N(WCHAR, wpathBuf, len + 1);
  MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(rb_path), RSTRING_LEN(rb_path), path, len);
  path[len] = L'\0';

  // query : To wide char
  len = MultiByteToWideChar(
    CP_UTF8, 0, RSTRING_PTR(rb_query), RSTRING_LEN(rb_query), NULL, 0);
  query = ALLOCV_N(WCHAR, wqueryBuf, len + 1);
  MultiByteToWideChar(
    CP_UTF8, 0, RSTRING_PTR(rb_query), RSTRING_LEN(rb_query), query, len);
  query[len] = L'\0';

  if (hBookmark) {
    flags |= EvtSubscribeStartAfterBookmark;
  } else if (winevtSubscribe->tailing) {
    flags |= EvtSubscribeToFutureEvents;
  } else {
    flags |= EvtSubscribeStartAtOldestRecord;
  }

  hSubscription =
    EvtSubscribe(NULL, hSignalEvent, path, query, hBookmark, NULL, NULL, flags);

  ALLOCV_END(wpathBuf);
  ALLOCV_END(wqueryBuf);

  winevtSubscribe->signalEvent = hSignalEvent;
  winevtSubscribe->subscription = hSubscription;
  if (hBookmark) {
    winevtSubscribe->bookmark = hBookmark;
  } else {
    winevtSubscribe->bookmark = EvtCreateBookmark(NULL);
  }

  status = GetLastError();

  if (status == ERROR_SUCCESS)
    return Qtrue;

  return Qfalse;
}

static VALUE
rb_winevt_subscribe_next(VALUE self)
{
  EVT_HANDLE event;
  ULONG count;
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

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
  WCHAR* wResult;
  struct WinevtSubscribe* winevtSubscribe;
  VALUE utf8str;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);
  wResult = render_event(winevtSubscribe->event, EvtRenderEventXml);
  utf8str = wstr_to_rb_str(CP_UTF8, wResult, -1);

  if (wResult != NULL)
    free(wResult);

  return utf8str;
}

static VALUE
rb_winevt_subscribe_message(VALUE self)
{
  WCHAR* wResult;
  struct WinevtSubscribe* winevtSubscribe;
  VALUE utf8str;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);
  wResult = get_description(winevtSubscribe->event);
  utf8str = wstr_to_rb_str(CP_UTF8, wResult, -1);

  if (wResult != NULL)
    free(wResult);

  return utf8str;
}

static VALUE
rb_winevt_subscribe_string_inserts(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);
  return get_values(winevtSubscribe->event);
}

static VALUE
rb_winevt_subscribe_close_handle(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  if (winevtSubscribe->event != NULL) {
    EvtClose(winevtSubscribe->event);
    winevtSubscribe->event = NULL;
  }

  return Qnil;
}

static VALUE
rb_winevt_subscribe_each_yield(VALUE self)
{
  RETURN_ENUMERATOR(self, 0, 0);

  rb_yield_values(3,
                  rb_winevt_subscribe_render(self),
                  rb_winevt_subscribe_message(self),
                  rb_winevt_subscribe_string_inserts(self));

  return Qnil;
}

static VALUE
rb_winevt_subscribe_each(VALUE self)
{
  RETURN_ENUMERATOR(self, 0, 0);

  while (rb_winevt_subscribe_next(self)) {
    rb_ensure(
      rb_winevt_subscribe_each_yield, self, rb_winevt_subscribe_close_handle, self);
  }

  return Qnil;
}

static VALUE
rb_winevt_subscribe_get_bookmark(VALUE self)
{
  WCHAR* wResult;
  struct WinevtSubscribe* winevtSubscribe;
  VALUE utf8str;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  wResult = render_event(winevtSubscribe->bookmark, EvtRenderBookmark);
  utf8str = wstr_to_rb_str(CP_UTF8, wResult, -1);

  return utf8str;
}

void
Init_winevt_subscribe(VALUE rb_cEventLog)
{
  rb_cSubscribe = rb_define_class_under(rb_cEventLog, "Subscribe", rb_cObject);

  rb_define_alloc_func(rb_cSubscribe, rb_winevt_subscribe_alloc);
  rb_define_method(rb_cSubscribe, "initialize", rb_winevt_subscribe_initialize, 0);
  rb_define_method(rb_cSubscribe, "subscribe", rb_winevt_subscribe_subscribe, -1);
  rb_define_method(rb_cSubscribe, "next", rb_winevt_subscribe_next, 0);
  rb_define_method(rb_cSubscribe, "render", rb_winevt_subscribe_render, 0);
  rb_define_method(rb_cSubscribe, "message", rb_winevt_subscribe_message, 0);
  rb_define_method(
    rb_cSubscribe, "string_inserts", rb_winevt_subscribe_string_inserts, 0);
  rb_define_method(rb_cSubscribe, "each", rb_winevt_subscribe_each, 0);
  rb_define_method(rb_cSubscribe, "close_handle", rb_winevt_subscribe_close_handle, 0);
  rb_define_method(rb_cSubscribe, "bookmark", rb_winevt_subscribe_get_bookmark, 0);
  rb_define_method(rb_cSubscribe, "tail?", rb_winevt_subscribe_tail_p, 0);
  rb_define_method(rb_cSubscribe, "tail=", rb_winevt_subscribe_set_tail, 1);
}
