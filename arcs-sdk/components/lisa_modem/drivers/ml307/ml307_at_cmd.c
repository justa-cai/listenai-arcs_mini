/**
 * @file ml307_at_cmd.c
 * @brief ML307 AT transaction helpers
 */

#include "drivers/ml307/ml307_at_cmd.h"
#include "drivers/ml307/ml307_at_tls.h"
#include "drivers/ml307/ml307_endpoint_internal.h"
#include "at_mem.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#define TAG "ml307_at"
#include "lisa_log.h"
#include "lisa_modem_perf_log.h"

#ifndef taskYIELD
#define taskYIELD() vTaskDelay(0)
#endif

#define ML307_PULL_TIMEOUT_FALLBACK_MS 100U
#define ML307_PULL_TIMEOUT_MIN_MS      50U
#define ML307_CONNECT_TIMEOUT_MIN_MS   1500U
#define ML307_CONNECT_URC_GRACE_MS     500U
#define ML307_CONNECT_CTRL_TIMEOUT_MS  1500U
#define ML307_MIPOPEN_TIMEOUT_MIN_SEC  2U
#define ML307_MIPOPEN_TIMEOUT_MAX_SEC  60U

static void ml307_at_cmd_push_rx(ml307_endpoint_t *endpoint, const char *data, size_t len);

static uint32_t ml307_at_cmd_tick_elapsed_ms(TickType_t start, TickType_t end)
{
    return (uint32_t)((end - start) * portTICK_PERIOD_MS);
}

static TickType_t ml307_at_cmd_ms_to_ticks_nonzero(uint16_t delay_ms)
{
    TickType_t ticks;

    if (delay_ms == 0U) {
        return 0;
    }

    ticks = pdMS_TO_TICKS(delay_ms);
    return ticks > 0 ? ticks : 1;
}

static size_t ml307_at_cmd_tcp_send_chunk_size(const ml307_endpoint_t *endpoint)
{
    uint16_t chunk_size;

    if (!endpoint || !endpoint->ctx) {
        return ML307_TCP_SEND_CHUNK_SIZE_DEFAULT;
    }

    chunk_size = endpoint->ctx->runtime_config.tcp_send_chunk_size;
    return chunk_size > 0U ? (size_t)chunk_size : ML307_TCP_SEND_CHUNK_SIZE_DEFAULT;
}

static size_t ml307_at_cmd_tcp_pull_chunk_size(const ml307_endpoint_t *endpoint)
{
    uint16_t chunk_size;

    if (!endpoint || !endpoint->ctx) {
        return ML307_TCP_PULL_CHUNK_SIZE_DEFAULT;
    }

    chunk_size = endpoint->ctx->runtime_config.tcp_pull_chunk_size;
    return chunk_size > 0U ? (size_t)chunk_size : ML307_TCP_PULL_CHUNK_SIZE_DEFAULT;
}

static uint16_t ml307_at_cmd_send_chunk_delay_ms(const ml307_endpoint_t *endpoint)
{
    if (!endpoint || !endpoint->ctx) {
        return ML307_SEND_CHUNK_DELAY_MS_DEFAULT;
    }

    return endpoint->ctx->runtime_config.send_chunk_delay_ms;
}

static uint32_t ml307_at_cmd_connect_timeout_ms(const ml307_endpoint_t *endpoint)
{
    uint32_t timeout_ms = (endpoint && endpoint->send_timeout_ms > 0U)
                        ? endpoint->send_timeout_ms
                        : ML307_SEND_TIMEOUT_MS;

    if (timeout_ms < ML307_CONNECT_TIMEOUT_MIN_MS) {
        timeout_ms = ML307_CONNECT_TIMEOUT_MIN_MS;
    }
    if (timeout_ms > ML307_CONNECT_TIMEOUT_MS) {
        timeout_ms = ML307_CONNECT_TIMEOUT_MS;
    }

    return timeout_ms;
}

static uint32_t ml307_at_cmd_control_timeout_ms(uint32_t connect_timeout_ms)
{
    return connect_timeout_ms < ML307_CONNECT_CTRL_TIMEOUT_MS
         ? connect_timeout_ms
         : ML307_CONNECT_CTRL_TIMEOUT_MS;
}

static uint32_t ml307_at_cmd_mipopen_wait_timeout_ms(uint32_t connect_timeout_ms)
{
    uint32_t timeout_ms = connect_timeout_ms + ML307_CONNECT_URC_GRACE_MS;

    return timeout_ms > ML307_CONNECT_TIMEOUT_MS ? ML307_CONNECT_TIMEOUT_MS : timeout_ms;
}

static uint32_t ml307_at_cmd_mipopen_timeout_s(uint32_t connect_timeout_ms)
{
    uint32_t timeout_s = (connect_timeout_ms + 999U) / 1000U;

    if (timeout_s < ML307_MIPOPEN_TIMEOUT_MIN_SEC) {
        timeout_s = ML307_MIPOPEN_TIMEOUT_MIN_SEC;
    }
    if (timeout_s > ML307_MIPOPEN_TIMEOUT_MAX_SEC) {
        timeout_s = ML307_MIPOPEN_TIMEOUT_MAX_SEC;
    }

    return timeout_s;
}

static uint32_t ml307_at_cmd_update_prefetch_rate_1s(ml307_endpoint_t *endpoint,
                                                     TickType_t now,
                                                     uint32_t pushed_bytes)
{
    uint32_t elapsed_ms;

    if (!endpoint) {
        return 0U;
    }

    if (endpoint->perf_rx_rate_window_start_tick == 0) {
        endpoint->perf_rx_rate_window_start_tick = now;
    }

    endpoint->perf_rx_rate_window_bytes += pushed_bytes;
    elapsed_ms = ml307_at_cmd_tick_elapsed_ms(endpoint->perf_rx_rate_window_start_tick, now);
    if (elapsed_ms >= 1000U) {
        endpoint->perf_rx_rate_last_1s_bps = (uint32_t)(((uint64_t)endpoint->perf_rx_rate_window_bytes * 1000ULL) /
                                                        (uint64_t)elapsed_ms);
        endpoint->perf_rx_rate_window_start_tick = now;
        endpoint->perf_rx_rate_window_bytes = 0U;
    }

    return endpoint->perf_rx_rate_last_1s_bps;
}

static void ml307_at_cmd_reset_prefetch_perf(ml307_endpoint_t *endpoint)
{
    if (!endpoint) {
        return;
    }

    endpoint->perf_prefetch_first_payload_tick = 0;
    endpoint->perf_prefetch_first_push_tick = 0;
    endpoint->perf_prefetch_last_push_tick = 0;
    endpoint->perf_prefetch_line_done_tick = 0;
}

static void ml307_at_cmd_note_prefetch_first_payload(ml307_endpoint_t *endpoint)
{
    if (!endpoint || !endpoint->prefetching || endpoint->perf_prefetch_first_payload_tick != 0) {
        return;
    }

    endpoint->perf_prefetch_first_payload_tick = xTaskGetTickCount();
}

static void ml307_at_cmd_note_prefetch_push(ml307_endpoint_t *endpoint)
{
    TickType_t now;

    if (!endpoint || !endpoint->prefetching) {
        return;
    }

    now = xTaskGetTickCount();
    if (endpoint->perf_prefetch_first_push_tick == 0) {
        endpoint->perf_prefetch_first_push_tick = now;
    }
    endpoint->perf_prefetch_last_push_tick = now;
}

