///! Zig Binding Struct Size Verification sample for ARCS SDK
///! Validates that Zig struct layouts match their C counterparts at runtime.
const arcs = @import("arcs");

const c_device = arcs.c.device;
const c_gpio = arcs.c.gpio;
const c_audio = arcs.c.audio;
const c_uart = arcs.c.uart;
const c_i2c = arcs.c.i2c;
const c_spi = arcs.c.spi;
const c_adc = arcs.c.adc;
const c_pwm = arcs.c.pwm;
const c_flash = arcs.c.flash;
const c_display = arcs.c.display;
const c_camera = arcs.c.camera;
const c_bluetooth = arcs.c.bluetooth;
const c_rtc = arcs.c.rtc;
const c_sysheap = arcs.c.sysheap;
const c_iomux = arcs.c.iomux;
const c_kv = arcs.c.kv;
const c_app_player = arcs.c.app_player;
const c_http = arcs.c.httpclient;
const c_types = arcs.c.types;

const c = @cImport({
    @cInclude("lisa_device.h");
    @cInclude("lisa_display.h");
    @cInclude("lisa_camera.h");
    @cInclude("sysheap.h");
    @cInclude("IOMuxManager.h");
    @cInclude("lisa_kv.h");
    @cInclude("app_player.h");
    @cInclude("lisa_bluetooth.h");
    @cInclude("lisa_bluetooth_gap.h");
    @cInclude("lisa_ble_api.h");
    @cInclude("netcfg_ble.h");
});

fn assertSizeAlign(comptime ZigType: type, comptime CType: type) void {
    if (@sizeOf(ZigType) != @sizeOf(CType)) @compileError(@typeName(ZigType) ++ " size mismatch");
    if (@alignOf(ZigType) != @alignOf(CType)) @compileError(@typeName(ZigType) ++ " align mismatch");
}

fn assertFieldOffset(comptime ZigType: type, comptime CType: type, comptime field: []const u8) void {
    if (@offsetOf(ZigType, field) != @offsetOf(CType, field)) {
        @compileError(@typeName(ZigType) ++ "." ++ field ++ " offset mismatch");
    }
}

