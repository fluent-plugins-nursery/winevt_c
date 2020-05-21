#include <winevt_c.h>

static LocaleInfo localeInfoTable [] = {
  { MAKELANGID(LANG_BULGARIAN,  SUBLANG_DEFAULT),              "bg_BG",   "Bulgarian"},
  { MAKELANGID(LANG_CHINESE,    SUBLANG_CHINESE_SIMPLIFIED),   "zh_CN",   "Simplified Chinese "},
  { MAKELANGID(LANG_CHINESE,    SUBLANG_CHINESE_TRADITIONAL),  "zh_TW",   "Traditional Chinese"},
  { MAKELANGID(LANG_CHINESE,    SUBLANG_CHINESE_HONGKONG),     "zh_HK",   "Chinese (Hong Kong)"},
  { MAKELANGID(LANG_CHINESE,    SUBLANG_CHINESE_SINGAPORE),    "zh_SG",   "Chinese (Singapore)"},
  { MAKELANGID(LANG_CROATIAN,   SUBLANG_DEFAULT),              "hr_HR",   "Croatian"},
  { MAKELANGID(LANG_CZECH,      SUBLANG_DEFAULT),              "cs_CZ",   "Czech"},
  { MAKELANGID(LANG_DANISH,     SUBLANG_DEFAULT),              "da_DK",   "Dannish"},
  { MAKELANGID(LANG_DUTCH,      SUBLANG_DUTCH),                "nl_NL",   "Dutch"},
  { MAKELANGID(LANG_DUTCH,      SUBLANG_DUTCH_BELGIAN),        "nl_BE",   "Dutch (Belgium)"},
  { MAKELANGID(LANG_ENGLISH,    SUBLANG_DEFAULT),              "en_US",   "English (United States)"},
  { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_UK),           "en_GB",   "English (UK)"},
  { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_AUS),          "en_AU",   "English (Australia)"},
  { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_CAN),          "en_CA",   "English (Canada)"},
  { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_NZ),           "en_NZ",   "English (New Zealand)"},
  { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_EIRE),         "en_IE",   "English (Ireland)"},
  { MAKELANGID(LANG_FINNISH,    SUBLANG_DEFAULT),              "fi_FI",   "Finnish"},
  { MAKELANGID(LANG_FRENCH,     SUBLANG_FRENCH),               "fr_FR",   "French"},
  { MAKELANGID(LANG_FRENCH,     SUBLANG_FRENCH_BELGIAN),       "fr_BE",   "French (Belgium)"},
  { MAKELANGID(LANG_FRENCH,     SUBLANG_FRENCH_CANADIAN),      "fr_CA",   "French (Canada)"},
  { MAKELANGID(LANG_FRENCH,     SUBLANG_FRENCH_SWISS),         "fr_CH",   "French (Swiss)"},
  { MAKELANGID(LANG_GERMAN,     SUBLANG_GERMAN),               "de_DE",   "German"},
  { MAKELANGID(LANG_GERMAN,     SUBLANG_GERMAN_SWISS),         "de_CH",   "German (Swiss)"},
  { MAKELANGID(LANG_GERMAN,     SUBLANG_GERMAN_AUSTRIAN),      "de_AT",   "German (Austria))"},
  { MAKELANGID(LANG_GREEK,      SUBLANG_DEFAULT),              "el_GR",   "Greek (Ελληνικά)"},
  { MAKELANGID(LANG_HUNGARIAN,  SUBLANG_DEFAULT),              "hu_HU",   "Hungarian"},
  { MAKELANGID(LANG_ICELANDIC,  SUBLANG_DEFAULT),              "is_IS",   "Icelandic"},
  { MAKELANGID(LANG_ITALIAN,    SUBLANG_ITALIAN),              "it_IT",   "Italian (Italy)"},
  { MAKELANGID(LANG_ITALIAN,    SUBLANG_ITALIAN_SWISS),        "it_CH",   "Italian (Swiss)"},
  { MAKELANGID(LANG_JAPANESE,   SUBLANG_DEFAULT),              "ja_JP",   "Japanases"},
  { MAKELANGID(LANG_KOREAN,     SUBLANG_DEFAULT),              "ko_KO",   "Korean"},
  { MAKELANGID(LANG_NORWEGIAN,  SUBLANG_NORWEGIAN_BOKMAL),     "no_NO",   "Norwegian (Bokmål)"},
  { MAKELANGID(LANG_NORWEGIAN,  SUBLANG_NORWEGIAN_BOKMAL),     "nb_NO",   "Norwegian (Bokmål)"},
  { MAKELANGID(LANG_NORWEGIAN,  SUBLANG_NORWEGIAN_NYNORSK),    "nn_NO",   "Norwegian (Nynorsk)"},
  { MAKELANGID(LANG_POLISH,     SUBLANG_DEFAULT),              "pl_PL",   "Polish"},
  { MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE),           "pt_PT",   "Portuguese"},
  { MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN), "pt_BR",   "Portuguese (Brazil)"},
  { MAKELANGID(LANG_ROMANIAN,   SUBLANG_DEFAULT),              "ro_RO",   "Romanian"},
  { MAKELANGID(LANG_RUSSIAN,    SUBLANG_DEFAULT),              "ru_RU",   "Russian  (русский язык)"},
  { MAKELANGID(LANG_SLOVAK,     SUBLANG_DEFAULT),              "sk_SK",   "Slovak"},
  { MAKELANGID(LANG_SLOVENIAN,  SUBLANG_DEFAULT),              "sl_SI",   "Slovenian"},
  { MAKELANGID(LANG_SPANISH,    SUBLANG_SPANISH),              "es_ES",   "Spanish"},
  { MAKELANGID(LANG_SPANISH,    SUBLANG_SPANISH),              "es_ES_T", "Spanish (Traditional)"},
  { MAKELANGID(LANG_SPANISH,    SUBLANG_SPANISH_MEXICAN),      "es_MX",   "Spanish (Mexico)"},
  { MAKELANGID(LANG_SPANISH,    SUBLANG_SPANISH_MODERN),       "es_ES_M", "Spanish (Modern)"},
  { MAKELANGID(LANG_SWEDISH,    SUBLANG_DEFAULT),              "sv_SE",   "Swedish"},
  { MAKELANGID(LANG_TURKISH,    SUBLANG_DEFAULT),              "tr_TR",   "Turkish"},
  { 0, NULL, NULL}
};

LocaleInfo default_locale = {MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), "neutral", "Default"};

LocaleInfo*
get_locale_from_rb_str(VALUE rb_locale_str)
{
  CHAR* locale_str = StringValuePtr(rb_locale_str);

  for (int i = 0; localeInfoTable[i].langCode != NULL; i++) {
    if (stricmp(localeInfoTable[i].langCode, locale_str) == 0) {
      return &localeInfoTable[i];
    }
  }

  rb_raise(rb_eArgError, "Unknown locale: %s", locale_str);
}

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
