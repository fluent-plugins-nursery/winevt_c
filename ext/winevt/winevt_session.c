#include <winevt_c.h>

VALUE rb_cSession;

static void session_free(void* ptr);

static const rb_data_type_t rb_winevt_session_type = { "winevt/session",
                                                       {
                                                         0,
                                                         session_free,
                                                         0,
                                                       },
                                                       NULL,
                                                       NULL,
                                                       RUBY_TYPED_FREE_IMMEDIATELY };

static void
session_free(void* ptr)
{
  xfree(ptr);
}

static VALUE
rb_winevt_session_alloc(VALUE klass)
{
  VALUE obj;
  struct WinevtSession* winevtSession;
  obj = TypedData_Make_Struct(
    klass, struct WinevtSession, &rb_winevt_session_type, winevtSession);
  return obj;
}

/*
 * Initalize Session class.
 *
 * @return [Session]
 *
 */
static VALUE
rb_winevt_session_initialize(VALUE self)
{
  struct WinevtSession* winevtSession;

  TypedData_Get_Struct(
      self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  winevtSession->computerName = NULL;
  winevtSession->domain = NULL;
  winevtSession->username = NULL;
  winevtSession->password= NULL;

  return Qnil;
}

/*
 * This method returns computer name for remoting access.
 *
 * @return [String]
 */
static VALUE
rb_winevt_session_get_computer_name(VALUE self)
{
  struct WinevtSession* winevtSession;

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  if (winevtSession->computerName) {
    return wstr_to_rb_str(CP_UTF8, winevtSession->computerName, -1);
  } else {
    return rb_str_new2("(NULL)");
  }
}

/*
 * This method specifies computer name for remoting access.
 *
 * @param rb_computer_name [String] computer name
 */
static VALUE
rb_winevt_session_set_computer_name(VALUE self, VALUE rb_computer_name)
{
  struct WinevtSession* winevtSession;
  DWORD len;
  VALUE wcomputerName;
  WCHAR* evtComputerName;

  Check_Type(rb_computer_name, T_STRING);

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  len =
    MultiByteToWideChar(CP_UTF8, 0,
                        RSTRING_PTR(rb_computer_name), RSTRING_LEN(rb_computer_name),
                        NULL, 0);
  evtComputerName = ALLOCV_N(WCHAR, wcomputerName, len + 1);
  MultiByteToWideChar(CP_UTF8, 0,
                      RSTRING_PTR(rb_computer_name), RSTRING_LEN(rb_computer_name),
                      evtComputerName, len);

  winevtSession->computerName = evtComputerName;

  return Qnil;
}

/*
 * This method returns domain for remoting access.
 *
 * @return [String]
 */
static VALUE
rb_winevt_session_get_domain(VALUE self)
{
  struct WinevtSession* winevtSession;

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  if (winevtSession->domain) {
    return wstr_to_rb_str(CP_UTF8, winevtSession->domain, -1);
  } else {
    return rb_str_new2("(NULL)");
  }
}

/*
 * This method specifies domain for remoting access.
 *
 * @param rb_domain [String] domain
 */
static VALUE
rb_winevt_session_set_domain(VALUE self, VALUE rb_domain)
{
  struct WinevtSession* winevtSession;
  DWORD len;
  VALUE wdomain;
  WCHAR* evtDomain;

  Check_Type(rb_domain, T_STRING);

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  len =
    MultiByteToWideChar(CP_UTF8, 0,
                        RSTRING_PTR(rb_domain), RSTRING_LEN(rb_domain),
                        NULL, 0);
  evtDomain = ALLOCV_N(WCHAR, wdomain, len + 1);
  MultiByteToWideChar(CP_UTF8, 0,
                      RSTRING_PTR(rb_domain), RSTRING_LEN(rb_domain),
                      evtDomain, len);

  winevtSession->domain = evtDomain;

  return Qnil;
}

/*
 * This method returns username for remoting access.
 *
 * @return [String]
 */
static VALUE
rb_winevt_session_get_username(VALUE self)
{
  struct WinevtSession* winevtSession;

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  if (winevtSession->username) {
    return wstr_to_rb_str(CP_UTF8, winevtSession->username, -1);
  } else {
    return rb_str_new2("(NULL)");
  }
}

/*
 * This method specifies username for remoting access.
 *
 * @param rb_username [String] username
 */
static VALUE
rb_winevt_session_set_username(VALUE self, VALUE rb_username)
{
  struct WinevtSession* winevtSession;
  DWORD len;
  VALUE wusername;
  WCHAR* evtUsername;

  Check_Type(rb_username, T_STRING);

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  len =
    MultiByteToWideChar(CP_UTF8, 0,
                        RSTRING_PTR(rb_username), RSTRING_LEN(rb_username),
                        NULL, 0);
  evtUsername = ALLOCV_N(WCHAR, wusername, len + 1);
  MultiByteToWideChar(CP_UTF8, 0,
                      RSTRING_PTR(rb_username), RSTRING_LEN(rb_username),
                      evtUsername, len);

  winevtSession->username = evtUsername;

  return Qnil;
}

/*
 * This method returns password for remoting access.
 *
 * @return [String]
 */
static VALUE
rb_winevt_session_get_password(VALUE self)
{
  struct WinevtSession* winevtSession;

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  if (winevtSession->password) {
    return wstr_to_rb_str(CP_UTF8, winevtSession->password, -1);
  } else {
    return rb_str_new2("(NULL)");
  }
}

/*
 * This method specifies password for remoting access.
 *
 * @param rb_password [String] password
 */
static VALUE
rb_winevt_session_set_password(VALUE self, VALUE rb_password)
{
  struct WinevtSession* winevtSession;
  DWORD len;
  VALUE wpassword;
  WCHAR* evtPassword;

  Check_Type(rb_password, T_STRING);

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  len =
    MultiByteToWideChar(CP_UTF8, 0,
                        RSTRING_PTR(rb_password), RSTRING_LEN(rb_password),
                        NULL, 0);
  evtPassword = ALLOCV_N(WCHAR, wpassword, len + 1);
  MultiByteToWideChar(CP_UTF8, 0,
                      RSTRING_PTR(rb_password), RSTRING_LEN(rb_password),
                      evtPassword, len);

  winevtSession->password = evtPassword;

  return Qnil;
}

void
Init_winevt_session(VALUE rb_cEventLog)
{
  rb_cSession = rb_define_class_under(rb_cEventLog, "Session", rb_cObject);

  rb_define_alloc_func(rb_cSession, rb_winevt_session_alloc);

  rb_define_method(rb_cSession, "initialize", rb_winevt_session_initialize, 0);
  rb_define_method(rb_cSession, "computer_name", rb_winevt_session_get_computer_name, 0);
  rb_define_method(rb_cSession, "computer_name=", rb_winevt_session_set_computer_name, 1);
  rb_define_method(rb_cSession, "domain", rb_winevt_session_get_domain, 0);
  rb_define_method(rb_cSession, "domain=", rb_winevt_session_set_domain, 1);
  rb_define_method(rb_cSession, "username", rb_winevt_session_get_username, 0);
  rb_define_method(rb_cSession, "username=", rb_winevt_session_set_username, 1);
  rb_define_method(rb_cSession, "password", rb_winevt_session_get_password, 0);
  rb_define_method(rb_cSession, "password=", rb_winevt_session_set_password, 1);
}
