#include <winevt_c.h>

#include <sddl.h>
#include <stdlib.h>
#include <string>
#include <vector>

VALUE
wstr_to_rb_str(UINT cp, const WCHAR* wstr, int clen)
{
  VALUE vstr;
  CHAR* ptr;
  int ret = -1;
  DWORD err = ERROR_SUCCESS;
  if (wstr == NULL) {
    return rb_utf8_str_new_cstr("");
  }

  int len = WideCharToMultiByte(cp, 0, wstr, clen, nullptr, 0, nullptr, nullptr);
  ptr = RB_ALLOCV_N(CHAR, vstr, len);
  // For memory safety.
  ZeroMemory(ptr, sizeof(CHAR) * len);
  ret = WideCharToMultiByte(cp, 0, wstr, clen, ptr, len, nullptr, nullptr);
  // return 0 should be failure.
  // ref: https://docs.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte#return-value
  if (ret == 0) {
    err = GetLastError();
    RB_ALLOCV_END(vstr);
    raise_system_error(rb_eRuntimeError, err);
  }
  VALUE str = rb_utf8_str_new_cstr(ptr);
  RB_ALLOCV_END(vstr);

  return str;
}

void
raise_system_error(VALUE error, DWORD errorCode)
{
  WCHAR msgBuf[256] = { 0 };
  VALUE errorMessage;

  FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL,
                 errorCode,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 msgBuf,
                 _countof(msgBuf),
                 NULL);
  errorMessage = wstr_to_rb_str(CP_UTF8, msgBuf, -1);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat="
#pragma GCC diagnostic ignored "-Wformat-extra-args"
  rb_raise(error, "ErrorCode: %lu\nError: %" PRIsVALUE "\n", errorCode, errorMessage);
#pragma GCC diagnostic pop
}

void
raise_channel_not_found_error(VALUE channelPath)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat="
#pragma GCC diagnostic ignored "-Wformat-extra-args"
  rb_raise(rb_eChannelNotFoundError, "Channel Not Found: %" PRIsVALUE, channelPath);
#pragma GCC diagnostic pop
}

VALUE
render_to_rb_str(EVT_HANDLE handle, DWORD flags)
{
  VALUE vbuffer;
  WCHAR* buffer;
  ULONG bufferSize = 0;
  ULONG bufferSizeUsed = 0;
  ULONG count;
  BOOL succeeded;
  VALUE result;

  if (flags != EvtRenderEventXml && flags != EvtRenderBookmark) {
    return Qnil;
  }

  // Get the size of the buffer
  EvtRender(nullptr, handle, flags, 0, NULL, &bufferSize, &count);

  // bufferSize is in bytes, not characters
  buffer = (WCHAR*)RB_ALLOCV(vbuffer, bufferSize);

  succeeded =
    EvtRender(nullptr, handle, flags, bufferSize, buffer, &bufferSizeUsed, &count);
  if (!succeeded) {
    DWORD status = GetLastError();
    RB_ALLOCV_END(vbuffer);
    raise_system_error(rb_eWinevtQueryError, status);
  }

  result = wstr_to_rb_str(CP_UTF8, buffer, -1);
  RB_ALLOCV_END(vbuffer);

  return result;
}

EVT_HANDLE
connect_to_remote(LPWSTR computerName, LPWSTR domain, LPWSTR username, LPWSTR password,
                  EVT_RPC_LOGIN_FLAGS flags, DWORD *error_code)
{
  EVT_HANDLE hRemote = NULL;
  EVT_RPC_LOGIN Credentials;

  RtlZeroMemory(&Credentials, sizeof(EVT_RPC_LOGIN));

  Credentials.Server = computerName;
  Credentials.Domain = domain;
  Credentials.User = username;
  Credentials.Password = password;
  Credentials.Flags = flags;

  hRemote = EvtOpenSession(EvtRpcLogin, &Credentials, 0, 0);
  if (!hRemote) {
    *error_code = GetLastError();
    return hRemote;
  }

  SecureZeroMemory(&Credentials, sizeof(EVT_RPC_LOGIN));

  return hRemote;
}

static std::wstring
guid_to_wstr(const GUID& guid)
{
  LPOLESTR p = nullptr;
  if (FAILED(StringFromCLSID(guid, &p))) {
    return nullptr;
  }
  std::wstring s(p);
  CoTaskMemFree(p);
  return s;
}

