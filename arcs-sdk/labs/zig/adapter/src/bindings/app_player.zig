//! ARCS SDK app_player component FFI bindings.
//! Mirrors components/app_player/app_player.h.

const c = @cImport({
    @cInclude("stdint.h");
    @cInclude("stdbool.h");
    @cInclude("app_player.h");
});

pub const c_api = c;

pub const Player = c.app_player_t;
pub const Error = c.app_player_err_t;
pub const PaCtrlCallback = c.app_player_pa_ctrl_cb_t;
pub const PcmOutputCallback = c.app_player_pcm_output_cb_t;
pub const Config = c.app_player_config_t;
pub const State = c.app_player_state_t;
pub const Event = c.app_player_event_t;
pub const EventCallback = c.app_player_event_cb_t;
pub const PlayOpt = c.app_player_play_opt_t;

pub const OK = c.APP_PLAYER_OK;
pub const ERR_INVALID_PARAM = c.APP_PLAYER_ERR_INVALID_PARAM;
pub const ERR_NO_MEMORY = c.APP_PLAYER_ERR_NO_MEMORY;
pub const ERR_INVALID_STATE = c.APP_PLAYER_ERR_INVALID_STATE;
pub const ERR_NOT_SUPPORTED = c.APP_PLAYER_ERR_NOT_SUPPORTED;
pub const ERR_TIMEOUT = c.APP_PLAYER_ERR_TIMEOUT;
pub const ERR_IO = c.APP_PLAYER_ERR_IO;

pub const STATE_IDLE = c.APP_PLAYER_STATE_IDLE;
pub const STATE_PREPARING = c.APP_PLAYER_STATE_PREPARING;
pub const STATE_PREPARED = c.APP_PLAYER_STATE_PREPARED;
pub const STATE_PLAYING = c.APP_PLAYER_STATE_PLAYING;
pub const STATE_PAUSED = c.APP_PLAYER_STATE_PAUSED;
pub const STATE_STOPPED = c.APP_PLAYER_STATE_STOPPED;
pub const STATE_ERROR = c.APP_PLAYER_STATE_ERROR;

pub const EVENT_ERROR = c.APP_PLAYER_EVENT_ERROR;
pub const EVENT_PREPARED = c.APP_PLAYER_EVENT_PREPARED;
pub const EVENT_PLAYING = c.APP_PLAYER_EVENT_PLAYING;
pub const EVENT_PAUSED = c.APP_PLAYER_EVENT_PAUSED;
pub const EVENT_STOPPED = c.APP_PLAYER_EVENT_STOPPED;
pub const EVENT_COMPLETED = c.APP_PLAYER_EVENT_COMPLETED;
pub const EVENT_SEEK_COMPLETE = c.APP_PLAYER_EVENT_SEEK_COMPLETE;

pub const init = c.app_player_init;
pub const create = c.app_player_create;
pub const destroy = c.app_player_destroy;
pub const registerCallback = c.app_player_register_callback;
pub const play = c.app_player_play;
pub const playEx = c.app_player_play_ex;
pub const stop = c.app_player_stop;
pub const pause = c.app_player_pause;
pub const resumePlayback = c.app_player_resume;
pub const reset = c.app_player_reset;
pub const seek = c.app_player_seek;
pub const getState = c.app_player_get_state;
pub const getPosition = c.app_player_get_position;
pub const getDuration = c.app_player_get_duration;
pub const setVolume = c.app_player_set_volume;
pub const playStream = c.app_player_play_stream;
pub const writeStream = c.app_player_write_stream;
pub const finishStream = c.app_player_finish_stream;

pub inline fn playUrl(player: ?*Player, url: [:0]const u8) c_int {
    return play(player, url.ptr);
}