comptime {
    if (@intFromEnum(c_types.DeviceError.invalid) != c.LISA_DEVICE_ERR_INVALID) @compileError("DeviceError.invalid binding mismatch");
    if (@intFromEnum(c_types.DeviceError.not_support) != c.LISA_DEVICE_ERR_NOT_SUPPORT) @compileError("DeviceError.not_support binding mismatch");

    const sysheap_init_check: @TypeOf(c.sysheap_init) = c_sysheap.sysheap_init;
    const inram_malloc_check: @TypeOf(c.inram_malloc) = c_sysheap.inram_malloc;
    const inram_calloc_check: @TypeOf(c.inram_calloc) = c_sysheap.inram_calloc;
    const inram_realloc_check: @TypeOf(c.inram_realloc) = c_sysheap.inram_realloc;
    const inram_free_check: @TypeOf(c.inram_free) = c_sysheap.inram_free;
    const exram_malloc_check: @TypeOf(c.exram_malloc) = c_sysheap.exram_malloc;
    const exram_calloc_check: @TypeOf(c.exram_calloc) = c_sysheap.exram_calloc;
    const exram_realloc_check: @TypeOf(c.exram_realloc) = c_sysheap.exram_realloc;
    const exram_free_check: @TypeOf(c.exram_free) = c_sysheap.exram_free;
    const psram_malloc_check: @TypeOf(c.psram_malloc) = c_sysheap.psram_malloc;
    const psram_malloc_align_check: @TypeOf(c.psram_malloc_align) = c_sysheap.psram_malloc_align;
    const psram_calloc_check: @TypeOf(c.psram_calloc) = c_sysheap.psram_calloc;
    const psram_calloc_align_check: @TypeOf(c.psram_calloc_align) = c_sysheap.psram_calloc_align;
    const psram_realloc_check: @TypeOf(c.psram_realloc) = c_sysheap.psram_realloc;
    const psram_free_check: @TypeOf(c.psram_free) = c_sysheap.psram_free;
    const psram_malloc_alias_check: @TypeOf(c.psram_malloc) = c_sysheap.psramMalloc;
    const psram_free_alias_check: @TypeOf(c.psram_free) = c_sysheap.psramFree;
    const iomux_pin_configure_check: @TypeOf(c.IOMuxManager_PinConfigure) = c_iomux.pinConfigure;
    const iomux_mode_configure_check: @TypeOf(c.IOMuxManager_ModeConfigure) = c_iomux.modeConfigure;
    const iomux_pin_force_check: @TypeOf(c.IOMuxManager_PinForce) = c_iomux.pinForce;
    const aon_iomux_pin_configure_check: @TypeOf(c.AON_IOMuxManager_PinConfigure) = c_iomux.aonPinConfigure;
    const aon_iomux_mode_configure_check: @TypeOf(c.AON_IOMuxManager_ModeConfigure) = c_iomux.aonModeConfigure;
    const aon_iomux_pin_force_check: @TypeOf(c.AON_IOMuxManager_PinForce) = c_iomux.aonPinForce;
    const ana_iomux_pin_configure_check: @TypeOf(c.ANA_IOMuxManager_PinConfigure) = c_iomux.anaPinConfigure;
    if (c_iomux.PAD_A != c.CSK_IOMUX_PAD_A) @compileError("iomux PAD_A binding mismatch");
    if (c_iomux.PAD_B != c.CSK_IOMUX_PAD_B) @compileError("iomux PAD_B binding mismatch");
    if (c_iomux.PAD_B_MAX_PIN != c.CSK_IOMUX_PAD_B_MAX_PIN) @compileError("iomux PAD_B_MAX_PIN binding mismatch");
    if (c_iomux.FUNC_DEFAULT != c.CSK_IOMUX_FUNC_DEFAULT) @compileError("iomux FUNC_DEFAULT binding mismatch");
    if (c_iomux.FUNC_ALTER1 != c.CSK_IOMUX_FUNC_ALTER1) @compileError("iomux FUNC_ALTER1 binding mismatch");
    if (c_iomux.FUNC_ALTER6 != c.CSK_IOMUX_FUNC_ALTER6) @compileError("iomux FUNC_ALTER6 binding mismatch");
    if (c_iomux.FUNC_ALTER12 != c.CSK_IOMUX_FUNC_ALTER12) @compileError("iomux FUNC_ALTER12 binding mismatch");
    if (c_iomux.FUNC_ALTER16 != c.CSK_IOMUX_FUNC_ALTER16) @compileError("iomux FUNC_ALTER16 binding mismatch");
    if (c_iomux.AON_FUNC_ALTER3 != c.CSK_AON_IOMUX_FUNC_ALTER3) @compileError("iomux AON_FUNC_ALTER3 binding mismatch");
    const bt_c = c_bluetooth.c_api;
    const bt_init_check: @TypeOf(bt_c.lisa_bluetooth_init) = c_bluetooth.lisa_bluetooth_init;
    const bt_inquiry_start_check: @TypeOf(bt_c.lisa_bluetooth_inquiry_start) = c_bluetooth.lisa_bluetooth_inquiry_start;
    const bt_connect_by_name_check: @TypeOf(bt_c.lisa_bluetooth_connect_by_name) = c_bluetooth.lisa_bluetooth_connect_by_name;
    const bt_connect_by_index_check: @TypeOf(bt_c.lisa_bluetooth_connect_by_index) = c_bluetooth.lisa_bluetooth_connect_by_index;
    const bt_connect_by_addr_check: @TypeOf(bt_c.lisa_bluetooth_connect_by_addr) = c_bluetooth.lisa_bluetooth_connect_by_addr;
    const bt_disconnect_by_addr_check: @TypeOf(bt_c.lisa_bluetooth_disconnect_by_addr) = c_bluetooth.lisa_bluetooth_disconnect_by_addr;
    const bt_disconnect_by_name_check: @TypeOf(bt_c.lisa_bluetooth_disconnect_by_name) = c_bluetooth.lisa_bluetooth_disconnect_by_name;
    const bt_disconnect_by_index_check: @TypeOf(bt_c.lisa_bluetooth_disconnect_by_index) = c_bluetooth.lisa_bluetooth_disconnect_by_index;
    const bt_get_discovered_devices_check: @TypeOf(bt_c.lisa_bluetooth_get_discovered_devices) = c_bluetooth.lisa_bluetooth_get_discovered_devices;
    const bt_clear_discovered_devices_check: @TypeOf(bt_c.lisa_bluetooth_clear_discovered_devices) = c_bluetooth.lisa_bluetooth_clear_discovered_devices;
    const bt_register_discovery_callback_check: @TypeOf(bt_c.lisa_bluetooth_register_discovery_callback) = c_bluetooth.lisa_bluetooth_register_discovery_callback;
    const bt_paired_list_get_check: @TypeOf(bt_c.bt_paired_list_get) = c_bluetooth.pairedListGet;
    const bt_paired_name_get_check: @TypeOf(bt_c.bt_paired_name_get) = c_bluetooth.pairedNameGet;
    const bt_paired_remove_check: @TypeOf(bt_c.bt_paired_remove) = c_bluetooth.pairedRemove;
    const ble_register_conn_cb_check: @TypeOf(bt_c.lisa_ble_register_conn_cb) = c_bluetooth.lisa_ble_register_conn_cb;
    const ble_register_disc_cb_check: @TypeOf(bt_c.lisa_ble_register_disc_cb) = c_bluetooth.lisa_ble_register_disc_cb;
    const ble_register_bond_cb_check: @TypeOf(bt_c.lisa_ble_register_bond_cb) = c_bluetooth.lisa_ble_register_bond_cb;
    const ble_register_key_req_cb_check: @TypeOf(bt_c.lisa_ble_register_key_req_cb) = c_bluetooth.lisa_ble_register_key_req_cb;
    const ble_key_confirm_check: @TypeOf(bt_c.lisa_ble_key_confirm) = c_bluetooth.lisa_ble_key_confirm;
    const bt_classic_register_conn_cb_check: @TypeOf(bt_c.lisa_bt_classic_register_conn_cb) = c_bluetooth.lisa_bt_classic_register_conn_cb;
    const bt_classic_register_disc_cb_check: @TypeOf(bt_c.lisa_bt_classic_register_disc_cb) = c_bluetooth.lisa_bt_classic_register_disc_cb;
    const bt_classic_register_avrcp_cb_check: @TypeOf(bt_c.lisa_bt_classic_register_avrcp_cb) = c_bluetooth.lisa_bt_classic_register_avrcp_cb;
    const bt_classic_register_profile_cb_check: @TypeOf(bt_c.lisa_bt_classic_register_profile_cb) = c_bluetooth.lisa_bt_classic_register_profile_cb;
    const ble_adv_start_check: @TypeOf(bt_c.lisa_ble_adv_start) = c_bluetooth.advStart;
    const ble_adv_stop_check: @TypeOf(bt_c.lisa_ble_adv_stop) = c_bluetooth.advStop;
    const ble_scan_start_check: @TypeOf(bt_c.lisa_ble_scan_start) = c_bluetooth.scanStart;
    const ble_scan_stop_check: @TypeOf(bt_c.lisa_ble_scan_stop) = c_bluetooth.scanStop;
    const ble_netcfg_set_custom_op_handler_check: @TypeOf(bt_c.lisa_ble_netcfg_set_custom_op_handler) = c_bluetooth.netcfgSetCustomOpHandler;
    const ble_netcfg_send_custom_data_check: @TypeOf(bt_c.lisa_ble_netcfg_send_custom_data) = c_bluetooth.netcfgSendCustomData;
    if (c_bluetooth.ADV_GEN != c.LISA_BLE_ADV_GEN) @compileError("bluetooth ADV_GEN binding mismatch");
    if (c_bluetooth.ADV_DIR != c.LISA_BLE_ADV_DIR) @compileError("bluetooth ADV_DIR binding mismatch");
    if (c_bluetooth.NETCFG_AUTH_INFO != c.NETCFG_BLE_AUTH_INFO) @compileError("bluetooth NETCFG_AUTH_INFO binding mismatch");
    if (c_bluetooth.NETCFG_ERR != c.NETCFG_BLE_ERR) @compileError("bluetooth NETCFG_ERR binding mismatch");
    if (c_bluetooth.GAP_AD_TYPE_MANUFACTURER_SPECIFIC != c.GAP_AD_TYPE_MANUFACTURER_SPECIFIC) @compileError("bluetooth GAP_AD_TYPE_MANUFACTURER_SPECIFIC binding mismatch");
    if (c_bluetooth.GAP_AD_TYPE_LOCAL_NAME_COMPLETE != c.GAP_AD_TYPE_LOCAL_NAME_COMPLETE) @compileError("bluetooth GAP_AD_TYPE_LOCAL_NAME_COMPLETE binding mismatch");
    if (c_bluetooth.MAX_DISCOVERED_DEVICES != c.MAX_DISCOVERED_DEVICES) @compileError("bluetooth MAX_DISCOVERED_DEVICES binding mismatch");
    if (c_bluetooth.DISCOVERY_GENERAL != c.GAPM_DISC_TYPE_GEN_DISC) @compileError("bluetooth DISCOVERY_GENERAL binding mismatch");
    if (c_bluetooth.DISCOVERY_LIMITED != c.GAPM_DISC_TYPE_LIM_DISC) @compileError("bluetooth DISCOVERY_LIMITED binding mismatch");
    assertSizeAlign(c_bluetooth.BdAddr, c.gap_bdaddr_t);
    assertFieldOffset(c_bluetooth.BdAddr, c.gap_bdaddr_t, "addr");
    assertFieldOffset(c_bluetooth.BdAddr, c.gap_bdaddr_t, "addr_type");
    assertSizeAlign(c_bluetooth.DiscoveryInfo, c.lisa_bt_discovery_info_t);
    assertFieldOffset(c_bluetooth.DiscoveryInfo, c.lisa_bt_discovery_info_t, "addr");
    assertFieldOffset(c_bluetooth.DiscoveryInfo, c.lisa_bt_discovery_info_t, "clk_off");
    assertFieldOffset(c_bluetooth.DiscoveryInfo, c.lisa_bt_discovery_info_t, "rssi");
    assertFieldOffset(c_bluetooth.DiscoveryInfo, c.lisa_bt_discovery_info_t, "cod");
    assertFieldOffset(c_bluetooth.DiscoveryInfo, c.lisa_bt_discovery_info_t, "name_len");
    assertFieldOffset(c_bluetooth.DiscoveryInfo, c.lisa_bt_discovery_info_t, "name");
    assertSizeAlign(c_bluetooth.BleAddr, c.lisa_ble_addr_t);
    assertFieldOffset(c_bluetooth.BleAddr, c.lisa_ble_addr_t, "addr");
    assertFieldOffset(c_bluetooth.BleAddr, c.lisa_ble_addr_t, "addr_type");
    const kv_init_check: @TypeOf(c.lisa_kv_init) = c_kv.init;
    const kv_del_check: @TypeOf(c.lisa_kv_del) = c_kv.del;
    const kv_free_check: @TypeOf(c.lisa_kv_free) = c_kv.free;
    const kv_dump_check: @TypeOf(c.lisa_kv_dump) = c_kv.dump;
    const kv_set_int_check: @TypeOf(c.lisa_kv_set_int) = c_kv.setIntRaw;
    const kv_get_int_check: @TypeOf(c.lisa_kv_get_int) = c_kv.getIntRaw;
    const kv_set_string_check: @TypeOf(c.lisa_kv_set_string) = c_kv.setStringRaw;
    const kv_get_string_check: @TypeOf(c.lisa_kv_get_string) = c_kv.getStringRaw;
    const kv_set_bool_check: @TypeOf(c.lisa_kv_set_bool) = c_kv.setBoolRaw;
    const kv_get_bool_check: @TypeOf(c.lisa_kv_get_bool) = c_kv.getBoolRaw;
    const kv_clear_check: @TypeOf(c.lisa_kv_clear) = c_kv.clear;
    const kv_get_blob_check: @TypeOf(c.lisa_kv_get_blob) = c_kv.getBlobRaw;
    const kv_set_blob_check: @TypeOf(c.lisa_kv_set_blob) = c_kv.setBlobRaw;
    if (@sizeOf(c_kv.StringValue) != @sizeOf([*c]u8)) @compileError("kv StringValue pointer size mismatch");
    if (@alignOf(c_kv.StringValue) != @alignOf([*c]u8)) @compileError("kv StringValue pointer align mismatch");
    if (@sizeOf(c_kv.BlobValue) != @sizeOf([*c]u8)) @compileError("kv BlobValue pointer size mismatch");
    if (@alignOf(c_kv.BlobValue) != @alignOf([*c]u8)) @compileError("kv BlobValue pointer align mismatch");
    const kv_set_blob_helper_check: fn ([:0]const u8, []u8) callconv(.Inline) c_int = c_kv.setBlob;
    const kv_get_blob_helper_check: fn ([:0]const u8, *c_kv.BlobValue, *c_int) callconv(.Inline) c_int = c_kv.getBlob;
    const kv_free_blob_helper_check: fn (c_kv.BlobValue) callconv(.Inline) void = c_kv.freeBlob;
    const app_player_init_check: fn ([*c]const c_app_player.Config) callconv(.C) c_int = c_app_player.init;
    const app_player_create_check: fn ([*c]const u8) callconv(.C) ?*c_app_player.Player = c_app_player.create;
    const app_player_destroy_check: fn (?*c_app_player.Player) callconv(.C) c_int = c_app_player.destroy;
    const app_player_register_callback_check: fn (?*c_app_player.Player, c_app_player.EventCallback, ?*anyopaque) callconv(.C) c_int = c_app_player.registerCallback;
    const app_player_play_check: fn (?*c_app_player.Player, [*c]const u8) callconv(.C) c_int = c_app_player.play;
    const app_player_play_ex_check: fn (?*c_app_player.Player, [*c]const c_app_player.PlayOpt) callconv(.C) c_int = c_app_player.playEx;
    const app_player_stop_check: fn (?*c_app_player.Player) callconv(.C) c_int = c_app_player.stop;
    const app_player_pause_check: fn (?*c_app_player.Player) callconv(.C) c_int = c_app_player.pause;
    const app_player_resume_check: fn (?*c_app_player.Player) callconv(.C) c_int = c_app_player.resumePlayback;
    const app_player_reset_check: fn (?*c_app_player.Player) callconv(.C) c_int = c_app_player.reset;
    const app_player_seek_check: fn (?*c_app_player.Player, u32) callconv(.C) c_int = c_app_player.seek;
    const app_player_get_state_check: fn (?*c_app_player.Player) callconv(.C) c_app_player.State = c_app_player.getState;
    const app_player_get_position_check: fn (?*c_app_player.Player, [*c]u32) callconv(.C) c_int = c_app_player.getPosition;
    const app_player_get_duration_check: fn (?*c_app_player.Player, [*c]u32) callconv(.C) c_int = c_app_player.getDuration;
    const app_player_set_volume_check: fn (?*c_app_player.Player, u8) callconv(.C) c_int = c_app_player.setVolume;
    const app_player_play_stream_check: fn (?*c_app_player.Player, u32, u8, u8) callconv(.C) c_int = c_app_player.playStream;
    const app_player_write_stream_check: fn (?*c_app_player.Player, [*c]const u8, u32, u32) callconv(.C) c_int = c_app_player.writeStream;
    const app_player_finish_stream_check: fn (?*c_app_player.Player) callconv(.C) c_int = c_app_player.finishStream;
    if (c_app_player.OK != c.APP_PLAYER_OK) @compileError("app_player OK binding mismatch");
    if (c_app_player.ERR_INVALID_PARAM != c.APP_PLAYER_ERR_INVALID_PARAM) @compileError("app_player ERR_INVALID_PARAM binding mismatch");
    if (c_app_player.STATE_IDLE != c.APP_PLAYER_STATE_IDLE) @compileError("app_player STATE_IDLE binding mismatch");
    if (c_app_player.STATE_PLAYING != c.APP_PLAYER_STATE_PLAYING) @compileError("app_player STATE_PLAYING binding mismatch");
    if (c_app_player.EVENT_PLAYING != c.APP_PLAYER_EVENT_PLAYING) @compileError("app_player EVENT_PLAYING binding mismatch");
    if (c_app_player.EVENT_PAUSED != c.APP_PLAYER_EVENT_PAUSED) @compileError("app_player EVENT_PAUSED binding mismatch");
    if (c_app_player.EVENT_COMPLETED != c.APP_PLAYER_EVENT_COMPLETED) @compileError("app_player EVENT_COMPLETED binding mismatch");
    const http_c = c_http.c_api;
    const http_open_check: @TypeOf(http_c.HTTPC_open) = c_http.open;
    const http_request_check: @TypeOf(http_c.HTTPC_request) = c_http.request;
    const http_request_r_check: @TypeOf(http_c.HTTPC_request_r) = c_http.requestR;
    const http_get_request_info_check: @TypeOf(http_c.HTTPC_get_request_info) = c_http.getRequestInfo;
    const http_write_check: @TypeOf(http_c.HTTPC_write) = c_http.write;
    const http_read_check: @TypeOf(http_c.HTTPC_read) = c_http.read;
    const http_close_check: @TypeOf(http_c.HTTPC_close) = c_http.close;
    const http_reset_session_check: @TypeOf(http_c.HTTPC_reset_session) = c_http.resetSession;
    const http_get_check: @TypeOf(http_c.HTTPC_get) = c_http.get;
    const http_register_user_certs_check: @TypeOf(http_c.HTTPC_Register_user_certs) = c_http.registerUserCerts;
    const http_set_ssl_verify_mode_check: @TypeOf(http_c.HTTPC_set_ssl_verify_mode) = c_http.setSslVerifyMode;
    const http_get_ssl_verify_mode_check: @TypeOf(http_c.HTTPC_get_ssl_verify_mode) = c_http.getSslVerifyMode;
    const http_get_callback_func_check: @TypeOf(http_c.HTTPC_get_callback_func) = c_http.getCallbackFunc;
    const http_set_callback_func_check: @TypeOf(http_c.HTTPC_set_callback_func) = c_http.setCallbackFunc;
    const http_remove_callback_func_check: @TypeOf(http_c.HTTPC_remove_callback_func) = c_http.removeCallbackFunc;
    if (c_http.max_url_length != http_c.HTTP_CLIENT_MAX_URL_LENGTH) @compileError("httpclient max_url_length binding mismatch");
    if (c_http.max_username_length != http_c.HTTP_CLIENT_MAX_USERNAME_LENGTH) @compileError("httpclient max_username_length binding mismatch");
    if (c_http.max_password_length != http_c.HTTP_CLIENT_MAX_PASSWORD_LENGTH) @compileError("httpclient max_password_length binding mismatch");
    if (c_http.SUCCESS != http_c.HTTP_CLIENT_SUCCESS) @compileError("httpclient SUCCESS binding mismatch");
    if (c_http.EOS != http_c.HTTP_CLIENT_EOS) @compileError("httpclient EOS binding mismatch");
    if (c_http.Verb.get != http_c.VerbGet) @compileError("httpclient Verb.get binding mismatch");
    if (c_http.AuthSchema.none != http_c.AuthSchemaNone) @compileError("httpclient AuthSchema.none binding mismatch");
    assertSizeAlign(c_http.Parameters, http_c.HTTPParameters);
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "Uri");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "HttpVerb");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "Verbose");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "UserName");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "Password");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "AuthType");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "isTransfer");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "pHTTP");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "Flags");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "pData");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "pLength");
    assertFieldOffset(c_http.Parameters, http_c.HTTPParameters, "nTimeout");
    assertSizeAlign(c_http.ClientInfo, http_c.HTTP_CLIENT);
    assertFieldOffset(c_http.ClientInfo, http_c.HTTP_CLIENT, "HTTPStatusCode");
    assertFieldOffset(c_http.ClientInfo, http_c.HTTP_CLIENT, "RequestBodyLengthSent");
    assertFieldOffset(c_http.ClientInfo, http_c.HTTP_CLIENT, "ResponseBodyLengthReceived");
    assertFieldOffset(c_http.ClientInfo, http_c.HTTP_CLIENT, "TotalResponseBodyLength");
    assertFieldOffset(c_http.ClientInfo, http_c.HTTP_CLIENT, "HttpState");
    if (@hasField(c_http.ClientInfo, "RedirectUrl")) {
        assertFieldOffset(c_http.ClientInfo, http_c.HTTP_CLIENT, "RedirectUrl");
    }
    if (@hasField(c_http.ClientInfo, "HttpFlags")) {
        assertFieldOffset(c_http.ClientInfo, http_c.HTTP_CLIENT, "HttpFlags");
    }
    assertSizeAlign(c_http.RedirectParam, http_c.HTTP_REDIRECT_PARAM);
    assertFieldOffset(c_http.RedirectParam, http_c.HTTP_REDIRECT_PARAM, "pParam");
    assertFieldOffset(c_http.RedirectParam, http_c.HTTP_REDIRECT_PARAM, "nLength");

    if (@intFromEnum(c_display.PixelFormat.rgb_888) != c.LISA_DISPLAY_PIXEL_FORMAT_RGB_888) @compileError("display PixelFormat.rgb_888 binding mismatch");
    if (@intFromEnum(c_display.PixelFormat.rgb_565) != c.LISA_DISPLAY_PIXEL_FORMAT_RGB_565) @compileError("display PixelFormat.rgb_565 binding mismatch");
    if (@intFromEnum(c_display.Orientation.@"0") != c.LISA_DISPLAY_ORIENTATION_0) @compileError("display Orientation.0 binding mismatch");
    if (@intFromEnum(c_display.Orientation.@"90") != c.LISA_DISPLAY_ORIENTATION_90) @compileError("display Orientation.90 binding mismatch");
    if (@intFromEnum(c_display.BusType.rgb) != c.LISA_DISPLAY_BUS_RGB) @compileError("display BusType.rgb binding mismatch");
    if (@intFromEnum(c_display.BacklightPolarity.high) != c.LISA_DISPLAY_BLACKLIGHT_POLARITY_HIGH) @compileError("display BacklightPolarity.high binding mismatch");
    assertSizeAlign(c_display.Capabilities, c.lisa_display_capabilities_t);
    assertFieldOffset(c_display.Capabilities, c.lisa_display_capabilities_t, "width");
    assertFieldOffset(c_display.Capabilities, c.lisa_display_capabilities_t, "height");
    assertFieldOffset(c_display.Capabilities, c.lisa_display_capabilities_t, "pixel_format");
    assertFieldOffset(c_display.Capabilities, c.lisa_display_capabilities_t, "orientation");
    assertFieldOffset(c_display.Capabilities, c.lisa_display_capabilities_t, "supported_pixel_formats");
    assertSizeAlign(c_display.BufferDesc, c.lisa_display_buffer_desc_t);
    assertFieldOffset(c_display.BufferDesc, c.lisa_display_buffer_desc_t, "width");
    assertFieldOffset(c_display.BufferDesc, c.lisa_display_buffer_desc_t, "height");
    assertFieldOffset(c_display.BufferDesc, c.lisa_display_buffer_desc_t, "pitch");
    assertFieldOffset(c_display.BufferDesc, c.lisa_display_buffer_desc_t, "buf_size");
    const CDisplayRgbTimings = @TypeOf(@as(c.lisa_display_bus_rgb_config_t, undefined).timings);
    assertSizeAlign(c_display.RgbTimings, CDisplayRgbTimings);
    assertFieldOffset(c_display.RgbTimings, CDisplayRgbTimings, "h_res");
    assertFieldOffset(c_display.RgbTimings, CDisplayRgbTimings, "v_res");
    assertFieldOffset(c_display.RgbTimings, CDisplayRgbTimings, "h_pulse_width");
    assertFieldOffset(c_display.RgbTimings, CDisplayRgbTimings, "v_pulse_width");
    assertFieldOffset(c_display.RgbTimings, CDisplayRgbTimings, "h_front_blanking");
    assertFieldOffset(c_display.RgbTimings, CDisplayRgbTimings, "h_back_blanking");
    assertFieldOffset(c_display.RgbTimings, CDisplayRgbTimings, "v_front_blanking");
    assertFieldOffset(c_display.RgbTimings, CDisplayRgbTimings, "v_back_blanking");
    assertSizeAlign(c_display.BusRgbConfig, c.lisa_display_bus_rgb_config_t);
    assertFieldOffset(c_display.BusRgbConfig, c.lisa_display_bus_rgb_config_t, "rgb_dev");
    assertFieldOffset(c_display.BusRgbConfig, c.lisa_display_bus_rgb_config_t, "output_lsb_first");
    assertFieldOffset(c_display.BusRgbConfig, c.lisa_display_bus_rgb_config_t, "timings");
    assertSizeAlign(c_display.Backlight, c.lisa_display_backlight_t);
    assertFieldOffset(c_display.Backlight, c.lisa_display_backlight_t, "type");
    assertFieldOffset(c_display.Backlight, c.lisa_display_backlight_t, "config");
    assertFieldOffset(c_display.Backlight, c.lisa_display_backlight_t, "blacklight_polarity");
    assertFieldOffset(c_display.Backlight, c.lisa_display_backlight_t, "priv_data");
    assertSizeAlign(c_display.Config, c.lisa_display_config_t);
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "panel_name");
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "bus_type");
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "bus_config");
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "cmd_bus_type");
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "cmd_bus_config");
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "rst_gpio");
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "te_gpio");
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "backlight");
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "panel_init_params");
    assertFieldOffset(c_display.Config, c.lisa_display_config_t, "panel_init_params_len");
    assertSizeAlign(c_display.DisplayApi, c.lisa_display_api_t);
    assertFieldOffset(c_display.DisplayApi, c.lisa_display_api_t, "get_capabilities");
    assertFieldOffset(c_display.DisplayApi, c.lisa_display_api_t, "write");
    assertFieldOffset(c_display.DisplayApi, c.lisa_display_api_t, "set_brightness");
    assertFieldOffset(c_display.DisplayApi, c.lisa_display_api_t, "attach_bus");

    if (c_camera.PIXFMT_RGB565 != c.LISA_CAMERA_PIXFMT_RGB565) @compileError("camera PIXFMT_RGB565 binding mismatch");
    if (c_camera.PIXFMT_JPEG != c.LISA_CAMERA_PIXFMT_JPEG) @compileError("camera PIXFMT_JPEG binding mismatch");
    if (c_camera.FRAMESIZE_FHD != c.LISA_CAMERA_FRAMESIZE_FHD) @compileError("camera FRAMESIZE_FHD binding mismatch");
    if (c_camera.BUS_SPI != c.LISA_CAMERA_BUS_SPI) @compileError("camera BUS_SPI binding mismatch");
    if (c_camera.SENSOR_MAX != c.LISA_CAMERA_SENSOR_MAX) @compileError("camera SENSOR_MAX binding mismatch");
    assertSizeAlign(c_camera.BusDvpConfig, c.lisa_camera_bus_dvp_config_t);
    assertFieldOffset(c_camera.BusDvpConfig, c.lisa_camera_bus_dvp_config_t, "dvp_dev");
    assertFieldOffset(c_camera.BusDvpConfig, c.lisa_camera_bus_dvp_config_t, "dvp_freq");
    assertFieldOffset(c_camera.BusDvpConfig, c.lisa_camera_bus_dvp_config_t, "pixel_offset");
    assertFieldOffset(c_camera.BusDvpConfig, c.lisa_camera_bus_dvp_config_t, "data_align");
    assertFieldOffset(c_camera.BusDvpConfig, c.lisa_camera_bus_dvp_config_t, "dma_dev");
    assertSizeAlign(c_camera.BusSpiConfig, c.lisa_camera_bus_spi_config_t);
    assertFieldOffset(c_camera.BusSpiConfig, c.lisa_camera_bus_spi_config_t, "spi_dev");
    assertFieldOffset(c_camera.BusSpiConfig, c.lisa_camera_bus_spi_config_t, "spi_freq");
    assertFieldOffset(c_camera.BusSpiConfig, c.lisa_camera_bus_spi_config_t, "cs_gpio");
    assertFieldOffset(c_camera.BusSpiConfig, c.lisa_camera_bus_spi_config_t, "dma_dev");
    assertSizeAlign(c_camera.BusConfig, c.lisa_camera_bus_config_t);
    assertFieldOffset(c_camera.BusConfig, c.lisa_camera_bus_config_t, "bus_type");
    assertFieldOffset(c_camera.BusConfig, c.lisa_camera_bus_config_t, "config");
    assertFieldOffset(c_camera.BusConfig, c.lisa_camera_bus_config_t, "pixel_format");
    assertFieldOffset(c_camera.BusConfig, c.lisa_camera_bus_config_t, "dma_channel");
    assertFieldOffset(c_camera.BusConfig, c.lisa_camera_bus_config_t, "width");
    assertFieldOffset(c_camera.BusConfig, c.lisa_camera_bus_config_t, "height");
    assertSizeAlign(c_camera.HwConfig, c.lisa_camera_hw_config_t);
    assertFieldOffset(c_camera.HwConfig, c.lisa_camera_hw_config_t, "mclk_pad");
    assertFieldOffset(c_camera.HwConfig, c.lisa_camera_hw_config_t, "reset_gpio_dev");
    assertFieldOffset(c_camera.HwConfig, c.lisa_camera_hw_config_t, "reset_pin");
    assertFieldOffset(c_camera.HwConfig, c.lisa_camera_hw_config_t, "reset_delay_us");
    assertFieldOffset(c_camera.HwConfig, c.lisa_camera_hw_config_t, "i2c_dev");
    assertFieldOffset(c_camera.HwConfig, c.lisa_camera_hw_config_t, "multiplex_camera");
    assertFieldOffset(c_camera.HwConfig, c.lisa_camera_hw_config_t, "sensor_pwdn");
    assertSizeAlign(c_camera.Config, c.lisa_camera_config_t);
    assertFieldOffset(c_camera.Config, c.lisa_camera_config_t, "hw_config");
    assertFieldOffset(c_camera.Config, c.lisa_camera_config_t, "xclk_freq_hz");
    assertFieldOffset(c_camera.Config, c.lisa_camera_config_t, "enable_colorbar");
    assertFieldOffset(c_camera.Config, c.lisa_camera_config_t, "mem_pool");
    assertFieldOffset(c_camera.Config, c.lisa_camera_config_t, "mem_pool_size");
    assertSizeAlign(c_camera.FrameBuffer, c.lisa_camera_fb_t);
    assertFieldOffset(c_camera.FrameBuffer, c.lisa_camera_fb_t, "buf");
    assertFieldOffset(c_camera.FrameBuffer, c.lisa_camera_fb_t, "len");
    assertFieldOffset(c_camera.FrameBuffer, c.lisa_camera_fb_t, "format");
    assertFieldOffset(c_camera.FrameBuffer, c.lisa_camera_fb_t, "timestamp");
    assertSizeAlign(c_camera.Crop, c.lisa_camera_crop_t);
    assertFieldOffset(c_camera.Crop, c.lisa_camera_crop_t, "x");
    assertFieldOffset(c_camera.Crop, c.lisa_camera_crop_t, "width");
    assertSizeAlign(c_camera.Capabilities, c.lisa_camera_capabilities_t);
    assertFieldOffset(c_camera.Capabilities, c.lisa_camera_capabilities_t, "max_width");
    assertFieldOffset(c_camera.Capabilities, c.lisa_camera_capabilities_t, "supported_formats");
    assertSizeAlign(c_camera.CameraApi, c.lisa_camera_api_t);
    assertFieldOffset(c_camera.CameraApi, c.lisa_camera_api_t, "setup");
    assertFieldOffset(c_camera.CameraApi, c.lisa_camera_api_t, "capture");
    assertFieldOffset(c_camera.CameraApi, c.lisa_camera_api_t, "attach_bus");
    assertFieldOffset(c_camera.CameraApi, c.lisa_camera_api_t, "get_pixformat");
    assertFieldOffset(c_camera.CameraApi, c.lisa_camera_api_t, "get_current_sensor");

    _ = sysheap_init_check;
    _ = inram_malloc_check;
    _ = inram_calloc_check;
    _ = inram_realloc_check;
    _ = inram_free_check;
    _ = exram_malloc_check;
    _ = exram_calloc_check;
    _ = exram_realloc_check;
    _ = exram_free_check;
    _ = psram_malloc_check;
    _ = psram_malloc_align_check;
    _ = psram_calloc_check;
    _ = psram_calloc_align_check;
    _ = psram_realloc_check;
    _ = psram_free_check;
    _ = psram_malloc_alias_check;
    _ = psram_free_alias_check;
    _ = iomux_pin_configure_check;
    _ = iomux_mode_configure_check;
    _ = iomux_pin_force_check;
    _ = aon_iomux_pin_configure_check;
    _ = aon_iomux_mode_configure_check;
    _ = aon_iomux_pin_force_check;
    _ = ana_iomux_pin_configure_check;
    _ = bt_init_check;
    _ = bt_inquiry_start_check;
    _ = bt_connect_by_name_check;
    _ = bt_connect_by_index_check;
    _ = bt_connect_by_addr_check;
    _ = bt_disconnect_by_addr_check;
    _ = bt_disconnect_by_name_check;
    _ = bt_disconnect_by_index_check;
    _ = bt_get_discovered_devices_check;
    _ = bt_clear_discovered_devices_check;
    _ = bt_register_discovery_callback_check;
    _ = bt_paired_list_get_check;
    _ = bt_paired_name_get_check;
    _ = bt_paired_remove_check;
    _ = ble_register_conn_cb_check;
    _ = ble_register_disc_cb_check;
    _ = ble_register_bond_cb_check;
    _ = ble_register_key_req_cb_check;
    _ = ble_key_confirm_check;
    _ = bt_classic_register_conn_cb_check;
    _ = bt_classic_register_disc_cb_check;
    _ = bt_classic_register_avrcp_cb_check;
    _ = bt_classic_register_profile_cb_check;
    _ = ble_adv_start_check;
    _ = ble_adv_stop_check;
    _ = ble_scan_start_check;
    _ = ble_scan_stop_check;
    _ = ble_netcfg_set_custom_op_handler_check;
    _ = ble_netcfg_send_custom_data_check;
    _ = kv_init_check;
    _ = kv_del_check;
    _ = kv_free_check;
    _ = kv_dump_check;
    _ = kv_set_int_check;
    _ = kv_get_int_check;
    _ = kv_set_string_check;
    _ = kv_get_string_check;
    _ = kv_set_bool_check;
    _ = kv_get_bool_check;
    _ = kv_clear_check;
    _ = kv_get_blob_check;
    _ = kv_set_blob_check;
    _ = kv_set_blob_helper_check;
    _ = kv_get_blob_helper_check;
    _ = kv_free_blob_helper_check;
    _ = app_player_init_check;
    _ = app_player_create_check;
    _ = app_player_destroy_check;
    _ = app_player_register_callback_check;
    _ = app_player_play_check;
    _ = app_player_play_ex_check;
    _ = app_player_stop_check;
    _ = app_player_pause_check;
    _ = app_player_resume_check;
    _ = app_player_reset_check;
    _ = app_player_seek_check;
    _ = app_player_get_state_check;
    _ = app_player_get_position_check;
    _ = app_player_get_duration_check;
    _ = app_player_set_volume_check;
    _ = app_player_play_stream_check;
    _ = app_player_write_stream_check;
    _ = app_player_finish_stream_check;
    _ = http_open_check;
    _ = http_request_check;
    _ = http_request_r_check;
    _ = http_get_request_info_check;
    _ = http_write_check;
    _ = http_read_check;
    _ = http_close_check;
    _ = http_reset_session_check;
    _ = http_get_check;
    _ = http_register_user_certs_check;
    _ = http_set_ssl_verify_mode_check;
    _ = http_get_ssl_verify_mode_check;
    _ = http_get_callback_func_check;
    _ = http_set_callback_func_check;
    _ = http_remove_callback_func_check;
}