static void ml307_at_cmd_note_prefetch_line_done(ml307_endpoint_t *endpoint)
{
    if (!endpoint || !endpoint->prefetching || endpoint->perf_prefetch_line_done_tick != 0) {
        return;
    }

    endpoint->perf_prefetch_line_done_tick = xTaskGetTickCount();
}

static void ml307_at_cmd_schedule_prefetch(ml307_endpoint_t *endpoint)
{
    if (!endpoint || !endpoint->ctx || !endpoint->ctx->dispatcher ||
        endpoint->public_sockfd < 0 || endpoint->generation == 0U) {
        return;
    }
    if (!modem_endpoint_runtime_should_pull(&endpoint->runtime)) {
        return;
    }

    (void)modem_dispatcher_mark_rx_ready(endpoint->ctx->dispatcher,
                                         endpoint->public_sockfd,
                                         endpoint->generation);
}

static void ml307_at_cmd_refresh_pending(ml307_endpoint_t *endpoint)
{
    bool pending = false;

    if (!endpoint) {
        return;
    }

    if (endpoint->protocol == IPPROTO_TCP) {
        pending = endpoint->connected && endpoint->rx_hint.available_data_len > 0U;
    } else if (endpoint->protocol == IPPROTO_UDP) {
        pending = endpoint->instance_active && endpoint->rx_hint.unread_packet_count > 0U;
    }

    modem_endpoint_runtime_mark_pending(&endpoint->runtime, pending);
    ml307_at_cmd_schedule_prefetch(endpoint);
}

static void ml307_line_stream_reset(ml307_line_stream_state_t *state)
{
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->field_index = (size_t)-1;
}

static void ml307_line_stream_prepare(ml307_endpoint_ctx_t *ctx,
                                      ml307_line_stream_kind_t kind,
                                      ml307_endpoint_t *endpoint)
{
    if (!ctx) {
        return;
    }

    ml307_line_stream_reset(&ctx->line_stream);
    ctx->line_stream.kind = kind;
    ctx->line_stream.endpoint = endpoint;
}

static ml307_endpoint_t *ml307_line_stream_get_tcp_endpoint(ml307_endpoint_ctx_t *ctx, int endpoint_id)
{
    ml307_endpoint_t *endpoint;

    if (!ctx || endpoint_id < 1 || endpoint_id > ML307_MAX_ENDPOINTS) {
        return NULL;
    }

    endpoint = &ctx->endpoints[endpoint_id - 1];
    if (!endpoint->in_use || !endpoint->initialized || endpoint->protocol != IPPROTO_TCP) {
        return NULL;
    }

    return endpoint;
}

static bool ml307_line_stream_parse_int(const char *token, size_t len, int *out)
{
    int value = 0;

    if (!token || len == 0U || !out) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        if (!isdigit((unsigned char)token[i])) {
            return false;
        }
        value = value * 10 + (token[i] - '0');
    }

    *out = value;
    return true;
}

static int ml307_line_stream_hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

static int ml307_line_stream_flush_payload(ml307_line_stream_state_t *state)
{
    if (!state || !state->endpoint || state->decode_buf_len == 0U) {
        return 0;
    }

    if (state->kind == ML307_LINE_STREAM_MIPRD) {
        ml307_at_cmd_note_prefetch_push(state->endpoint);
    }
    ml307_at_cmd_push_rx(state->endpoint, (const char *)state->decode_buf, state->decode_buf_len);
    state->decode_buf_len = 0U;
    return 0;
}

static int ml307_line_stream_feed_hex_char(ml307_line_stream_state_t *state, char ch)
{
    int value;

    if (!state) {
        return -1;
    }

    value = ml307_line_stream_hex_value(ch);
    if (value < 0) {
        return -1;
    }

    if (!state->has_pending_hex_nibble) {
        state->pending_hex_nibble = (uint8_t)value;
        state->has_pending_hex_nibble = true;
        return 0;
    }

    state->decode_buf[state->decode_buf_len++] =
        (uint8_t)((state->pending_hex_nibble << 4) | value);
    state->has_pending_hex_nibble = false;

    if (state->kind == ML307_LINE_STREAM_MIPRD) {
        ml307_at_cmd_note_prefetch_first_payload(state->endpoint);
    }

    if (state->decode_buf_len >= sizeof(state->decode_buf)) {
        return ml307_line_stream_flush_payload(state);
    }

    return 0;
}

static int ml307_line_stream_decode_hex_span(ml307_line_stream_state_t *state,
                                             const uint8_t *data,
                                             size_t len)
{
    size_t pos = 0U;

    if (!state || (!data && len > 0U)) {
        return -1;
    }

    if (len == 0U) {
        return 0;
    }

    if (state->has_pending_hex_nibble) {
        int lo = ml307_line_stream_hex_value((char)data[pos]);

        if (lo < 0) {
            return -1;
        }

        state->decode_buf[state->decode_buf_len++] =
            (uint8_t)((state->pending_hex_nibble << 4) | (uint8_t)lo);
        state->has_pending_hex_nibble = false;
        pos++;

        if (state->kind == ML307_LINE_STREAM_MIPRD) {
            ml307_at_cmd_note_prefetch_first_payload(state->endpoint);
        }
        if (state->decode_buf_len >= sizeof(state->decode_buf)) {
            if (ml307_line_stream_flush_payload(state) < 0) {
                return -1;
            }
        }
    }

    while (pos + 1U < len) {
        size_t space = sizeof(state->decode_buf) - state->decode_buf_len;
        size_t pairs;

        if (space == 0U) {
            if (ml307_line_stream_flush_payload(state) < 0) {
                return -1;
            }
            space = sizeof(state->decode_buf);
        }

        pairs = (len - pos) / 2U;
        if (pairs > space) {
            pairs = space;
        }

        for (size_t i = 0; i < pairs; ++i) {
            int hi = ml307_line_stream_hex_value((char)data[pos]);
            int lo = ml307_line_stream_hex_value((char)data[pos + 1U]);

            if (hi < 0 || lo < 0) {
                return -1;
            }

            state->decode_buf[state->decode_buf_len++] =
                (uint8_t)(((uint8_t)hi << 4) | (uint8_t)lo);
            pos += 2U;
        }

        if (state->kind == ML307_LINE_STREAM_MIPRD) {
            ml307_at_cmd_note_prefetch_first_payload(state->endpoint);
        }
        if (state->decode_buf_len >= sizeof(state->decode_buf)) {
            if (ml307_line_stream_flush_payload(state) < 0) {
                return -1;
            }
        }
    }

    if (pos < len) {
        int hi = ml307_line_stream_hex_value((char)data[pos]);

        if (hi < 0) {
            return -1;
        }

        state->pending_hex_nibble = (uint8_t)hi;
        state->has_pending_hex_nibble = true;
    }

    return 0;
}

static int ml307_line_stream_commit_mipurc_final_field(ml307_line_stream_state_t *state)
{
    int hint = 0;
    bool treat_as_payload;

    if (!state || !state->endpoint) {
        return -1;
    }

    treat_as_payload = state->final_field_payload;
    if (!treat_as_payload && state->token_len > 0U && state->final_field_digits_only &&
        state->rtcp_declared_len > 0 &&
        state->token_len == ((size_t)state->rtcp_declared_len * 2U)) {
        treat_as_payload = true;
    }

    if (treat_as_payload) {
        for (size_t i = 0; i < state->token_len; ++i) {
            if (ml307_line_stream_feed_hex_char(state, state->token[i]) < 0) {
                return -1;
            }
        }
        state->token_len = 0U;
        return ml307_line_stream_flush_payload(state);
    }

    if (state->token_len == 0U) {
        state->endpoint->rx_hint.available_data_len = 0U;
        ml307_at_cmd_refresh_pending(state->endpoint);
        return 0;
    }

    if (!state->final_field_digits_only ||
        !ml307_line_stream_parse_int(state->token, state->token_len, &hint)) {
        return -1;
    }

    state->endpoint->rx_hint.available_data_len = (size_t)hint;
    ml307_at_cmd_refresh_pending(state->endpoint);
    return 0;
}

