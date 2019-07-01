#include <winevt_c.h>

static void bookmark_free(void *ptr);

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
  WCHAR* wResult;
  char* result;
  struct WinevtBookmark *winevtBookmark;
  VALUE utf8str;

  TypedData_Get_Struct(self, struct WinevtBookmark, &rb_winevt_bookmark_type, winevtBookmark);
  wResult = render_event(winevtBookmark->bookmark, EvtRenderBookmark);
  result = wstr_to_mbstr(CP_UTF8, wResult, -1);

  utf8str = rb_utf8_str_new_cstr(result);
  free_allocated_mbstr(result);
  if (wResult != NULL)
    free(wResult);

  return utf8str;
}

void Init_winevt_bookmark(VALUE rb_cEventLog)
{
  rb_cBookmark = rb_define_class_under(rb_cEventLog, "Bookmark", rb_cObject);

  rb_define_alloc_func(rb_cBookmark, rb_winevt_bookmark_alloc);
  rb_define_method(rb_cBookmark, "initialize", rb_winevt_bookmark_initialize, -1);
  rb_define_method(rb_cBookmark, "update", rb_winevt_bookmark_update, 1);
  rb_define_method(rb_cBookmark, "render", rb_winevt_bookmark_render, 0);
}