static VALUE
make_displayable_binary_string(PBYTE bin, size_t length)
{
  const char *HEX_TABLE = "0123456789ABCDEF";
  CHAR *buffer;
  int size = length * 2 + 1;
  size_t i, j;
  unsigned int idx = 0;
  VALUE vbuffer;

  if (length == 0) {
    return rb_str_new2("(NULL)");
  }

  buffer = RB_ALLOCV_N(CHAR, vbuffer, size);

  for (i = 0; i < length; i++) {
    for (j = 0; j < 2; j++) {
      idx = (unsigned int)(bin[i] >> (j * 4) & 0x0F);
      buffer[2*i+(1-j)] = HEX_TABLE[idx];
    }
  }
  buffer[size - 1] = '\0';

  VALUE str = rb_str_new2(buffer);
  RB_ALLOCV_END(vbuffer);

  return str;
}

static VALUE
extract_user_evt_variants(PEVT_VARIANT pRenderedValues, DWORD propCount)
{
  VALUE userValues = rb_ary_new();
  VALUE rbObj;

  for (DWORD i = 0; i < propCount; i++) {
    switch (pRenderedValues[i].Type) {
      case EvtVarTypeNull:
        rb_ary_push(userValues, Qnil);
        break;
      case EvtVarTypeString:
        if (pRenderedValues[i].StringVal == nullptr) {
          rb_ary_push(userValues, rb_utf8_str_new_cstr("(NULL)"));
        } else {
          std::wstring wStr(pRenderedValues[i].StringVal);
          rbObj = wstr_to_rb_str(CP_UTF8, &wStr[0], -1);
          rb_ary_push(userValues, rbObj);
        }
        break;
      case EvtVarTypeAnsiString:
        if (pRenderedValues[i].AnsiStringVal == nullptr) {
          rb_ary_push(userValues, rb_utf8_str_new_cstr("(NULL)"));
        } else {
          rb_ary_push(
            userValues,
            rb_utf8_str_new_cstr(const_cast<char*>(pRenderedValues[i].AnsiStringVal)));
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
      case EvtVarTypeSingle: {
        CHAR sResult[256];
        _snprintf_s(
          sResult, _countof(sResult), _TRUNCATE, "%f", pRenderedValues[i].SingleVal);
        rb_ary_push(userValues, rb_utf8_str_new_cstr(sResult));
        break;
      }
      case EvtVarTypeDouble: {
        CHAR sResult[256];
        _snprintf_s(
          sResult, _countof(sResult), _TRUNCATE, "%lf", pRenderedValues[i].DoubleVal);
        rb_ary_push(userValues, rb_utf8_str_new_cstr(sResult));
        break;
      }
      case EvtVarTypeBoolean:
        rbObj = pRenderedValues[i].BooleanVal ? Qtrue : Qfalse;
        rb_ary_push(userValues, rbObj);
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
      case EvtVarTypeFileTime: {
        LARGE_INTEGER timestamp;
        CHAR strTime[128];
        FILETIME ft;
        SYSTEMTIME st;
        timestamp.QuadPart = pRenderedValues[i].FileTimeVal;
        ft.dwHighDateTime = timestamp.HighPart;
        ft.dwLowDateTime = timestamp.LowPart;
        if (FileTimeToSystemTime(&ft, &st)) {
          _snprintf_s(strTime,
                      _countof(strTime),
                      _TRUNCATE,
                      "%04d-%02d-%02d %02d:%02d:%02d.%dZ",
                      st.wYear,
                      st.wMonth,
                      st.wDay,
                      st.wHour,
                      st.wMinute,
                      st.wSecond,
                      st.wMilliseconds);
          rb_ary_push(userValues, rb_utf8_str_new_cstr(strTime));
        } else {
          rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
        }
        break;
      }
      case EvtVarTypeSysTime: {
        CHAR strTime[128];
        SYSTEMTIME st;
        if (pRenderedValues[i].SysTimeVal != nullptr) {
          st = *pRenderedValues[i].SysTimeVal;
          _snprintf_s(strTime,
                      _countof(strTime),
                      _TRUNCATE,
                      "%04d-%02d-%02d %02d:%02d:%02d.%dZ",
                      st.wYear,
                      st.wMonth,
                      st.wDay,
                      st.wHour,
                      st.wMinute,
                      st.wSecond,
                      st.wMilliseconds);
          rb_ary_push(userValues, rb_utf8_str_new_cstr(strTime));
        } else {
          rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
        }
        break;
      }
      case EvtVarTypeSid: {
        WCHAR* tmpWChar = nullptr;
        if (ConvertSidToStringSidW(pRenderedValues[i].SidVal, &tmpWChar)) {
          rbObj = wstr_to_rb_str(CP_UTF8, tmpWChar, -1);
          rb_ary_push(userValues, rbObj);
          LocalFree(tmpWChar);
        } else {
          rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
        }
        break;
      }
      case EvtVarTypeHexInt32:
        rbObj = rb_sprintf("%#x", pRenderedValues[i].UInt32Val);
        rb_ary_push(userValues, rbObj);
        break;
      case EvtVarTypeHexInt64:
        uint32_t  high;
        uint32_t  low;

        high = pRenderedValues[i].UInt64Val >> 32;
        low = pRenderedValues[i].UInt64Val & 0x00000000FFFFFFFF;
        rbObj = rb_sprintf("0x%08x%08x", high, low);
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
      case EvtVarTypeBinary:
        if (pRenderedValues[i].BinaryVal == nullptr) {
          rb_ary_push(userValues, rb_utf8_str_new_cstr("(NULL)"));
        } else {
          rbObj = make_displayable_binary_string(pRenderedValues[i].BinaryVal, pRenderedValues[i].Count);
          rb_ary_push(userValues, rbObj);
        }
        break;
      default:
        rb_ary_push(userValues, rb_utf8_str_new_cstr("?"));
        break;
    }
  }

  return userValues;
}

VALUE
get_values(EVT_HANDLE handle)
{
  VALUE vbuffer;
  PEVT_VARIANT pRenderedValues;
  ULONG bufferSize = 0;
  ULONG bufferSizeUsed = 0;
  DWORD propCount = 0;
  BOOL succeeded;
  VALUE userValues = Qnil;

  EVT_HANDLE renderContext = EvtCreateRenderContext(0, nullptr, EvtRenderContextUser);
  if (renderContext == nullptr) {
    rb_raise(rb_eWinevtQueryError, "Failed to create renderContext");
  }

  // Get the size of the buffer
  EvtRender(
    renderContext, handle, EvtRenderEventValues, 0, NULL, &bufferSize, &propCount);

  // bufferSize is in bytes, not array size
  pRenderedValues = (PEVT_VARIANT)RB_ALLOCV(vbuffer, bufferSize);

  succeeded = EvtRender(renderContext,
                        handle,
                        EvtRenderEventValues,
                        bufferSize,
                        pRenderedValues,
                        &bufferSizeUsed,
                        &propCount);
  if (!succeeded) {
    DWORD status = GetLastError();
    RB_ALLOCV_END(vbuffer);
    EvtClose(renderContext);
    raise_system_error(rb_eWinevtQueryError, status);
  }

  userValues = extract_user_evt_variants(pRenderedValues, propCount);

  RB_ALLOCV_END(vbuffer);
  EvtClose(renderContext);

  return userValues;
}

static std::vector<WCHAR>
get_message(EVT_HANDLE hMetadata, EVT_HANDLE handle)
{
#define BUFSIZE 4096
  std::vector<WCHAR> result;
  ULONG status;
  ULONG bufferSizeNeeded = 0;
  LPVOID lpMsgBuf;
  std::vector<WCHAR> message(BUFSIZE);

  if (!EvtFormatMessage(hMetadata,
                        handle,
                        0xffffffff,
                        0,
                        nullptr,
                        EvtFormatMessageEvent,
                        message.size(),
                        &message[0],
                        &bufferSizeNeeded)) {
    status = GetLastError();

    if (status != ERROR_EVT_UNRESOLVED_VALUE_INSERT) {
      switch (status) {
        case ERROR_EVT_MESSAGE_NOT_FOUND:
        case ERROR_EVT_MESSAGE_ID_NOT_FOUND:
        case ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND:
        case ERROR_RESOURCE_DATA_NOT_FOUND:
        case ERROR_RESOURCE_TYPE_NOT_FOUND:
        case ERROR_RESOURCE_NAME_NOT_FOUND:
        case ERROR_RESOURCE_LANG_NOT_FOUND:
        case ERROR_MUI_FILE_NOT_FOUND:
        case ERROR_EVT_UNRESOLVED_PARAMETER_INSERT: {
          if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr,
                             status,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             reinterpret_cast<WCHAR*>(&lpMsgBuf),
                             0,
                             nullptr) == 0)
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr,
                           status,
                           MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                           reinterpret_cast<WCHAR*>(&lpMsgBuf),
                           0,
                           nullptr);

          std::wstring ret(reinterpret_cast<WCHAR*>(lpMsgBuf));
          std::copy(ret.begin(), ret.end(), std::back_inserter(result));
          LocalFree(lpMsgBuf);

          goto cleanup;
        }
      }

      if (status != ERROR_INSUFFICIENT_BUFFER)
        rb_raise(rb_eWinevtQueryError, "ErrorCode: %lu", status);
    }

    if (status == ERROR_INSUFFICIENT_BUFFER) {
      message.resize(bufferSizeNeeded);
      message.shrink_to_fit();

      if (!EvtFormatMessage(hMetadata,
                            handle,
                            0xffffffff,
                            0,
                            nullptr,
                            EvtFormatMessageEvent,
                            message.size(),
                            &message.front(),
                            &bufferSizeNeeded)) {
        status = GetLastError();

        if (status != ERROR_EVT_UNRESOLVED_VALUE_INSERT) {
          switch (status) {
            case ERROR_EVT_MESSAGE_NOT_FOUND:
            case ERROR_EVT_MESSAGE_ID_NOT_FOUND:
            case ERROR_EVT_MESSAGE_LOCALE_NOT_FOUND:
            case ERROR_RESOURCE_DATA_NOT_FOUND:
            case ERROR_RESOURCE_TYPE_NOT_FOUND:
            case ERROR_RESOURCE_NAME_NOT_FOUND:
            case ERROR_RESOURCE_LANG_NOT_FOUND:
            case ERROR_MUI_FILE_NOT_FOUND:
            case ERROR_EVT_UNRESOLVED_PARAMETER_INSERT:
              if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                   FORMAT_MESSAGE_FROM_SYSTEM |
                                   FORMAT_MESSAGE_IGNORE_INSERTS,
                                 nullptr,
                                 status,
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 reinterpret_cast<WCHAR*>(&lpMsgBuf),
                                 0,
                                 nullptr) == 0)
                FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                 FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                               nullptr,
                               status,
                               MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                               reinterpret_cast<WCHAR*>(&lpMsgBuf),
                               0,
                               nullptr);

              std::wstring ret(reinterpret_cast<WCHAR*>(lpMsgBuf));
              std::copy(ret.begin(), ret.end(), std::back_inserter(result));
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

  return result;

#undef BUFSIZE
}