static int ml307_line_stream_finalize_field(ml307_line_stream_state_t *state)
{
    int value = 0;

    if (!state) {
        return -1;
    }

    switch (state->kind) {
    case ML307_LINE_STREAM_MIPRD:
        if (state->field_index == 0U) {
            if (!ml307_line_stream_parse_int(state->token, state->token_len, &value) ||
                !state->endpoint || value != state->endpoint->id) {
                return -1;
            }
        }
        break;
    case ML307_LINE_STREAM_MIPURC_RTCP:
        if (state->field_index == 0U) {
            if (state->token_len != 4U || memcmp(state->token, "rtcp", 4U) != 0) {
                return -1;
            }
        } else if (state->field_index == 1U) {
            if (!ml307_line_stream_parse_int(state->token, state->token_len, &value) ||
                !state->endpoint || value != state->endpoint->id) {
                return -1;
            }
        } else if (state->field_index == 2U) {
            if (!ml307_line_stream_parse_int(state->token, state->token_len, &value)) {
                return -1;
            }
            state->rtcp_declared_len = value;
        }
        break;
    default:
        return -1;
    }

    state->token_len = 0U;
    return 0;
}

static int ml307_line_stream_consume_char(ml307_line_stream_state_t *state, char ch)
{
    if (!state || state->kind == ML307_LINE_STREAM_NONE) {
        return -1;
    }

    if (state->field_index == (size_t)-1) {
        if (ch == ':') {
            state->field_index = 0U;
        }
        return 0;
    }

    if (!state->parsing_payload) {
        if (state->token_len == 0U && (ch == ' ' || ch == '\t')) {
            return 0;
        }

        if (ch == ',') {
            if (ml307_line_stream_finalize_field(state) < 0) {
                return -1;
            }

            if (state->kind == ML307_LINE_STREAM_MIPRD && state->field_index == 2U) {
                state->parsing_payload = true;
            } else if (state->kind == ML307_LINE_STREAM_MIPURC_RTCP && state->field_index == 2U) {
                state->parsing_payload = true;
                state->final_field_digits_only = true;
                state->final_field_len = 0U;
            }

            state->field_index++;
            return 0;
        }

        if (state->token_len + 1U >= sizeof(state->token)) {
            return -1;
        }

        if (state->kind == ML307_LINE_STREAM_MIPURC_RTCP &&
            state->field_index == 0U &&
            ch == '"' &&
            state->token_len == 0U) {
            return 0;
        }

        if (state->kind == ML307_LINE_STREAM_MIPURC_RTCP &&
            state->field_index == 0U &&
            ch == '"' &&
            state->token_len > 0U) {
            return 0;
        }

        state->token[state->token_len++] = ch;
        return 0;
    }

    if (state->kind == ML307_LINE_STREAM_MIPRD) {
        return ml307_line_stream_feed_hex_char(state, ch);
    }

    if (!state->final_field_payload) {
        if (state->token_len + 1U >= sizeof(state->token)) {
            state->final_field_payload = true;
            for (size_t i = 0; i < state->token_len; ++i) {
                if (ml307_line_stream_feed_hex_char(state, state->token[i]) < 0) {
                    return -1;
                }
            }
            state->token_len = 0U;
        } else {
            state->token[state->token_len++] = ch;
            state->final_field_len++;
            if (!isdigit((unsigned char)ch)) {
                state->final_field_digits_only = false;
            }

            if (!state->final_field_digits_only || state->final_field_len > 10U) {
                state->final_field_payload = true;
                for (size_t i = 0; i < state->token_len; ++i) {
                    if (ml307_line_stream_feed_hex_char(state, state->token[i]) < 0) {
                        return -1;
                    }
                }
                state->token_len = 0U;
            }
            return 0;
        }
    }

    return ml307_line_stream_feed_hex_char(state, ch);
}

static int ml307_line_stream_complete(ml307_line_stream_state_t *state)
{
    if (!state) {
        return -1;
    }

    if (state->parsing_payload) {
        if (state->kind == ML307_LINE_STREAM_MIPURC_RTCP) {
            if (ml307_line_stream_commit_mipurc_final_field(state) < 0) {
                return -1;
            }
        } else if (ml307_line_stream_flush_payload(state) < 0) {
            return -1;
        }

        if (state->has_pending_hex_nibble) {
            return -1;
        }
        if (state->kind == ML307_LINE_STREAM_MIPRD) {
            ml307_at_cmd_note_prefetch_line_done(state->endpoint);
        }
        return 0;
    }

    if (state->token_len > 0U && ml307_line_stream_finalize_field(state) < 0) {
        return -1;
    }

    return 0;
}

static bool ml307_line_stream_prefix_equals(const uint8_t *line, size_t len, const char *prefix)
{
    size_t prefix_len = strlen(prefix);

    if (!line || !prefix || len > prefix_len) {
        return false;
    }

    return memcmp(line, prefix, len) == 0;
}

static at_line_stream_claim_t ml307_line_stream_try_claim_miprd(ml307_endpoint_ctx_t *ctx,
                                                                const uint8_t *line, size_t len)
{
    static const char prefix[] = "+MIPRD:";
    const uint8_t *p;
    const uint8_t *end;
    int endpoint_id = 0;
    int comma_count = 0;

    if (len < sizeof(prefix) - 1U) {
        return ml307_line_stream_prefix_equals(line, len, prefix)
             ? AT_LINE_STREAM_NEED_MORE
             : AT_LINE_STREAM_PASS;
    }
    if (memcmp(line, prefix, sizeof(prefix) - 1U) != 0) {
        return AT_LINE_STREAM_PASS;
    }

    p = line + sizeof(prefix) - 1U;
    end = line + len;
    while (p < end && (*p == ' ' || *p == '\t')) {
        ++p;
    }
    while (p < end && isdigit((unsigned char)*p)) {
        endpoint_id = endpoint_id * 10 + (*p - '0');
        ++p;
    }
    if (p == end) {
        return AT_LINE_STREAM_NEED_MORE;
    }
    if (*p != ',') {
        return AT_LINE_STREAM_PASS;
    }
    if (!ml307_line_stream_get_tcp_endpoint(ctx, endpoint_id)) {
        return AT_LINE_STREAM_PASS;
    }

    for (; p < end; ++p) {
        if (*p == ',') {
            ++comma_count;
        }
    }

    return (comma_count >= 3) ? AT_LINE_STREAM_CLAIM : AT_LINE_STREAM_NEED_MORE;
}

