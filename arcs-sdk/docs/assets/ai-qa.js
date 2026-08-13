/**
 * ARCS SDK AI 文档助手
 *
 * 对接 docs/AI_assistant 提供的问答接口，优先走流式事件流。
 */

const ASK_REQUEST_TIMEOUT_MS = 240000;
const UNIFIED_ERROR_MESSAGE = '连接超时';

function shouldEnableAssistant(config = {}) {
  return true;
}

function toggleAssistantVisibility(visible) {
  const shell = document.querySelector('.docs-ai-shell');
  const rail = document.querySelector('.docs-ai-rail');
  if (shell) {
    shell.classList.toggle('docs-ai-shell-with-rail', visible);
  }
  if (rail) {
    rail.hidden = !visible;
  }
  document.body.classList.toggle('docs-ai-rail-expanded', visible);
  if (!visible) {
    document.body.classList.remove('docs-ai-assistant-minimized');
  }
}

function deriveAssistantWidgetTemplateUrl(config = {}) {
  const explicitUrl = String(config.widgetTemplateUrl || '').trim();
  if (explicitUrl) {
    return explicitUrl;
  }

  const scriptNodes = Array.from(document.querySelectorAll('script[src]'));
  const assistantScript = scriptNodes.reverse().find(script => {
    const src = script.getAttribute('src') || '';
    return /ai-qa\.js(?:[?#].*)?$/.test(src);
  });

  if (!assistantScript) {
    return '';
  }

  try {
    const scriptUrl = new URL(assistantScript.getAttribute('src'), window.location.href);
    scriptUrl.pathname = scriptUrl.pathname.replace(/ai-qa\.js$/, 'ai-qa.html');
    scriptUrl.search = '';
    scriptUrl.hash = '';
    return scriptUrl.href;
  } catch (error) {
    console.warn('推导 AI 助手模板地址失败:', error);
    return '';
  }
}

async function ensureLatestAssistantWidgetTemplate(config = {}) {
  const currentWidget = document.getElementById('ai-qa-widget');
  if (!currentWidget) {
    return null;
  }

  const widgetTemplateUrl = deriveAssistantWidgetTemplateUrl(config);
  if (!widgetTemplateUrl) {
    return currentWidget;
  }

  try {
    const response = await fetch(widgetTemplateUrl, {
      method: 'GET',
      cache: 'no-cache',
      headers: {
        Accept: 'text/html',
      },
    });

    if (!response.ok) {
      throw new Error(`模板请求失败: ${response.status}`);
    }

    const html = (await response.text()).trim();
    if (!html) {
      return currentWidget;
    }

    const wrapper = document.createElement('div');
    wrapper.innerHTML = html;
    const nextWidget = wrapper.firstElementChild;
    if (!nextWidget || nextWidget.id !== 'ai-qa-widget') {
      return currentWidget;
    }

    currentWidget.replaceWith(nextWidget);
    return nextWidget;
  } catch (error) {
    console.warn('加载最新 AI 助手模板失败，继续使用页面内嵌模板:', error);
    return currentWidget;
  }
}

class AIDocumentAssistant {
  constructor(config) {
    this.apiEndpoint = config.apiEndpoint || 'http://127.0.0.1:8001/api/v1/ask';
    this.streamEndpoint = config.streamEndpoint || this.deriveStreamEndpoint(this.apiEndpoint);
    this.rateEndpoint = config.rateEndpoint || this.apiEndpoint.replace('/ask', '/rate');
    this.maxPersistedMessages = config.maxPersistedMessages || 30;
    this.maxRenderedSources = config.maxRenderedSources || 4;
    this.pageVersion = this.detectPageVersion();
    this.assistantVersion = this.resolveDefaultAssistantVersion(config.version);
    this.sessionStorageKey = '';
    this.historyStorageKey = '';
    this.sessionId = '';
    this.messages = [];
    this.isLoading = false;
    this.defaultWelcomeMarkup = '';
    this.markdownRenderer = this.createMarkdownRenderer();
    this.collapseMediaQuery = window.matchMedia('(max-width: 1279px)');
    this.isCollapsedMode = false;
    this.isCollapsedOpen = false;
    this.isUserMinimized = false;
    this.streamPendingBuffers = new Map();
    this.streamFlushHandles = new Map();

    this.setVersionScopedState(this.assistantVersion);
    this.initUI();
    this.bindEvents();
  }

  createMarkdownRenderer() {
    if (typeof window.markdownit !== 'function') {
      return null;
    }

    return window.markdownit({
      html: false,
      linkify: true,
      breaks: true,
    });
  }

  detectPageVersion() {
    const url = new URL(window.location.href);
    const pathParts = url.pathname.split('/').filter(Boolean);

    let version = 'latest';
    const versionMatch = pathParts.find(part => part.startsWith('v') || part === 'latest');
    if (versionMatch) {
      version = versionMatch;
    }

    return version;
  }

  deriveStreamEndpoint(apiEndpoint) {
    const normalized = String(apiEndpoint || '').trim();
    if (!normalized) {
      return '';
    }
    return normalized.replace(/\/ask(?=$|[?#])/, '/ask/stream');
  }

  resolveDefaultAssistantVersion(configVersion) {
    return String(configVersion || '').trim() || this.pageVersion || 'latest';
  }

  getSessionStorageKey() {
    return `arcs-ai-assistant-session:${window.location.origin}:${this.assistantVersion}`;
  }

  getHistoryStorageKey() {
    return `arcs-ai-assistant-history:${window.location.origin}:${this.assistantVersion}`;
  }

  setVersionScopedState(version) {
    this.assistantVersion = version;
    this.sessionStorageKey = this.getSessionStorageKey();
    this.historyStorageKey = this.getHistoryStorageKey();
    this.sessionId = this.loadSessionId();
    this.messages = this.loadPersistedMessages();
  }

  loadSessionId() {
    try {
      return window.sessionStorage.getItem(this.sessionStorageKey) || '';
    } catch (error) {
      console.warn('读取 AI 助手会话失败:', error);
      return '';
    }
  }

  saveSessionId(sessionId) {
    this.sessionId = sessionId || '';

    try {
      if (this.sessionId) {
        window.sessionStorage.setItem(this.sessionStorageKey, this.sessionId);
      } else {
        window.sessionStorage.removeItem(this.sessionStorageKey);
      }
    } catch (error) {
      console.warn('保存 AI 助手会话失败:', error);
    }
  }

  normalizePersistedMessage(message) {
    if (!message || typeof message !== 'object') {
      return null;
    }

    const role = typeof message.role === 'string' ? message.role : '';
    const content = typeof message.content === 'string' ? message.content : '';
    if (!role || !content) {
      return null;
    }

    return {
      id: typeof message.id === 'string' ? message.id : `${Date.now()}-${Math.random()}`,
      role,
      content,
      sources: Array.isArray(message.sources) ? message.sources.filter(source => source && typeof source === 'object') : [],
      rating: ['good', 'medium', 'poor'].includes(message.rating) ? message.rating : null,
      showRating: Boolean(message.showRating),
      isStreaming: false,
    };
  }

  loadPersistedMessages() {
    try {
      const raw = window.sessionStorage.getItem(this.historyStorageKey);
      if (!raw) {
        return [];
      }

      const parsed = JSON.parse(raw);
      if (!Array.isArray(parsed)) {
        return [];
      }

      return parsed
        .map(message => this.normalizePersistedMessage(message))
        .filter(Boolean);
    } catch (error) {
      console.warn('读取 AI 助手历史消息失败:', error);
      return [];
    }
  }

  persistMessages() {
    const compactMessages = this.messages.slice(-this.maxPersistedMessages);
    this.messages = compactMessages;

    try {
      if (compactMessages.length) {
        window.sessionStorage.setItem(this.historyStorageKey, JSON.stringify(compactMessages));
      } else {
        window.sessionStorage.removeItem(this.historyStorageKey);
      }
    } catch (error) {
      console.warn('保存 AI 助手历史消息失败:', error);
    }
  }

  renderWelcomeState(messagesEl) {
    if (!messagesEl) {
      return;
    }

    messagesEl.innerHTML = this.defaultWelcomeMarkup;
  }

  restoreMessages() {
    const messagesEl = document.getElementById('ai-qa-messages');
    if (!messagesEl) {
      return;
    }

    this.renderWelcomeState(messagesEl);
    this.messages.forEach(message => {
      this.renderMessage(message);
    });
  }

  initUI() {
    const messagesEl = document.getElementById('ai-qa-messages');
    if (messagesEl) {
      this.defaultWelcomeMarkup = messagesEl.innerHTML;
    }

    this.updateHeaderVersionUI();
    this.restoreMessages();
    this.syncResponsiveMode();
  }

  bindEvents() {
    const sendBtn = document.getElementById('ai-qa-send');
    const input = document.getElementById('ai-qa-input');
    const toggleButton = document.getElementById('ai-qa-toggle');
    const minimizeButton = document.getElementById('ai-qa-minimize');

    if (sendBtn) {
      sendBtn.addEventListener('click', () => this.handleSend());
    }

    if (input) {
      input.addEventListener('keydown', event => {
        if (event.key === 'Enter' && !event.shiftKey) {
          event.preventDefault();
          this.handleSend();
        }
      });

      input.addEventListener('input', () => {
        input.style.height = 'auto';
        input.style.height = `${Math.min(input.scrollHeight, 148)}px`;
      });
    }


    if (toggleButton) {
      toggleButton.addEventListener('click', event => {
        event.preventDefault();
        this.handleToggleButtonClick();
      });
    }

    if (minimizeButton) {
      minimizeButton.addEventListener('click', event => {
        event.preventDefault();
        this.minimizeAssistantPanel();
      });
    }

    document.addEventListener('click', event => {
      if (this.isCollapsedMode && this.isCollapsedOpen) {
        const widget = this.getWidget();
        if (widget && !widget.contains(event.target)) {
          this.closeCollapsedPanel();
        }
      }
    });

    document.addEventListener('keydown', event => {
      if (event.key === 'Escape' && this.isCollapsedMode && this.isCollapsedOpen) {
        this.closeCollapsedPanel();
      }
    });

    if (typeof this.collapseMediaQuery.addEventListener === 'function') {
      this.collapseMediaQuery.addEventListener('change', () => this.syncResponsiveMode());
    } else if (typeof this.collapseMediaQuery.addListener === 'function') {
      this.collapseMediaQuery.addListener(() => this.syncResponsiveMode());
    }
  }

  getWidget() {
    return document.getElementById('ai-qa-widget');
  }

  updateCollapsedUI() {
    const widget = this.getWidget();
    const toggleButton = document.getElementById('ai-qa-toggle');
    const minimizeButton = document.getElementById('ai-qa-minimize');

    if (!widget || !toggleButton) {
      return;
    }

    const isPanelOpen = !this.isUserMinimized && (!this.isCollapsedMode || this.isCollapsedOpen);

    widget.classList.toggle('is-collapsed-mode', this.isCollapsedMode);
    widget.classList.toggle('is-user-minimized', this.isUserMinimized);
    widget.classList.toggle('is-open', isPanelOpen);
    document.body.classList.toggle('docs-ai-rail-expanded', isPanelOpen && !this.isCollapsedMode);
    document.body.classList.toggle('docs-ai-assistant-minimized', this.isUserMinimized);
    document.body.classList.add('docs-ai-floating-actions');

    toggleButton.setAttribute('aria-expanded', String(isPanelOpen));
    toggleButton.setAttribute('aria-hidden', String(!this.isCollapsedMode && !this.isUserMinimized));
    toggleButton.title = isPanelOpen ? '关闭 AI 助手' : '打开 AI 助手';

    if (minimizeButton) {
      minimizeButton.setAttribute('aria-expanded', String(isPanelOpen));
    }
  }

  syncResponsiveMode() {
    const shouldCollapse = this.collapseMediaQuery.matches;

    if (this.isCollapsedMode !== shouldCollapse) {
      this.isCollapsedMode = shouldCollapse;
      if (!this.isUserMinimized) {
        this.isCollapsedOpen = shouldCollapse ? false : true;
      }
    } else if (!shouldCollapse && !this.isUserMinimized) {
      this.isCollapsedOpen = true;
    }

    this.updateCollapsedUI();
  }

  focusInputSoon() {
    const input = document.getElementById('ai-qa-input');
    if (input) {
      window.setTimeout(() => input.focus(), 30);
    }
  }

  openCollapsedPanel() {
    if (!this.isCollapsedMode) {
      return;
    }

    this.isUserMinimized = false;
    this.isCollapsedOpen = true;
    this.updateCollapsedUI();
    this.focusInputSoon();
  }

  closeCollapsedPanel() {
    if (!this.isCollapsedMode) {
      return;
    }

    this.isCollapsedOpen = false;
    this.updateCollapsedUI();
  }

  minimizeAssistantPanel() {
    this.isUserMinimized = true;
    this.isCollapsedOpen = false;
    this.updateCollapsedUI();
  }

  restoreAssistantPanel() {
    this.isUserMinimized = false;
    this.isCollapsedOpen = true;
    this.updateCollapsedUI();
    this.focusInputSoon();
  }

  handleToggleButtonClick() {
    if (this.isUserMinimized) {
      this.restoreAssistantPanel();
      return;
    }

    this.toggleCollapsedPanel();
  }

  toggleCollapsedPanel() {
    if (!this.isCollapsedMode) {
      return;
    }

    if (this.isCollapsedOpen) {
      this.closeCollapsedPanel();
      return;
    }

    this.openCollapsedPanel();
  }

  updateHeaderVersionUI() {
    const versionEl = document.getElementById('ai-qa-version');
    if (versionEl) {
      versionEl.textContent = `Assistant: ${this.assistantVersion}`;
    }
  }

  async handleSend() {
    const input = document.getElementById('ai-qa-input');
    if (!input) {
      return;
    }

    const message = input.value.trim();
    if (!message || this.isLoading) {
      return;
    }

    input.value = '';
    input.style.height = 'auto';
    this.addMessage('user', message);

    this.isLoading = true;
    this.updateStatus(`正在使用 ${this.assistantVersion} 检索...`);
    this.setSendButtonState(false);

    let assistantMessageId = '';

    try {
      if (this.canUseStreamTransport()) {
        assistantMessageId = this.addMessage('assistant', '', {
          sources: [],
          rating: null,
          showRating: false,
          isStreaming: true,
        });
        const streamed = await this.sendMessageStreaming(message, assistantMessageId);
        if (!streamed) {
          this.removeMessage(assistantMessageId);
          assistantMessageId = '';
          await this.sendMessage(message);
        }
      } else {
        await this.sendMessage(message);
      }
    } catch (error) {
      console.error('发送消息失败:', error);
      if (assistantMessageId) {
        const partialMessage = this.messages.find(messageItem => messageItem.id === assistantMessageId);
        this.clearStreamState(assistantMessageId);
        if (partialMessage && partialMessage.content.trim()) {
          this.updateMessage(assistantMessageId, {
            isStreaming: false,
            showRating: false,
          });
        } else {
          this.removeMessage(assistantMessageId);
        }
      }
      this.addMessage('error', error.message || '抱歉，发生了错误。请稍后重试。');
    } finally {
      this.isLoading = false;
      this.updateStatus('');
      this.setSendButtonState(true);
    }
  }

  async sendMessage(message) {
    const controller = typeof AbortController !== 'undefined' ? new AbortController() : null;
    const timeoutId = controller
      ? window.setTimeout(() => controller.abort(), ASK_REQUEST_TIMEOUT_MS)
      : 0;
    let response;
    try {
      response = await fetch(this.apiEndpoint, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        signal: controller ? controller.signal : undefined,
        body: JSON.stringify({
          question: message,
          session_id: this.sessionId || undefined,
          assistant_version: this.assistantVersion,
          top_k: 4,
        }),
      });
    } catch (fetchError) {
      console.error('Fetch error:', fetchError);
      throw new Error(UNIFIED_ERROR_MESSAGE);
    } finally {
      if (timeoutId) {
        window.clearTimeout(timeoutId);
      }
    }

    if (!response.ok) {
      let errorText = '';
      try {
        errorText = await response.text();
      } catch (readError) {
        console.error('读取错误响应失败:', readError);
      }
      console.error('文档助手请求失败:', response.status, errorText);
      throw new Error(UNIFIED_ERROR_MESSAGE);
    }

    let data;
    try {
      data = await response.json();
    } catch (parseError) {
      console.error('解析问答响应失败:', parseError);
      throw new Error(UNIFIED_ERROR_MESSAGE);
    }
    if (!data || typeof data.answer !== 'string' || !data.answer.trim()) {
      console.error('问答响应缺少有效 answer:', data);
      throw new Error(UNIFIED_ERROR_MESSAGE);
    }
    if (data && data.meta && data.meta.session_id) {
      this.saveSessionId(data.meta.session_id);
    }
    this.addMessage('assistant', data.answer, {
      sources: data.sources || [],
      rating: null,
      showRating: true,
      isStreaming: false,
    });
  }

  canUseStreamTransport() {
    return Boolean(
      this.streamEndpoint
      && typeof window.fetch === 'function'
      && typeof window.TextDecoder === 'function'
    );
  }

  async sendMessageStreaming(message, assistantMessageId) {
    if (!this.canUseStreamTransport()) {
      return false;
    }

    const controller = typeof AbortController !== 'undefined' ? new AbortController() : null;
    const timeoutId = controller
      ? window.setTimeout(() => controller.abort(), ASK_REQUEST_TIMEOUT_MS)
      : 0;
    let response;

    try {
      response = await fetch(this.streamEndpoint, {
        method: 'POST',
        headers: {
          Accept: 'text/event-stream',
          'Content-Type': 'application/json',
        },
        signal: controller ? controller.signal : undefined,
        body: JSON.stringify({
          question: message,
          session_id: this.sessionId || undefined,
          assistant_version: this.assistantVersion,
          top_k: 4,
        }),
      });
    } catch (error) {
      console.warn('流式请求失败，回退到普通请求:', error);
      if (timeoutId) {
        window.clearTimeout(timeoutId);
      }
      return false;
    }

    const contentType = String(response.headers.get('content-type') || '').toLowerCase();
    if (!response.ok || !response.body || !contentType.includes('text/event-stream')) {
      if (timeoutId) {
        window.clearTimeout(timeoutId);
      }
      return false;
    }

    const reader = response.body.getReader();
    const decoder = new TextDecoder('utf-8');
    let buffer = '';
    let receivedEvents = false;
    let sawDone = false;

    try {
      while (true) {
        const { value, done } = await reader.read();
        buffer += decoder.decode(value || new Uint8Array(), { stream: !done });

        const parsed = this.extractStreamEvents(buffer, done);
        buffer = parsed.buffer;

        parsed.events.forEach(event => {
          receivedEvents = true;
          const outcome = this.applyStreamEvent(event, assistantMessageId);
          if (outcome?.type === 'done') {
            sawDone = true;
          } else if (outcome?.type === 'error') {
            throw new Error(outcome.message || UNIFIED_ERROR_MESSAGE);
          }
        });

        if (done) {
          break;
        }
      }
    } catch (error) {
      if (!receivedEvents) {
        return false;
      }
      if (error && error.name === 'AbortError') {
        throw new Error(UNIFIED_ERROR_MESSAGE);
      }
      throw error;
    } finally {
      if (timeoutId) {
        window.clearTimeout(timeoutId);
      }
    }

    if (!receivedEvents) {
      return false;
    }
    if (!sawDone) {
      throw new Error(UNIFIED_ERROR_MESSAGE);
    }
    return true;
  }

  extractStreamEvents(buffer, flush = false) {
    const normalized = String(buffer || '').replace(/\r\n/g, '\n');
    const parts = normalized.split('\n\n');
    const pending = flush ? '' : (parts.pop() || '');
    const events = parts
      .map(rawEvent => this.parseStreamEvent(rawEvent))
      .filter(Boolean);
    return { events, buffer: pending };
  }

  parseStreamEvent(rawEvent) {
    const text = String(rawEvent || '').trim();
    if (!text) {
      return null;
    }

    let eventName = 'message';
    const dataLines = [];
    text.split('\n').forEach(line => {
      if (line.startsWith('event:')) {
        eventName = line.slice(6).trim();
      } else if (line.startsWith('data:')) {
        dataLines.push(line.slice(5).trimStart());
      }
    });

    let data = null;
    const payload = dataLines.join('\n');
    if (payload) {
      try {
        data = JSON.parse(payload);
      } catch (error) {
        console.warn('解析流式事件失败:', error, payload);
        return null;
      }
    }

    return { event: eventName, data };
  }

  applyStreamEvent(streamEvent, assistantMessageId) {
    if (!streamEvent || !assistantMessageId) {
      return null;
    }

    const { event, data } = streamEvent;
    if (event === 'status') {
      this.updateStatus(data && data.message ? data.message : '');
      return null;
    }

    if (event === 'answer_delta') {
      const delta = data && typeof data.delta === 'string' ? data.delta : '';
      if (delta) {
        this.queueStreamDelta(assistantMessageId, delta);
      }
      return null;
    }

    if (event === 'answer_reset') {
      this.clearStreamState(assistantMessageId);
      this.updateMessage(assistantMessageId, {
        content: '',
        sources: [],
        rating: null,
        showRating: false,
        isStreaming: true,
      });
      return null;
    }

    if (event === 'done') {
      this.flushStreamBuffer(assistantMessageId);
      const answer = data && typeof data.answer === 'string' ? data.answer : '';
      if (!answer.trim()) {
        return { type: 'error', message: UNIFIED_ERROR_MESSAGE };
      }
      if (data && data.meta && data.meta.session_id) {
        this.saveSessionId(data.meta.session_id);
      }
      this.updateMessage(assistantMessageId, {
        content: answer,
        sources: Array.isArray(data && data.sources) ? data.sources : [],
        rating: null,
        showRating: true,
        isStreaming: false,
      });
      return { type: 'done' };
    }

    if (event === 'error') {
      this.flushStreamBuffer(assistantMessageId);
      this.updateMessage(assistantMessageId, {
        isStreaming: false,
        showRating: false,
      });
      return {
        type: 'error',
        message: (data && data.message && data.message !== 'request_failed')
          ? data.message
          : UNIFIED_ERROR_MESSAGE,
      };
    }

    return null;
  }

  addMessage(role, content, options = {}) {
    const message = {
      id: options.id || `${Date.now()}-${Math.random()}`,
      role,
      content,
      sources: Array.isArray(options.sources) ? options.sources : [],
      rating: options.rating || null,
      showRating: Boolean(options.showRating),
      isStreaming: Boolean(options.isStreaming),
    };

    if (role === 'assistant') {
      const downgradedMessages = [];
      this.messages = this.messages.map(existingMessage => {
        if (existingMessage.role !== 'assistant' || !existingMessage.showRating) {
          return existingMessage;
        }

        const downgradedMessage = { ...existingMessage, showRating: false };
        if (!downgradedMessage.isStreaming && downgradedMessage.content.trim()) {
          downgradedMessages.push(downgradedMessage);
        }
        return downgradedMessage;
      });
      downgradedMessages.forEach(downgradedMessage => this.renderMessage(downgradedMessage));
      message.showRating = Boolean(options.showRating);
    }

    this.messages.push(message);
    this.persistMessages();
    return this.renderMessage(message);
  }

  findMessageIndex(messageId) {
    return this.messages.findIndex(message => message.id === messageId);
  }

  updateMessage(messageId, updates = {}) {
    const index = this.findMessageIndex(messageId);
    if (index === -1) {
      return null;
    }

    const nextMessage = {
      ...this.messages[index],
      ...updates,
    };
    this.messages[index] = nextMessage;
    this.persistMessages();
    return this.renderMessage(nextMessage);
  }

  removeMessage(messageId) {
    const index = this.findMessageIndex(messageId);
    if (index === -1) {
      return;
    }

    this.clearStreamState(messageId);
    this.messages.splice(index, 1);
    this.persistMessages();

    const element = document.getElementById(`message-${messageId}`);
    if (element) {
      element.remove();
    }

    if (!this.messages.length) {
      const messagesEl = document.getElementById('ai-qa-messages');
      this.renderWelcomeState(messagesEl);
    }
  }

  clearStreamState(messageId) {
    this.streamPendingBuffers.delete(messageId);
    const handle = this.streamFlushHandles.get(messageId);
    if (!handle) {
      return;
    }

    this.streamFlushHandles.delete(messageId);
    if (handle.type === 'raf' && typeof window.cancelAnimationFrame === 'function') {
      window.cancelAnimationFrame(handle.id);
      return;
    }
    window.clearTimeout(handle.id);
  }

  queueStreamDelta(messageId, delta) {
    const previous = this.streamPendingBuffers.get(messageId) || '';
    this.streamPendingBuffers.set(messageId, previous + delta);

    if (this.streamFlushHandles.has(messageId)) {
      return;
    }

    const flush = () => {
      this.streamFlushHandles.delete(messageId);
      this.flushStreamBuffer(messageId);
    };

    if (typeof window.requestAnimationFrame === 'function') {
      const id = window.requestAnimationFrame(flush);
      this.streamFlushHandles.set(messageId, { type: 'raf', id });
      return;
    }

    const id = window.setTimeout(flush, 24);
    this.streamFlushHandles.set(messageId, { type: 'timeout', id });
  }

  flushStreamBuffer(messageId) {
    const pending = this.streamPendingBuffers.get(messageId);
    if (!pending) {
      return;
    }

    this.streamPendingBuffers.delete(messageId);
    const index = this.findMessageIndex(messageId);
    if (index === -1) {
      return;
    }

    const message = this.messages[index];
    this.updateMessage(messageId, {
      content: `${message.content || ''}${pending}`,
      isStreaming: true,
    });
  }

  clearRenderedRatings() {
    const messagesEl = document.getElementById('ai-qa-messages');
    if (!messagesEl) {
      return;
    }

    const oldRatings = messagesEl.querySelectorAll('.ai-qa-action-bar');
    oldRatings.forEach(el => el.remove());
  }

  buildAssistantMessageStack(message) {
    const stackDiv = document.createElement('div');
    stackDiv.className = 'ai-qa-assistant-stack';

    const contentDiv = document.createElement('div');
    contentDiv.className = 'ai-qa-message-content';
    if (message.isStreaming) {
      contentDiv.classList.add('is-streaming');
    }
    if (message.content.trim()) {
      contentDiv.innerHTML = this.formatMarkdown(message.content);
      this.enhanceCodeBlocks(contentDiv);
    } else {
      contentDiv.innerHTML = '<p class="ai-qa-stream-placeholder">正在整理回答...</p>';
    }
    stackDiv.appendChild(contentDiv);

    const displaySources = this.pickDisplaySources(message.sources || []);
    if (displaySources.length) {
      const sourcesSection = document.createElement('div');
      sourcesSection.className = 'ai-qa-meta-section ai-qa-sources-section';

      const sourcesTitle = document.createElement('div');
      sourcesTitle.className = 'ai-qa-meta-title';
      sourcesTitle.textContent = '相关链接';
      sourcesSection.appendChild(sourcesTitle);

      const sourcesDiv = document.createElement('div');
      sourcesDiv.className = 'ai-qa-sources';
      sourcesSection.appendChild(sourcesDiv);
      this.renderSources(displaySources, sourcesDiv);
      stackDiv.appendChild(sourcesSection);
    }

    if (!message.isStreaming && message.content.trim()) {
      const actionBar = this.createAnswerActions(message.id, message.rating, message.showRating ? 'full' : 'copy-only');
      stackDiv.appendChild(actionBar);
    }

    return stackDiv;
  }

  buildPlainMessageContent(message) {
    const contentDiv = document.createElement('div');
    contentDiv.className = 'ai-qa-message-content';
    contentDiv.textContent = message.content;
    return contentDiv;
  }

  patchExistingMessage(messageDiv, message) {
    messageDiv.className = `ai-qa-message ai-qa-message-${message.role}`;

    if (message.role === 'assistant') {
      const nextStack = this.buildAssistantMessageStack(message);
      const existingStack = messageDiv.querySelector('.ai-qa-assistant-stack');
      if (existingStack) {
        existingStack.replaceWith(nextStack);
      } else {
        messageDiv.appendChild(nextStack);
      }
      return;
    }

    const nextContent = this.buildPlainMessageContent(message);
    const existingContent = messageDiv.querySelector('.ai-qa-message-content');
    if (existingContent) {
      existingContent.replaceWith(nextContent);
    } else {
      messageDiv.appendChild(nextContent);
    }
  }

  renderMessage(message) {
    const messagesEl = document.getElementById('ai-qa-messages');
    if (!messagesEl) {
      return null;
    }

    const welcomeEl = messagesEl.querySelector('.ai-qa-welcome');
    if (welcomeEl) {
      welcomeEl.remove();
    }

    const messageId = `message-${message.id}`;
    const existingMessageDiv = document.getElementById(messageId);

    if (existingMessageDiv) {
      this.patchExistingMessage(existingMessageDiv, message);
      this.scrollToBottom();
      return message.id;
    }

    const messageDiv = document.createElement('div');
    messageDiv.id = messageId;
    messageDiv.className = `ai-qa-message ai-qa-message-${message.role}`;

    if (message.role === 'assistant') {
      messageDiv.appendChild(this.buildAssistantMessageStack(message));
    } else {
      messageDiv.appendChild(this.buildPlainMessageContent(message));
    }

    messagesEl.appendChild(messageDiv);
    this.scrollToBottom();
    return message.id;
  }

  renderSources(sources, sourcesEl) {
    if (!sourcesEl || !Array.isArray(sources)) {
      return;
    }

    sourcesEl.innerHTML = '';
    sources.forEach(source => {
      if (!source.displayUrl) {
        return;
      }

      const sourceLink = document.createElement('a');
      sourceLink.href = source.displayUrl;
      sourceLink.target = '_self';
      sourceLink.className = 'ai-qa-source-link';
      sourceLink.textContent = source.displayLabel;
      sourceLink.title = source.title || source.displayUrl;
      sourcesEl.appendChild(sourceLink);
    });
  }

  normalizeSourceUrl(url) {
    try {
      return new URL(url, window.location.href);
    } catch (error) {
      return null;
    }
  }

  deriveSourceLabel(source) {
    const normalizedUrl = this.normalizeSourceUrl(source.url);
    const genericNames = new Set(['readme', 'index']);

    if (normalizedUrl) {
      const segments = normalizedUrl.pathname.split('/').filter(Boolean);
      if (segments.length) {
        const lastSegment = segments[segments.length - 1];
        const stem = lastSegment.replace(/\.html?$/i, '');
        const normalizedStem = stem.toLowerCase();
        if (stem && !genericNames.has(normalizedStem)) {
          return decodeURIComponent(stem);
        }

        if (segments.length >= 2) {
          return decodeURIComponent(segments[segments.length - 2]);
        }
      }
    }

    return String(source.title || source.url || '').trim();
  }

  pickDisplaySources(sources) {
    const uniqueSources = [];
    const seenPageKeys = new Set();

    sources.forEach(source => {
      if (!source || !source.url) {
        return;
      }

      const normalizedUrl = this.normalizeSourceUrl(source.url);
      if (!normalizedUrl) {
        return;
      }

      const pageKey = `${normalizedUrl.origin}${normalizedUrl.pathname}`;
      if (seenPageKeys.has(pageKey)) {
        return;
      }
      seenPageKeys.add(pageKey);

      uniqueSources.push({
        ...source,
        displayUrl: normalizedUrl.href,
        displayLabel: this.deriveSourceLabel(source),
      });
    });

    return uniqueSources.slice(0, this.maxRenderedSources);
  }

  formatMarkdown(text) {
    const content = String(text || '');
    if (this.markdownRenderer) {
      return this.markdownRenderer.render(content);
    }

    return content
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/\n/g, '<br>');
  }

  createActionIcon(name) {
    const icons = {
      newChat: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 5H7.75A2.75 2.75 0 0 0 5 7.75v8.5A2.75 2.75 0 0 0 7.75 19h8.5A2.75 2.75 0 0 0 19 16.25V12"/><path d="M16.5 4.5h3v3"/><path d="m11.5 12.5 7.25-7.25"/></svg>',
      copy: '<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="8" y="8" width="10" height="12" rx="2"/><path d="M6 16H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h8a2 2 0 0 1 2 2v1"/></svg>',
      good: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M7 10v10"/><path d="M15 6.5 14 10h4.75a2 2 0 0 1 1.95 2.45l-1.4 6A2 2 0 0 1 17.35 20H7"/><path d="M7 10H4.75A1.75 1.75 0 0 0 3 11.75v6.5A1.75 1.75 0 0 0 4.75 20H7"/><path d="M14 10V5.75A1.75 1.75 0 0 0 12.25 4h-.1a1 1 0 0 0-.9.55L8 11"/></svg>',
      bad: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M7 14V4"/><path d="m15 17.5-1-3.5h4.75a2 2 0 0 0 1.95-2.45l-1.4-6A2 2 0 0 0 17.35 4H7"/><path d="M7 14H4.75A1.75 1.75 0 0 1 3 12.25v-6.5A1.75 1.75 0 0 1 4.75 4H7"/><path d="M14 14v4.25A1.75 1.75 0 0 1 12.25 20h-.1a1 1 0 0 1-.9-.55L8 13"/></svg>',
    };

    const icon = document.createElement('span');
    icon.className = 'ai-qa-action-icon';
    icon.setAttribute('aria-hidden', 'true');
    icon.innerHTML = icons[name] || '';
    return icon;
  }

  createActionButton({ label, icon, className }) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = `ai-qa-action-btn ${className}`;
    button.dataset.defaultLabel = label;
    button.appendChild(this.createActionIcon(icon));

    const labelEl = document.createElement('span');
    labelEl.className = 'ai-qa-action-label';
    labelEl.textContent = label;
    button.appendChild(labelEl);
    return button;
  }

  setActionButtonLabel(button, label) {
    const labelEl = button ? button.querySelector('.ai-qa-action-label') : null;
    if (labelEl) {
      labelEl.textContent = label;
    }
  }

  resetActionButtonLabelSoon(button, delay = 1600) {
    if (!button) {
      return;
    }
    const defaultLabel = button.dataset.defaultLabel || '';
    window.setTimeout(() => this.setActionButtonLabel(button, defaultLabel), delay);
  }

  createAnswerActions(messageId, selectedRating = null, mode = 'full') {
    const actionBar = document.createElement('div');
    actionBar.className = mode === 'copy-only' ? 'ai-qa-action-bar is-copy-only' : 'ai-qa-action-bar';
    actionBar.dataset.messageId = messageId;

    const leftGroup = document.createElement('div');
    leftGroup.className = 'ai-qa-action-group ai-qa-action-group-primary';

    if (mode === 'full') {
      const newChatButton = this.createActionButton({
        label: '新建会话',
        icon: 'newChat',
        className: 'ai-qa-action-new-chat',
      });
      newChatButton.addEventListener('click', event => {
        event.preventDefault();
        event.stopPropagation();
        this.startNewChat();
      });
      leftGroup.appendChild(newChatButton);
    }

    const copyButton = this.createActionButton({
      label: '复制',
      icon: 'copy',
      className: 'ai-qa-action-copy',
    });
    copyButton.addEventListener('click', event => {
      event.preventDefault();
      event.stopPropagation();
      this.copyAssistantAnswer(messageId, copyButton);
    });
    leftGroup.appendChild(copyButton);
    actionBar.appendChild(leftGroup);

    if (mode === 'copy-only') {
      return actionBar;
    }

    const rightGroup = document.createElement('div');
    rightGroup.className = 'ai-qa-action-group ai-qa-action-group-feedback';

    const feedbackActions = [
      { value: 'good', label: '优', icon: 'good', className: 'ai-qa-rating-good' },
      { value: 'poor', label: '差', icon: 'bad', className: 'ai-qa-rating-poor' },
    ];

    feedbackActions.forEach(({ value, label, icon, className }) => {
      const button = this.createActionButton({
        label,
        icon,
        className: `ai-qa-feedback-btn ${className}`,
      });
      button.dataset.rating = value;
      if (selectedRating === value) {
        button.classList.add('selected');
      }
      button.addEventListener('click', event => {
        event.preventDefault();
        event.stopPropagation();
        this.submitRating(value, actionBar);
      });
      rightGroup.appendChild(button);
    });

    const status = document.createElement('span');
    status.className = 'ai-qa-action-status';
    status.setAttribute('role', 'status');
    status.setAttribute('aria-live', 'polite');

    if (['good', 'poor'].includes(selectedRating)) {
      const buttons = rightGroup.querySelectorAll('.ai-qa-feedback-btn');
      buttons.forEach(button => { button.disabled = true; });
    }

    actionBar.appendChild(status);
    actionBar.appendChild(rightGroup);
    return actionBar;
  }

  startNewChat() {
    Array.from(this.streamFlushHandles.keys()).forEach(messageId => this.clearStreamState(messageId));
    this.messages = [];
    this.persistMessages();
    this.saveSessionId('');
    this.updateStatus('');
    this.isLoading = false;
    this.setSendButtonState(true);
    this.renderWelcomeState(document.getElementById('ai-qa-messages'));
    this.focusInputSoon();
  }

  async copyAssistantAnswer(messageId, button) {
    const message = this.messages.find(item => item.id === messageId && item.role === 'assistant');
    const answerText = message && typeof message.content === 'string' ? message.content.trim() : '';
    if (!answerText) {
      this.setActionButtonLabel(button, 'Copy failed');
      this.resetActionButtonLabelSoon(button);
      return;
    }

    try {
      await this.copyTextToClipboard(answerText);
      this.setActionButtonLabel(button, 'Copied');
    } catch (error) {
      console.error('复制回答失败:', error);
      this.setActionButtonLabel(button, 'Copy failed');
    } finally {
      this.resetActionButtonLabelSoon(button);
    }
  }

  async copyTextToClipboard(text) {
    const content = String(text || '');
    if (!content) {
      throw new Error('empty copy content');
    }

    if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
      await navigator.clipboard.writeText(content);
      return;
    }

    const textarea = document.createElement('textarea');
    textarea.value = content;
    textarea.setAttribute('readonly', '');
    textarea.style.position = 'fixed';
    textarea.style.top = '-1000px';
    document.body.appendChild(textarea);
    textarea.select();
    const copied = document.execCommand('copy');
    textarea.remove();
    if (!copied) {
      throw new Error('copy command failed');
    }
  }

  createCodeCopyButton(codeEl) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'ai-qa-code-copy';
    button.title = 'Copy code';
    button.setAttribute('aria-label', 'Copy code');
    button.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="8" y="8" width="10" height="12" rx="2"/><path d="M6 16H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h8a2 2 0 0 1 2 2v1"/></svg>';

    button.addEventListener('click', async event => {
      event.preventDefault();
      event.stopPropagation();

      try {
        await this.copyTextToClipboard(codeEl.textContent || '');
        button.classList.remove('is-error');
        button.classList.add('is-copied');
        button.title = 'Copied';
        button.setAttribute('aria-label', 'Copied');
      } catch (error) {
        console.error('复制代码块失败:', error);
        button.classList.remove('is-copied');
        button.classList.add('is-error');
        button.title = 'Copy failed';
        button.setAttribute('aria-label', 'Copy failed');
      } finally {
        window.setTimeout(() => {
          button.classList.remove('is-copied', 'is-error');
          button.title = 'Copy code';
          button.setAttribute('aria-label', 'Copy code');
        }, 1600);
      }
    });

    return button;
  }

  enhanceCodeBlocks(contentEl) {
    if (!contentEl) {
      return;
    }

    const codeBlocks = contentEl.querySelectorAll('pre > code');
    codeBlocks.forEach(codeEl => {
      const pre = codeEl.parentElement;
      if (!pre || pre.parentElement?.classList.contains('ai-qa-code-block')) {
        return;
      }

      const wrapper = document.createElement('div');
      wrapper.className = 'ai-qa-code-block';
      pre.replaceWith(wrapper);
      wrapper.appendChild(pre);
      wrapper.appendChild(this.createCodeCopyButton(codeEl));
    });
  }

  updateMessageRating(messageId, rating) {
    this.messages = this.messages.map(message => {
      if (message.id !== messageId) {
        return message;
      }
      return { ...message, rating };
    });
    this.persistMessages();
  }

  async submitRating(rating, ratingDiv) {
    const buttons = ratingDiv.querySelectorAll('.ai-qa-feedback-btn');
    const status = ratingDiv.querySelector('.ai-qa-action-status');

    if (!this.sessionId) {
      if (status) {
        status.textContent = '当前会话未就绪，请先重新提问后再评分';
      }
      return;
    }

    buttons.forEach(btn => { btn.disabled = true; });

    try {
      const response = await fetch(this.rateEndpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          session_id: this.sessionId,
          rating,
        }),
      });

      if (!response.ok) {
        throw new Error(`评分请求失败: ${response.status}`);
      }

      buttons.forEach(btn => {
        if (btn.dataset.rating === rating) {
          btn.classList.add('selected');
        }
      });

      this.updateMessageRating(ratingDiv.dataset.messageId, rating);

      if (status) {
        status.textContent = '';
      }
    } catch (error) {
      console.error('提交评分失败:', error);
      buttons.forEach(btn => { btn.disabled = false; });
      if (status) {
        status.textContent = '评分失败，请重试';
      }
    }
  }

  scrollToBottom() {
    const messagesEl = document.getElementById('ai-qa-messages');
    if (messagesEl) {
      messagesEl.scrollTop = messagesEl.scrollHeight;
    }
  }

  updateStatus(status) {
    const statusEl = document.getElementById('ai-qa-status');
    if (statusEl) {
      statusEl.textContent = status;
    }
  }

  setSendButtonState(enabled) {
    const sendBtn = document.getElementById('ai-qa-send');
    if (sendBtn) {
      sendBtn.disabled = !enabled;
      sendBtn.classList.toggle('disabled', !enabled);
    }
  }
}

document.addEventListener('DOMContentLoaded', () => {
  const bootstrap = async () => {
    const config = window.AI_ASSISTANT_CONFIG || {
      apiEndpoint: 'http://127.0.0.1:8001/api/v1/ask',
    };

    if (!shouldEnableAssistant(config)) {
      toggleAssistantVisibility(false);
      return;
    }

    const widget = await ensureLatestAssistantWidgetTemplate(config);
    if (!widget) {
      return;
    }

    toggleAssistantVisibility(true);
    window.aiAssistant = new AIDocumentAssistant(config);
  };

  bootstrap().catch(error => {
    console.error('初始化 AI 助手失败:', error);
  });
});
