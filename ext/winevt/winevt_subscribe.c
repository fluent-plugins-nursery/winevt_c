#include <winevt_c.h>

/* clang-format off */
/*
 * Document-class: Winevt::EventLog::Subscribe
 *
 * Subscribe Windows EventLog channel.
 *
 * @example
 *  require 'winevt'
 *
 *  @subscribe = Winevt::EventLog::Subscribe.new
 *  @subscribe.tail = true
 *  @subscribe.rate_limit = 80
 *  @subscribe.subscribe(
 *    "Application", "*[System[(Level <= 4) and TimeCreated[timediff(@SystemTime) <= 86400000]]]"
 *  )
 *  while true do
 *    @subscribe.each do |eventlog, message, string_inserts|
 *      puts ({eventlog: eventlog, data: message})
 *    end
 *    sleep(0.1)
 *  end
 *
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winevt/nf-winevt-evtsubscribe
 */
/* clang-format on */

static void subscribe_free(void* ptr);

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
close_handles(struct WinevtSubscribe* winevtSubscribe)
{
  if (winevtSubscribe->signalEvent) {
    CloseHandle(winevtSubscribe->signalEvent);
    winevtSubscribe->signalEvent = NULL;
  }

  if (winevtSubscribe->subscription) {
    EvtClose(winevtSubscribe->subscription);
    winevtSubscribe->subscription = NULL;
  }

  if (winevtSubscribe->bookmark) {
    EvtClose(winevtSubscribe->bookmark);
    winevtSubscribe->bookmark = NULL;
  }

  for (int i = 0; i < winevtSubscribe->count; i++) {
    if (winevtSubscribe->hEvents[i]) {
      EvtClose(winevtSubscribe->hEvents[i]);
      winevtSubscribe->hEvents[i] = NULL;
    }
  }
  winevtSubscribe->count = 0;

  if (winevtSubscribe->remoteHandle) {
    EvtClose(winevtSubscribe->remoteHandle);
    winevtSubscribe->remoteHandle = NULL;
  }
}

static void
subscribe_free(void* ptr)
{
  struct WinevtSubscribe* winevtSubscribe = (struct WinevtSubscribe*)ptr;
  close_handles(winevtSubscribe);

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

/*
 * Initalize Subscribe class.
 *
 * @return [Subscribe]
 *
 */
static VALUE
rb_winevt_subscribe_initialize(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  winevtSubscribe->rateLimit = SUBSCRIBE_RATE_INFINITE;
  winevtSubscribe->lastTime = 0;
  winevtSubscribe->currentRate = 0;
  winevtSubscribe->renderAsXML = TRUE;
  winevtSubscribe->readExistingEvents = TRUE;
  winevtSubscribe->preserveQualifiers = FALSE;
  winevtSubscribe->localeInfo = &default_locale;

  return Qnil;
}

/*
 * This method specifies whether read existing events or not.
 *
 * @param rb_read_existing_events_p [Boolean]
 */
static VALUE
rb_winevt_subscribe_set_read_existing_events(VALUE self, VALUE rb_read_existing_events_p)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  winevtSubscribe->readExistingEvents = RTEST(rb_read_existing_events_p);

  return Qnil;
}

/*
 * This method returns whether read existing events or not.
 *
 * @return [Boolean]
 */
static VALUE
rb_winevt_subscribe_read_existing_events_p(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  return winevtSubscribe->readExistingEvents ? Qtrue : Qfalse;
}

/*
 * Subscribe into a Windows EventLog channel.
 *
 * @overload subscribe(path, query, bookmark=nil, session=nil)
 *   @param path [String] Subscribe Channel
 *   @param query [String] Query string for channel
 *   @param bookmark [Bookmark] bookmark Bookmark class instance.
 *   @param session [Session] Session information for remoting access.
 * @return [Boolean]
 *
 */