static at_line_stream_claim_t ml307_line_stream_try_claim_mipurc(ml307_endpoint_ctx_t *ctx,
                                                                 const uint8_t *line, size_t len)
{
    static const char prefix[] = "+MIPURC:";
    const uint8_t *p;
    const uint8_t *end;
    const uint8_t *token_start;
    size_t token_len;
    int endpoint_id = 0;
    int comma_count = 0;

    if (len < sizeof(prefix) - 1U) {
        return ml307_line_stream_prefix_equals(line, len, prefix)
             ? AT_LINE_STREAM_NEED_MORE
             : AT_LINE_STREAM_PASS;
    }
    if (memcmp(line, prefix, sizeof(prefix) - 1U) != 0) {
        return AT_LINE_STREAM_PASS;
    }

    p = line + sizeof(prefix) - 1U;
    end = line + len;
    while (p < end && (*p == ' ' || *p == '\t')) {
        ++p;
    }
    token_start = p;
    while (p < end && *p != ',') {
        ++p;
    }
    if (p == end) {
        return AT_LINE_STREAM_NEED_MORE;
    }

    token_len = (size_t)(p - token_start);
    if (!((token_len == 6U && memcmp(token_start, "\"rtcp\"", 6U) == 0) ||
          (token_len == 4U && memcmp(token_start, "rtcp", 4U) == 0))) {
        return AT_LINE_STREAM_PASS;
    }

    ++p;
    while (p < end && (*p == ' ' || *p == '\t')) {
        ++p;
    }
    while (p < end && isdigit((unsigned char)*p)) {
        endpoint_id = endpoint_id * 10 + (*p - '0');
        ++p;
    }
    if (p == end) {
        return AT_LINE_STREAM_NEED_MORE;
    }
    if (*p != ',') {
        return AT_LINE_STREAM_PASS;
    }
    if (!ml307_line_stream_get_tcp_endpoint(ctx, endpoint_id)) {
        return AT_LINE_STREAM_PASS;
    }

    for (; p < end; ++p) {
        if (*p == ',') {
            ++comma_count;
        }
    }

    return (comma_count >= 2) ? AT_LINE_STREAM_CLAIM : AT_LINE_STREAM_NEED_MORE;
}

static at_line_stream_claim_t ml307_line_stream_claim(const uint8_t *line_prefix,
                                                      size_t prefix_len,
                                                      bool line_complete,
                                                      void *user_data,
                                                      void **claim_ctx)
{
    ml307_endpoint_ctx_t *ctx = (ml307_endpoint_ctx_t *)user_data;
    at_line_stream_claim_t decision;
    ml307_endpoint_t *endpoint = NULL;
    int endpoint_id = 0;
    const uint8_t *p;
    const uint8_t *end;

    (void)line_complete;

    if (!ctx || !line_prefix || prefix_len == 0U || !claim_ctx) {
        return AT_LINE_STREAM_PASS;
    }

    decision = ml307_line_stream_try_claim_miprd(ctx, line_prefix, prefix_len);
    if (decision == AT_LINE_STREAM_CLAIM) {
        p = line_prefix + sizeof("+MIPRD:") - 1U;
        end = line_prefix + prefix_len;
        while (p < end && (*p == ' ' || *p == '\t')) {
            ++p;
        }
        while (p < end && isdigit((unsigned char)*p)) {
            endpoint_id = endpoint_id * 10 + (*p - '0');
            ++p;
        }
        endpoint = ml307_line_stream_get_tcp_endpoint(ctx, endpoint_id);
        if (!endpoint) {
            return AT_LINE_STREAM_PASS;
        }
        ml307_line_stream_prepare(ctx, ML307_LINE_STREAM_MIPRD, endpoint);
        *claim_ctx = &ctx->line_stream;
        return AT_LINE_STREAM_CLAIM;
    }
    if (decision != AT_LINE_STREAM_PASS) {
        return decision;
    }

    decision = ml307_line_stream_try_claim_mipurc(ctx, line_prefix, prefix_len);
    if (decision == AT_LINE_STREAM_CLAIM) {
        p = line_prefix + sizeof("+MIPURC:") - 1U;
        end = line_prefix + prefix_len;
        while (p < end && *p != ',') {
            ++p;
        }
        if (p == end) {
            return AT_LINE_STREAM_NEED_MORE;
        }
        ++p;
        while (p < end && (*p == ' ' || *p == '\t')) {
            ++p;
        }
        while (p < end && isdigit((unsigned char)*p)) {
            endpoint_id = endpoint_id * 10 + (*p - '0');
            ++p;
        }
        endpoint = ml307_line_stream_get_tcp_endpoint(ctx, endpoint_id);
        if (!endpoint) {
            return AT_LINE_STREAM_PASS;
        }
        ml307_line_stream_prepare(ctx, ML307_LINE_STREAM_MIPURC_RTCP, endpoint);
        *claim_ctx = &ctx->line_stream;
        return AT_LINE_STREAM_CLAIM;
    }

    return decision;
}

static int ml307_line_stream_consume(void *claim_ctx, const uint8_t *data, size_t len,
                                     bool line_complete, void *user_data)
{
    ml307_line_stream_state_t *state = (ml307_line_stream_state_t *)claim_ctx;
    size_t offset = 0U;

    (void)user_data;

    if (!state || (!data && len > 0U)) {
        return -1;
    }

    while (offset < len) {
        if (state->kind == ML307_LINE_STREAM_MIPRD && state->parsing_payload) {
            if (ml307_line_stream_decode_hex_span(state, data + offset, len - offset) < 0) {
                return -1;
            }
            offset = len;
            break;
        }

        if (ml307_line_stream_consume_char(state, (char)data[offset]) < 0) {
            return -1;
        }
        offset++;
    }

    if (line_complete) {
        return ml307_line_stream_complete(state);
    }

    return 0;
}

static void ml307_line_stream_finish(void *claim_ctx, bool success, void *user_data)
{
    ml307_line_stream_state_t *state = (ml307_line_stream_state_t *)claim_ctx;

    (void)user_data;
    if (!success && state && state->endpoint && state->decode_buf_len > 0U) {
        state->decode_buf_len = 0U;
    }
    ml307_line_stream_reset(state);
}

static const at_line_stream_handler_t s_ml307_line_stream_handler = {
    .claim = ml307_line_stream_claim,
    .consume = ml307_line_stream_consume,
    .finish = ml307_line_stream_finish,
};

static void ml307_udp_peer_clear(ml307_udp_peer_t *peer)
{
    if (peer) {
        memset(peer, 0, sizeof(*peer));
    }
}

static void ml307_udp_peer_set(ml307_udp_peer_t *peer, const char *host, uint16_t port)
{
    if (!peer) {
        return;
    }

    ml307_udp_peer_clear(peer);
    if (!host) {
        return;
    }

    peer->valid = true;
    peer->addr.family = AF_INET;
    strncpy(peer->addr.host, host, sizeof(peer->addr.host) - 1);
    peer->addr.host[sizeof(peer->addr.host) - 1] = '\0';
    peer->addr.port = port;
}

static size_t ml307_udp_data_index(at_arg_value_t *arguments, size_t arg_count, size_t start_index,
                                   ml307_udp_peer_t *source)
{
    size_t index = start_index;

    ml307_udp_peer_clear(source);

    if (index + 1 < arg_count &&
        arguments[index].type == AT_ARG_TYPE_STRING &&
        arguments[index + 1].type == AT_ARG_TYPE_INT) {
        ml307_udp_peer_set(source, arguments[index].data.string_val.value,
                           (uint16_t)arguments[index + 1].data.int_val);
        index += 2;
    }

    if (index + 1 < arg_count &&
        arguments[index].type == AT_ARG_TYPE_INT &&
        arguments[index + 1].type == AT_ARG_TYPE_STRING) {
        index += 1;
    }

    return index;
}

