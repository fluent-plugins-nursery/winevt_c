#include <winevt_c.h>
#include <sddl.h>
#include <stdlib.h>

char*
wstr_to_mbstr(UINT cp, const WCHAR *wstr, int clen)
{
    char *ptr;
    int len = WideCharToMultiByte(cp, 0, wstr, clen, NULL, 0, NULL, NULL);
    if (!(ptr = xmalloc(len))) return 0;
    WideCharToMultiByte(cp, 0, wstr, clen, ptr, len, NULL, NULL);

    return ptr;
}

void free_allocated_mbstr(const char* str)
{
  if (str)
    xfree((char *)str);
}

WCHAR* render_event(EVT_HANDLE handle, DWORD flags)
{
  PWSTR      buffer = NULL;
  ULONG      bufferSize = 0;
  ULONG      bufferSizeNeeded = 0;
  ULONG      status, count;
  static WCHAR* result = L"";
  LPTSTR     msgBuf;

  do {
    if (bufferSizeNeeded > bufferSize) {
      free(buffer);
      bufferSize = bufferSizeNeeded;
      buffer = xmalloc(bufferSize);
      if (buffer == NULL) {
        status = ERROR_OUTOFMEMORY;
        bufferSize = 0;
        rb_raise(rb_eWinevtQueryError, "Out of memory");
        break;
      }
    }

    if (EvtRender(NULL,
                  handle,
                  flags,
                  bufferSize,
                  buffer,
                  &bufferSizeNeeded,
                  &count) != FALSE) {
      status = ERROR_SUCCESS;
    } else {
      status = GetLastError();
    }
  } while (status == ERROR_INSUFFICIENT_BUFFER);

  if (status != ERROR_SUCCESS) {
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgBuf, 0, NULL);

    VALUE errmsg = rb_str_new2(msgBuf);
    LocalFree(msgBuf);

    rb_raise(rb_eWinevtQueryError, "ErrorCode: %d\nError: %s\n", status, RSTRING_PTR(errmsg));
  }

  result = _wcsdup(buffer);

  if (buffer)
    xfree(buffer);

  return result;
}

