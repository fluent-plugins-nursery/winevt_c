#include <winevt_c.h>

static void query_free(void *ptr);

static const rb_data_type_t rb_winevt_query_type = {
  "winevt/query", {
    0, query_free, 0,
  }, NULL, NULL,
  RUBY_TYPED_FREE_IMMEDIATELY
};

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

void Init_winevt_query(VALUE rb_cEventLog)
{
  rb_cQuery = rb_define_class_under(rb_cEventLog, "Query", rb_cObject);

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