WCHAR*
get_description(EVT_HANDLE handle, LANGID langID, EVT_HANDLE hRemote)
{
#define BUFSIZE 4096
  std::vector<WCHAR> buffer(BUFSIZE);
  ULONG bufferSizeNeeded = 0;
  ULONG status, count;
  std::vector<WCHAR> result;
  EVT_HANDLE hMetadata = nullptr;

  static PCWSTR eventProperties[] = { L"Event/System/Provider/@Name" };
  EVT_HANDLE renderContext =
    EvtCreateRenderContext(1, eventProperties, EvtRenderContextValues);
  if (renderContext == nullptr) {
    rb_raise(rb_eWinevtQueryError, "Failed to create renderContext");
  }

  if (EvtRender(renderContext,
                handle,
                EvtRenderEventValues,
                buffer.size(),
                &buffer.front(),
                &bufferSizeNeeded,
                &count) != FALSE) {
    status = ERROR_SUCCESS;
  } else {
    status = GetLastError();
  }

  if (status != ERROR_SUCCESS) {
    EvtClose(renderContext);
    raise_system_error(rb_eWinevtQueryError, status);
  }

  // Obtain buffer as EVT_VARIANT pointer. To avoid ErrorCide 87 in EvtRender.
  const PEVT_VARIANT values = reinterpret_cast<PEVT_VARIANT>(&buffer.front());

  // Open publisher metadata
  hMetadata = EvtOpenPublisherMetadata(
    hRemote,
    values[0].StringVal,
    nullptr,
    MAKELCID(langID, SORT_DEFAULT),
    0);
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

  return _wcsdup(result.data());
}