// 用函数返回 sizeof (避免 export var 静态初始化问题)
export fn zig_get_sizeof(id: u32) callconv(.C) u32 {
    return switch (id) {
        0 => @sizeOf(c_device.Device),
        1 => @sizeOf(c_device.DeviceStats),
        2 => @sizeOf(c_gpio.GpioApi),
        3 => @sizeOf(c_audio.AudioFormat),
        4 => @sizeOf(c_audio.AudioGain),
        5 => @sizeOf(c_audio.AudioEvent),
        6 => @sizeOf(c_audio.RecordConfig),
        64 => @sizeOf(c_audio.RecordChannelGain),
        7 => @sizeOf(c_audio.PlayConfig),
        8 => @sizeOf(c_audio.AudioApi),
        9 => @sizeOf(c_uart.Config),
        10 => @sizeOf(c_uart.UartApi),
        11 => @sizeOf(c_i2c.I2cMsg),
        12 => @sizeOf(c_i2c.I2cApi),
        13 => @sizeOf(c_spi.SpiConfig),
        14 => @sizeOf(c_spi.SpiApi),
        15 => @sizeOf(c_adc.AdcApi),
        16 => @sizeOf(c_pwm.PwmApi),
        17 => @sizeOf(c_flash.FlashParameters),
        18 => @sizeOf(c_flash.FlashApi),
        19 => @sizeOf(c_display.Capabilities),
        20 => @sizeOf(c_display.BufferDesc),
        21 => @sizeOf(c_display.DisplayApi),
        22 => @sizeOf(c_rtc.RtcTime),
        23 => @sizeOf(c_rtc.RtcApi),
        24 => @sizeOf(c_display.BusType),
        25 => @sizeOf(c_display.CmdBusType),
        26 => @sizeOf(c_display.RgbInputFormat),
        27 => @sizeOf(c_display.RgbPolarity),
        28 => @sizeOf(c_display.RgbOutputFormat),
        29 => @sizeOf(c_display.BacklightType),
        30 => @sizeOf(c_display.BacklightPolarity),
        31 => @sizeOf(c_display.BusSpi4WireConfig),
        32 => @sizeOf(c_display.BusSpi3WireConfig),
        33 => @sizeOf(c_display.BusQspiConfig),
        34 => @sizeOf(c_display.RgbTimings),
        35 => @sizeOf(c_display.BusRgbConfig),
        36 => @sizeOf(c_display.CmdSwSpiConfig),
        37 => @sizeOf(c_display.CmdBusConfig),
        38 => @sizeOf(c_display.BacklightPwmConfig),
        39 => @sizeOf(c_display.BacklightSingleWireConfig),
        40 => @sizeOf(c_display.BacklightConfig),
        41 => @sizeOf(c_display.Backlight),
        42 => @sizeOf(c_display.BusConfig),
        43 => @sizeOf(c_display.Config),
        44 => @sizeOf(c_camera.PixelFormat),
        45 => @sizeOf(c_camera.FrameSize),
        46 => @sizeOf(c_camera.BusType),
        47 => @sizeOf(c_camera.SensorIndex),
        48 => @sizeOf(c_camera.SensorPwdn),
        49 => @sizeOf(c_camera.BusDvpConfig),
        50 => @sizeOf(c_camera.BusSpiConfig),
        51 => @sizeOf(c_camera.BusConfigUnion),
        52 => @sizeOf(c_camera.BusConfig),
        53 => @sizeOf(c_camera.HwConfig),
        54 => @sizeOf(c_camera.Config),
        55 => @sizeOf(c_camera.FrameBuffer),
        56 => @sizeOf(c_camera.Crop),
        57 => @sizeOf(c_camera.Capabilities),
        58 => @sizeOf(c_camera.CameraApi),
        59 => @sizeOf(c_app_player.Config),
        60 => @sizeOf(c_app_player.PlayOpt),
        61 => @sizeOf(c_http.Parameters),
        62 => @sizeOf(c_http.ClientInfo),
        63 => @sizeOf(c_http.RedirectParam),
        else => 0,
    };
}

export fn zig_check_size(name: [*:0]const u8, c_sz: u32, zig_sz: u32) callconv(.C) i32 {
    if (c_sz == zig_sz) {
        arcs.log.info("[PASS] {s}: c={d} zig={d}", .{ name, c_sz, zig_sz });
        return 0;
    } else {
        arcs.log.err("[FAIL] {s}: c={d} zig={d}", .{ name, c_sz, zig_sz });
        return 1;
    }
}

export fn zig_binding_test_start() callconv(.C) i32 {
    arcs.log.init() catch return -1;
    arcs.log.info("=== Zig Binding Struct Verification ===", .{});
    return 0;
}

export fn zig_binding_test_end(fails: i32) callconv(.C) i32 {
    if (fails == 0) {
        arcs.log.info("=== ALL 65 BINDING TESTS PASSED ===", .{});
    } else {
        arcs.log.err("=== {d}/65 BINDING TESTS FAILED ===", .{fails});
    }
    return fails;
}
