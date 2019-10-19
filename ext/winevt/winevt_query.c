#include <winevt_c.h>

static void query_free(void* ptr);

static const rb_data_type_t rb_winevt_query_type = { "winevt/query",
                                                     {
                                                       0,
                                                       query_free,
                                                       0,
                                                     },
                                                     NULL,
                                                     NULL,
                                                     RUBY_TYPED_FREE_IMMEDIATELY };

static void
query_free(void* ptr)
{
  struct WinevtQuery* winevtQuery = (struct WinevtQuery*)ptr;
  if (winevtQuery->query)
    EvtClose(winevtQuery->query);

  for (int i = 0; i < winevtQuery->count; i++) {
    if (winevtQuery->hEvents[i])
      EvtClose(winevtQuery->hEvents[i]);
  }
  xfree(ptr);
}

static VALUE
rb_winevt_query_alloc(VALUE klass)
{
  VALUE obj;
  struct WinevtQuery* winevtQuery;
  obj =
    TypedData_Make_Struct(klass, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);
  return obj;
}

/*
 * Initalize Query class.
 *
 * @param channel [String] Querying EventLog channel.
 * @param xpath [String] Querying XPath.
 * @return [Query]
 *
 */
static VALUE
rb_winevt_query_initialize(VALUE self, VALUE channel, VALUE xpath)
{
  PWSTR evtChannel, evtXPath;
  struct WinevtQuery* winevtQuery;
  DWORD len;
  VALUE wchannelBuf, wpathBuf;

  Check_Type(channel, T_STRING);
  Check_Type(xpath, T_STRING);

  // channel : To wide char
  len =
    MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(channel), RSTRING_LEN(channel), NULL, 0);
  evtChannel = ALLOCV_N(WCHAR, wchannelBuf, len + 1);
  MultiByteToWideChar(
    CP_UTF8, 0, RSTRING_PTR(channel), RSTRING_LEN(channel), evtChannel, len);
  evtChannel[len] = L'\0';

  // xpath : To wide char
  len = MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(xpath), RSTRING_LEN(xpath), NULL, 0);
  evtXPath = ALLOCV_N(WCHAR, wpathBuf, len + 1);
  MultiByteToWideChar(CP_UTF8, 0, RSTRING_PTR(xpath), RSTRING_LEN(xpath), evtXPath, len);
  evtXPath[len] = L'\0';

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->query = EvtQuery(
    NULL, evtChannel, evtXPath, EvtQueryChannelPath | EvtQueryTolerateQueryErrors);
  winevtQuery->offset = 0L;
  winevtQuery->timeout = 0L;
  winevtQuery->renderAsXML = TRUE;

  ALLOCV_END(wchannelBuf);
  ALLOCV_END(wpathBuf);

  return Qnil;
}

/*
 * This function returns querying event offset.
 *
 * @return [Integer]
 */
static VALUE
rb_winevt_query_get_offset(VALUE self)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  return LONG2NUM(winevtQuery->offset);
}

/*
 * This function specifies querying event offset.
 *
 * @param offset [Integer] offset value
 */
static VALUE
rb_winevt_query_set_offset(VALUE self, VALUE offset)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->offset = NUM2LONG(offset);

  return Qnil;
}

/*
 * This function returns timeout value.
 *
 * @return [Integer]
 */
static VALUE
rb_winevt_query_get_timeout(VALUE self)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  return LONG2NUM(winevtQuery->timeout);
}

/*
 * This function specifies timeout value.
 *
 * @param timeout [Integer] timeout value
 */
static VALUE
rb_winevt_query_set_timeout(VALUE self, VALUE timeout)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->timeout = NUM2LONG(timeout);

  return Qnil;
}

/*
 * This function processes consuming Windows EventLog operation to the
 * next values.
 *
 */
static VALUE
rb_winevt_query_next(VALUE self)
{
  EVT_HANDLE hEvents[QUERY_ARRAY_SIZE];
  ULONG count;
  DWORD status = ERROR_SUCCESS;
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  if (!EvtNext(winevtQuery->query, QUERY_ARRAY_SIZE, hEvents, INFINITE, 0, &count)) {
    status = GetLastError();
    if (ERROR_NO_MORE_ITEMS != status) {
      return Qfalse;
    }
  }

  if (status == ERROR_SUCCESS) {
    winevtQuery->count = count;
    for (int i = 0; i < count; i++){
      winevtQuery->hEvents[i] = hEvents[i];
    }

    return Qtrue;
  }

  return Qfalse;
}

static VALUE
rb_winevt_query_render(VALUE self, EVT_HANDLE event)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  if (winevtQuery->renderAsXML) {
    return render_to_rb_str(event, EvtRenderEventXml);
  } else {
    return render_system_event(event);
  }
}

static VALUE
rb_winevt_query_message(EVT_HANDLE event)
{
  WCHAR* wResult;
  VALUE utf8str;

  wResult = get_description(event);
  utf8str = wstr_to_rb_str(CP_UTF8, wResult, -1);
  free(wResult);

  return utf8str;
}

static VALUE
rb_winevt_query_string_inserts(EVT_HANDLE event)
{
  return get_values(event);
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
  else
    rb_raise(rb_eArgError, "Unknown seek flag: %s", flag_str);

  return 0;
}

/*
 * This function specify seek strategy.
 *
 * @param bookmark_or_flag [Bookmark|Query::Flag]
 * @return [Boolean]
 */
