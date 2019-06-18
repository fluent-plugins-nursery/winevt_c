#include <winevt_c.h>

char*
wstr_to_mbstr(UINT cp, const WCHAR *wstr, int clen)
{
    char *ptr;
    int len = WideCharToMultiByte(cp, 0, wstr, clen, NULL, 0, NULL, NULL);
    if (!(ptr = malloc(len))) return 0;
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
        &msgBuf, 0, NULL);
    result = wstr_to_mbstr(CP_ACP, msgBuf, -1);

    rb_raise(rb_eWinevtQueryError, "ErrorCode: %d\nError: %s\n", status, result);
  }

  result = wstr_to_mbstr(CP_ACP, buffer, -1);

  if (buffer)
    free(buffer);

  return result;
}

char* get_description(EVT_HANDLE handle)
{
#define MAX_BUFFER 65535
  WCHAR      buffer[4096], file[4096];
  WCHAR      descriptionBuffer[MAX_BUFFER];
  ULONG      bufferSize = 0;
  ULONG      bufferSizeNeeded = 0;
  EVT_HANDLE event;
  ULONG      status, count;
  char*      errBuf;
  char*      result = "";
  LPTSTR     msgBuf;
  TCHAR publisherName[MAX_PATH];
  TCHAR fileName[MAX_PATH];
  EVT_HANDLE hMetadata = NULL;
  PEVT_VARIANT values = NULL;
  PEVT_VARIANT pProperty = NULL;
  PEVT_VARIANT pTemp = NULL;
  TCHAR paramEXE[MAX_PATH], messageEXE[MAX_PATH];
  HMODULE hModule = NULL;

  static PCWSTR eventProperties[] = {L"Event/System/Provider/@Name", L"Event/System/EventID"};
  EVT_HANDLE renderContext = EvtCreateRenderContext(2, eventProperties, EvtRenderContextValues);
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
        &msgBuf, 0, NULL);
    result = wstr_to_mbstr(CP_ACP, msgBuf, -1);

    rb_raise(rb_eWinevtQueryError, "ErrorCode: %d\nError: %s\n", status, result);
  }

  // Obtain buffer as EVT_VARIANT pointer. To avoid ErrorCide 87 in EvtRender.
  values = (PEVT_VARIANT)buffer;
  if ((values[0].Type == EvtVarTypeString) && (values[0].StringVal != NULL)) {
    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, values[0].StringVal, -1, publisherName, MAX_PATH, NULL, NULL);
  }

  DWORD eventId = 0;
  if (values[1].Type == EvtVarTypeUInt16) {
    eventId = values[1].UInt16Val;
  }

  // Open publisher metadata
  hMetadata = EvtOpenPublisherMetadata(NULL, values[0].StringVal, NULL, MAKELCID(MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), SORT_DEFAULT), 0);
  if (hMetadata == NULL) {
    // When winevt_c cannot open metadata, then give up to obtain
    // message file and clean up immediately.
    goto cleanup;
  }

  /* TODO: Should we implement parameter file reading in C?
  // Get the metadata property. If the buffer is not big enough, reallocate the buffer.
  // Get parameter file first.
  if  (!EvtGetPublisherMetadataProperty(hMetadata, EvtPublisherMetadataParameterFilePath, 0, bufferSize, pProperty, &count)) {
    status = GetLastError();
    if (ERROR_INSUFFICIENT_BUFFER == status) {
      bufferSize = count;
      pTemp = (PEVT_VARIANT)realloc(pProperty, bufferSize);
      if (pTemp) {
        pProperty = pTemp;
        pTemp = NULL;
        EvtGetPublisherMetadataProperty(hMetadata, EvtPublisherMetadataParameterFilePath, 0, bufferSize, pProperty, &count);
      } else {
        rb_raise(rb_eWinevtQueryError, "realloc failed");
      }
    }

    if (ERROR_SUCCESS != (status = GetLastError())) {
      rb_raise(rb_eWinevtQueryError, "EvtGetPublisherMetadataProperty for parameter file failed with %d\n", GetLastError());
    }
  }

  if ((pProperty->Type == EvtVarTypeString) && (pProperty->StringVal != NULL)) {
    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, pProperty->StringVal, -1, fileName, MAX_PATH, NULL, NULL);
  }
  if (paramEXE) {
    ExpandEnvironmentStrings(fileName, paramEXE, _countof(paramEXE));
  }
  */

  // Get the metadata property. If the buffer is not big enough, reallocate the buffer.
  // Get message file contents.
  if  (!EvtGetPublisherMetadataProperty(hMetadata, EvtPublisherMetadataMessageFilePath, 0, bufferSize, pProperty, &count)) {
    status = GetLastError();
    if (ERROR_INSUFFICIENT_BUFFER == status) {
      bufferSize = count;
      pTemp = (PEVT_VARIANT)realloc(pProperty, bufferSize);
      if (pTemp) {
        pProperty = pTemp;
        pTemp = NULL;
        EvtGetPublisherMetadataProperty(hMetadata, EvtPublisherMetadataMessageFilePath, 0, bufferSize, pProperty, &count);
      } else {
        rb_raise(rb_eWinevtQueryError, "realloc failed");
      }
    }

    if (ERROR_SUCCESS != (status = GetLastError())) {
      rb_raise(rb_eWinevtQueryError, "EvtGetPublisherMetadataProperty for message file failed with %d\n", GetLastError());
    }
  }

  if ((pProperty->Type == EvtVarTypeString) && (pProperty->StringVal != NULL)) {
    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, pProperty->StringVal, -1, fileName, MAX_PATH, NULL, NULL);
  }
  if (messageEXE) {
    ExpandEnvironmentStrings(fileName, messageEXE, _countof(messageEXE));
  }

  if (messageEXE != NULL) {
    hModule = LoadLibraryEx(messageEXE, NULL,
                            DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);

    if (FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
                       hModule,
                       eventId,
                       0, // Use current code page. Users must specify character encoding in Ruby side.
                       descriptionBuffer,
                       MAX_BUFFER,
                       NULL)) {
    } else if (!FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
                               hModule,
                               0xB0000000 | eventId,
                               0, // Use current code page. Users must specify character encoding in Ruby side.
                               descriptionBuffer,
                               MAX_BUFFER,
                               NULL)){
      goto cleanup;
    }
  }

  result = wstr_to_mbstr(CP_ACP, descriptionBuffer, -1);

#undef MAX_BUFFER

cleanup:

  if (hMetadata)
    EvtClose(hMetadata);

  if (hModule)
    FreeLibrary(hModule);

  return result;
}