VALUE get_values(EVT_HANDLE handle)
{
  PWSTR buffer = NULL;
  ULONG bufferSize = 0;
  ULONG bufferSizeNeeded = 0;
  DWORD status, propCount = 0;
  char *result = "";
  LPTSTR msgBuf;
  WCHAR* tmpWChar = NULL;
  VALUE userValues = rb_ary_new();

  static PCWSTR eventProperties[] = { L"Event/EventData/Data[1]" };
  EVT_HANDLE renderContext = EvtCreateRenderContext(0, NULL, EvtRenderContextUser);
  if (renderContext == NULL) {
    rb_raise(rb_eWinevtQueryError, "Failed to create renderContext");
  }

  do {
    if (bufferSizeNeeded > bufferSize) {
      free(buffer);
      bufferSize = bufferSizeNeeded;
      buffer = xmalloc(bufferSize);
      if (buffer == NULL) {
        status = ERROR_OUTOFMEMORY;
        bufferSize = 0;
        rb_raise(rb_eWinevtQueryError, "Out of memory");
        break;
      }
    }

    if (EvtRender(renderContext,
                  handle,
                  EvtRenderEventValues,
                  bufferSize,
                  buffer,
                  &bufferSizeNeeded,
                  &propCount) != FALSE) {
      status = ERROR_SUCCESS;
    } else {
      status = GetLastError();
    }
  } while (status == ERROR_INSUFFICIENT_BUFFER);

  if (status != ERROR_SUCCESS) {
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgBuf, 0, NULL);

    VALUE errmsg = rb_str_new2(msgBuf);
    LocalFree(msgBuf);

    rb_raise(rb_eWinevtQueryError, "ErrorCode: %d\nError: %s\n", status, RSTRING_PTR(errmsg));
  }

  PEVT_VARIANT pRenderedValues = (PEVT_VARIANT)buffer;
  LARGE_INTEGER timestamp;
  SYSTEMTIME st;
  FILETIME ft;
  CHAR strTime[128];
  VALUE rbObj;

  for (int i = 0; i < propCount; i++) {
    switch (pRenderedValues[i].Type) {
    case EvtVarTypeNull:
      rb_ary_push(userValues, Qnil);
      break;
    case EvtVarTypeString:
      if (pRenderedValues[i].StringVal == NULL) {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("(NULL)"));
      } else {
        result = wstr_to_mbstr(CP_UTF8, pRenderedValues[i].StringVal, -1);
        rb_ary_push(userValues, rb_utf8_str_new_cstr(result));
        free_allocated_mbstr(result);
      }
      break;
    case EvtVarTypeAnsiString:
      if (pRenderedValues[i].AnsiStringVal == NULL) {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("(NULL)"));
      } else {
        rb_ary_push(userValues, rb_utf8_str_new_cstr((char *)pRenderedValues[i].AnsiStringVal));
      }
      break;
    case EvtVarTypeSByte:
      rbObj = INT2NUM((INT32)pRenderedValues[i].SByteVal);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeByte:
      rbObj = INT2NUM((UINT32)pRenderedValues[i].ByteVal);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeInt16:
      rbObj = INT2NUM((INT32)pRenderedValues[i].Int16Val);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeUInt16:
      rbObj = UINT2NUM((UINT32)pRenderedValues[i].UInt16Val);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeInt32:
      rbObj = INT2NUM(pRenderedValues[i].Int32Val);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeUInt32:
      rbObj = UINT2NUM(pRenderedValues[i].UInt32Val);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeInt64:
      rbObj = LONG2NUM(pRenderedValues[i].Int64Val);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeUInt64:
      rbObj = ULONG2NUM(pRenderedValues[i].UInt64Val);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeSingle:
      sprintf(result, "%f", pRenderedValues[i].SingleVal);
      rb_ary_push(userValues, rb_utf8_str_new_cstr(result));
      free_allocated_mbstr(result);
      break;
    case EvtVarTypeDouble:
      sprintf(result, "%lf", pRenderedValues[i].DoubleVal);
      rb_ary_push(userValues, rb_utf8_str_new_cstr(result));
      break;
    case EvtVarTypeBoolean:
      result = pRenderedValues[i].BooleanVal ? "true" : "false";
      rb_ary_push(userValues, rb_utf8_str_new_cstr(result));
      break;
    case EvtVarTypeGuid:
      if (pRenderedValues[i].GuidVal != NULL) {
        StringFromCLSID(pRenderedValues[i].GuidVal, &tmpWChar);
        result = wstr_to_mbstr(CP_UTF8, tmpWChar, -1);
        rb_ary_push(userValues, rb_utf8_str_new_cstr(result));
        free_allocated_mbstr(result);
        CoTaskMemFree(tmpWChar);
      } else {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
      }
      break;
    case EvtVarTypeSizeT:
      rbObj = SIZET2NUM(pRenderedValues[i].SizeTVal);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeFileTime:
      timestamp.QuadPart = pRenderedValues[i].FileTimeVal;
      ft.dwHighDateTime = timestamp.HighPart;
      ft.dwLowDateTime  = timestamp.LowPart;
      if (FileTimeToSystemTime( &ft, &st )) {
        sprintf(strTime, "%04d-%02d-%02d %02d:%02d:%02d.%dZ",
                st.wYear , st.wMonth , st.wDay ,
                st.wHour , st.wMinute , st.wSecond,
                st.wMilliseconds);
        rb_ary_push(userValues, rb_utf8_str_new_cstr(strTime));
      } else {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
      }
      break;
    case EvtVarTypeSysTime:
      if (pRenderedValues[i].SysTimeVal != NULL) {
        st = *pRenderedValues[i].SysTimeVal;
        sprintf(strTime, "%04d-%02d-%02d %02d:%02d:%02d.%dZ",
                st.wYear , st.wMonth , st.wDay ,
                st.wHour , st.wMinute , st.wSecond,
                st.wMilliseconds);
        rb_ary_push(userValues, rb_utf8_str_new_cstr(strTime));
      } else {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
      }
      break;
    case EvtVarTypeSid:
      if (ConvertSidToStringSidW(pRenderedValues[i].SidVal, &tmpWChar)) {
        result = wstr_to_mbstr(CP_UTF8, tmpWChar, -1);
        rb_ary_push(userValues, rb_utf8_str_new_cstr(result));
        free_allocated_mbstr(result);
        LocalFree(tmpWChar);
      } else {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
      }
      break;
    case EvtVarTypeHexInt32:
      rbObj = ULONG2NUM(pRenderedValues[i].UInt32Val);
      rbObj = rb_sprintf("%#x", rbObj);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeHexInt64:
      rbObj = ULONG2NUM(pRenderedValues[i].UInt64Val);
      rbObj = rb_sprintf("%#x", rbObj);
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeEvtXml:
      if (pRenderedValues[i].XmlVal == NULL) {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("(NULL)"));
      } else {
        result = wstr_to_mbstr(CP_UTF8, pRenderedValues[i].XmlVal, -1);
        rb_ary_push(userValues, rb_utf8_str_new_cstr(result));
        free_allocated_mbstr(result);
      }
      break;
    default:
      rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
      break;
    }
  }

  if (buffer)
    xfree(buffer);

  if (renderContext)
    EvtClose(renderContext);

  return userValues;
}