static char* convert_wstr(wchar_t *wstr)
{
  VALUE vstr;
  int len = 0;
  CHAR *ptr = NULL;
  DWORD err = ERROR_SUCCESS;
  char *dup_str = NULL;

  len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  if (len == 0) {
    return NULL;
  }

  ptr = RB_ALLOCV_N(CHAR, vstr, len);
  // For memory safety.
  ZeroMemory(ptr, sizeof(CHAR) * len);

  len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, ptr, len, NULL, NULL);
  // return 0 should be failure.
  // ref: https://docs.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte#return-value
  if (len == 0) {
    err = GetLastError();
    RB_ALLOCV_END(vstr);
    raise_system_error(rb_eRuntimeError, err);
  }

  dup_str = strdup(ptr);
  RB_ALLOCV_END(vstr);

  return dup_str;
}

static int ExpandSIDWString(PSID sid, CHAR **out_expanded)
{
#define MAX_NAME 256
  DWORD len = MAX_NAME, err = ERROR_SUCCESS;
  SID_NAME_USE sid_type = SidTypeUnknown;
  WCHAR wAccount[MAX_NAME];
  WCHAR wDomain[MAX_NAME];
  CHAR *account = NULL, *domain = NULL;
  DWORD result_len = 0;
  CHAR *formatted = NULL;
  VALUE vformatted;
#undef MAX_NAME

  if (!LookupAccountSidW(NULL, sid,
                         wAccount, &len, wDomain,
                         &len, &sid_type)) {
    err = GetLastError();
    if (err == ERROR_NONE_MAPPED) {
      goto none_mapped_error;
    }
    else {
      return WINEVT_UTILS_ERROR_OTHERS;
    }

    goto error;
  }

  domain = convert_wstr(wDomain);
  if (domain == NULL) {
    goto error;
  }
  account = convert_wstr(wAccount);
  if (account == NULL) {
    goto error;
  }

  result_len = strlen(domain) + 1 + strlen(account) + 1;
  formatted = (CHAR *)RB_ALLOCV(vformatted, result_len);
  if (formatted == NULL) {
    goto error;
  }

  _snprintf_s(formatted, result_len, _TRUNCATE, "%s\\%s", domain, account);

  *out_expanded = strdup(formatted);

  free(domain);
  free(account);
  RB_ALLOCV_END(vformatted);


  return 0;

none_mapped_error:

  return WINEVT_UTILS_ERROR_NONE_MAPPED;

error:
  err = GetLastError();

  RB_ALLOCV_END(vformatted);
  free(domain);
  free(account);
  raise_system_error(rb_eRuntimeError, err);
}

