#ifndef _WINEVT_C_H_
#define _WINEVT_C_H_

#include <ruby.h>
#include <ruby/encoding.h>

#ifdef __GNUC__
# include <w32api.h>
# define MINIMUM_WINDOWS_VERSION WindowsVista
#else /* __GNUC__ */
# define MINIMUM_WINDOWS_VERSION 0x0600 /* Vista */
#endif /* __GNUC__ */

#ifdef _WIN32_WINNT
#  undef WIN32_WINNT
#endif /* WIN32_WINNT */
#define _WIN32_WINNT MINIMUM_WINDOWS_VERSION

#include <winevt.h>
#define EventQuery(object) ((struct WinevtQuery *)DATA_PTR(object))
#define EventBookMark(object) ((struct WinevtBookmark *)DATA_PTR(object))
#define EventChannel(object) ((struct WinevtChannel *)DATA_PTR(object))

char* wstr_to_mbstr(UINT cp, const WCHAR *wstr, int clen);
char* render_event(EVT_HANDLE handle, DWORD flags);

VALUE rb_cQuery;
VALUE rb_cChannel;
VALUE rb_cBookmark;
VALUE rb_cSubscribe;
VALUE rb_eWinevtQueryError;

struct WinevtChannel {
  EVT_HANDLE channels;
};

struct WinevtBookmark {
  EVT_HANDLE bookmark;
  ULONG      count;
};

struct WinevtQuery {
  EVT_HANDLE query;
  EVT_HANDLE event;
  ULONG      count;
  LONG       offset;
  LONG       timeout;
};

struct WinevtSubscribe {
  HANDLE     signalEvent;
  EVT_HANDLE subscription;
  EVT_HANDLE bookmark;
  EVT_HANDLE event;
  DWORD      flags;
  BOOL       tailing;
};

void Init_winevt_query(VALUE rb_cEventLog);
void Init_winevt_channel(VALUE rb_cEventLog);
void Init_winevt_bookmark(VALUE rb_cEventLog);
void Init_winevt_subscribe(VALUE rb_cEventLog);

#endif // _WINEVT_C_H