static VALUE
rb_winevt_subscribe_subscribe(int argc, VALUE* argv, VALUE self)
{
  VALUE rb_path, rb_query, rb_bookmark, rb_session;
  EVT_HANDLE hSubscription = NULL, hBookmark = NULL;
  HANDLE hSignalEvent;
  EVT_HANDLE hRemoteHandle = NULL;
  DWORD len, flags = 0L;
  DWORD err = ERROR_SUCCESS;
  VALUE wpathBuf, wqueryBuf, wBookmarkBuf;
  PWSTR path, query, bookmarkXml;
  DWORD status = ERROR_SUCCESS;
  struct WinevtSession* winevtSession;
  struct WinevtSubscribe* winevtSubscribe;

  hSignalEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  rb_scan_args(argc, argv, "22", &rb_path, &rb_query, &rb_bookmark, &rb_session);
  Check_Type(rb_path, T_STRING);
  Check_Type(rb_query, T_STRING);

  if (rb_obj_is_kind_of(rb_bookmark, rb_cString)) {
    // bookmarkXml : To wide char
    len = MultiByteToWideChar(
        CP_UTF8, 0, RSTRING_PTR(rb_bookmark), RSTRING_LEN(rb_bookmark), NULL, 0);
    bookmarkXml = ALLOCV_N(WCHAR, wBookmarkBuf, len + 1);
    MultiByteToWideChar(CP_UTF8,
                        0,
                        RSTRING_PTR(rb_bookmark),
                        RSTRING_LEN(rb_bookmark),
                        bookmarkXml,
                        len);
    bookmarkXml[len] = L'\0';
    hBookmark = EvtCreateBookmark(bookmarkXml);
    ALLOCV_END(wBookmarkBuf);
    if (hBookmark == NULL) {
      status = GetLastError();
      raise_system_error(rb_eWinevtQueryError, status);
    }
  }
  if (rb_obj_is_kind_of(rb_session, rb_cSession)) {
    winevtSession = EventSession(rb_session);
    hRemoteHandle = connect_to_remote(winevtSession->server,
                                      winevtSession->domain,
                                      winevtSession->username,
                                      winevtSession->password,
                                      winevtSession->flags);

    err = GetLastError();
    if (err != ERROR_SUCCESS) {
      raise_system_error(rb_eRuntimeError, err);
    }
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
  } else if (winevtSubscribe->readExistingEvents) {
    flags |= EvtSubscribeStartAtOldestRecord;
  } else {
    flags |= EvtSubscribeToFutureEvents;
  }

  hSubscription =
    EvtSubscribe(hRemoteHandle, hSignalEvent, path, query, hBookmark, NULL, NULL, flags);
  if (!hSubscription) {
    status = GetLastError();
    if (hBookmark != NULL) {
      EvtClose(hBookmark);
    }
    if (hSignalEvent != NULL) {
      CloseHandle(hSignalEvent);
    }
    if (rb_obj_is_kind_of(rb_session, rb_cSession)) {
      rb_raise(rb_eRemoteHandlerError, "Remoting subscription is not working. errCode: %ld\n", status);
    } else {
      raise_system_error(rb_eWinevtQueryError, status);
    }
  }

  if (winevtSubscribe->subscription != NULL) {
    // should be disgarded the old event subscription handle.
    EvtClose(winevtSubscribe->subscription);
  }

  ALLOCV_END(wpathBuf);
  ALLOCV_END(wqueryBuf);

  winevtSubscribe->signalEvent = hSignalEvent;
  winevtSubscribe->subscription = hSubscription;
  winevtSubscribe->remoteHandle = hRemoteHandle;
  if (hBookmark) {
    winevtSubscribe->bookmark = hBookmark;
  } else {
    winevtSubscribe->bookmark = EvtCreateBookmark(NULL);
    if (winevtSubscribe->bookmark == NULL) {
      status = GetLastError();
      if (hSubscription != NULL) {
        EvtClose(hSubscription);
      }
      if (hSignalEvent != NULL) {
        CloseHandle(hSignalEvent);
      }
      raise_system_error(rb_eWinevtQueryError, status);
    }
  }

  return Qtrue;
}

