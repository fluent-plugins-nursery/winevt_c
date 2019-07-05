#include <winevt_c.h>
#include <sddl.h>
#include <stdlib.h>
#include <string>
#include <memory>

char*
wstr_to_mbstr(UINT cp, const WCHAR *wstr, int clen)
{
    char *ptr;
    int len = WideCharToMultiByte(cp, 0, wstr, clen, nullptr, 0, nullptr, nullptr);
    if (!(ptr = static_cast<char *>(xmalloc(len)))) return nullptr;
    WideCharToMultiByte(cp, 0, wstr, clen, ptr, len, nullptr, nullptr);

    return ptr;
}

void free_allocated_mbstr(const char* str)
{
  if (str)
    xfree((char *)str);
}

VALUE
wstr_to_rb_str(UINT cp, const WCHAR *wstr, int clen)
{
    int len = WideCharToMultiByte(cp, 0, wstr, clen, nullptr, 0, nullptr, nullptr);
    std::unique_ptr<char[]> ptr(new char[len]);
    WideCharToMultiByte(cp, 0, wstr, clen, ptr.get(), len, nullptr, nullptr);
    VALUE str = rb_utf8_str_new_cstr(ptr.get());

    return str;
}

WCHAR* render_event(EVT_HANDLE handle, DWORD flags)
{
  std::unique_ptr<WCHAR[]> buffer(new WCHAR[0]);
  ULONG      bufferSize = 0;
  ULONG      bufferSizeNeeded = 0;
  ULONG      status, count;
  static WCHAR* result;
  LPTSTR     msgBuf;

  do {
    if (bufferSizeNeeded > bufferSize) {
      bufferSize = bufferSizeNeeded;
      buffer.reset(new WCHAR[bufferSize]);
      if (buffer == nullptr) {
        status = ERROR_OUTOFMEMORY;
        bufferSize = 0;
        rb_raise(rb_eWinevtQueryError, "Out of memory");
        break;
      }
    }

    if (EvtRender(nullptr,
                  handle,
                  flags,
                  bufferSize,
                  buffer.get(),
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
        nullptr, status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgBuf, 0, nullptr);

    VALUE errmsg = rb_str_new2(msgBuf);
    LocalFree(msgBuf);

    rb_raise(rb_eWinevtQueryError, "ErrorCode: %ld\nError: %s\n", status, RSTRING_PTR(errmsg));
  }

  result = _wcsdup(buffer.get());

  return result;
}

static std::wstring guid_to_wstr(const GUID& guid) {
  LPOLESTR p = nullptr;
  if (FAILED(StringFromCLSID(guid, &p))) {
    return nullptr;
  }
  std::wstring s(p);
  CoTaskMemFree(p);
  return s;
}

VALUE get_values(EVT_HANDLE handle)
{
  std::wstring buffer;
  ULONG bufferSize = 0;
  ULONG bufferSizeNeeded = 0;
  DWORD status, propCount = 0;
  char *result;
  LPTSTR msgBuf;
  WCHAR* tmpWChar = nullptr;
  VALUE userValues = rb_ary_new();

  static PCWSTR eventProperties[] = { L"Event/EventData/Data[1]" };
  EVT_HANDLE renderContext = EvtCreateRenderContext(0, nullptr, EvtRenderContextUser);
  if (renderContext == nullptr) {
    rb_raise(rb_eWinevtQueryError, "Failed to create renderContext");
  }

  do {
    if (bufferSizeNeeded > bufferSize) {
      bufferSize = bufferSizeNeeded;
      try {
        buffer.resize(bufferSize);
      } catch (std::bad_alloc e) {
        status = ERROR_OUTOFMEMORY;
        bufferSize = 0;
        rb_raise(rb_eWinevtQueryError, "Out of memory");
        break;
      }
    }

    if (EvtRender(renderContext,
                  handle,
                  EvtRenderEventValues,
                  buffer.size(),
                  &buffer[0],
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
        nullptr, status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgBuf, 0, nullptr);

    VALUE errmsg = rb_str_new2(msgBuf);
    LocalFree(msgBuf);

    rb_raise(rb_eWinevtQueryError, "ErrorCode: %lu\nError: %s\n", status, RSTRING_PTR(errmsg));
  }

  PEVT_VARIANT pRenderedValues = reinterpret_cast<PEVT_VARIANT>(const_cast<WCHAR *>(buffer.c_str()));
  LARGE_INTEGER timestamp;
  SYSTEMTIME st;
  FILETIME ft;
  CHAR strTime[128];
  std::unique_ptr<CHAR> sResult(new CHAR[256]);
  VALUE rbObj;

  for (int i = 0; i < propCount; i++) {
    switch (pRenderedValues[i].Type) {
    case EvtVarTypeNull:
      rb_ary_push(userValues, Qnil);
      break;
    case EvtVarTypeString:
      if (pRenderedValues[i].StringVal == nullptr) {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("(NULL)"));
      } else {
        std::wstring wStr(pRenderedValues[i].StringVal);
        rbObj = wstr_to_rb_str(CP_UTF8, wStr.c_str(), -1);
        rb_ary_push(userValues, rbObj);
      }
      break;
    case EvtVarTypeAnsiString:
      if (pRenderedValues[i].AnsiStringVal == nullptr) {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("(NULL)"));
      } else {
        rb_ary_push(userValues, rb_utf8_str_new_cstr(const_cast<char *>(pRenderedValues[i].AnsiStringVal)));
      }
      break;
    case EvtVarTypeSByte:
      rbObj = INT2NUM(static_cast<UINT32>(pRenderedValues[i].SByteVal));
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeByte:
      rbObj = INT2NUM(static_cast<UINT32>(pRenderedValues[i].ByteVal));
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeInt16:
      rbObj = INT2NUM(static_cast<INT32>(pRenderedValues[i].Int16Val));
      rb_ary_push(userValues, rbObj);
      break;
    case EvtVarTypeUInt16:
      rbObj = UINT2NUM(static_cast<UINT32>(pRenderedValues[i].UInt16Val));
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
      sprintf(sResult.get(), "%f", pRenderedValues[i].SingleVal);
      rb_ary_push(userValues, rb_utf8_str_new_cstr(sResult.get()));
      break;
    case EvtVarTypeDouble:
      sprintf(sResult.get(), "%lf", pRenderedValues[i].DoubleVal);
      rb_ary_push(userValues, rb_utf8_str_new_cstr(sResult.get()));
      break;
    case EvtVarTypeBoolean:
      result = const_cast<char *>(pRenderedValues[i].BooleanVal ? "true" : "false");
      rb_ary_push(userValues, rb_utf8_str_new_cstr(result));
      break;
    case EvtVarTypeGuid:
      if (pRenderedValues[i].GuidVal != nullptr) {
        const GUID guid = *pRenderedValues[i].GuidVal;
        std::wstring wstr = guid_to_wstr(guid);
        rbObj = wstr_to_rb_str(CP_UTF8, wstr.c_str(), -1);
        rb_ary_push(userValues, rbObj);
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
      if (pRenderedValues[i].SysTimeVal != nullptr) {
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
        rbObj = wstr_to_rb_str(CP_UTF8, tmpWChar, -1);
        rb_ary_push(userValues, rbObj);
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
      if (pRenderedValues[i].XmlVal == nullptr) {
        rb_ary_push(userValues, rb_utf8_str_new_cstr("(NULL)"));
      } else {
        rbObj = wstr_to_rb_str(CP_UTF8, pRenderedValues[i].XmlVal, -1);
        rb_ary_push(userValues, rbObj);
      }
      break;
    default:
      rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
      break;
    }
  }

  if (renderContext)
    EvtClose(renderContext);

  return userValues;
}

static std::wstring get_message(EVT_HANDLE hMetadata, EVT_HANDLE handle)
{
#define BUFSIZE 4096
  std::wstring result;
  ULONG  status;
  ULONG bufferSizeNeeded = 0;
  LPVOID lpMsgBuf;
  std::wstring message(BUFSIZE, '\0');

  if (!EvtFormatMessage(hMetadata, handle, 0xffffffff, 0, nullptr, EvtFormatMessageEvent, message.size(), &message[0], &bufferSizeNeeded)) {
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
                           nullptr,
                           status,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           reinterpret_cast<WCHAR *>(&lpMsgBuf), 0, nullptr) == 0)
          FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                         FORMAT_MESSAGE_FROM_SYSTEM |
                         FORMAT_MESSAGE_IGNORE_INSERTS,
                         nullptr,
                         status,
                         MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                         reinterpret_cast<WCHAR *>(&lpMsgBuf), 0, nullptr);

        result = reinterpret_cast<WCHAR *>(lpMsgBuf);
        LocalFree(lpMsgBuf);

        goto cleanup;
      }

      }

      if (status != ERROR_INSUFFICIENT_BUFFER)
        rb_raise(rb_eWinevtQueryError, "ErrorCode: %lu", status);
    }

    if (status == ERROR_INSUFFICIENT_BUFFER) {
      message.resize(bufferSizeNeeded);

      if(!EvtFormatMessage(hMetadata, handle, 0xffffffff, 0, nullptr, EvtFormatMessageEvent, message.size(), &message[0], &bufferSizeNeeded)) {
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
                               nullptr,
                               status,
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               reinterpret_cast<WCHAR *>(&lpMsgBuf), 0, nullptr) == 0)
              FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                             FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr,
                             status,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                             reinterpret_cast<WCHAR *>(&lpMsgBuf), 0, nullptr);

            result = reinterpret_cast<WCHAR *>(lpMsgBuf);
            LocalFree(lpMsgBuf);

            goto cleanup;
          }

          rb_raise(rb_eWinevtQueryError, "ErrorCode: %lu", status);
        }
      }
    }
  }

  result = message;

