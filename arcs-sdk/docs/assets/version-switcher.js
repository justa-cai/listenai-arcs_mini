/**
 * ARCS SDK 文档版本切换器
 * 从 OSS JSON 文件动态加载版本列表
 */

(function() {
    'use strict';

    // JSON 文件配置
    const VERSIONS_JSON_CONFIG = {
        // 主 URL（直接访问 OSS）
        jsonUrl: 'https://docs2.listenai.com/arcs-sdk/arcs-sdk-versions.json',
        // 请求超时时间（毫秒）
        timeout: 5000
    };

    // 缓存配置（用于跨页面共享版本列表）
    const CACHE_CONFIG = {
        key: 'arcs_sdk_versions_cache',
        duration: 10 * 60 * 1000  // 10分钟缓存，避免频繁请求
    };

    /**
     * 创建超时 AbortSignal（兼容不支持 AbortSignal.timeout 的浏览器）
     */
    function createTimeoutSignal(timeout) {
        if (AbortSignal.timeout) {
            return AbortSignal.timeout(timeout);
        }
        
        // 降级方案：使用 AbortController
        const controller = new AbortController();
        setTimeout(() => controller.abort(), timeout);
        return controller.signal;
    }

    /**
     * 从缓存获取版本列表
     */
    function getCachedVersions() {
        try {
            const cached = sessionStorage.getItem(CACHE_CONFIG.key);
            if (cached) {
                const { data, timestamp } = JSON.parse(cached);
                // 检查缓存是否过期
                if (Date.now() - timestamp < CACHE_CONFIG.duration) {
                    return data;
                } else {
                    // 缓存过期，清除
                    sessionStorage.removeItem(CACHE_CONFIG.key);
                }
            }
        } catch (e) {
            console.warn('Failed to read cached versions:', e);
        }
        return null;
    }

    /**
     * 保存版本列表到缓存
     */
    function setCachedVersions(versions) {
        try {
            sessionStorage.setItem(CACHE_CONFIG.key, JSON.stringify({
                data: versions,
                timestamp: Date.now()
            }));
        } catch (e) {
            console.warn('Failed to cache versions:', e);
        }
    }

    /**
     * 从 OSS JSON 文件加载版本列表
     * @param {boolean} forceRefresh - 是否强制刷新（忽略缓存）
     */
    async function loadVersionsFromJSON(forceRefresh = false) {
        // 如果不是强制刷新，先检查缓存
        if (!forceRefresh) {
            const cached = getCachedVersions();
            if (cached) {
                console.log('Using cached version list');
                return cached;
            }
        }

        // 缓存不存在或已过期，请求服务器
        try {
            const response = await fetch(VERSIONS_JSON_CONFIG.jsonUrl, {
                method: 'GET',
                headers: {
                    'Accept': 'application/json'
                },
                signal: createTimeoutSignal(VERSIONS_JSON_CONFIG.timeout)
            });
            
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            
            const data = await response.json();
            
            // 验证 JSON 格式
            if (!data || !Array.isArray(data.versions)) {
                throw new Error('Invalid JSON format: versions array not found');
            }
            
            // 保存到缓存（跨页面共享）
            setCachedVersions(data.versions);
            
            return data.versions;
        } catch (error) {
            console.error('Failed to load versions from JSON:', error);
            
            // 请求失败时，尝试使用缓存（即使可能过期）
            const cached = getCachedVersions();
            if (cached) {
                console.warn('Using stale cached version list due to fetch error');
                return cached;
            }
            
            return null;
        }
    }

    /**
     * 从 URL 检测当前版本
     */
    function detectCurrentVersionFromURL() {
        const pathParts = window.location.pathname.split('/');
        let version = 'latest'; // 默认版本

        // 查找版本模式（v*.*.* 或 latest）
        for (const part of pathParts) {
            if (part === 'latest' || /^v\d+\.\d+\.\d+$/.test(part)) {
                version = part;
                break;
            }
        }

        return version;
    }

    /**
     * 更新当前版本标签显示
     */
    function updateVersionLabel(currentVersion) {
        const versionLabel = document.querySelector('.current-version-label');
        if (versionLabel) {
            versionLabel.textContent = currentVersion;
        }
    }

    /**
     * 动态渲染版本列表
     */
    function renderVersionList(versions, currentVersion) {
        const versionContainer = document.querySelector('.rst-other-versions dl');
        if (!versionContainer) {
            console.warn('Version container not found');
            return;
        }
        
        // 清空现有版本列表（保留 dt 标签）
        const dt = versionContainer.querySelector('dt');
        versionContainer.innerHTML = '';
        if (dt) {
            versionContainer.appendChild(dt);
        }
        
        // 渲染版本列表
        versions.forEach(version => {
            const dd = document.createElement('dd');
            if (version.name === currentVersion) {
                const strong = document.createElement('strong');
                strong.textContent = version.label || version.name;
                dd.appendChild(strong);
            } else {
                const link = document.createElement('a');
                link.href = version.url;
                link.textContent = version.label || version.name;
                dd.appendChild(link);
            }
            versionContainer.appendChild(dd);
        });
    }

    /**
     * 使用降级方案：从页面已有的版本列表读取
     */
    function useFallbackVersionList(currentVersion) {
        const existingVersions = document.querySelectorAll('.rst-other-versions dd');
        if (existingVersions.length > 0) {
            // 已有版本列表，只更新当前版本高亮
            updateVersionList(currentVersion);
            return;
        }
        
        // 如果页面也没有版本列表，使用最小默认列表
        const defaultVersions = [
            { 
                name: 'latest', 
                url: 'https://docs2.listenai.com/arcs-sdk/latest/zh/html/index.html', 
                label: 'latest' 
            }
        ];
        renderVersionList(defaultVersions, currentVersion);
    }

    /**
     * 更新版本列表的显示（用于降级方案）
     */
    function updateVersionList(currentVersion) {
        const versionItems = document.querySelectorAll('.rst-other-versions dd');
        versionItems.forEach(item => {
            const link = item.querySelector('a');
            const strong = item.querySelector('strong');
            
            let versionName = '';
            if (link) {
                versionName = link.textContent.trim();
            } else if (strong) {
                versionName = strong.textContent.trim();
            }
            
            // 移除现有元素
            if (link) link.remove();
            if (strong) strong.remove();
            
            // 根据是否是当前版本决定显示方式
            if (versionName === currentVersion) {
                const strongEl = document.createElement('strong');
                strongEl.textContent = versionName;
                item.appendChild(strongEl);
            } else {
                // 构造链接 URL
                const targetHref = link ? link.href : 
                    `https://docs2.listenai.com/arcs-sdk/${versionName}/zh/html/index.html`;
                
                const newLink = document.createElement('a');
                newLink.href = targetHref;
                newLink.textContent = versionName;
                item.appendChild(newLink);
            }
        });
    }

    /**
     * 初始化版本切换器交互功能
     */
    function initVersionSwitcherInteractions() {
        const versionSelector = document.querySelector('.rst-versions');
        if (!versionSelector) return;

        // 点击切换显示/隐藏
        const currentVersion = versionSelector.querySelector('.rst-current-version');
        if (currentVersion) {
            currentVersion.addEventListener('click', function(e) {
                e.preventDefault();
                e.stopPropagation();
                versionSelector.classList.toggle('shift-up');
            });
        }

        // 点击外部关闭
        document.addEventListener('click', function(e) {
            if (!versionSelector.contains(e.target)) {
                versionSelector.classList.remove('shift-up');
            }
        });

        // 点击版本链接时更新显示
        document.addEventListener('click', function(e) {
            if (e.target.tagName === 'A' && e.target.closest('.rst-other-versions')) {
                const versionName = e.target.textContent.trim();
                const versionLabel = document.querySelector('.current-version-label');
                if (versionLabel) {
                    versionLabel.textContent = currentVersion;
                }
            }
        });
    }

    /**
     * 主初始化函数
     */
    async function init() {
        // 1. 从 URL 检测当前版本
        const currentVersion = detectCurrentVersionFromURL();
        
        // 2. 更新当前版本标签
        updateVersionLabel(currentVersion);
        
        // 3. 初始化交互功能（不阻塞版本列表加载）
        initVersionSwitcherInteractions();
        
        // 4. 从 JSON 文件加载版本列表（异步，不阻塞页面渲染）
        // 优先使用缓存，缓存不存在或过期时才请求
        try {
            const versions = await loadVersionsFromJSON();
            
            if (versions && versions.length > 0) {
                // 5. 动态渲染版本列表
                renderVersionList(versions, currentVersion);
                console.log(`Version switcher initialized with ${versions.length} versions`);
            } else {
                // 6. JSON 加载失败时，使用降级方案
                console.warn('Using fallback version list');
                useFallbackVersionList(currentVersion);
            }
        } catch (error) {
            console.error('Error initializing version switcher:', error);
            useFallbackVersionList(currentVersion);
        }
    }

    // 页面加载完成后初始化
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        // DOM 已经加载完成
        init();
    }
})();
