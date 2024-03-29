#include <winevt_c.h>

VALUE rb_mWinevt;
VALUE rb_cQuery;
VALUE rb_cEventLog;
VALUE rb_cSubscribe;
VALUE rb_eWinevtQueryError;
VALUE rb_eChannelNotFoundError;
VALUE rb_eRemoteHandlerError;
VALUE rb_eSubscribeHandlerError;

static ID id_call;

void
Init_winevt(void)
{
  rb_mWinevt = rb_define_module("Winevt");
  rb_cEventLog = rb_define_class_under(rb_mWinevt, "EventLog", rb_cObject);
  rb_cQuery = rb_define_class_under(rb_cEventLog, "Query", rb_cObject);
  rb_cSubscribe = rb_define_class_under(rb_cEventLog, "Subscribe", rb_cObject);
  rb_eWinevtQueryError = rb_define_class_under(rb_cQuery, "Error", rb_eStandardError);
  rb_eChannelNotFoundError = rb_define_class_under(rb_cEventLog, "ChannelNotFoundError", rb_eStandardError);
  rb_eRemoteHandlerError = rb_define_class_under(rb_cSubscribe, "RemoteHandlerError", rb_eRuntimeError);
  rb_eSubscribeHandlerError = rb_define_class_under(rb_cSubscribe, "SubscribeHandlerError", rb_eRuntimeError);

  Init_winevt_channel(rb_cEventLog);
  Init_winevt_bookmark(rb_cEventLog);
  Init_winevt_query(rb_cEventLog);
  Init_winevt_subscribe(rb_cEventLog);
  Init_winevt_locale(rb_cEventLog);
  Init_winevt_session(rb_cEventLog);

  id_call = rb_intern("call");
}