static void ml307_at_cmd_push_rx(ml307_endpoint_t *endpoint, const char *data, size_t len)
{
    TickType_t now;
    uint32_t written;
    uint32_t ring_before;
    uint32_t ring_after;

    if (!endpoint || !data || len == 0) {
        return;
    }

    now = xTaskGetTickCount();
    endpoint->perf_last_rx_push_tick = now;
    endpoint->perf_last_push_bytes = (uint32_t)len;
    endpoint->perf_rx_push_seq++;
    endpoint->perf_rx_push_total_bytes += (uint32_t)len;

    ring_before = ring_buf_size_get(&endpoint->ring_buf);
    written = modem_endpoint_runtime_write_rx(&endpoint->runtime, (const uint8_t *)data, (uint32_t)len);
    ring_after = ring_buf_size_get(&endpoint->ring_buf);
    if (endpoint->protocol == IPPROTO_TCP) {
        size_t consumed = (size_t)written;

        if (endpoint->rx_hint.available_data_len >= consumed) {
            endpoint->rx_hint.available_data_len -= consumed;
        } else {
            endpoint->rx_hint.available_data_len = 0;
        }
    }
    ml307_at_cmd_refresh_pending(endpoint);
    /* Wake recv waiters once per empty->non-empty transition to avoid mid-burst partial reads. */
    if (written > 0U && ring_before == 0U && ring_after > 0U && endpoint->data_sem) {
        xSemaphoreGive(endpoint->data_sem);
    }
    if (written > 0U && endpoint->recv_event) {
        xEventGroupSetBits(endpoint->recv_event, ML307_ENDPOINT_PREFETCH_AVAILABLE);
    }
}

static bool ml307_at_cmd_match_endpoint_urc(const char *command,
                                            at_arg_value_t *arguments,
                                            size_t arg_count,
                                            void *user_data)
{
    ml307_endpoint_t *endpoint = (ml307_endpoint_t *)user_data;

    (void)command;

    return endpoint && arguments && arg_count >= 1 &&
           arguments[0].type == AT_ARG_TYPE_INT &&
           arguments[0].data.int_val == endpoint->id;
}

static bool ml307_at_cmd_parse_mipopen_result(ml307_endpoint_t *endpoint,
                                              at_arg_value_t *arguments,
                                              size_t arg_count)
{
    int result;

    if (!endpoint || !arguments || arg_count < 2 ||
        arguments[0].type != AT_ARG_TYPE_INT ||
        arguments[0].data.int_val != endpoint->id ||
        arguments[1].type != AT_ARG_TYPE_INT) {
        return false;
    }

    result = arguments[1].data.int_val;
    endpoint->connected = (result == 0);
    if (endpoint->connected) {
        endpoint->instance_active = true;
        xEventGroupClearBits(endpoint->event_group, ML307_ENDPOINT_DISCONNECTED | ML307_ENDPOINT_ERROR);
        xEventGroupSetBits(endpoint->event_group, ML307_ENDPOINT_CONNECTED);
    } else {
        endpoint->last_error = result;
        xEventGroupSetBits(endpoint->event_group, ML307_ENDPOINT_ERROR);
    }

    return endpoint->connected;
}

static void ml307_at_cmd_handle_mipopen(ml307_endpoint_t *endpoint, at_arg_value_t *arguments, size_t arg_count)
{
    (void)ml307_at_cmd_parse_mipopen_result(endpoint, arguments, arg_count);
}

static void ml307_at_cmd_handle_mipclose(ml307_endpoint_t *endpoint, at_arg_value_t *arguments, size_t arg_count)
{
    if (!endpoint || arg_count < 1 ||
        arguments[0].type != AT_ARG_TYPE_INT ||
        arguments[0].data.int_val != endpoint->id) {
        return;
    }

    endpoint->instance_active = false;
    endpoint->connected = false;
    endpoint->rx_hint.available_data_len = 0U;
    modem_endpoint_runtime_mark_pending(&endpoint->runtime, false);
    xEventGroupSetBits(endpoint->event_group, ML307_ENDPOINT_DISCONNECTED);
}

static void ml307_at_cmd_handle_mipsend(ml307_endpoint_t *endpoint, at_arg_value_t *arguments, size_t arg_count)
{
    if (!endpoint || arg_count < 2 ||
        arguments[0].type != AT_ARG_TYPE_INT ||
        arguments[0].data.int_val != endpoint->id) {
        return;
    }

    if (arguments[1].type == AT_ARG_TYPE_INT) {
        endpoint->last_sent_bytes = arguments[1].data.int_val;
    }
    xEventGroupSetBits(endpoint->event_group, ML307_ENDPOINT_SEND_COMPLETE);
}

static void ml307_at_cmd_handle_mipstate(ml307_endpoint_t *endpoint, at_arg_value_t *arguments, size_t arg_count)
{
    const char *state;

    if (!endpoint || arg_count < 5 ||
        arguments[0].type != AT_ARG_TYPE_INT ||
        arguments[0].data.int_val != endpoint->id ||
        arguments[4].type != AT_ARG_TYPE_STRING) {
        return;
    }

    state = arguments[4].data.string_val.value;
    if (state && strcmp(state, "CONNECTED") == 0) {
        endpoint->connected = true;
        endpoint->instance_active = true;
        xEventGroupSetBits(endpoint->event_group, ML307_ENDPOINT_CONNECTED);
    } else if (state && strcmp(state, "INITIAL") == 0) {
        endpoint->connected = false;
        endpoint->instance_active = true;
        xEventGroupSetBits(endpoint->event_group, ML307_ENDPOINT_INITIALIZED);
    } else {
        endpoint->connected = false;
        endpoint->instance_active = false;
    }

    ml307_at_cmd_refresh_pending(endpoint);
}

static void ml307_at_cmd_handle_miprd(ml307_endpoint_t *endpoint, at_arg_value_t *arguments, size_t arg_count)
{
    int hex_data_index = 2;
    ml307_udp_peer_t source = {0};

    if (!endpoint || arg_count < 2 ||
        arguments[0].type != AT_ARG_TYPE_INT ||
        arguments[0].data.int_val != endpoint->id) {
        return;
    }

    if (endpoint->protocol == IPPROTO_UDP && arguments[1].type == AT_ARG_TYPE_INT) {
        endpoint->rx_hint.unread_packet_count = (size_t)arguments[1].data.int_val;
        hex_data_index = (int)ml307_udp_data_index(arguments, arg_count, 2, &source);
    } else if (arg_count >= 4 && arguments[2].type == AT_ARG_TYPE_INT) {
        hex_data_index = 3;
    }
    if (arg_count <= (size_t)hex_data_index || arguments[hex_data_index].type != AT_ARG_TYPE_STRING) {
        return;
    }

    size_t decoded_len = 0;
    char *decoded = at_decode_hex(arguments[hex_data_index].data.string_val.value,
                                  arguments[hex_data_index].data.string_val.len,
                                  &decoded_len);
    if (decoded) {
        if (source.valid) {
            endpoint->udp_last_source = source;
        }
        ml307_at_cmd_push_rx(endpoint, decoded, decoded_len);
        at_mem_free(decoded);
    }
}

static void ml307_at_cmd_handle_mipurc_tcp(ml307_endpoint_t *endpoint, at_arg_value_t *arguments, size_t arg_count)
{
    const char *urc_type;

    if (!endpoint || arg_count < 3 ||
        arguments[0].type != AT_ARG_TYPE_STRING ||
        arguments[1].type != AT_ARG_TYPE_INT ||
        arguments[1].data.int_val != endpoint->id) {
        return;
    }

    urc_type = arguments[0].data.string_val.value;
    if (strcmp(urc_type, "rtcp") == 0 && endpoint->connected && arg_count >= 4) {
        if (arguments[3].type == AT_ARG_TYPE_STRING) {
            size_t decoded_len = 0;
            char *decoded = at_decode_hex(arguments[3].data.string_val.value,
                                          arguments[3].data.string_val.len,
                                          &decoded_len);
            if (decoded) {
                ml307_at_cmd_push_rx(endpoint, decoded, decoded_len);
                at_mem_free(decoded);
            }
        } else if (arguments[3].type == AT_ARG_TYPE_INT) {
            endpoint->rx_hint.available_data_len = (size_t)arguments[3].data.int_val;
            ml307_at_cmd_refresh_pending(endpoint);
        }
    } else if (strcmp(urc_type, "disconn") == 0) {
        endpoint->connected = false;
        endpoint->instance_active = false;
        endpoint->rx_hint.available_data_len = 0U;
        modem_endpoint_runtime_mark_pending(&endpoint->runtime, false);
        xEventGroupSetBits(endpoint->event_group, ML307_ENDPOINT_DISCONNECTED);
    }
}