static VALUE
rb_winevt_query_seek(VALUE self, VALUE bookmark_or_flag)
{
  struct WinevtQuery* winevtQuery;
  struct WinevtBookmark* winevtBookmark = NULL;
  DWORD flag = 0;

  switch (TYPE(bookmark_or_flag)) {
    case T_SYMBOL:
      flag = get_evt_seek_flag_from_cstr(RSTRING_PTR(rb_sym2str(bookmark_or_flag)));
      break;
    case T_STRING:
      flag = get_evt_seek_flag_from_cstr(StringValueCStr(bookmark_or_flag));
      break;
    case T_FIXNUM:
      flag = NUM2LONG(bookmark_or_flag);
      break;
    default:
      if (!rb_obj_is_kind_of(bookmark_or_flag, rb_cBookmark))
        rb_raise(rb_eArgError, "Expected a String or a Symbol or a Bookmark instance");

      winevtBookmark = EventBookMark(bookmark_or_flag);
  }

  if (winevtBookmark) {
    TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);
    if (EvtSeek(winevtQuery->query,
                winevtQuery->offset,
                winevtBookmark->bookmark,
                winevtQuery->timeout,
                EvtSeekRelativeToBookmark))
      return Qtrue;
  } else {
    TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);
    if (EvtSeek(
          winevtQuery->query, winevtQuery->offset, NULL, winevtQuery->timeout, flag)) {
      return Qtrue;
    }
  }

  return Qfalse;
}

static VALUE
rb_winevt_query_close_handle(VALUE self)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  for (int i = 0; i < winevtQuery->count; i++){
    if (winevtQuery->hEvents[i] != NULL) {
      EvtClose(winevtQuery->hEvents[i]);
      winevtQuery->hEvents[i] = NULL;
    }
  }

  return Qnil;
}

static VALUE
rb_winevt_query_each_yield(VALUE self)
{
  RETURN_ENUMERATOR(self, 0, 0);

  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  for (int i = 0; i < winevtQuery->count; i++) {
    rb_yield_values(3,
                    rb_winevt_query_render(self, winevtQuery->hEvents[i]),
                    rb_winevt_query_message(winevtQuery->hEvents[i]),
                    rb_winevt_query_string_inserts(winevtQuery->hEvents[i]));
  }
  return Qnil;
}

/*
 * Enumerate to obtain Windows EventLog contains.
 *
 * @yield ([String],[String],[String])
 *
 */
static VALUE
rb_winevt_query_each(VALUE self)
{
  RETURN_ENUMERATOR(self, 0, 0);

  while (rb_winevt_query_next(self)) {
    rb_ensure(rb_winevt_query_each_yield, self, rb_winevt_query_close_handle, self);
  }

  return Qnil;
}

/*
 * This function returns whether render as xml or not.
 *
 * @return [Boolean]
 */
static VALUE
rb_winevt_query_render_as_xml_p(VALUE self)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  return winevtQuery->renderAsXML ? Qtrue : Qfalse;
}

/*
 * This function specifies whether render as xml or not.
 *
 * @param rb_render_as_xml [Boolean]
 */
static VALUE
rb_winevt_query_set_render_as_xml(VALUE self, VALUE rb_render_as_xml)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->renderAsXML = RTEST(rb_render_as_xml);

  return Qnil;
}

void
Init_winevt_query(VALUE rb_cEventLog)
{
  rb_cQuery = rb_define_class_under(rb_cEventLog, "Query", rb_cObject);
  rb_define_alloc_func(rb_cQuery, rb_winevt_query_alloc);

  rb_cFlag = rb_define_module_under(rb_cQuery, "Flag");

  rb_define_const(rb_cFlag, "RelativeToFirst", LONG2NUM(EvtSeekRelativeToFirst));
  rb_define_const(rb_cFlag, "RelativeToLast", LONG2NUM(EvtSeekRelativeToLast));
  rb_define_const(rb_cFlag, "RelativeToCurrent", LONG2NUM(EvtSeekRelativeToCurrent));
  rb_define_const(rb_cFlag, "RelativeToBookmark", LONG2NUM(EvtSeekRelativeToBookmark));
  rb_define_const(rb_cFlag, "OriginMask", LONG2NUM(EvtSeekOriginMask));
  rb_define_const(rb_cFlag, "Strict", LONG2NUM(EvtSeekStrict));

  rb_define_method(rb_cQuery, "initialize", rb_winevt_query_initialize, 2);
  rb_define_method(rb_cQuery, "next", rb_winevt_query_next, 0);
  rb_define_method(rb_cQuery, "seek", rb_winevt_query_seek, 1);
  rb_define_method(rb_cQuery, "offset", rb_winevt_query_get_offset, 0);
  rb_define_method(rb_cQuery, "offset=", rb_winevt_query_set_offset, 1);
  rb_define_method(rb_cQuery, "timeout", rb_winevt_query_get_timeout, 0);
  rb_define_method(rb_cQuery, "timeout=", rb_winevt_query_set_timeout, 1);
  rb_define_method(rb_cQuery, "each", rb_winevt_query_each, 0);
  rb_define_method(rb_cQuery, "render_as_xml?", rb_winevt_query_render_as_xml_p, 0);
  rb_define_method(rb_cQuery, "render_as_xml=", rb_winevt_query_set_render_as_xml, 1);
}