static WCHAR* get_message(EVT_HANDLE hMetadata, EVT_HANDLE handle)
{
#define BUFSIZE 4096
  static WCHAR* result = L"";
  ULONG  status;
  ULONG bufferSizeNeeded = 0;
  LPVOID lpMsgBuf;
  WCHAR*     prevBuffer;
  WCHAR     *message;
  WCHAR     *reallocatedMessage;

  message = (WCHAR *)xmalloc(sizeof(WCHAR) * BUFSIZE);
  if (!EvtFormatMessage(hMetadata, handle, 0xffffffff, 0, NULL, EvtFormatMessageEvent, BUFSIZE, message, &bufferSizeNeeded)) {
    status = GetLastError();

    if (status != ERROR_EVT_UNRESOLVED_VALUE_INSERT) {
      switch (status) {
      case ERROR_EVT_MESSAGE_NOT_FOUND:
      case ERROR_EVT_MESSAGE_ID_NOT_FOUND:
      case ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND:
      case ERROR_RESOURCE_LANG_NOT_FOUND:
      case ERROR_MUI_FILE_NOT_FOUND:
      case ERROR_EVT_UNRESOLVED_PARAMETER_INSERT: {
        if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                           FORMAT_MESSAGE_FROM_SYSTEM |
                           FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL,
                           status,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           (WCHAR *) &lpMsgBuf, 0, NULL) == 0)
          FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                         FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS,
                         NULL,
                         status,
                         MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                         (WCHAR *) &lpMsgBuf, 0, NULL);

        result = _wcsdup((WCHAR *)lpMsgBuf);
        LocalFree(lpMsgBuf);

        goto cleanup;
      }

      }

      if (status != ERROR_INSUFFICIENT_BUFFER)
        rb_raise(rb_eWinevtQueryError, "ErrorCode: %d", status);
    }

    if (status == ERROR_INSUFFICIENT_BUFFER) {
      prevBuffer = message;
      reallocatedMessage = (WCHAR *)realloc(prevBuffer, sizeof(WCHAR) * bufferSizeNeeded);
      if (reallocatedMessage == NULL) {
        rb_raise(rb_eWinevtQueryError, "Reallocation failed.");
      }
      message = reallocatedMessage;

      if(!EvtFormatMessage(hMetadata, handle, 0xffffffff, 0, NULL, EvtFormatMessageEvent, bufferSizeNeeded, message, &bufferSizeNeeded)) {
        status = GetLastError();

        if (status != ERROR_EVT_UNRESOLVED_VALUE_INSERT) {
          switch (status) {
          case ERROR_EVT_MESSAGE_NOT_FOUND:
          case ERROR_EVT_MESSAGE_ID_NOT_FOUND:
          case ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND:
          case ERROR_RESOURCE_LANG_NOT_FOUND:
          case ERROR_MUI_FILE_NOT_FOUND:
          case ERROR_EVT_UNRESOLVED_PARAMETER_INSERT:
            if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL,
                               status,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               (WCHAR *) &lpMsgBuf, 0, NULL) == 0)
              FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                             FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL,
                             status,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                             (WCHAR *) &lpMsgBuf, 0, NULL);

            result = _wcsdup((WCHAR *)lpMsgBuf);
            LocalFree(lpMsgBuf);

            goto cleanup;
          }

          rb_raise(rb_eWinevtQueryError, "ErrorCode: %d", status);
        }
      }
    }
  }

  result = _wcsdup(message);

cleanup:

  if (message)
    xfree(message);

  return result;

#undef BUFSIZE
}

WCHAR* get_description(EVT_HANDLE handle)
{
#define BUFSIZE 4096
  WCHAR      buffer[BUFSIZE];
  ULONG      bufferSize = 0;
  ULONG      bufferSizeNeeded = 0;
  ULONG      status, count;
  static WCHAR *result = L"";
  LPTSTR     msgBuf = "";
  EVT_HANDLE hMetadata = NULL;
  PEVT_VARIANT values = NULL;

  static PCWSTR eventProperties[] = {L"Event/System/Provider/@Name"};
  EVT_HANDLE renderContext = EvtCreateRenderContext(1, eventProperties, EvtRenderContextValues);
  if (renderContext == NULL) {
    rb_raise(rb_eWinevtQueryError, "Failed to create renderContext");
  }

  if (EvtRender(renderContext,
                handle,
                EvtRenderEventValues,
                _countof(buffer),
                buffer,
                &bufferSizeNeeded,
                &count) != FALSE) {
    status = ERROR_SUCCESS;
  } else {
    status = GetLastError();
  }

  if (status != ERROR_SUCCESS) {
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgBuf, 0, NULL);

    VALUE errmsg = rb_str_new2(msgBuf);
    LocalFree(msgBuf);

    rb_raise(rb_eWinevtQueryError, "ErrorCode: %d\nError: %s\n", status, RSTRING_PTR(errmsg));
  }

  // Obtain buffer as EVT_VARIANT pointer. To avoid ErrorCide 87 in EvtRender.
  values = (PEVT_VARIANT)buffer;

  // Open publisher metadata
  hMetadata = EvtOpenPublisherMetadata(NULL, values[0].StringVal, NULL, MAKELCID(MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), SORT_DEFAULT), 0);
  if (hMetadata == NULL) {
    // When winevt_c cannot open metadata, then give up to obtain
    // message file and clean up immediately.
    goto cleanup;
  }

  result = _wcsdup(get_message(hMetadata, handle));

#undef BUFSIZE

cleanup:

  if (renderContext)
    EvtClose(renderContext);

  if (hMetadata)
    EvtClose(hMetadata);

  return result;
}