BOOL
is_rate_limit_exceeded(struct WinevtSubscribe* winevtSubscribe)
{
  time_t now;

  if (winevtSubscribe->rateLimit == SUBSCRIBE_RATE_INFINITE)
    return FALSE;

  time(&now);

  if (now <= winevtSubscribe->lastTime) {
    if (winevtSubscribe->currentRate >= winevtSubscribe->rateLimit) {
      return TRUE;
    }
  } else {
    winevtSubscribe->currentRate = 0;
  }

  return FALSE;
}

void
update_to_reflect_rate_limit_state(struct WinevtSubscribe* winevtSubscribe, ULONG count)
{
  time_t lastTime = 0;

  if (winevtSubscribe->rateLimit == SUBSCRIBE_RATE_INFINITE)
    return;

  time(&lastTime);
  winevtSubscribe->lastTime = lastTime;
  winevtSubscribe->currentRate += count;
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
rb_winevt_subscribe_next(VALUE self)
{
  EVT_HANDLE hEvents[SUBSCRIBE_ARRAY_SIZE];
  ULONG count = 0;
  DWORD status = ERROR_SUCCESS;
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  if (is_rate_limit_exceeded(winevtSubscribe)) {
    return Qfalse;
  }

  /* If subscription handle is NULL, it should return false. */
  if (!winevtSubscribe->subscription) {
    return Qfalse;
  }

  if (!EvtNext(winevtSubscribe->subscription,
               SUBSCRIBE_ARRAY_SIZE,
               hEvents,
               INFINITE,
               0,
               &count)) {
    status = GetLastError();
    if (ERROR_CANCELLED == status) {
      return Qfalse;
    }
    if (ERROR_NO_MORE_ITEMS != status) {
      return Qfalse;
    }
  }

  if (status == ERROR_SUCCESS) {
    winevtSubscribe->count = count;
    for (int i = 0; i < count; i++) {
      winevtSubscribe->hEvents[i] = hEvents[i];
      EvtUpdateBookmark(winevtSubscribe->bookmark, winevtSubscribe->hEvents[i]);
    }

    update_to_reflect_rate_limit_state(winevtSubscribe, count);

    return Qtrue;
  }

  return Qfalse;
}

static VALUE
rb_winevt_subscribe_render(VALUE self, EVT_HANDLE event)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  if (winevtSubscribe->renderAsXML) {
    return render_to_rb_str(event, EvtRenderEventXml);
  } else {
    return render_system_event(event, winevtSubscribe->preserveQualifiers);
  }
}

static VALUE
rb_winevt_subscribe_message(EVT_HANDLE event, LocaleInfo* localeInfo, EVT_HANDLE hRemote)
{
  WCHAR* wResult;
  VALUE utf8str;

  wResult = get_description(event, localeInfo->langID, hRemote);
  utf8str = wstr_to_rb_str(CP_UTF8, wResult, -1);
  free(wResult);

  return utf8str;
}

static VALUE
rb_winevt_subscribe_string_inserts(EVT_HANDLE event)
{
  return get_values(event);
}

static VALUE
rb_winevt_subscribe_close_handle(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  for (int i = 0; i < winevtSubscribe->count; i++) {
    if (winevtSubscribe->hEvents[i] != NULL) {
      EvtClose(winevtSubscribe->hEvents[i]);
      winevtSubscribe->hEvents[i] = NULL;
    }
  }

  return Qnil;
}