static void ml307_at_cmd_handle_mipurc_udp(ml307_endpoint_t *endpoint, at_arg_value_t *arguments, size_t arg_count)
{
    const char *urc_type;
    size_t data_index = 2;
    ml307_udp_peer_t source = {0};

    if (!endpoint || arg_count < 3 ||
        arguments[0].type != AT_ARG_TYPE_STRING ||
        arguments[1].type != AT_ARG_TYPE_INT ||
        arguments[1].data.int_val != endpoint->id) {
        return;
    }

    urc_type = arguments[0].data.string_val.value;
    if (strcmp(urc_type, "rudp") == 0 && endpoint->connected) {
        if (arguments[2].type == AT_ARG_TYPE_INT) {
            endpoint->rx_hint.unread_packet_count = (size_t)arguments[2].data.int_val;
            data_index = 3;
        }

        data_index = ml307_udp_data_index(arguments, arg_count, data_index, &source);
        if (arg_count > data_index && arguments[data_index].type == AT_ARG_TYPE_STRING) {
            size_t decoded_len = 0;
            char *decoded = at_decode_hex(arguments[data_index].data.string_val.value,
                                         arguments[data_index].data.string_val.len,
                                         &decoded_len);
            if (decoded) {
                if (source.valid) {
                    endpoint->udp_last_source = source;
                }
                ml307_at_cmd_push_rx(endpoint, decoded, decoded_len);
                at_mem_free(decoded);
            }
        } else if (endpoint->rx_hint.unread_packet_count > 0) {
            ml307_at_cmd_refresh_pending(endpoint);
        }
    } else if (strcmp(urc_type, "disconn") == 0) {
        endpoint->connected = false;
        endpoint->instance_active = false;
        endpoint->rx_hint.unread_packet_count = 0U;
        modem_endpoint_runtime_mark_pending(&endpoint->runtime, false);
        xEventGroupSetBits(endpoint->event_group, ML307_ENDPOINT_DISCONNECTED);
    }
}

void ml307_at_cmd_handle_urc(ml307_endpoint_t *endpoint, const char *command,
                             at_arg_value_t *arguments, size_t arg_count)
{
    if (!endpoint || !command) {
        return;
    }

    if (strcmp(command, "MIPOPEN") == 0) {
        ml307_at_cmd_handle_mipopen(endpoint, arguments, arg_count);
        return;
    }
    if (strcmp(command, "MIPCLOSE") == 0) {
        ml307_at_cmd_handle_mipclose(endpoint, arguments, arg_count);
        return;
    }
    if (strcmp(command, "MIPSEND") == 0) {
        ml307_at_cmd_handle_mipsend(endpoint, arguments, arg_count);
        return;
    }
    if (strcmp(command, "MIPSTATE") == 0) {
        ml307_at_cmd_handle_mipstate(endpoint, arguments, arg_count);
        return;
    }
    if (strcmp(command, "MIPRD") == 0) {
        ml307_at_cmd_handle_miprd(endpoint, arguments, arg_count);
        return;
    }
    if (strcmp(command, "MIPURC") != 0) {
        return;
    }

    if (endpoint->protocol == IPPROTO_TCP) {
        ml307_at_cmd_handle_mipurc_tcp(endpoint, arguments, arg_count);
    } else if (endpoint->protocol == IPPROTO_UDP) {
        ml307_at_cmd_handle_mipurc_udp(endpoint, arguments, arg_count);
    }
}

const at_line_stream_handler_t *ml307_at_cmd_get_line_stream_handler(void)
{
    return &s_ml307_line_stream_handler;
}

bool ml307_at_cmd_connect(ml307_endpoint_t *endpoint, const char *host, int port)
{
    char command[128];
    EventBits_t bits;
    at_arg_value_t *mipopen_args = NULL;
    size_t mipopen_arg_count = 0U;
    bool connected;
    const char *socket_type;
    int pdp_cid;
    int open_mode;
    bool use_tls;
    uint32_t connect_timeout_ms;
    uint32_t control_timeout_ms;
    uint32_t mipopen_wait_timeout_ms;
    uint32_t mipopen_timeout_s;

    if (!endpoint || !endpoint->client || !host) {
        return false;
    }

    socket_type = (endpoint->protocol == IPPROTO_TCP) ? "TCP" : "UDP";
    open_mode = (endpoint->protocol == IPPROTO_TCP) ? 2 : 3;
    use_tls = (endpoint->protocol == IPPROTO_TCP) ? endpoint->is_tls : false;
    pdp_cid = (endpoint->ctx && endpoint->ctx->active_pdp_cid > 0) ? endpoint->ctx->active_pdp_cid : 1;
    connect_timeout_ms = ml307_at_cmd_connect_timeout_ms(endpoint);
    control_timeout_ms = ml307_at_cmd_control_timeout_ms(connect_timeout_ms);
    mipopen_wait_timeout_ms = ml307_at_cmd_mipopen_wait_timeout_ms(connect_timeout_ms);
    mipopen_timeout_s = ml307_at_cmd_mipopen_timeout_s(connect_timeout_ms);

    xEventGroupClearBits(endpoint->event_group,
                         ML307_ENDPOINT_CONNECTED | ML307_ENDPOINT_DISCONNECTED |
                         ML307_ENDPOINT_ERROR | ML307_ENDPOINT_INITIALIZED);

    snprintf(command, sizeof(command), "AT+MIPSTATE=%d", endpoint->id);
    if (!at_client_send_cmd(endpoint->client, command, 1000, true)) {
        return false;
    }

    bits = xEventGroupWaitBits(endpoint->event_group,
                               ML307_ENDPOINT_INITIALIZED | ML307_ENDPOINT_CONNECTED,
                               pdTRUE, pdFALSE, pdMS_TO_TICKS(control_timeout_ms));
    if ((bits & (ML307_ENDPOINT_INITIALIZED | ML307_ENDPOINT_CONNECTED)) == 0) {
        return false;
    }

    if (endpoint->connected) {
        snprintf(command, sizeof(command), "AT+MIPCLOSE=%d", endpoint->id);
        if (at_client_send_cmd(endpoint->client, command, 1000, true)) {
            xEventGroupWaitBits(endpoint->event_group, ML307_ENDPOINT_DISCONNECTED,
                                pdTRUE, pdFALSE, pdMS_TO_TICKS(control_timeout_ms));
        }
    }

    if (!ml307_at_tls_configure_socket(endpoint->client, endpoint->id, use_tls)) {
        return false;
    }

    snprintf(command, sizeof(command), "AT+MIPCFG=\"cid\",%d,%d", endpoint->id, pdp_cid);
    if (!at_client_send_cmd(endpoint->client, command, 1000, true)) {
        endpoint->last_error = at_client_get_cme_error(endpoint->client);
        return false;
    }

    snprintf(command, sizeof(command), "AT+MIPCFG=\"encoding\",%d,1,1", endpoint->id);
    if (!at_client_send_cmd(endpoint->client, command, 1000, true)) {
        return false;
    }

    snprintf(command, sizeof(command), "AT+MIPOPEN=%d,\"%s\",\"%s\",%d,%u,%d,0",
             endpoint->id, socket_type, host, port, (unsigned)mipopen_timeout_s, open_mode);
    if (!at_client_send_cmd_wait_urc_match(endpoint->client, command, "MIPOPEN",
                                           ml307_at_cmd_match_endpoint_urc, endpoint,
                                           &mipopen_args, &mipopen_arg_count,
                                           mipopen_wait_timeout_ms, true)) {
        at_arg_array_destroy(mipopen_args, mipopen_arg_count);
        endpoint->last_error = at_client_get_cme_error(endpoint->client);
        return false;
    }

    connected = ml307_at_cmd_parse_mipopen_result(endpoint, mipopen_args, mipopen_arg_count);
    at_arg_array_destroy(mipopen_args, mipopen_arg_count);
    return connected;
}

