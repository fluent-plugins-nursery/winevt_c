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

char* render_event(EVT_HANDLE handle, DWORD flags)
{
  PWSTR      buffer = NULL;
  ULONG      bufferSize = 0;
  ULONG      bufferSizeNeeded = 0;
  EVT_HANDLE event;
  ULONG      status, count;
  char*      errBuf;
  char*      result;
  LPTSTR     msgBuf;

  do {
    if (bufferSizeNeeded > bufferSize) {
      free(buffer);
      bufferSize = bufferSizeNeeded;
      buffer = malloc(bufferSize);
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

  result = wstr_to_mbstr(CP_UTF8, buffer, -1);

  if (buffer)
    free(buffer);

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
      buffer = malloc(bufferSize);
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
      }
      break;
    default:
      rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
      break;
    }
  }

  if (buffer)
    free(buffer);

  return userValues;
}

char* get_description(EVT_HANDLE handle)
{
#define MAX_BUFFER 65535
  WCHAR      buffer[4096], *msg = buffer;
  WCHAR      descriptionBuffer[MAX_BUFFER];
  ULONG      bufferSize = 0;
  ULONG      bufferSizeNeeded = 0;
  EVT_HANDLE event;
  ULONG      status, count;
  char*      result = "";
  LPTSTR     msgBuf = "";
  EVT_HANDLE hMetadata = NULL;
  PEVT_VARIANT values = NULL;
  PWSTR pwBuffer = NULL;

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

  if (!EvtFormatMessage(hMetadata, handle, 0xffffffff, 0, NULL, EvtFormatMessageEvent, 4096, buffer, &bufferSizeNeeded)) {
    status = GetLastError();

    if (status != ERROR_EVT_UNRESOLVED_VALUE_INSERT) {
      switch (status) {
      case ERROR_EVT_MESSAGE_NOT_FOUND:
      case ERROR_EVT_MESSAGE_ID_NOT_FOUND:
      case ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND:
      case ERROR_RESOURCE_LANG_NOT_FOUND:
      case ERROR_MUI_FILE_NOT_FOUND:
      case ERROR_EVT_UNRESOLVED_PARAMETER_INSERT:
        return "";
      }

      if (status != ERROR_INSUFFICIENT_BUFFER)
        rb_raise(rb_eWinevtQueryError, "ErrorCode: %d", status);
    }

    if (status == ERROR_INSUFFICIENT_BUFFER) {
      msg = (WCHAR *)malloc(sizeof(WCHAR) * bufferSizeNeeded);

      if(!EvtFormatMessage(hMetadata, handle, 0xffffffff, 0, NULL, EvtFormatMessageEvent, bufferSizeNeeded, msg, &bufferSizeNeeded)) {
        status = GetLastError();

        if (status != ERROR_EVT_UNRESOLVED_VALUE_INSERT) {
          switch (status) {
          case ERROR_EVT_MESSAGE_NOT_FOUND:
          case ERROR_EVT_MESSAGE_ID_NOT_FOUND:
          case ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND:
          case ERROR_RESOURCE_LANG_NOT_FOUND:
          case ERROR_MUI_FILE_NOT_FOUND:
          case ERROR_EVT_UNRESOLVED_PARAMETER_INSERT:
            return "";
          }

          rb_raise(rb_eWinevtQueryError, "ErrorCode: %d", status);
        }
      }
    }
  }
  result = wstr_to_mbstr(CP_UTF8, msg, -1);

#undef MAX_BUFFER

cleanup:

  if (renderContext)
    EvtClose(renderContext);

  if (hMetadata)
    EvtClose(hMetadata);

  return result;
}
