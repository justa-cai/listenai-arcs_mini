/**
 * ARCS SDK 文档反馈组件
 * 用于收集用户反馈并提交到 SeaTable
 */

(function() {
    'use strict';

    // SeaTable 表单提交配置（可通过 window.SEATABLE_FEEDBACK_CONFIG 覆盖）
    const DEFAULT_CONFIG = {
        formId: '6bb54f8a-2740-4bf5-a63e-836c37ec9406',
        // 使用相对路径，通过 Nginx 代理到 SeaTable
        formSubmitUrl: '/arcs-docs/api/v2.1/form-submit/6bb54f8a-2740-4bf5-a63e-836c37ec9406/',
        uploadLinkUrl: '/arcs-docs/api/v2.1/forms/6bb54f8a-2740-4bf5-a63e-836c37ec9406/upload-link/?upload_type=image',
        tableId: '0000',
        // 保留完整URL作为备用
        directFormSubmitUrl: 'https://inner-table.listenai.com/api/v2.1/form-submit/6bb54f8a-2740-4bf5-a63e-836c37ec9406/',
        directUploadLinkUrl: 'https://inner-table.listenai.com/api/v2.1/forms/6bb54f8a-2740-4bf5-a63e-836c37ec9406/upload-link/?upload_type=image'
    };
    
    // 合并外部配置
    const SEATABLE_CONFIG = Object.assign({}, DEFAULT_CONFIG, window.SEATABLE_FEEDBACK_CONFIG || {});

    // 获取当前文档信息
    function getDocumentInfo() {
        // 获取文档版本
        const versionLabel = document.querySelector('.current-version-label');
        const version = versionLabel ? versionLabel.textContent.trim() : 'latest';

        // 获取文档路径/面包屑导航
        let pagePath = '';
        const breadcrumbs = document.querySelectorAll('.wy-breadcrumbs li');
        if (breadcrumbs.length > 0) {
            const pathParts = [];
            breadcrumbs.forEach((crumb, index) => {
                // 跳过最后的 "Edit on GitHub" 等链接
                if (index < breadcrumbs.length - 1) {
                    const text = crumb.textContent.trim();
                    if (text && text !== '»') {
                        pathParts.push(text);
                    }
                }
            });
            pagePath = pathParts.join(' / ');
        }

        // 如果没有面包屑，尝试从标题获取
        if (!pagePath) {
            const title = document.querySelector('h1');
            pagePath = title ? title.textContent.trim() : document.title;
        }

        // 获取完整 URL
        const pageUrl = window.location.href;

        return {
            version: version,
            pagePath: pagePath,
            pageUrl: pageUrl
        };
    }

    // 上传图片到 SeaTable
    async function uploadImagesToSeaTable(images) {
        const uploadedUrls = [];
        
        for (const imageData of images) {
            try {
                console.log('开始上传图片:', imageData.file.name);
                // 尝试上传单张图片
                const imageUrl = await uploadSingleImage(imageData.file);
                if (imageUrl) {
                    uploadedUrls.push(imageUrl);
                    console.log('✓ 图片上传成功:', imageUrl);
                } else {
                    // 上传失败，记录图片信息作为占位符
                    const fileInfo = `图片: ${imageData.file.name} (${(imageData.file.size / 1024).toFixed(1)}KB)`;
                    uploadedUrls.push(fileInfo);
                    console.log('✗ 图片上传失败，记录文件信息:', fileInfo);
                }
            } catch (error) {
                console.error('图片上传异常:', error);
                uploadedUrls.push(`图片: ${imageData.file.name}`);
            }
        }
        
        return uploadedUrls;
    }

    // 上传单张图片
    async function uploadSingleImage(file) {
        try {
            console.log('→ 步骤1: 获取上传凭证');
            
            // 步骤1: GET 请求获取上传凭证（简化请求避免 CORS preflight）
            const credentialResponse = await fetch(SEATABLE_CONFIG.uploadLinkUrl, {
                method: 'GET',
                credentials: 'include', // 包含 cookies
                mode: 'cors' // 明确指定 CORS 模式
            });

            if (!credentialResponse.ok) {
                console.error('获取上传凭证失败:', credentialResponse.status);
                return null;
            }

            const credential = await credentialResponse.json();
            console.log('获取到凭证:', credential);

            if (!credential.upload_link || !credential.parent_path) {
                console.error('凭证数据不完整');
                return null;
            }

            // 步骤2: 上传图片到 upload_link
            console.log('→ 步骤2: 上传图片文件');
            
            const uploadFormData = new FormData();
            uploadFormData.append('parent_dir', credential.parent_path);
            uploadFormData.append('file', file);

            // 如果 upload_link 是完整URL，需要转换为代理路径
            let uploadUrl = credential.upload_link;
            if (uploadUrl.startsWith('https://inner-table.listenai.com')) {
                uploadUrl = uploadUrl.replace('https://inner-table.listenai.com', '/arcs-docs');
            }
            
            const uploadResponse = await fetch(uploadUrl + '?ret-json=1', {
                method: 'POST',
                body: uploadFormData,
                credentials: 'include',
                mode: 'cors'
            });

            if (!uploadResponse.ok) {
                console.error('上传图片失败:', uploadResponse.status);
                return null;
            }

            const uploadResult = await uploadResponse.json();
            console.log('上传结果:', uploadResult);

            // uploadResult 是一个数组
            if (!Array.isArray(uploadResult) || uploadResult.length === 0) {
                console.error('上传结果格式错误');
                return null;
            }

            const uploadedFile = uploadResult[0];
            const fileName = uploadedFile.name;

            // 步骤3: 构建最终图片URL
            // 注意：提交给SeaTable的URL必须是完整URL，这样SeaTable后台才能正确显示图片
            const imageUrl = `https://inner-table.listenai.com/workspace/24${credential.parent_path}/${fileName}`;
            console.log('✓ 图片URL:', imageUrl);

            return imageUrl;

        } catch (error) {
            console.error('上传图片过程出错:', error);
            
            // 如果是跨域错误，提示用户
            if (error.name === 'TypeError' && error.message.includes('fetch')) {
                console.warn('⚠️ 图片上传遇到跨域限制');
                console.warn('💡 提示：文档需要部署在 inner-table.listenai.com 同域下才能上传图片');
            }
            
            return null;
        }
    }

    // 提交反馈到 SeaTable
    async function submitFeedback(feedbackData) {
        const docInfo = getDocumentInfo();

        // 准备个人信息字段（合并姓名和邮箱）
        let personalInfo = '';
        if (feedbackData.name && feedbackData.email) {
            personalInfo = `${feedbackData.name}（${feedbackData.email}）`;
        } else if (feedbackData.name) {
            personalInfo = feedbackData.name;
        } else if (feedbackData.email) {
            personalInfo = feedbackData.email;
        }

        // 上传图片并获取URL
        let imageUrls = [];
        if (feedbackData.images && feedbackData.images.length > 0) {
            console.log('开始上传', feedbackData.images.length, '张图片...');
            imageUrls = await uploadImagesToSeaTable(feedbackData.images);
        }

        // 准备提交的数据，按照 SeaTable 表单要求的格式
        const now = new Date();
        const submitDateTime = now.getFullYear() + '-' + 
            String(now.getMonth() + 1).padStart(2, '0') + '-' + 
            String(now.getDate()).padStart(2, '0') + ' ' + 
            String(now.getHours()).padStart(2, '0') + ':' + 
            String(now.getMinutes()).padStart(2, '0') + ':' + 
            String(now.getSeconds()).padStart(2, '0');
        
        const rowData = {
            '文档目录': docInfo.pagePath || '未知页面',
            '提交日期': submitDateTime, // YYYY-MM-DD HH:MM:SS 格式
            '文档版本': docInfo.version || 'latest',
            '问题描述': feedbackData.feedback || (imageUrls.length > 0 ? '[见贴图]' : ''),
            '个人信息': personalInfo,
            '页面URL': docInfo.pageUrl
        };

        // 如果有图片URL，添加到 row_data
        if (imageUrls.length > 0) {
            rowData['问题贴图'] = imageUrls;
        }

        // 使用表单提交方式
        return submitViaForm(rowData, feedbackData.images);
    }

    // 通过 SeaTable 表单 API 提交
    async function submitViaForm(rowData, images) {
        return new Promise((resolve) => {
            try {
                console.log('提交反馈数据:', {
                    table_id: SEATABLE_CONFIG.tableId,
                    row_data: rowData
                });

                // 创建隐藏的 form 元素
                const form = document.createElement('form');
                form.method = 'POST';
                form.action = SEATABLE_CONFIG.formSubmitUrl;
                form.target = 'feedback-submit-frame';
                form.style.display = 'none';
                form.enctype = 'multipart/form-data';

                // 添加 table_id 字段
                const tableIdInput = document.createElement('input');
                tableIdInput.type = 'hidden';
                tableIdInput.name = 'table_id';
                tableIdInput.value = SEATABLE_CONFIG.tableId;
                form.appendChild(tableIdInput);

                // 添加 row_data 字段
                const rowDataInput = document.createElement('input');
                rowDataInput.type = 'hidden';
                rowDataInput.name = 'row_data';
                rowDataInput.value = JSON.stringify(rowData);
                form.appendChild(rowDataInput);

                // 创建或获取 iframe
                let iframe = document.getElementById('feedback-submit-frame');
                if (!iframe) {
                    iframe = document.createElement('iframe');
                    iframe.id = 'feedback-submit-frame';
                    iframe.name = 'feedback-submit-frame';
                    iframe.style.display = 'none';
                    document.body.appendChild(iframe);
                }

                // 提交表单
                document.body.appendChild(form);
                form.submit();

                setTimeout(() => {
                    document.body.removeChild(form);
                    console.log('反馈已提交');
                    resolve({ success: true });
                }, 800);

            } catch (error) {
                console.error('提交失败:', error);
                resolve({ success: true });
            }
        });
    }

    // 初始化反馈组件
    function initFeedbackWidget() {
        // 创建反馈按钮 HTML
        const feedbackHTML = `
            <button class="feedback-button" id="feedbackButton" title="提交反馈">
                提交反馈
            </button>

            <div class="feedback-modal-overlay" id="feedbackModal">
                <div class="feedback-modal">
                    <button class="feedback-modal-close" id="feedbackClose">×</button>
                    
                    <div class="feedback-message" id="feedbackMessage"></div>
                    
                    <form class="feedback-form" id="feedbackForm">
                        <div class="feedback-form-group">
                            <label for="feedbackContent">问题描述（支持粘贴图片）：</label>
                            <div 
                                id="feedbackContent" 
                                class="feedback-editor"
                                contenteditable="true"
                                data-placeholder="请描述您遇到的问题或建议"
                            ></div>
                            <input type="hidden" name="feedback" id="feedbackText">
                        </div>
                 
                        <div class="feedback-contact-info">
                            <p>如果希望我们联系您，请留下信息</p>
                            <div class="feedback-contact-fields">
                                <div class="feedback-form-group">
                                    <label for="feedbackName">姓名</label>
                                    <input 
                                        type="text" 
                                        id="feedbackName" 
                                        name="name"
                                        placeholder="您的姓名"
                                    />
                                </div>
                                <div class="feedback-form-group">
                                    <label for="feedbackEmail">邮箱</label>
                                    <input 
                                        type="email" 
                                        id="feedbackEmail" 
                                        name="email"
                                        placeholder="您的邮箱"
                                    />
                                </div>
                            </div>
                        </div>

                        <button type="submit" class="feedback-submit-btn">提交</button>
                    </form>
                </div>
            </div>
        `;

        // 将 HTML 插入到页面
        document.body.insertAdjacentHTML('beforeend', feedbackHTML);

        // 获取元素
        const feedbackButton = document.getElementById('feedbackButton');
        const feedbackModal = document.getElementById('feedbackModal');
        const feedbackClose = document.getElementById('feedbackClose');
        const feedbackForm = document.getElementById('feedbackForm');
        const feedbackMessage = document.getElementById('feedbackMessage');
        const feedbackEditor = document.getElementById('feedbackContent');
        const feedbackTextInput = document.getElementById('feedbackText');
        
        // 存储图片数据
        let uploadedImages = [];

        // 显示消息
        function showMessage(message, type) {
            feedbackMessage.textContent = message;
            feedbackMessage.className = `feedback-message ${type}`;
            setTimeout(() => {
                feedbackMessage.className = 'feedback-message';
            }, 5000);
        }

        // 打开弹窗
        feedbackButton.addEventListener('click', function() {
            feedbackModal.classList.add('active');
            document.body.style.overflow = 'hidden';
        });

        // 关闭弹窗
        function closeModal() {
            feedbackModal.classList.remove('active');
            document.body.style.overflow = '';
            feedbackForm.reset();
            feedbackEditor.innerHTML = '';
            uploadedImages = [];
            feedbackMessage.className = 'feedback-message';
        }

        feedbackClose.addEventListener('click', closeModal);

        // 点击遮罩关闭
        feedbackModal.addEventListener('click', function(e) {
            if (e.target === feedbackModal) {
                closeModal();
            }
        });

        // ESC 键关闭
        document.addEventListener('keydown', function(e) {
            if (e.key === 'Escape' && feedbackModal.classList.contains('active')) {
                closeModal();
            }
        });

        // 处理粘贴事件
        feedbackEditor.addEventListener('paste', async function(e) {
            e.preventDefault();
            
            const items = e.clipboardData.items;
            let hasImage = false;
            let textContent = '';

            for (let item of items) {
                if (item.type.indexOf('image') !== -1) {
                    hasImage = true;
                    const file = item.getAsFile();
                    await handleImagePaste(file);
                } else if (item.type === 'text/plain') {
                    textContent = e.clipboardData.getData('text/plain');
                }
            }

            // 如果只有文字，插入文字
            if (!hasImage && textContent) {
                document.execCommand('insertText', false, textContent);
            }
        });

        // 处理图片粘贴
        async function handleImagePaste(file) {
            // 创建图片预览
            const reader = new FileReader();
            reader.onload = function(e) {
                const img = document.createElement('img');
                img.src = e.target.result;
                img.style.maxWidth = '100%';
                img.style.margin = '10px 0';
                img.style.borderRadius = '4px';
                img.classList.add('pasted-image');
                
                // 插入图片到编辑器
                feedbackEditor.appendChild(img);
                
                // 保存图片数据
                uploadedImages.push({
                    file: file,
                    dataUrl: e.target.result,
                    element: img
                });

                console.log('图片已粘贴，共', uploadedImages.length, '张图片');
            };
            reader.readAsDataURL(file);
        }

        // 提取编辑器内容
        function extractEditorContent() {
            const text = feedbackEditor.innerText.trim();
            return {
                text: text,
                images: uploadedImages
            };
        }

        // 表单提交
        feedbackForm.addEventListener('submit', async function(e) {
            e.preventDefault();

            const content = extractEditorContent();
            const formData = new FormData(feedbackForm);
            
            const feedbackData = {
                feedback: content.text,
                images: content.images,
                name: formData.get('name'),
                email: formData.get('email')
            };

            // 验证反馈内容：至少要有文字或图片
            const hasText = feedbackData.feedback && feedbackData.feedback.trim() !== '';
            const hasImages = feedbackData.images && feedbackData.images.length > 0;
            
            if (!hasText && !hasImages) {
                showMessage('请输入反馈内容或粘贴图片', 'error');
                return;
            }

            // 禁用提交按钮
            const submitBtn = feedbackForm.querySelector('.feedback-submit-btn');
            submitBtn.disabled = true;
            submitBtn.textContent = '提交中...';

            try {
                const result = await submitFeedback(feedbackData);
                
                if (result.success) {
                    showMessage('感谢您的反馈！我们已收到您的意见。', 'success');
                    feedbackForm.reset();
                    
                    // 3秒后关闭弹窗
                    setTimeout(() => {
                        closeModal();
                    }, 3000);
                } else {
                    showMessage('提交失败，请稍后重试。', 'error');
                }
            } catch (error) {
                console.error('提交反馈失败:', error);
                showMessage('提交失败，请稍后重试。', 'error');
            } finally {
                submitBtn.disabled = false;
                submitBtn.textContent = '提交';
            }
        });

        console.log('反馈组件已初始化');
    }

    // 页面加载完成后初始化
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initFeedbackWidget);
    } else {
        initFeedbackWidget();
    }
})();

