#include <winevt_c.h>


/* clang-format off */
/*
 * Document-class: Winevt::EventLog::Locale
 *
 * handle locales for Windows EventLog's description.
 *
 * @example
 *  require 'winevt'
 *
 *  @locale = Winevt::EventLog::Locale.new
 *  @locale.each {|code, desc|
 *    print code, desc
 *  }
 * @since v0.8.1
 */
/* clang-format on */

VALUE rb_cLocale;

static void locale_free(void* ptr);

static const rb_data_type_t rb_winevt_locale_type = { "winevt/locale",
                                                       {
                                                         0,
                                                         locale_free,
                                                         0,
                                                       },
                                                       NULL,
                                                       NULL,
                                                       RUBY_TYPED_FREE_IMMEDIATELY };

static void
locale_free(void* ptr)
{
  xfree(ptr);
}

static VALUE
rb_winevt_locale_alloc(VALUE klass)
{
  VALUE obj;
  struct WinevtLocale* winevtLocale;
  obj = TypedData_Make_Struct(
    klass, struct WinevtLocale, &rb_winevt_locale_type, winevtLocale);
  return obj;
}

/*
 * Initalize Locale class.
 *
 * @return [Locale]
 *
 */
static VALUE
rb_winevt_locale_initialize(VALUE self)
{
  return Qnil;
}

/*
 * Enumerate supported locales and its descriptions
 *
 * @yield (String, String)
 *
 */
static VALUE
rb_winevt_locale_each(VALUE self)
{
  RETURN_ENUMERATOR(self, 0, 0);

  for (int i = 0; localeInfoTable[i].langCode != NULL; i++) {
    rb_yield_values(2,
                    rb_utf8_str_new_cstr(localeInfoTable[i].langCode),
                    rb_utf8_str_new_cstr(localeInfoTable[i].description));
  }

  return Qnil;
}

void
Init_winevt_locale(VALUE rb_cEventLog)
{
  rb_cLocale = rb_define_class_under(rb_cEventLog, "Locale", rb_cObject);

  rb_define_alloc_func(rb_cLocale, rb_winevt_locale_alloc);

  rb_define_method(rb_cLocale, "initialize", rb_winevt_locale_initialize, 0);
  rb_define_method(rb_cLocale, "each", rb_winevt_locale_each, 0);
}
