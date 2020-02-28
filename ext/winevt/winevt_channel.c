#include <winevt_c.h>

/*
 * Document-class: Winevt::EventLog::Channel
 *
 * Retrieve Windows EventLog channel name.
 *
 * @example
 *  require 'winevt'
 *  channels = []
 *  @channel = Winevt::EventLog::Channel.new
 *  # If users want to retrieve all channel names that
 *  # including type of Debug and Analytical,
 *  # it should be set as true.
 *  @channel.force_enumerate = false
 *  @channel.each do |channel|
 *    channels << channel
 *  end
 *  print channels
 */

DWORD is_subscribable_channel_p(EVT_HANDLE hChannel, BOOL force_enumerate);
DWORD check_subscribable_with_channel_config_type(int Id, PEVT_VARIANT pProperty, BOOL force_enumerate);
static void channel_free(void* ptr);

static const rb_data_type_t rb_winevt_channel_type = { "winevt/channel",
                                                       {
                                                         0,
                                                         channel_free,
                                                         0,
                                                       },
                                                       NULL,
                                                       NULL,
                                                       RUBY_TYPED_FREE_IMMEDIATELY };

static void
channel_free(void* ptr)
{
  struct WinevtChannel* winevtChannel = (struct WinevtChannel*)ptr;
  if (winevtChannel->channels)
    EvtClose(winevtChannel->channels);

  xfree(ptr);
}

static VALUE
rb_winevt_channel_alloc(VALUE klass)
{
  VALUE obj;
  struct WinevtChannel* winevtChannel;
  obj = TypedData_Make_Struct(
    klass, struct WinevtChannel, &rb_winevt_channel_type, winevtChannel);
  return obj;
}

/*
 * Initalize Channel class.
 *
 * @return [Channel]
 *
 */
static VALUE
rb_winevt_channel_initialize(VALUE self)
{
  struct WinevtChannel* winevtChannel;

  TypedData_Get_Struct(
    self, struct WinevtChannel, &rb_winevt_channel_type, winevtChannel);

  winevtChannel->force_enumerate = FALSE;

  return Qnil;
}

/*
 * This method specifies whether forcing to enumerate channel which
 * type is Debug and Analytical or not.
 *
 * @param rb_force_enumerate_p [Boolean]
 * @since 0.7.1
 */
static VALUE
rb_winevt_channel_set_force_enumerate(VALUE self, VALUE rb_force_enumerate_p)
{
  struct WinevtChannel* winevtChannel;

  TypedData_Get_Struct(
    self, struct WinevtChannel, &rb_winevt_channel_type, winevtChannel);

  winevtChannel->force_enumerate = RTEST(rb_force_enumerate_p);

  return Qnil;
}

/*
 * This method returns whether forcing to enumerate channel which type
 * is Debug and Analytical or not.
 *
 * @return [Boolean]
 * @since 0.7.1
 */
static VALUE
rb_winevt_channel_get_force_enumerate(VALUE self)
{
  struct WinevtChannel* winevtChannel;

  TypedData_Get_Struct(
    self, struct WinevtChannel, &rb_winevt_channel_type, winevtChannel);

  return winevtChannel->force_enumerate ? Qtrue : Qfalse;
}

DWORD is_subscribable_channel_p(EVT_HANDLE hChannel, BOOL force_enumerate)
{
  PEVT_VARIANT pProperty = NULL;
  PEVT_VARIANT pTemp = NULL;
  DWORD dwBufferSize = 0;
  DWORD dwBufferUsed = 0;
  DWORD status = ERROR_SUCCESS;

  for (int Id = 0; Id < EvtChannelConfigPropertyIdEND; Id++) {
    if (!EvtGetChannelConfigProperty(hChannel, (EVT_CHANNEL_CONFIG_PROPERTY_ID)Id, 0, dwBufferSize, pProperty, &dwBufferUsed)) {
      status = GetLastError();
      if (ERROR_INSUFFICIENT_BUFFER == status) {
        dwBufferSize = dwBufferUsed;
        pTemp = (PEVT_VARIANT)realloc(pProperty, dwBufferSize);
        if (pTemp) {
          pProperty = pTemp;
          pTemp = NULL;
          EvtGetChannelConfigProperty(hChannel, (EVT_CHANNEL_CONFIG_PROPERTY_ID)Id, 0, dwBufferSize, pProperty, &dwBufferUsed);
          status = GetLastError();
        } else {
          free(pProperty);

          status = ERROR_OUTOFMEMORY;

          goto cleanup;
        }
      }

      if (ERROR_SUCCESS != status) {
        free(pProperty);

        goto cleanup;
      }
    }

    status = check_subscribable_with_channel_config_type(Id, pProperty, force_enumerate);
    if (status != ERROR_SUCCESS)
      break;
  }

cleanup:

  free(pProperty);

  return status;
}

#define EVENT_DEBUG_TYPE 2
#define EVENT_ANALYTICAL_TYPE 3

