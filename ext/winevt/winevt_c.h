#ifndef _WINEVT_C_H_
#define _WINEVT_C_H_

#include <ruby.h>
#include <ruby/encoding.h>

#ifdef __GNUC__
#include <w32api.h>
#define MINIMUM_WINDOWS_VERSION WindowsVista
#else                                  /* __GNUC__ */
#define MINIMUM_WINDOWS_VERSION 0x0600 /* Vista */
#endif                                 /* __GNUC__ */

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif /* WIN32_WINNT */
#define _WIN32_WINNT MINIMUM_WINDOWS_VERSION

#if !defined(HAVE_RB_ALLOCV)
#define ALLOCV     RB_ALLOCV
#define ALLOCV_N   RB_ALLOCV_N
#endif

#include <time.h>
#include <winevt.h>
#define EventQuery(object) ((struct WinevtQuery*)DATA_PTR(object))
#define EventBookMark(object) ((struct WinevtBookmark*)DATA_PTR(object))
#define EventChannel(object) ((struct WinevtChannel*)DATA_PTR(object))
#define EventSession(object) ((struct WinevtSession*)DATA_PTR(object))

typedef struct {
  LANGID langID;
  CHAR* langCode;
  CHAR* description;
} LocaleInfo;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

VALUE wstr_to_rb_str(UINT cp, const WCHAR* wstr, int clen);
#if defined(__cplusplus)
[[ noreturn ]]
#endif /* __cplusplus */
void raise_system_error(VALUE error, DWORD errorCode);
void raise_channel_not_found_error(VALUE channelPath);
VALUE render_to_rb_str(EVT_HANDLE handle, DWORD flags);
EVT_HANDLE connect_to_remote(LPWSTR computerName, LPWSTR domain,
                             LPWSTR username, LPWSTR password,
                             EVT_RPC_LOGIN_FLAGS flags,
                             DWORD *error_code);
WCHAR* get_description(EVT_HANDLE handle, LANGID langID, EVT_HANDLE hRemote);
VALUE get_values(EVT_HANDLE handle);
VALUE render_system_event(EVT_HANDLE handle, BOOL preserve_qualifiers, BOOL preserveSID);
LocaleInfo* get_locale_info_from_rb_str(VALUE rb_locale_str);

#ifdef __cplusplus
}
#endif /* __cplusplus */

extern VALUE rb_cQuery;
extern VALUE rb_cFlag;
extern VALUE rb_cChannel;
extern VALUE rb_cBookmark;
extern VALUE rb_cSubscribe;
extern VALUE rb_eWinevtQueryError;
extern VALUE rb_eChannelNotFoundError;
extern VALUE rb_eRemoteHandlerError;
extern VALUE rb_eSubscribeHandlerError;
extern VALUE rb_cLocale;
extern VALUE rb_cSession;

struct WinevtSession {
  LPWSTR server;
  LPWSTR domain;
  LPWSTR username;
  LPWSTR password;
  EVT_RPC_LOGIN_FLAGS flags;
};

extern LocaleInfo localeInfoTable[];
extern LocaleInfo default_locale;

struct WinevtLocale {};

struct WinevtChannel
{
  EVT_HANDLE channels;
  BOOL force_enumerate;
};

struct WinevtBookmark
{
  EVT_HANDLE bookmark;
  ULONG count;
};

#define QUERY_ARRAY_SIZE 10

struct WinevtQuery
{
  EVT_HANDLE query;
  EVT_HANDLE hEvents[QUERY_ARRAY_SIZE];
  ULONG count;
  LONG offset;
  LONG timeout;
  BOOL renderAsXML;
  BOOL preserveQualifiers;
  BOOL preserveSID;
  LocaleInfo *localeInfo;
  EVT_HANDLE remoteHandle;
};

#define SUBSCRIBE_ARRAY_SIZE 10
#define SUBSCRIBE_RATE_INFINITE -1

struct WinevtSubscribe
{
  HANDLE signalEvent;
  EVT_HANDLE subscription;
  EVT_HANDLE bookmark;
  EVT_HANDLE hEvents[SUBSCRIBE_ARRAY_SIZE];
  DWORD count;
  DWORD flags;
  BOOL readExistingEvents;
  DWORD rateLimit;
  time_t lastTime;
  DWORD currentRate;
  BOOL renderAsXML;
  BOOL preserveQualifiers;
  BOOL preserveSID;
  LocaleInfo* localeInfo;
  EVT_HANDLE remoteHandle;
};

void Init_winevt_query(VALUE rb_cEventLog);
void Init_winevt_channel(VALUE rb_cEventLog);
void Init_winevt_bookmark(VALUE rb_cEventLog);
void Init_winevt_subscribe(VALUE rb_cEventLog);
void Init_winevt_locale(VALUE rb_cEventLog);
void Init_winevt_session(VALUE rb_cEventLog);

#endif // _WINEVT_C_H