cleanup:

  return std::wstring(result);

#undef BUFSIZE
}

WCHAR* get_description(EVT_HANDLE handle)
{
#define BUFSIZE 4096
  std::wstring buffer(BUFSIZE, '\0');
  ULONG      bufferSize = 0;
  ULONG      bufferSizeNeeded = 0;
  ULONG      status, count;
  std::wstring result;
  LPTSTR     msgBuf;
  EVT_HANDLE hMetadata = nullptr;

  static PCWSTR eventProperties[] = {L"Event/System/Provider/@Name"};
  EVT_HANDLE renderContext = EvtCreateRenderContext(1, eventProperties, EvtRenderContextValues);
  if (renderContext == nullptr) {
    rb_raise(rb_eWinevtQueryError, "Failed to create renderContext");
  }

  if (EvtRender(renderContext,
                handle,
                EvtRenderEventValues,
                buffer.size(),
                &buffer[0],
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
        nullptr, status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgBuf, 0, nullptr);

    VALUE errmsg = rb_str_new2(msgBuf);
    LocalFree(msgBuf);

    rb_raise(rb_eWinevtQueryError, "ErrorCode: %lu\nError: %s\n", status, RSTRING_PTR(errmsg));
  }

  // Obtain buffer as EVT_VARIANT pointer. To avoid ErrorCide 87 in EvtRender.
  const PEVT_VARIANT values = reinterpret_cast<PEVT_VARIANT>(const_cast<WCHAR *>(buffer.c_str()));

  // Open publisher metadata
  hMetadata = EvtOpenPublisherMetadata(nullptr, values[0].StringVal, nullptr, MAKELCID(MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), SORT_DEFAULT), 0);
  if (hMetadata == nullptr) {
    // When winevt_c cannot open metadata, then give up to obtain
    // message file and clean up immediately.
    goto cleanup;
  }

  result = get_message(hMetadata, handle);

#undef BUFSIZE

cleanup:

  if (renderContext)
    EvtClose(renderContext);

  if (hMetadata)
    EvtClose(hMetadata);

  return _wcsdup(result.c_str());
}
