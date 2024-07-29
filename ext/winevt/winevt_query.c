#include <winevt_c.h>

/* clang-format off */
/*
 * Document-class: Winevt::EventLog::Query
 *
 * Query Windows EventLog channel.
 *
 * @example
 *  require 'winevt'
 *
 *  @query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")
 *
 *  @query.each do |eventlog, message, string_inserts|
 *    puts ({eventlog: eventlog, data: message})
 *  end
 */
/* clang-format on */

VALUE rb_cFlag;

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
close_handles(struct WinevtQuery* winevtQuery)
{
  if (winevtQuery->query) {
    EvtClose(winevtQuery->query);
    winevtQuery->query = NULL;
  }

  for (int i = 0; i < winevtQuery->count; i++) {
    if (winevtQuery->hEvents[i]) {
      EvtClose(winevtQuery->hEvents[i]);
      winevtQuery->hEvents[i] = NULL;
    }
  }

  if (winevtQuery->remoteHandle) {
    EvtClose(winevtQuery->remoteHandle);
    winevtQuery->remoteHandle = NULL;
  }
}

static void
query_free(void* ptr)
{
  struct WinevtQuery* winevtQuery = (struct WinevtQuery*)ptr;
  close_handles(winevtQuery);

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
 * @overload initialize(channel, xpath, session=nil)
 *   @param channel [String] Querying EventLog channel.
 *   @param xpath [String] Querying XPath.
 *   @param session [Session] Session information for remoting access.
 * @return [Query]
 *
 */
static VALUE
rb_winevt_query_initialize(VALUE argc, VALUE *argv, VALUE self)
{
  PWSTR evtChannel, evtXPath;
  VALUE channel, xpath, session, rb_flags;
  struct WinevtQuery* winevtQuery;
  struct WinevtSession* winevtSession;
  EVT_HANDLE hRemoteHandle = NULL;
  DWORD len, flags = 0;
  VALUE wchannelBuf, wpathBuf;
  DWORD err = ERROR_SUCCESS;

  rb_scan_args(argc, argv, "22", &channel, &xpath, &session, &rb_flags);
  Check_Type(channel, T_STRING);
  Check_Type(xpath, T_STRING);

  if (rb_obj_is_kind_of(session, rb_cSession)) {
    winevtSession = EventSession(session);

    hRemoteHandle = connect_to_remote(winevtSession->server,
                                      winevtSession->domain,
                                      winevtSession->username,
                                      winevtSession->password,
                                      winevtSession->flags,
                                      &err);
    if (err != ERROR_SUCCESS) {
      raise_system_error(rb_eRuntimeError, err);
    }
  }

  switch (TYPE(rb_flags)) {
  case T_FIXNUM:
    flags = NUM2LONG(rb_flags);
    break;
  case T_NIL:
    flags = EvtQueryChannelPath | EvtQueryTolerateQueryErrors;
    break;
  default:
    rb_raise(rb_eArgError, "Expected a String, a Symbol, a Fixnum, or a NilClass instance");
  }

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
    hRemoteHandle, evtChannel, evtXPath, flags);
  err = GetLastError();
  if (err != ERROR_SUCCESS) {
    if (err == ERROR_EVT_CHANNEL_NOT_FOUND) {
      raise_channel_not_found_error(channel);
    }
    raise_system_error(rb_eRuntimeError, err);
  }
  winevtQuery->offset = 0L;
  winevtQuery->timeout = 0L;
  winevtQuery->renderAsXML = TRUE;
  winevtQuery->preserveQualifiers = FALSE;
  winevtQuery->localeInfo = &default_locale;
  winevtQuery->remoteHandle = hRemoteHandle;
  winevtQuery->preserveSID = FALSE;

  ALLOCV_END(wchannelBuf);
  ALLOCV_END(wpathBuf);

  return Qnil;
}

/*
 * This method returns querying event offset.
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
 * This method specifies querying event offset.
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
 * This method returns timeout value.
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
 * This method specifies timeout value.
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
 * Handle the next values. Since v0.6.0, this method is used for
 * testing only. Please use #each instead.
 *
 * @return [Boolean]
 *
 * @see each
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
    if (ERROR_CANCELLED == status) {
      return Qfalse;
    }
    if (ERROR_NO_MORE_ITEMS != status) {
      return Qfalse;
    }
  }

  if (status == ERROR_SUCCESS) {
    winevtQuery->count = count;
    for (int i = 0; i < count; i++) {
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
    return render_system_event(event, winevtQuery->preserveQualifiers,
                               winevtQuery->preserveSID);
  }
}

static VALUE
rb_winevt_query_message(EVT_HANDLE event, LocaleInfo* localeInfo, EVT_HANDLE hRemote)
{
  WCHAR* wResult;
  VALUE utf8str;

  wResult = get_description(event, localeInfo->langID, hRemote);
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
 * This method specifies seek strategy.
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

  for (int i = 0; i < winevtQuery->count; i++) {
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
                    rb_winevt_query_message(winevtQuery->hEvents[i], winevtQuery->localeInfo,
                                            winevtQuery->remoteHandle),
                    rb_winevt_query_string_inserts(winevtQuery->hEvents[i]));
  }
  return Qnil;
}

/*
 * Enumerate to obtain Windows EventLog contents.
 *
 * This method yields the following:
 * (Stringified EventLog, Stringified detail message, Stringified
 * insert values)
 *
 * @yield (String,String,String)
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
 * This method returns whether render as xml or not.
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
 * This method specifies whether render as xml or not.
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

/*
 * This method specifies whether preserving qualifiers key or not.
 *
 * @since 0.7.3
 * @param rb_preserve_qualifiers [Boolean]
 */