int ml307_at_cmd_disconnect(ml307_endpoint_t *endpoint)
{
    char command[32];
    uint32_t control_timeout_ms;

    if (!endpoint || !endpoint->client) {
        return -1;
    }
    if (!endpoint->instance_active) {
        return 0;
    }

    control_timeout_ms = ml307_at_cmd_control_timeout_ms(ml307_at_cmd_connect_timeout_ms(endpoint));
    snprintf(command, sizeof(command), "AT+MIPCLOSE=%d", endpoint->id);
    if (at_client_send_cmd(endpoint->client, command, 1000, true)) {
        xEventGroupWaitBits(endpoint->event_group, ML307_ENDPOINT_DISCONNECTED,
                            pdTRUE, pdFALSE, pdMS_TO_TICKS(control_timeout_ms));
    }
    return 0;
}

int ml307_at_cmd_send_chunk(ml307_endpoint_t *endpoint, const char *data, size_t length)
{
    char command[64];
    EventBits_t bits;
    TickType_t t0;
    TickType_t t_cmd_start;
    TickType_t t_cmd_done;
    TickType_t t_urc_done;

    if (!endpoint || !endpoint->client || !data || length == 0U) {
        return -1;
    }

    t0 = xTaskGetTickCount();
    if (snprintf(command, sizeof(command), "AT+MIPSEND=%d,%zu", endpoint->id, length) <= 0) {
        return -1;
    }

    xEventGroupClearBits(endpoint->event_group, ML307_ENDPOINT_SEND_COMPLETE);
    endpoint->last_sent_bytes = 0;

    t_cmd_start = xTaskGetTickCount();
    if (!at_client_send_cmd_with_data(endpoint->client, command, endpoint->send_timeout_ms, true,
                                      (const uint8_t *)data, length)) {
        LISA_MODEM_PERF_LOGW(TAG, "send AT failed, cmd_wait=%ums",
                             (unsigned)(xTaskGetTickCount() - t_cmd_start));
        return -1;
    }
    t_cmd_done = xTaskGetTickCount();

    bits = xEventGroupWaitBits(endpoint->event_group, ML307_ENDPOINT_SEND_COMPLETE,
                               pdTRUE, pdFALSE, pdMS_TO_TICKS(endpoint->send_timeout_ms));
    t_urc_done = xTaskGetTickCount();
    if ((bits & ML307_ENDPOINT_SEND_COMPLETE) == 0) {
        LISA_MODEM_PERF_LOGW(TAG, "send URC timeout, cmd=%ums urc_wait=%ums",
                             (unsigned)(t_cmd_done - t_cmd_start),
                             (unsigned)(t_urc_done - t_cmd_done));
        return -1;
    }

    LISA_MODEM_PERF_LOGI(TAG, "send ep=%d chunk %zuB cmd=%ums urc=%ums total=%ums",
                         endpoint->id, length,
                         (unsigned)(t_cmd_done - t_cmd_start),
                         (unsigned)(t_urc_done - t_cmd_done),
                         (unsigned)(t_urc_done - t0));
    return (endpoint->last_sent_bytes > 0 && endpoint->last_sent_bytes != (int)length)
         ? endpoint->last_sent_bytes
         : (int)length;
}

int ml307_at_cmd_send(ml307_endpoint_t *endpoint, const char *data, size_t length)
{
    size_t total_sent = 0;
    size_t max_chunk;
    TickType_t t_total_start = xTaskGetTickCount();

    if (!endpoint || !endpoint->client || !data || length == 0) {
        return -1;
    }

    max_chunk = (endpoint->protocol == IPPROTO_TCP) ? ml307_at_cmd_tcp_send_chunk_size(endpoint)
                                                    : ML307_UDP_MAX_PACKET_SIZE;

    while (total_sent < length) {
        size_t chunk_size = (length - total_sent > max_chunk) ? max_chunk : (length - total_sent);
        int sent = ml307_at_cmd_send_chunk(endpoint, data + total_sent, chunk_size);

        if (sent < 0) {
            return -1;
        }
        total_sent += (size_t)sent;

        if (total_sent < length) {
            /* Let other sockets compete for the serialized AT lane between chunks. */
            TickType_t delay_ticks = ml307_at_cmd_ms_to_ticks_nonzero(ml307_at_cmd_send_chunk_delay_ms(endpoint));
            if (delay_ticks > 0) {
                vTaskDelay(delay_ticks);
            } else {
                taskYIELD();
            }
        }
    }

    LISA_MODEM_PERF_LOGI(TAG, "send ep=%d total %zuB done in %ums",
                         endpoint->id, length, (unsigned)(xTaskGetTickCount() - t_total_start));

    if (endpoint->protocol == IPPROTO_UDP) {
        endpoint->connected = true;
        endpoint->instance_active = true;
    }

    return (int)total_sent;
}

int ml307_at_cmd_sendto(ml307_endpoint_t *endpoint, const char *host, uint16_t port,
                        const char *data, size_t length)
{
    if (!endpoint || !host || !data || length == 0) {
        return -1;
    }

    if (!ml307_at_cmd_connect(endpoint, host, (int)port)) {
        return -1;
    }

    return ml307_at_cmd_send(endpoint, data, length);
}

