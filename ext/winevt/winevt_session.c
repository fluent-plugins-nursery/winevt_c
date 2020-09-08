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

  winevtSession->server = NULL;
  winevtSession->domain = NULL;
  winevtSession->username = NULL;
  winevtSession->password = NULL;

  return Qnil;
}

/*
 * This method returns server for remoting access.
 *
 * @return [String]
 */
static VALUE
rb_winevt_session_get_server(VALUE self)
{
  struct WinevtSession* winevtSession;

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  if (winevtSession->server) {
    return wstr_to_rb_str(CP_UTF8, winevtSession->server, -1);
  } else {
    return rb_str_new2("(NULL)");
  }
}

/*
 * This method specifies server for remoting access.
 *
 * @param rb_server [String] server
 */
static VALUE
rb_winevt_session_set_server(VALUE self, VALUE rb_server)
{
  struct WinevtSession* winevtSession;
  DWORD len;
  VALUE vserverBuf;
  PWSTR wServer;

  Check_Type(rb_server, T_STRING);

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  len =
    MultiByteToWideChar(CP_UTF8, 0,
                        RSTRING_PTR(rb_server), RSTRING_LEN(rb_server),
                        NULL, 0);
  wServer = ALLOCV_N(WCHAR, vserverBuf, len + 1);
  MultiByteToWideChar(CP_UTF8, 0,
                      RSTRING_PTR(rb_server), RSTRING_LEN(rb_server),
                      wServer, len);
  winevtSession->server = _wcsdup(wServer);
  wServer[len] = L'\0';

  ALLOCV_END(vserverBuf);

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
  VALUE vdomainBuf;
  PWSTR wDomain;

  Check_Type(rb_domain, T_STRING);

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  len =
    MultiByteToWideChar(CP_UTF8, 0,
                        RSTRING_PTR(rb_domain), RSTRING_LEN(rb_domain),
                        NULL, 0);
  wDomain = ALLOCV_N(WCHAR, vdomainBuf, len + 1);
  MultiByteToWideChar(CP_UTF8, 0,
                      RSTRING_PTR(rb_domain), RSTRING_LEN(rb_domain),
                      wDomain, len);
  wDomain[len] = L'\0';

  winevtSession->domain = _wcsdup(wDomain);

  ALLOCV_END(vdomainBuf);

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
  VALUE vusernameBuf;
  PWSTR wUsername;

  Check_Type(rb_username, T_STRING);

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  len =
    MultiByteToWideChar(CP_UTF8, 0,
                        RSTRING_PTR(rb_username), RSTRING_LEN(rb_username),
                        NULL, 0);
  wUsername = ALLOCV_N(WCHAR, vusernameBuf, len + 1);
  MultiByteToWideChar(CP_UTF8, 0,
                      RSTRING_PTR(rb_username), RSTRING_LEN(rb_username),
                      wUsername, len);
  wUsername[len] = L'\0';

  winevtSession->username = _wcsdup(wUsername);

  ALLOCV_END(vusernameBuf);

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
  VALUE vpasswordBuf;
  PWSTR wPassword;

  Check_Type(rb_password, T_STRING);

  TypedData_Get_Struct(self, struct WinevtSession, &rb_winevt_session_type, winevtSession);

  len =
    MultiByteToWideChar(CP_UTF8, 0,
                        RSTRING_PTR(rb_password), RSTRING_LEN(rb_password),
                        NULL, 0);
  wPassword = ALLOCV_N(WCHAR, vpasswordBuf, len + 1);
  MultiByteToWideChar(CP_UTF8, 0,
                      RSTRING_PTR(rb_password), RSTRING_LEN(rb_password),
                      wPassword, len);
  wPassword[len] = L'\0';

  winevtSession->password = _wcsdup(wPassword);

  ALLOCV_END(vpasswordBuf);

  return Qnil;
}

void
Init_winevt_session(VALUE rb_cEventLog)
{
  rb_cSession = rb_define_class_under(rb_cEventLog, "Session", rb_cObject);

  rb_define_alloc_func(rb_cSession, rb_winevt_session_alloc);

  rb_define_method(rb_cSession, "initialize", rb_winevt_session_initialize, 0);
  rb_define_method(rb_cSession, "server", rb_winevt_session_get_server, 0);
  rb_define_method(rb_cSession, "server=", rb_winevt_session_set_server, 1);
  rb_define_method(rb_cSession, "domain", rb_winevt_session_get_domain, 0);
  rb_define_method(rb_cSession, "domain=", rb_winevt_session_set_domain, 1);
  rb_define_method(rb_cSession, "username", rb_winevt_session_get_username, 0);
  rb_define_method(rb_cSession, "username=", rb_winevt_session_set_username, 1);
  rb_define_method(rb_cSession, "password", rb_winevt_session_get_password, 0);
  rb_define_method(rb_cSession, "password=", rb_winevt_session_set_password, 1);
}
