// Version switcher toggle functionality
document.addEventListener('DOMContentLoaded', function() {
    var versionSelector = document.querySelector('.rst-versions');
    if (versionSelector) {
        var currentVersion = versionSelector.querySelector('.rst-current-version');
        if (currentVersion) {
            currentVersion.addEventListener('click', function(e) {
                e.preventDefault();
                e.stopPropagation();
                versionSelector.classList.toggle('shift-up');
            });
        }

        // Close when clicking outside
        document.addEventListener('click', function(e) {
            if (!versionSelector.contains(e.target)) {
                versionSelector.classList.remove('shift-up');
            }
        });
    }

    // Update displayed version when a version link is clicked
    document.addEventListener('click', function(e) {
        if (e.target.tagName === 'A' && e.target.closest('.rst-other-versions')) {
            const versionName = e.target.textContent;
            const versionLabel = document.querySelector('.current-version-label');
            if (versionLabel) {
                versionLabel.textContent = versionName;
            }
        }
    }); 

    // Detect current version from URL and update label on page load
    function updateVersionFromURL() {
        const versionLabel = document.querySelector('.current-version-label');
        if (!versionLabel) return;

        const pathParts = window.location.pathname.split('/');
        let version = 'latest'; // default version

        // Look for version pattern in URL path
        for (const part of pathParts) {
            if (part.startsWith('v') || part === 'latest') {
                version = part;
                break;
            }
        }

        versionLabel.textContent = version;

        // 更新版本列表中各项的显示方式（strong 或 a 标签）
        updateVersionList(version);
    }
    
    // 根据当前版本更新版本列表的显示
    function updateVersionList(currentVersion) {
        const versionItems = document.querySelectorAll('.rst-other-versions dd');
        versionItems.forEach(item => {
            const link = item.querySelector('a');
            const strong = item.querySelector('strong');
            
            // 获取版本名称
            let versionName = '';
            if (link) {
                versionName = link.textContent;
            } else if (strong) {
                versionName = strong.textContent;
            }
            
            // 移除现有元素
            if (link) {
                link.remove();
            }
            if (strong) {
                strong.remove();
            }
            
            // 根据是否是当前版本决定显示方式
            if (versionName === currentVersion) {
                const strongEl = document.createElement('strong');
                strongEl.textContent = versionName;
                item.appendChild(strongEl);
            } else {
                // 找到对应版本的链接地址
                const allLinks = Array.from(document.querySelectorAll('.rst-other-versions dd a, .rst-other-versions dd strong'));
                let targetHref = '';
                
                // 尝试从现有链接中找到目标URL
                for (const el of allLinks) {
                    if (el.textContent === versionName) {
                        // 找到包含相同版本名的父级dd元素中的链接
                        const parentDD = el.closest('dd');
                        const parentLink = parentDD.querySelector('a');
                        if (parentLink) {
                            targetHref = parentLink.href;
                            break;
                        }
                    }
                }
                
                // 如果没找到链接，则从当前点击的链接或其他地方构造
                if (!targetHref) {
                    targetHref = `https://docs2.listenai.com/arcs-sdk/${versionName}/zh/html/index.html`
                }
                
                if (targetHref) {
                    const newLink = document.createElement('a');
                    newLink.href = targetHref;
                    newLink.textContent = versionName;
                    item.appendChild(newLink);
                } else {
                    // 如果找不到链接，至少显示版本名称
                    item.textContent = versionName;
                }
            }
        });
    }

    // Update version label on page load
    updateVersionFromURL();
    
    console.log('Version switcher initialized');
});