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