static VALUE
rb_winevt_subscribe_each_yield(VALUE self)
{
  RETURN_ENUMERATOR(self, 0, 0);
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  for (int i = 0; i < winevtSubscribe->count; i++) {
    rb_yield_values(3,
                    rb_winevt_subscribe_render(self, winevtSubscribe->hEvents[i]),
                    rb_winevt_subscribe_message(winevtSubscribe->hEvents[i], winevtSubscribe->localeInfo,
                                                winevtSubscribe->remoteHandle),
                    rb_winevt_subscribe_string_inserts(winevtSubscribe->hEvents[i]));
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
rb_winevt_subscribe_each(VALUE self)
{
  RETURN_ENUMERATOR(self, 0, 0);

  while (rb_winevt_subscribe_next(self)) {
    rb_ensure(
      rb_winevt_subscribe_each_yield, self, rb_winevt_subscribe_close_handle, self);
  }

  return Qnil;
}

/*
 * This method renders bookmark content which is related to Subscribe class instance.
 *
 * @return [String]
 */
static VALUE
rb_winevt_subscribe_get_bookmark(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  return render_to_rb_str(winevtSubscribe->bookmark, EvtRenderBookmark);
}

/*
 * This method returns rate limit value.
 *
 * @since 0.6.0
 * @return [Integer]
 */
static VALUE
rb_winevt_subscribe_get_rate_limit(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  return INT2NUM(winevtSubscribe->rateLimit);
}

/*
 * This method specifies rate limit value.
 *
 * @since 0.6.0
 * @param rb_rate_limit [Integer] rate_limit value
 */
static VALUE
rb_winevt_subscribe_set_rate_limit(VALUE self, VALUE rb_rate_limit)
{
  struct WinevtSubscribe* winevtSubscribe;
  DWORD rateLimit;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  rateLimit = NUM2LONG(rb_rate_limit);

  if ((rateLimit != SUBSCRIBE_RATE_INFINITE) && (rateLimit < 10 || rateLimit % 10)) {
    rb_raise(rb_eArgError, "Specify a multiples of 10 or RATE_INFINITE constant");
  } else {
    winevtSubscribe->rateLimit = rateLimit;
  }

  return Qnil;
}

/*
 * This method returns whether render as xml or not.
 *
 * @since 0.6.0
 * @return [Boolean]
 */
static VALUE
rb_winevt_subscribe_render_as_xml_p(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  return winevtSubscribe->renderAsXML ? Qtrue : Qfalse;
}

/*
 * This method specifies whether render as xml or not.
 *
 * @since 0.6.0
 * @param rb_render_as_xml [Boolean]
 */
static VALUE
rb_winevt_subscribe_set_render_as_xml(VALUE self, VALUE rb_render_as_xml)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  winevtSubscribe->renderAsXML = RTEST(rb_render_as_xml);

  return Qnil;
}

/*
 * This method specifies whether preserving qualifiers key or not.
 *
 * @since 0.7.3
 * @param rb_preserve_qualifiers [Boolean]
 */
static VALUE
rb_winevt_subscribe_set_preserve_qualifiers(VALUE self, VALUE rb_preserve_qualifiers)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  winevtSubscribe->preserveQualifiers = RTEST(rb_preserve_qualifiers);

  return Qnil;
}

/*
 * This method returns whether preserving qualifiers or not.
 *
 * @since 0.7.3
 * @return [Integer]
 */
static VALUE
rb_winevt_subscribe_get_preserve_qualifiers_p(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  return winevtSubscribe->preserveQualifiers ? Qtrue : Qfalse;
}

/*
 * This method specifies locale with [String].
 *
 * @since 0.8.0
 * @param rb_locale_str [String]
 */
static VALUE
rb_winevt_subscribe_set_locale(VALUE self, VALUE rb_locale_str)
{
  struct WinevtSubscribe* winevtSubscribe;
  LocaleInfo* locale_info = &default_locale;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  locale_info = get_locale_info_from_rb_str(rb_locale_str);

  winevtSubscribe->localeInfo = locale_info;

  return Qnil;
}

/*
 * This method obtains specified locale with [String].
 *
 * @since 0.8.0
 */