DWORD check_subscribable_with_channel_config_type(int Id, PEVT_VARIANT pProperty, BOOL force_enumerate)
{
  DWORD status = ERROR_SUCCESS;
  switch(Id) {
  case EvtChannelConfigType:
    if (!force_enumerate &&
        (pProperty->UInt32Val == EVENT_DEBUG_TYPE ||
         pProperty->UInt32Val == EVENT_ANALYTICAL_TYPE)) {
      return ERROR_INVALID_DATA;
    }
    break;
  }

  return status;
}

#undef EVENT_DEBUG_TYPE
#undef EVENT_ANALYTICAL_TYPE

/*
 * Enumerate Windows EventLog channels
 *
 * @yield (String)
 *
 */
static VALUE
rb_winevt_channel_each(VALUE self)
{
  EVT_HANDLE hChannels;
  EVT_HANDLE hChannelConfig = NULL;
  struct WinevtChannel* winevtChannel;
  char errBuf[256];
  LPWSTR buffer = NULL;
  DWORD bufferSize = 0;
  DWORD bufferUsed = 0;
  DWORD status = ERROR_SUCCESS;
  VALUE utf8str;

  RETURN_ENUMERATOR(self, 0, 0);

  TypedData_Get_Struct(
    self, struct WinevtChannel, &rb_winevt_channel_type, winevtChannel);

  hChannels = EvtOpenChannelEnum(NULL, 0);

  if (hChannels) {
    winevtChannel->channels = hChannels;
  } else {
    _snprintf_s(errBuf,
                _countof(errBuf),
                _TRUNCATE,
                "Failed to enumerate channels with %lu\n",
                GetLastError());
    rb_raise(rb_eRuntimeError, errBuf);
  }

  while (1) {
    if (!EvtNextChannelPath(winevtChannel->channels, bufferSize, buffer, &bufferUsed)) {
      status = GetLastError();

      if (ERROR_NO_MORE_ITEMS == status) {
        break;
      } else if (ERROR_INSUFFICIENT_BUFFER == status) {
        bufferSize = bufferUsed;
        buffer = (LPWSTR)malloc(bufferSize * sizeof(WCHAR));
        if (buffer) {
          continue;
        } else {
          free(buffer);
          EvtClose(winevtChannel->channels);
          winevtChannel->channels = NULL;
          rb_raise(rb_eRuntimeError, "realloc failed");
        }
      } else {
        free(buffer);
        EvtClose(winevtChannel->channels);
        winevtChannel->channels = NULL;
        _snprintf_s(errBuf,
                    _countof(errBuf),
                    _TRUNCATE,
                    "EvtNextChannelPath failed with %lu.\n",
                    status);
        rb_raise(rb_eRuntimeError, errBuf);
      }
    }
    hChannelConfig = EvtOpenChannelConfig(NULL, buffer, 0);
    if (NULL == hChannelConfig) {
      _snprintf_s(errBuf,
                  _countof(errBuf),
                  _TRUNCATE,
                  "EvtOpenChannelConfig failed with %lu.\n",
                  GetLastError());

      EvtClose(winevtChannel->channels);
      winevtChannel->channels = NULL;

      free(buffer);
      buffer = NULL;
      bufferSize = 0;

      rb_raise(rb_eRuntimeError, errBuf);
    }

    status = is_subscribable_channel_p(hChannelConfig, winevtChannel->force_enumerate);
    EvtClose(hChannelConfig);

    if (status == ERROR_OUTOFMEMORY) {
      EvtClose(winevtChannel->channels);
      winevtChannel->channels = NULL;

      free(buffer);
      buffer = NULL;
      bufferSize = 0;

      rb_raise(rb_eRuntimeError, "realloc failed\n");
    } else if (status == ERROR_INVALID_DATA) {
      free(buffer);
      buffer = NULL;
      bufferSize = 0;

      continue;
    } else if (status != ERROR_SUCCESS) {
      EvtClose(winevtChannel->channels);
      winevtChannel->channels = NULL;

      free(buffer);
      buffer = NULL;
      bufferSize = 0;

      rb_raise(rb_eRuntimeError, "is_subscribe_channel_p is failed with %ld\n", status);
    }

    utf8str = wstr_to_rb_str(CP_UTF8, buffer, -1);

    free(buffer);
    buffer = NULL;
    bufferSize = 0;

    rb_yield(utf8str);
  }

  if (winevtChannel->channels) {
    EvtClose(winevtChannel->channels);
    winevtChannel->channels = NULL;
  }

  free(buffer);

  return Qnil;
}

void
Init_winevt_channel(VALUE rb_cEventLog)
{
  rb_cChannel = rb_define_class_under(rb_cEventLog, "Channel", rb_cObject);
  rb_define_alloc_func(rb_cChannel, rb_winevt_channel_alloc);
  rb_define_method(rb_cChannel, "initialize", rb_winevt_channel_initialize, 0);
  rb_define_method(rb_cChannel, "each", rb_winevt_channel_each, 0);
  rb_define_method(rb_cChannel, "force_enumerate", rb_winevt_channel_get_force_enumerate, 0);
  rb_define_method(rb_cChannel, "force_enumerate=", rb_winevt_channel_set_force_enumerate, 1);
}
