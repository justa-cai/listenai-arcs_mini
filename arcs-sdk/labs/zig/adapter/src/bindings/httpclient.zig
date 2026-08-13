//! ARCS SDK HTTP client FFI bindings.
//! Maps HTTPCUsr_api.h through @cImport so Zig signatures track the C API.

const c = @cImport({
    @cInclude("stdint.h");
    @cInclude("stdbool.h");
    @cDefine("MBEDTLS_CONFIG_FILE", "\"arcs_zig_mbedtls_config.h\"");
    @cInclude("HTTPCUsr_api.h");
});

pub const c_api = c;

pub const max_url_length: comptime_int = c.HTTP_CLIENT_MAX_URL_LENGTH;
pub const max_username_length: comptime_int = c.HTTP_CLIENT_MAX_USERNAME_LENGTH;
pub const max_password_length: comptime_int = c.HTTP_CLIENT_MAX_PASSWORD_LENGTH;
pub const SUCCESS: c_int = c.HTTP_CLIENT_SUCCESS;
pub const EOS: c_int = c.HTTP_CLIENT_EOS;

pub const Verb = struct {
    pub const get: c.HTTP_VERB = c.VerbGet;
    pub const head: c.HTTP_VERB = c.VerbHead;
    pub const post: c.HTTP_VERB = c.VerbPost;
    pub const not_supported: c.HTTP_VERB = c.VerbNotSupported;
};

pub const AuthSchema = struct {
    pub const none: c.HTTP_AUTH_SCHEMA = c.AuthSchemaNone;
    pub const basic: c.HTTP_AUTH_SCHEMA = c.AuthSchemaBasic;
    pub const digest: c.HTTP_AUTH_SCHEMA = c.AuthSchemaDigest;
    pub const kerberos: c.HTTP_AUTH_SCHEMA = c.AuthSchemaKerberos;
    pub const not_supported: c.HTTP_AUTH_SCHEMA = c.AuthNotSupported;
};

pub const Parameters = c.HTTPParameters;
pub const ClientInfo = c.HTTP_CLIENT;
pub const RedirectParam = c.HTTP_REDIRECT_PARAM;
pub const HeaderCallback = c.HTTP_CLIENT_GET_HEADER;

pub const open = c.HTTPC_open;
pub const request = c.HTTPC_request;
pub const requestR = c.HTTPC_request_r;
pub const getRequestInfo = c.HTTPC_get_request_info;
pub const write = c.HTTPC_write;
pub const read = c.HTTPC_read;
pub const close = c.HTTPC_close;
pub const resetSession = c.HTTPC_reset_session;
pub const get = c.HTTPC_get;
pub const registerUserCerts = c.HTTPC_Register_user_certs;
pub const setSslVerifyMode = c.HTTPC_set_ssl_verify_mode;
pub const getSslVerifyMode = c.HTTPC_get_ssl_verify_mode;
pub const getCallbackFunc = c.HTTPC_get_callback_func;
pub const setCallbackFunc = c.HTTPC_set_callback_func;
pub const removeCallbackFunc = c.HTTPC_remove_callback_func;