static VALUE
rb_winevt_subscribe_get_locale(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  if (winevtSubscribe->localeInfo->langCode) {
    return rb_str_new2(winevtSubscribe->localeInfo->langCode);
  } else {
    return rb_str_new2(default_locale.langCode);
  }
}

/*
 * This method cancels channel subscription.
 *
 * @return [Boolean]
 * @since 0.9.1
 */
static VALUE
rb_winevt_subscribe_cancel(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;
  BOOL result = FALSE;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  if (winevtSubscribe->subscription) {
    result = EvtCancel(winevtSubscribe->subscription);
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
rb_winevt_subscribe_close(VALUE self)
{
  struct WinevtSubscribe* winevtSubscribe;

  TypedData_Get_Struct(
    self, struct WinevtSubscribe, &rb_winevt_subscribe_type, winevtSubscribe);

  close_handles(winevtSubscribe);

  return Qnil;
}


void
Init_winevt_subscribe(VALUE rb_cEventLog)
{
  rb_cSubscribe = rb_define_class_under(rb_cEventLog, "Subscribe", rb_cObject);

  rb_define_alloc_func(rb_cSubscribe, rb_winevt_subscribe_alloc);

  /*
   * For Subscribe#rate_limit=. It represents unspecified rate limit.
   * @since 0.6.0
   */
  rb_define_const(rb_cSubscribe, "RATE_INFINITE", SUBSCRIBE_RATE_INFINITE);

  rb_define_method(rb_cSubscribe, "initialize", rb_winevt_subscribe_initialize, 0);
  rb_define_method(rb_cSubscribe, "subscribe", rb_winevt_subscribe_subscribe, -1);
  rb_define_method(rb_cSubscribe, "next", rb_winevt_subscribe_next, 0);
  rb_define_method(rb_cSubscribe, "each", rb_winevt_subscribe_each, 0);
  rb_define_method(rb_cSubscribe, "bookmark", rb_winevt_subscribe_get_bookmark, 0);
  /*
   * @since 0.7.0
   */
  rb_define_method(rb_cSubscribe, "read_existing_events?", rb_winevt_subscribe_read_existing_events_p, 0);
  /*
   * @since 0.7.0
   */
  rb_define_method(rb_cSubscribe, "read_existing_events=", rb_winevt_subscribe_set_read_existing_events, 1);
  rb_define_method(rb_cSubscribe, "rate_limit", rb_winevt_subscribe_get_rate_limit, 0);
  rb_define_method(rb_cSubscribe, "rate_limit=", rb_winevt_subscribe_set_rate_limit, 1);
  rb_define_method(
    rb_cSubscribe, "render_as_xml?", rb_winevt_subscribe_render_as_xml_p, 0);
  rb_define_method(
    rb_cSubscribe, "render_as_xml=", rb_winevt_subscribe_set_render_as_xml, 1);
  /*
   * @since 0.7.3
   */
  rb_define_method(
    rb_cSubscribe, "preserve_qualifiers?", rb_winevt_subscribe_get_preserve_qualifiers_p, 0);
  /*
   * @since 0.7.3
   */
  rb_define_method(
    rb_cSubscribe, "preserve_qualifiers=", rb_winevt_subscribe_set_preserve_qualifiers, 1);
  /*
   * @since 0.8.0
   */
  rb_define_method(
    rb_cSubscribe, "locale", rb_winevt_subscribe_get_locale, 0);
  /*
   * @since 0.8.0
   */
  rb_define_method(
    rb_cSubscribe, "locale=", rb_winevt_subscribe_set_locale, 1);
  /*
   * @since 0.9.1
   */
  rb_define_method(
    rb_cSubscribe, "cancel", rb_winevt_subscribe_cancel, 0);
  /*
   * @since 0.9.1
   */
  rb_define_method(
    rb_cSubscribe, "close", rb_winevt_subscribe_close, 0);
}
