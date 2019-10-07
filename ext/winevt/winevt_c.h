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

#include <time.h>
#include <winevt.h>
#define EventQuery(object) ((struct WinevtQuery*)DATA_PTR(object))
#define EventBookMark(object) ((struct WinevtBookmark*)DATA_PTR(object))
#define EventChannel(object) ((struct WinevtChannel*)DATA_PTR(object))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

VALUE wstr_to_rb_str(UINT cp, const WCHAR* wstr, int clen);
VALUE render_to_rb_str(EVT_HANDLE handle, DWORD flags);
WCHAR* get_description(EVT_HANDLE handle);
VALUE get_values(EVT_HANDLE handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

VALUE rb_cQuery;
VALUE rb_cChannel;
VALUE rb_cBookmark;
VALUE rb_cSubscribe;
VALUE rb_eWinevtQueryError;

struct WinevtChannel
{
  EVT_HANDLE channels;
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
  BOOL tailing;
  DWORD rateLimit;
  time_t lastTime;
  DWORD currentRate;
};

void Init_winevt_query(VALUE rb_cEventLog);
void Init_winevt_channel(VALUE rb_cEventLog);
void Init_winevt_bookmark(VALUE rb_cEventLog);
void Init_winevt_subscribe(VALUE rb_cEventLog);

#endif // _WINEVT_C_H
