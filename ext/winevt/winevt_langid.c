#include <winevt_c.h>

static LocaleInfo localeStrTable [] = {
    { MAKELANGID(LANG_BULGARIAN,  SUBLANG_DEFAULT),              "bg_BG"},
    { MAKELANGID(LANG_CHINESE,    SUBLANG_CHINESE_SIMPLIFIED),   "zh_CN"},
    { MAKELANGID(LANG_CHINESE,    SUBLANG_CHINESE_TRADITIONAL),  "zh_TW"},
    { MAKELANGID(LANG_CHINESE,    SUBLANG_CHINESE_HONGKONG),     "zh_HK"},
    { MAKELANGID(LANG_CHINESE,    SUBLANG_CHINESE_SINGAPORE),    "zh_SG"},
    { MAKELANGID(LANG_CROATIAN,   SUBLANG_DEFAULT),              "hr_HR"},
    { MAKELANGID(LANG_CZECH,      SUBLANG_DEFAULT),              "cs_CZ"},
    { MAKELANGID(LANG_DANISH,     SUBLANG_DEFAULT),              "da_DK"},
    { MAKELANGID(LANG_DUTCH,      SUBLANG_DUTCH),                "nl_NL"},
    { MAKELANGID(LANG_DUTCH,      SUBLANG_DUTCH_BELGIAN),        "nl_BE"},
    { MAKELANGID(LANG_ENGLISH,    SUBLANG_DEFAULT),              "en_US"},
    { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_UK),           "en_GB"},
    { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_AUS),          "en_AU"},
    { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_CAN),          "en_CA"},
    { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_NZ),           "en_NZ"},
    { MAKELANGID(LANG_ENGLISH,    SUBLANG_ENGLISH_EIRE),         "en_IE"},
    { MAKELANGID(LANG_FINNISH,    SUBLANG_DEFAULT),              "fi_FI"},
    { MAKELANGID(LANG_FRENCH,     SUBLANG_FRENCH),               "fr_FR"},
    { MAKELANGID(LANG_FRENCH,     SUBLANG_FRENCH_BELGIAN),       "fr_BE"},
    { MAKELANGID(LANG_FRENCH,     SUBLANG_FRENCH_CANADIAN),      "fr_CA"},
    { MAKELANGID(LANG_FRENCH,     SUBLANG_FRENCH_SWISS),         "fr_CH"},
    { MAKELANGID(LANG_GERMAN,     SUBLANG_GERMAN),               "de_DE"},
    { MAKELANGID(LANG_GERMAN,     SUBLANG_GERMAN_SWISS),         "de_CH"},
    { MAKELANGID(LANG_GERMAN,     SUBLANG_GERMAN_AUSTRIAN),      "de_AT"},
    { MAKELANGID(LANG_GREEK,      SUBLANG_DEFAULT),              "el_GR"},
    { MAKELANGID(LANG_HUNGARIAN,  SUBLANG_DEFAULT),              "hu_HU"},
    { MAKELANGID(LANG_ICELANDIC,  SUBLANG_DEFAULT),              "is_IS"},
    { MAKELANGID(LANG_ITALIAN,    SUBLANG_ITALIAN),              "it_IT"},
    { MAKELANGID(LANG_ITALIAN,    SUBLANG_ITALIAN_SWISS),        "it_CH"},
    { MAKELANGID(LANG_JAPANESE,   SUBLANG_DEFAULT),              "ja_JP"},
    { MAKELANGID(LANG_KOREAN,     SUBLANG_DEFAULT),              "ko_KO"},
    { MAKELANGID(LANG_NORWEGIAN,  SUBLANG_NORWEGIAN_BOKMAL),     "no_NO"},
    { MAKELANGID(LANG_NORWEGIAN,  SUBLANG_NORWEGIAN_BOKMAL),     "nb_NO"},
    { MAKELANGID(LANG_NORWEGIAN,  SUBLANG_NORWEGIAN_NYNORSK),    "nn_NO"},
    { MAKELANGID(LANG_POLISH,     SUBLANG_DEFAULT),              "pl_PL"},
    { MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE),           "pt_PT"},
    { MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN), "pt_BR"},
    { MAKELANGID(LANG_ROMANIAN,   SUBLANG_DEFAULT),              "ro_RO"},
    { MAKELANGID(LANG_RUSSIAN,    SUBLANG_DEFAULT),              "ru_RU"},
    { MAKELANGID(LANG_SLOVAK,     SUBLANG_DEFAULT),              "sk_SK"},
    { MAKELANGID(LANG_SLOVENIAN,  SUBLANG_DEFAULT),              "sl_SI"},
    { MAKELANGID(LANG_SPANISH,    SUBLANG_SPANISH),              "es_ES"},
    { MAKELANGID(LANG_SPANISH,    SUBLANG_SPANISH),              "es_ES_T"},
    { MAKELANGID(LANG_SPANISH,    SUBLANG_SPANISH_MEXICAN),      "es_MX"},
    { MAKELANGID(LANG_SPANISH,    SUBLANG_SPANISH_MODERN),       "es_ES_M"},
    { MAKELANGID(LANG_SWEDISH,    SUBLANG_DEFAULT),              "sv_SE"},
    { MAKELANGID(LANG_TURKISH,    SUBLANG_DEFAULT),              "tr_TR"},
    { 0, NULL}
};

const LocaleInfo default_locale = {MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), "neutral"};

LocaleInfo
get_locale_from_rb_str(VALUE rb_locale_str)
{
  CHAR* locale_str = StringValuePtr(rb_locale_str);
  int localeStrTableSize = _countof(localeStrTable);

  for (int i = 0; i < localeStrTableSize; i++) {
    if (stricmp(localeStrTable[i].langCode, locale_str) == 0) {
      return localeStrTable[i];
    }
  }

  return default_locale;
}