static VALUE
rb_winevt_query_set_preserve_qualifiers(VALUE self, VALUE rb_preserve_qualifiers)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(
    self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->preserveQualifiers = RTEST(rb_preserve_qualifiers);

  return Qnil;
}

/*
 * This method returns whether preserving qualifiers or not.
 *
 * @since 0.7.3
 * @return [Integer]
 */
static VALUE
rb_winevt_query_get_preserve_qualifiers_p(VALUE self)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(
    self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  return winevtQuery->preserveQualifiers ? Qtrue : Qfalse;
}

/*
 * This method specifies locale with [String].
 *
 * @since 0.8.0
 * @param rb_locale_str [String]
 */
static VALUE
rb_winevt_query_set_locale(VALUE self, VALUE rb_locale_str)
{
  struct WinevtQuery* winevtQuery;
  LocaleInfo* locale_info = &default_locale;

  TypedData_Get_Struct(
    self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  locale_info = get_locale_info_from_rb_str(rb_locale_str);

  winevtQuery->localeInfo = locale_info;

  return Qnil;
}

/*
 * This method obtains specified locale with [String].
 *
 * @since 0.8.0
 */
static VALUE
rb_winevt_query_get_locale(VALUE self)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(
    self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  if (winevtQuery->localeInfo->langCode) {
    return rb_str_new2(winevtQuery->localeInfo->langCode);
  } else {
    return rb_str_new2(default_locale.langCode);
  }
}

/*
 * This method specifies whether preserving SID or not.
 *
 * @param rb_preserve_sid_p [Boolean]
 */
static VALUE
rb_winevt_query_set_preserve_sid(VALUE self, VALUE rb_preserve_sid_p)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(
    self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  winevtQuery->preserveSID = RTEST(rb_preserve_sid_p);

  return Qnil;
}

/*
 * This method returns whether preserving SID or not.
 *
 * @return [Boolean]
 */
static VALUE
rb_winevt_query_preserve_sid_p(VALUE self)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(
    self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  return winevtQuery->preserveSID ? Qtrue : Qfalse;
}

/*
 * This method cancels channel query.
 *
 * @return [Boolean]
 * @since 0.9.1
 */
static VALUE
rb_winevt_query_cancel(VALUE self)
{
  struct WinevtQuery* winevtQuery;
  BOOL result = FALSE;

  TypedData_Get_Struct(
    self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  if (winevtQuery->query) {
    result = EvtCancel(winevtQuery->query);
  }

  if (result) {
    return Qtrue;
  } else {
    return Qfalse;
  }
}

/*
 * This method closes channel handles forcibly.
 *
 * @since 0.9.1
 */
static VALUE
rb_winevt_query_close(VALUE self)
{
  struct WinevtQuery* winevtQuery;

  TypedData_Get_Struct(
    self, struct WinevtQuery, &rb_winevt_query_type, winevtQuery);

  close_handles(winevtQuery);

  return Qnil;
}

void
Init_winevt_query(VALUE rb_cEventLog)
{
  rb_cQuery = rb_define_class_under(rb_cEventLog, "Query", rb_cObject);
  rb_define_alloc_func(rb_cQuery, rb_winevt_query_alloc);

  rb_cFlag = rb_define_module_under(rb_cQuery, "Flag");

  /* clang-format off */
  /*
   * EVT_SEEK_FLAGS enumeration: EvtSeekRelativeToFirst
   * @since 0.6.0
   * @see https://msdn.microsoft.com/en-us/windows/desktop/aa385575#EvtSeekRelativeToFirst
   */
  rb_define_const(rb_cFlag, "RelativeToFirst", LONG2NUM(EvtSeekRelativeToFirst));
  /*
   * EVT_SEEK_FLAGS enumeration: EvtSeekRelativeToLast
   * @since 0.6.0
   * @see https://msdn.microsoft.com/en-us/windows/desktop/aa385575#EvtSeekRelativeToLast
   */
  rb_define_const(rb_cFlag, "RelativeToLast", LONG2NUM(EvtSeekRelativeToLast));
  /*
   * EVT_SEEK_FLAGS enumeration: EvtSeekRelativeToCurrent
   * @since 0.6.0
   * @see https://msdn.microsoft.com/en-us/windows/desktop/aa385575#EvtSeekRelativeToCurrent
   */
  rb_define_const(rb_cFlag, "RelativeToCurrent", LONG2NUM(EvtSeekRelativeToCurrent));
  /*
   * EVT_SEEK_FLAGS enumeration: EvtSeekRelativeToBookmark
   * @since 0.6.0
   * @see https://msdn.microsoft.com/en-us/windows/desktop/aa385575#EvtSeekRelativeToBookmark
   */
  rb_define_const(rb_cFlag, "RelativeToBookmark", LONG2NUM(EvtSeekRelativeToBookmark));
  /*
   * EVT_SEEK_FLAGS enumeration: EvtSeekOriginMask
   * @since 0.6.0
   * @see https://msdn.microsoft.com/en-us/windows/desktop/aa385575#EvtSeekOriginMask
   */
  rb_define_const(rb_cFlag, "OriginMask", LONG2NUM(EvtSeekOriginMask));
  /*
   * EVT_SEEK_FLAGS enumeration: EvtSeekStrict
   * @since 0.6.0
   * @see https://msdn.microsoft.com/en-us/windows/desktop/aa385575#EvtSeekStrict
   */
  rb_define_const(rb_cFlag, "Strict", LONG2NUM(EvtSeekStrict));

  /*
   * EVT_QUERY_FLAGS enumeration: EvtQueryChannelPath
   * @since 0.10.3
   * @see https://learn.microsoft.com/en-us/windows/win32/api/winevt/ne-winevt-evt_query_flags
   */
  rb_define_const(rb_cFlag, "ChannelPath", LONG2NUM(EvtQueryChannelPath));
  /*
   * EVT_QUERY_FLAGS enumeration: EvtQueryFilePath
   * @since 0.10.3
   * @see https://learn.microsoft.com/en-us/windows/win32/api/winevt/ne-winevt-evt_query_flags
   */
  rb_define_const(rb_cFlag, "FilePath", LONG2NUM(EvtQueryFilePath));
  /*
   * EVT_QUERY_FLAGS enumeration: EvtQueryForwardDirection
   * @since 0.10.3
   * @see https://learn.microsoft.com/en-us/windows/win32/api/winevt/ne-winevt-evt_query_flags
   */
  rb_define_const(rb_cFlag, "ForwardDirection", LONG2NUM(EvtQueryForwardDirection));
  /*
   * EVT_QUERY_FLAGS enumeration: EvtQueryReverseDirection
   * @since 0.10.3
   * @see https://learn.microsoft.com/en-us/windows/win32/api/winevt/ne-winevt-evt_query_flags
   */
  rb_define_const(rb_cFlag, "ReverseDirection", LONG2NUM(EvtQueryReverseDirection));
  /*
   * EVT_QUERY_FLAGS enumeration: EvtSeekOriginMask
   * @since 0.10.3
   * @see https://learn.microsoft.com/en-us/windows/win32/api/winevt/ne-winevt-evt_query_flags
   */
  rb_define_const(rb_cFlag, "TolerateQueryErrors", LONG2NUM(EvtQueryTolerateQueryErrors));
  /* clang-format on */

  rb_define_method(rb_cQuery, "initialize", rb_winevt_query_initialize, -1);
  rb_define_method(rb_cQuery, "next", rb_winevt_query_next, 0);
  rb_define_method(rb_cQuery, "seek", rb_winevt_query_seek, 1);
  rb_define_method(rb_cQuery, "offset", rb_winevt_query_get_offset, 0);
  rb_define_method(rb_cQuery, "offset=", rb_winevt_query_set_offset, 1);
  rb_define_method(rb_cQuery, "timeout", rb_winevt_query_get_timeout, 0);
  rb_define_method(rb_cQuery, "timeout=", rb_winevt_query_set_timeout, 1);
  rb_define_method(rb_cQuery, "each", rb_winevt_query_each, 0);
  rb_define_method(rb_cQuery, "render_as_xml?", rb_winevt_query_render_as_xml_p, 0);
  rb_define_method(rb_cQuery, "render_as_xml=", rb_winevt_query_set_render_as_xml, 1);
  /*
   * @since 0.7.3
   */
  rb_define_method(rb_cQuery, "preserve_qualifiers?", rb_winevt_query_get_preserve_qualifiers_p, 0);
  /*
   * @since 0.7.3
   */
  rb_define_method(rb_cQuery, "preserve_qualifiers=", rb_winevt_query_set_preserve_qualifiers, 1);
  /*
   * @since 0.8.0
   */
  rb_define_method(rb_cQuery, "locale", rb_winevt_query_get_locale, 0);
  /*
   * @since 0.8.0
   */
  rb_define_method(rb_cQuery, "locale=", rb_winevt_query_set_locale, 1);
  /*
   * @since 0.10.3
   */
  rb_define_method(rb_cQuery, "preserve_sid?", rb_winevt_query_preserve_sid_p, 0);
  /*
   * @since 0.10.3
   */
  rb_define_method(rb_cQuery, "preserve_sid=", rb_winevt_query_set_preserve_sid, 1);
  /*
   * @since 0.9.1
   */
  rb_define_method(rb_cQuery, "cancel", rb_winevt_query_cancel, 0);
  /*
   * @since 0.9.1
   */
  rb_define_method(rb_cQuery, "close", rb_winevt_query_close, 0);
}
