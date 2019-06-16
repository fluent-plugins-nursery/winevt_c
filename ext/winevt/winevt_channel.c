#include <winevt_c.h>

static void channel_free(void *ptr);

static const rb_data_type_t rb_winevt_channel_type = {
  "winevt/channel", {
    0, channel_free, 0,
  }, NULL, NULL,
  RUBY_TYPED_FREE_IMMEDIATELY
};

static void
channel_free(void *ptr)
{
  struct WinevtChannel *winevtChannel = (struct WinevtChannel *)ptr;
  if (winevtChannel->channels)
    EvtClose(winevtChannel->channels);

  xfree(ptr);
}

static VALUE
rb_winevt_channel_alloc(VALUE klass)
{
  VALUE obj;
  struct WinevtChannel *winevtChannel;
  obj = TypedData_Make_Struct(klass,
                              struct WinevtChannel,
                              &rb_winevt_channel_type,
                              winevtChannel);
  return obj;
}

static VALUE
rb_winevt_channel_initialize(VALUE klass)
{
  return Qnil;
}

static VALUE
rb_winevt_channel_each(VALUE self)
{
  EVT_HANDLE hChannels;
  struct WinevtChannel *winevtChannel;
  char *errBuf;
  char * result;
  LPWSTR buffer = NULL;
  LPWSTR temp = NULL;
  DWORD bufferSize = 0;
  DWORD bufferUsed = 0;
  DWORD status = ERROR_SUCCESS;

  RETURN_ENUMERATOR(self, 0, 0);

  TypedData_Get_Struct(self, struct WinevtChannel, &rb_winevt_channel_type, winevtChannel);

  hChannels = EvtOpenChannelEnum(NULL, 0);

  if (hChannels) {
    winevtChannel->channels = hChannels;
  } else {
    sprintf(errBuf, "Failed to enumerate channels with %s\n", GetLastError());
    rb_raise(rb_eRuntimeError, errBuf);
  }

  while (1) {
    if (!EvtNextChannelPath(winevtChannel->channels, bufferSize, buffer, &bufferUsed)) {
      status = GetLastError();

      if (ERROR_NO_MORE_ITEMS == status) {
        break;
      } else if (ERROR_INSUFFICIENT_BUFFER == status) {
        bufferSize = bufferUsed;
        temp = (LPWSTR)realloc(buffer, bufferSize * sizeof(WCHAR));
        if (temp) {
          buffer = temp;
          temp = NULL;
          EvtNextChannelPath(winevtChannel->channels, bufferSize, buffer, &bufferUsed);
        } else {
          status = ERROR_OUTOFMEMORY;
          rb_raise(rb_eRuntimeError, "realloc failed");
        }
      } else {
        sprintf(errBuf, "EvtNextChannelPath failed with %lu.\n", status);
        rb_raise(rb_eRuntimeError, errBuf);
      }
    }

    result = wstr_to_mbstr(CP_UTF8, buffer, -1);

    rb_yield(rb_str_new2(result));
  }

  return Qnil;
}

void Init_winevt_channel(VALUE rb_cEventLog)
{
  rb_cChannel = rb_define_class_under(rb_cEventLog, "Channel", rb_cObject);
  rb_define_alloc_func(rb_cChannel, rb_winevt_channel_alloc);
  rb_define_method(rb_cChannel, "initialize", rb_winevt_channel_initialize, 0);
  rb_define_method(rb_cChannel, "each", rb_winevt_channel_each, 0);
}