VALUE
render_system_event(EVT_HANDLE hEvent, BOOL preserve_qualifiers, BOOL preserveSID_p)
{
  DWORD status = ERROR_SUCCESS;
  EVT_HANDLE hContext = NULL;
  DWORD dwBufferSize = 0;
  DWORD dwBufferUsed = 0;
  DWORD dwPropertyCount = 0;
  VALUE vRenderedValues;
  PEVT_VARIANT pRenderedValues = NULL;
  WCHAR wsGuid[50];
  LPSTR pwsSid = NULL;
  ULONGLONG ullTimeStamp = 0;
  ULONGLONG ullNanoseconds = 0;
  SYSTEMTIME st;
  FILETIME ft;
  CHAR buffer[32];
  VALUE rbstr;
  DWORD EventID;
  VALUE hash = rb_hash_new();

  hContext = EvtCreateRenderContext(0, NULL, EvtRenderContextSystem);
  if (NULL == hContext) {
    rb_raise(
      rb_eWinevtQueryError, "Failed to create renderContext with %lu\n", GetLastError());
  }

  if (!EvtRender(hContext,
                 hEvent,
                 EvtRenderEventValues,
                 dwBufferSize,
                 pRenderedValues,
                 &dwBufferUsed,
                 &dwPropertyCount)) {
    status = GetLastError();
    if (ERROR_INSUFFICIENT_BUFFER == status) {
      dwBufferSize = dwBufferUsed;
      pRenderedValues = (PEVT_VARIANT)RB_ALLOCV(vRenderedValues, dwBufferSize);
      if (pRenderedValues) {
        EvtRender(hContext,
                  hEvent,
                  EvtRenderEventValues,
                  dwBufferSize,
                  pRenderedValues,
                  &dwBufferUsed,
                  &dwPropertyCount);
        status = GetLastError();
      } else {
        EvtClose(hContext);
        rb_raise(rb_eRuntimeError, "Failed to malloc memory with %lu\n", status);
      }
    }

    if (ERROR_SUCCESS != status) {
      EvtClose(hContext);
      RB_ALLOCV_END(vRenderedValues);

      rb_raise(rb_eWinevtQueryError, "EvtRender failed with %lu\n", status);
    }
  }

  // EVT_VARIANT value with EvtRenderContextSystem will be decomposed
  // as the following enum definition:
  // https://docs.microsoft.com/en-us/windows/win32/api/winevt/ne-winevt-evt_system_property_id
  rbstr = wstr_to_rb_str(CP_UTF8, pRenderedValues[EvtSystemProviderName].StringVal, -1);
  rb_hash_aset(hash, rb_str_new2("ProviderName"), rbstr);
  if (NULL != pRenderedValues[EvtSystemProviderGuid].GuidVal) {
    const GUID* Guid = pRenderedValues[EvtSystemProviderGuid].GuidVal;
    StringFromGUID2(*Guid, wsGuid, _countof(wsGuid));
    rbstr = wstr_to_rb_str(CP_UTF8, wsGuid, -1);
    rb_hash_aset(hash, rb_str_new2("ProviderGuid"), rbstr);
  } else {
    rb_hash_aset(hash, rb_str_new2("ProviderGuid"), Qnil);
  }

  EventID = pRenderedValues[EvtSystemEventID].UInt16Val;
  if (preserve_qualifiers) {
    if (EvtVarTypeNull != pRenderedValues[EvtSystemQualifiers].Type) {
      rb_hash_aset(hash, rb_str_new2("Qualifiers"),
                   INT2NUM(pRenderedValues[EvtSystemQualifiers].UInt16Val));
    } else {
      rb_hash_aset(hash, rb_str_new2("Qualifiers"), rb_str_new2(""));
    }

    rb_hash_aset(hash, rb_str_new2("EventID"), INT2NUM(EventID));
  } else {
    if (EvtVarTypeNull != pRenderedValues[EvtSystemQualifiers].Type) {
      EventID = MAKELONG(pRenderedValues[EvtSystemEventID].UInt16Val,
                         pRenderedValues[EvtSystemQualifiers].UInt16Val);
    }

    rb_hash_aset(hash, rb_str_new2("EventID"), ULONG2NUM(EventID));
  }

  rb_hash_aset(hash,
               rb_str_new2("Version"),
               (EvtVarTypeNull == pRenderedValues[EvtSystemVersion].Type)
                 ? INT2NUM(0)
                 : INT2NUM(pRenderedValues[EvtSystemVersion].ByteVal));
  rb_hash_aset(hash,
               rb_str_new2("Level"),
               (EvtVarTypeNull == pRenderedValues[EvtSystemLevel].Type)
                 ? INT2NUM(0)
                 : INT2NUM(pRenderedValues[EvtSystemLevel].ByteVal));
  rb_hash_aset(hash,
               rb_str_new2("Task"),
               (EvtVarTypeNull == pRenderedValues[EvtSystemTask].Type)
                 ? INT2NUM(0)
                 : INT2NUM(pRenderedValues[EvtSystemTask].UInt16Val));
  rb_hash_aset(hash,
               rb_str_new2("Opcode"),
               (EvtVarTypeNull == pRenderedValues[EvtSystemOpcode].Type)
                 ? INT2NUM(0)
                 : INT2NUM(pRenderedValues[EvtSystemOpcode].ByteVal));
  _snprintf_s(buffer,
              _countof(buffer),
              _TRUNCATE,
              "0x%llx",
              pRenderedValues[EvtSystemKeywords].UInt64Val);
  rb_hash_aset(hash,
               rb_str_new2("Keywords"),
               (EvtVarTypeNull == pRenderedValues[EvtSystemKeywords].Type)
                 ? Qnil
                 : rb_str_new2(buffer));

  if (EvtVarTypeNull != pRenderedValues[EvtSystemTimeCreated].Type) {
    ullTimeStamp = pRenderedValues[EvtSystemTimeCreated].FileTimeVal;
    ft.dwHighDateTime = (DWORD)((ullTimeStamp >> 32) & 0xFFFFFFFF);
    ft.dwLowDateTime = (DWORD)(ullTimeStamp & 0xFFFFFFFF);

    FileTimeToSystemTime(&ft, &st);
    ullNanoseconds =
        (ullTimeStamp % 10000000) *
        100; // Display nanoseconds instead of milliseconds for higher resolution
    _snprintf_s(buffer,
                _countof(buffer),
                _TRUNCATE,
                "%02d/%02d/%02d %02d:%02d:%02d.%llu",
                st.wYear,
                st.wMonth,
                st.wDay,
                st.wHour,
                st.wMinute,
                st.wSecond,
                ullNanoseconds);
    rb_hash_aset(hash,
                 rb_str_new2("TimeCreated"),
                 rb_str_new2(buffer));
  } else {
    rb_hash_aset(hash,
                 rb_str_new2("TimeCreated"),
                 Qnil);
  }
  _snprintf_s(buffer,
              _countof(buffer),
              _TRUNCATE,
              "%llu",
              pRenderedValues[EvtSystemEventRecordId].UInt64Val);
  rb_hash_aset(hash,
               rb_str_new2("EventRecordID"),
               (EvtVarTypeNull == pRenderedValues[EvtSystemEventRecordId].UInt64Val)
                 ? Qnil
                 : rb_str_new2(buffer));

  if (EvtVarTypeNull != pRenderedValues[EvtSystemActivityID].Type) {
    const GUID* Guid = pRenderedValues[EvtSystemActivityID].GuidVal;
    StringFromGUID2(*Guid, wsGuid, _countof(wsGuid));
    rbstr = wstr_to_rb_str(CP_UTF8, wsGuid, -1);
    rb_hash_aset(hash, rb_str_new2("ActivityID"), rbstr);
  }

  if (EvtVarTypeNull != pRenderedValues[EvtSystemRelatedActivityID].Type) {
    const GUID* Guid = pRenderedValues[EvtSystemRelatedActivityID].GuidVal;
    StringFromGUID2(*Guid, wsGuid, _countof(wsGuid));
    rbstr = wstr_to_rb_str(CP_UTF8, wsGuid, -1);
    rb_hash_aset(hash, rb_str_new2("RelatedActivityID"), rbstr);
  }

  rb_hash_aset(hash,
               rb_str_new2("ProcessID"),
               UINT2NUM(pRenderedValues[EvtSystemProcessID].UInt32Val));
  rb_hash_aset(hash,
               rb_str_new2("ThreadID"),
               UINT2NUM(pRenderedValues[EvtSystemThreadID].UInt32Val));
  rbstr = wstr_to_rb_str(CP_UTF8, pRenderedValues[EvtSystemChannel].StringVal, -1);
  rb_hash_aset(hash, rb_str_new2("Channel"), rbstr);
  rbstr = wstr_to_rb_str(CP_UTF8, pRenderedValues[EvtSystemComputer].StringVal, -1);
  rb_hash_aset(hash, rb_str_new2("Computer"), rbstr);

  if (EvtVarTypeNull != pRenderedValues[EvtSystemUserID].Type) {
    if (ConvertSidToStringSid(pRenderedValues[EvtSystemUserID].SidVal, &pwsSid)) {
      CHAR *expandSID = NULL;
      if (preserveSID_p) {
        rbstr = rb_utf8_str_new_cstr(pwsSid);
        rb_hash_aset(hash, rb_str_new2("UserID"), rbstr);
      }
      /* S-1-15-3- is used for capability SIDs. So, we need to skip
       * SID translation.
       * ref: https://learn.microsoft.com/en-us/windows-server/identity/ad-ds/manage/understand-security-identifiers
       * See also: https://learn.microsoft.com/en-us/troubleshoot/windows-server/windows-security/sids-not-resolve-into-friendly-names
       */
      if (strnicmp(pwsSid, "S-1-15-3-", 9) != 0) {
        if (ExpandSIDWString(pRenderedValues[EvtSystemUserID].SidVal,
                             &expandSID) == 0) {
          rbstr = rb_utf8_str_new_cstr(expandSID);
          free(expandSID);
          rb_hash_aset(hash, rb_str_new2("User"), rbstr);
        }
      }
      LocalFree(pwsSid);
    }
  }

  EvtClose(hContext);
  RB_ALLOCV_END(vRenderedValues);

  return hash;
}