int ml307_at_cmd_prefetch(ml307_endpoint_t *endpoint)
{
    char command[64];
    uint32_t max_space;
    uint32_t prefetch_timeout_ms;
    uint32_t ring_before;
    uint32_t ring_after;
    uint32_t hint_before;
    uint32_t push_seq_before;
    uint32_t pushed_before;
    uint32_t pushed_bytes;
    uint32_t push_count;
    uint32_t idle_gap_ms;
    uint32_t cmd_wait_ms;
    uint32_t rsp_ms;
    uint32_t decode_ms;
    uint32_t ok_tail_ms;
    uint32_t ok_resume_ms;
    uint32_t cmd_total_ms;
    uint32_t throughput_1s_bps;
    size_t request_len = 0U;
    EventBits_t recv_bits;
    TickType_t t_pf_start;
    TickType_t t_pf_cmd;
    TickType_t t_pf_done;
    TickType_t last_push_before;
    bool empty_prefetch;

    if (!endpoint || !endpoint->initialized) {
        return 0;
    }

    prefetch_timeout_ms = modem_endpoint_runtime_effective_pull_timeout(endpoint->pull_timeout_ms,
                                                                        ML307_PULL_TIMEOUT_FALLBACK_MS,
                                                                        ML307_PULL_TIMEOUT_MIN_MS);

    max_space = ring_buf_space_get(&endpoint->ring_buf);
    ring_before = ring_buf_size_get(&endpoint->ring_buf);
    hint_before = (endpoint->protocol == IPPROTO_TCP)
                ? (uint32_t)endpoint->rx_hint.available_data_len
                : (uint32_t)endpoint->rx_hint.unread_packet_count;
    push_seq_before = endpoint->perf_rx_push_seq;
    pushed_before = endpoint->perf_rx_push_total_bytes;
    last_push_before = endpoint->perf_last_rx_push_tick;
    if (endpoint->protocol == IPPROTO_TCP) {
        if (!endpoint->connected) {
            return 0;
        }

        size_t read_len = endpoint->rx_hint.available_data_len < max_space
                        ? endpoint->rx_hint.available_data_len
                        : max_space;

        if (read_len == 0) {
            ml307_at_cmd_refresh_pending(endpoint);
            return 0;
        }
        /* Bound each pull so one socket cannot turn a single MIPRD into a long stall. */
        size_t pull_chunk_size = ml307_at_cmd_tcp_pull_chunk_size(endpoint);
        if (read_len > pull_chunk_size) {
            read_len = pull_chunk_size;
        }

        xEventGroupClearBits(endpoint->recv_event, ML307_ENDPOINT_PREFETCH_AVAILABLE);
        request_len = read_len;
        snprintf(command, sizeof(command), "AT+MIPRD=%d,%zu", endpoint->id, read_len);
    } else if (endpoint->protocol == IPPROTO_UDP) {
        if (endpoint->rx_hint.unread_packet_count == 0 || max_space == 0) {
            ml307_at_cmd_refresh_pending(endpoint);
            return 0;
        }

        xEventGroupClearBits(endpoint->recv_event, ML307_ENDPOINT_PREFETCH_AVAILABLE);
        request_len = 1U;
        snprintf(command, sizeof(command), "AT+MIPRD=%d,%u", endpoint->id, 1U);
    } else {
        return -1;
    }

    ml307_at_cmd_reset_prefetch_perf(endpoint);
    endpoint->prefetching = true;

    t_pf_start = xTaskGetTickCount();
    if (!at_client_send_cmd(endpoint->client, command, prefetch_timeout_ms, true)) {
        endpoint->prefetching = false;
        LISA_MODEM_PERF_LOGW(TAG, "prefetch ep=%d ask=%zu hint=%u ring=%u AT failed after %ums",
                             endpoint->id, request_len, hint_before, ring_before,
                             (unsigned)ml307_at_cmd_tick_elapsed_ms(t_pf_start, xTaskGetTickCount()));
        return -1;
    }
    t_pf_cmd = xTaskGetTickCount();

    recv_bits = xEventGroupWaitBits(endpoint->recv_event, ML307_ENDPOINT_PREFETCH_AVAILABLE,
                                    pdTRUE, pdFALSE, pdMS_TO_TICKS(prefetch_timeout_ms));
    t_pf_done = xTaskGetTickCount();

    endpoint->prefetching = false;
    ring_after = ring_buf_size_get(&endpoint->ring_buf);
    pushed_bytes = endpoint->perf_rx_push_total_bytes - pushed_before;
    push_count = endpoint->perf_rx_push_seq - push_seq_before;
    cmd_total_ms = ml307_at_cmd_tick_elapsed_ms(t_pf_start, t_pf_cmd);
    cmd_wait_ms = at_client_get_last_cmd_wait_ms(endpoint->client);
    ok_resume_ms = at_client_get_last_ok_wait_resume_ms(endpoint->client);
    throughput_1s_bps = ml307_at_cmd_update_prefetch_rate_1s(endpoint, t_pf_done, pushed_bytes);
    idle_gap_ms = (push_seq_before > 0U)
                ? ml307_at_cmd_tick_elapsed_ms(last_push_before, t_pf_start)
                : 0U;
    if (endpoint->perf_prefetch_first_payload_tick != 0 &&
        endpoint->perf_prefetch_first_payload_tick >= t_pf_start) {
        rsp_ms = ml307_at_cmd_tick_elapsed_ms(t_pf_start, endpoint->perf_prefetch_first_payload_tick);
        rsp_ms = (rsp_ms > cmd_wait_ms) ? (rsp_ms - cmd_wait_ms) : 0U;
    } else {
        rsp_ms = 0U;
    }
    if (endpoint->perf_prefetch_line_done_tick != 0 &&
        endpoint->perf_prefetch_first_payload_tick != 0 &&
        endpoint->perf_prefetch_line_done_tick >= endpoint->perf_prefetch_first_payload_tick) {
        decode_ms = ml307_at_cmd_tick_elapsed_ms(endpoint->perf_prefetch_first_payload_tick,
                                                 endpoint->perf_prefetch_line_done_tick);
    } else {
        decode_ms = 0U;
    }
    if (endpoint->perf_prefetch_line_done_tick != 0 &&
        t_pf_cmd >= endpoint->perf_prefetch_line_done_tick) {
        ok_tail_ms = ml307_at_cmd_tick_elapsed_ms(endpoint->perf_prefetch_line_done_tick, t_pf_cmd);
    } else if (endpoint->perf_prefetch_last_push_tick != 0 &&
               t_pf_cmd >= endpoint->perf_prefetch_last_push_tick) {
        ok_tail_ms = ml307_at_cmd_tick_elapsed_ms(endpoint->perf_prefetch_last_push_tick, t_pf_cmd);
    } else {
        ok_tail_ms = 0U;
    }
    empty_prefetch = (pushed_bytes == 0U && ring_after <= ring_before);
    if (empty_prefetch) {
        endpoint->perf_empty_prefetch_streak++;
    } else {
        endpoint->perf_empty_prefetch_streak = 0U;
    }

    if ((recv_bits & ML307_ENDPOINT_PREFETCH_AVAILABLE) == 0U && pushed_bytes == 0U) {
        LISA_MODEM_PERF_LOGW(TAG,
                             "prefetch ep=%d ask=%zu hint=%u ring=%u->%u cmd=%ums lock=%ums rsp=%ums decode=%ums ok_tail=%ums rate1s=%uBps wait=%ums pushed=%u pushes=%u idle=%ums empty_streak=%u timeout=%u",
                             endpoint->id, request_len, hint_before, ring_before, ring_after,
                             cmd_total_ms,
                             cmd_wait_ms, rsp_ms, decode_ms, ok_tail_ms,
                             throughput_1s_bps,
                             (unsigned)ml307_at_cmd_tick_elapsed_ms(t_pf_cmd, t_pf_done),
                             pushed_bytes, push_count, idle_gap_ms,
                             endpoint->perf_empty_prefetch_streak,
                             (unsigned)prefetch_timeout_ms);
    } else {
        LISA_MODEM_PERF_LOGI(TAG,
                             "prefetch ep=%d ask=%zu hint=%u ring=%u->%u cmd=%ums lock=%ums rsp=%ums decode=%ums ok_tail=%ums ok_resume=%ums rate1s=%uBps wait=%ums pushed=%u pushes=%u idle=%ums empty_streak=%u",
                             endpoint->id, request_len, hint_before, ring_before, ring_after,
                             cmd_total_ms,
                             cmd_wait_ms, rsp_ms, decode_ms, ok_tail_ms, ok_resume_ms,
                             throughput_1s_bps,
                             (unsigned)ml307_at_cmd_tick_elapsed_ms(t_pf_cmd, t_pf_done),
                             pushed_bytes, push_count, idle_gap_ms,
                             endpoint->perf_empty_prefetch_streak);
    }
    return 0;
}
