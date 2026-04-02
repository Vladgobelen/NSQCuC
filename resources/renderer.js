function showError(message) {
    console.error("[NSQCuT] Error:", message);
    if (typeof alert !== 'undefined') {
        alert('Ошибка: ' + message);
    }
}

let isInstalling = false;
let isVoiceChatActive = false;
let isMicMuted = false;
let mediaStream = null;
const VOICE_CHAT_URL = "https://ns.fiber-gate.ru";

function waitForBackend(timeout = 5000) {
    return new Promise((resolve, reject) => {
        if (window.backend) {
            console.log('[NSQCuT] Backend already available');
            resolve();
            return;
        }
        const startTime = Date.now();
        const checkInterval = setInterval(() => {
            if (window.backend) {
                clearInterval(checkInterval);
                console.log('[NSQCuT] Backend became available');
                resolve();
            } else if (Date.now() - startTime > timeout) {
                clearInterval(checkInterval);
                reject(new Error('Backend not ready after ' + timeout + 'ms'));
            }
        }, 50);
    });
}

async function checkMediaSupport() {
    try {
        if (!navigator.mediaDevices) {
            console.warn('[NSQCuT] mediaDevices not available');
            return false;
        }
        const devices = await navigator.mediaDevices.enumerateDevices();
        console.log('[NSQCuT] Available devices:', devices);
        const hasAudioInput = devices.some(d => d.kind === 'audioinput');
        console.log('[NSQCuT] Has audio input:', hasAudioInput);
        if (!hasAudioInput) {
            console.warn('[NSQCuT] No audio input detected');
            showError('Нет микрофона! Проверьте подключения.');
            return false;
        }
        return true;
    } catch (error) {
        console.error('[NSQCuT] Media check error:', error);
        showError('Ошибка проверки микрофона: ' + error.message);
        return false;
    }
}

async function requestMediaPermissions() {
    try {
        if (!navigator.mediaDevices) {
            console.warn('[NSQCuT] mediaDevices not available');
            return false;
        }
        console.log('[NSQCuT] Requesting media permissions...');
        
        // 🔥 ПРАВИЛЬНЫЕ CONSTRAINTS ДЛЯ МИКРОФОНА
        const constraints = {
            audio: {
                echoCancellation: true,
                noiseSuppression: true,
                autoGainControl: true,
                sampleRate: 48000,
                sampleSize: 16,
                channelCount: 1
            },
            video: false
        };
        
        if (mediaStream) {
            mediaStream.getTracks().forEach(track => track.stop());
            mediaStream = null;
        }
        
        const stream = await navigator.mediaDevices.getUserMedia(constraints);
        mediaStream = stream;
        console.log('[NSQCuT] Media permissions granted, stream:', stream);
        
        const audioTrack = stream.getAudioTracks()[0];
        if (audioTrack) {
            const settings = audioTrack.getSettings();
            console.log('[NSQCuT] Audio track settings:', settings);
        }
        
        return true;
    } catch (mediaError) {
        console.error('[NSQCuT] getUserMedia failed:', mediaError);
        console.error('[NSQCuT] Error name:', mediaError.name);
        console.error('[NSQCuT] Error message:', mediaError.message);
        
        if (mediaError.name === 'NotAllowedError') {
            showError('НЕТ ДОСТУПА К МИКРОФОНУ! Разрешите доступ в настройках системы.');
        } else if (mediaError.name === 'NotFoundError') {
            showError('Микрофон не найден! Проверьте подключение.');
        } else if (mediaError.name === 'NotReadableError') {
            showError('Микрофон занят другим приложением!');
        } else {
            showError('Ошибка микрофона: ' + mediaError.message);
        }
        
        return false;
    }
}

function disableMicrophone() {
    if (mediaStream) {
        mediaStream.getAudioTracks().forEach(track => {
            track.enabled = false;
            console.log('[NSQCuT] Microphone track disabled');
        });
    }
}

function enableMicrophone() {
    if (mediaStream) {
        mediaStream.getAudioTracks().forEach(track => {
            track.enabled = true;
            console.log('[NSQCuT] Microphone track enabled');
        });
    }
}

document.addEventListener("DOMContentLoaded", async () => {
    console.log("[NSQCuT] DOM loaded");
    const gameStatus = document.getElementById("game-status");
    const launchBtn = document.getElementById("launch-btn");
    const addonsList = document.getElementById("addons-list");
    const logsBtn = document.getElementById("logs-btn");
    const voiceBtn = document.getElementById("voice-btn");
    const changePathBtn = document.getElementById("change-path-btn");
    const voiceChatView = document.getElementById("voice-chat-view");
    const voiceChatFrame = document.getElementById("voice-chat-frame");
    const voiceChatHeader = document.getElementById("voice-chat-header");
    const backToAddonsBtn = document.getElementById("back-to-addons-btn");
    const voiceHoverZone = document.getElementById("voice-hover-zone");
    const voiceMicBtn = document.getElementById("voice-mic-btn");

    async function loadAddons() {
        try {
            await waitForBackend(5000);
            console.log("[NSQCuT] Backend ready, calling loadAddons()");
            const result = await window.backend.loadAddons();
            const addons = Array.isArray(result) ? result : [];
            console.log("[NSQCuT] Addons loaded:", addons.length);
            if (!addons || addons.length === 0) {
                addonsList.innerHTML = '<div style="padding: 20px; text-align: center; color: #f39c12;">Не удалось загрузить список аддонов.</div>';
                return;
            }
            renderAddons(addons);
        } catch (error) {
            console.error("[NSQCuT] Error loading addons:", error);
            showError("Не удалось загрузить список аддонов: " + error);
        }
    }

    window.onBackendReady = function() {
        console.log("[NSQCuT] Backend ready callback triggered");
        loadAddons();
        checkGame();
    };

    function renderAddons(addons) {
        addonsList.innerHTML = "";
        for (let i = 0; i < addons.length; i++) {
            const addon = addons[i];
            const name = addon.name;
            addonsList.appendChild(createAddonElement(name, addon));
        }
    }

    function createAddonElement(name, addon) {
        const card = document.createElement("div");
        card.className = "addon-card";
        card.dataset.name = name;
        const contentWrapper = document.createElement("div");
        contentWrapper.className = "addon-content-wrapper";
        const overlay = document.createElement("div");
        overlay.className = "progress-overlay hidden";
        card.overlay = overlay;
        const topRow = document.createElement("div");
        topRow.className = "addon-top";
        const nameEl = document.createElement("span");
        nameEl.className = "addon-name";
        nameEl.textContent = name;
        const updateLabel = document.createElement("span");
        updateLabel.className = "update-label";
        updateLabel.style.display = addon.needs_update ? "inline" : "none";
        updateLabel.textContent = "Доступно обновление";
        const checkbox = document.createElement("input");
        checkbox.type = "checkbox";
        checkbox.id = 'checkbox-' + name;
        checkbox.checked = addon.installed;
        checkbox.disabled = addon.being_processed || addon.updating || isInstalling;
        const label = document.createElement("label");
        label.htmlFor = 'checkbox-' + name;
        label.className = "custom-checkbox";
        topRow.appendChild(nameEl);
        topRow.appendChild(updateLabel);
        topRow.appendChild(checkbox);
        topRow.appendChild(label);
        const description = document.createElement("div");
        description.className = "addon-description";
        description.textContent = addon.description;
        card.checkbox = checkbox;
        card.updateLabel = updateLabel;
        card.appendChild(overlay);
        contentWrapper.appendChild(topRow);
        contentWrapper.appendChild(description);
        card.appendChild(contentWrapper);
        checkbox.addEventListener("change", () => {
            const willInstall = checkbox.checked;
            checkbox.disabled = true;
            console.log("[NSQCuT] Toggle addon:", name, "install:", willInstall);
            window.backend.toggleAddon(name, willInstall);
        });
        return card;
    }

    function unblockAddonCheckbox(name) {
        const cards = document.querySelectorAll(".addon-card");
        for (const card of cards) {
            if (card.dataset.name === name && card.checkbox) {
                card.checkbox.disabled = false;
                break;
            }
        }
    }

    function updateAddonProgress(name, progress) {
        const cards = document.querySelectorAll(".addon-card");
        for (const card of cards) {
            if (card.dataset.name === name && card.overlay) {
                const overlay = card.overlay;
                overlay.style.setProperty("--progress", Math.min(progress, 1.0) * 100 + "%");
                if (progress > 0 && progress < 1.0) {
                    overlay.classList.remove("hidden");
                    overlay.style.opacity = "1";
                }
                if (progress >= 1.0) {
                    setTimeout(() => {
                        overlay.classList.add("hidden");
                        overlay.style.opacity = "0";
                    }, 300);
                }
                break;
            }
        }
    }

    function refreshAddonStatus(name) {
        window.backend.loadAddons().then((addons) => {
            const addonsArray = Array.isArray(addons) ? addons : [];
            const addon = addonsArray.find(a => a.name === name);
            if (!addon) return;
            document.querySelectorAll(".addon-card").forEach(card => {
                if (card.dataset.name === name) {
                    card.checkbox.disabled = addon.being_processed || addon.updating || isInstalling;
                    card.checkbox.checked = addon.installed;
                    card.updateLabel.style.display = addon.needs_update ? "inline" : "none";
                }
            });
        }).catch((error) => {
            console.error("[NSQCuT] Refresh error:", error);
        });
    }

    async function checkGame() {
        if (!window.backend) return;
        try {
            const result = await window.backend.checkGame();
            const exists = result.exists;
            gameStatus.textContent = exists ? "Готова к запуску" : "Игра не найдена";
            gameStatus.style.color = exists ? "#4CAF50" : "#F44336";
            launchBtn.disabled = !exists || isInstalling || isVoiceChatActive;
        } catch (error) {
            gameStatus.textContent = "Ошибка проверки игры";
            gameStatus.style.color = "#F44336";
            launchBtn.disabled = true;
        }
    }

    async function launchGame() {
        if (isInstalling) {
            showError("Идёт установка аддона, подождите...");
            return;
        }
        if (isVoiceChatActive) {
            showError("Сначала закройте Щебетало");
            return;
        }
        try {
            const result = await window.backend.launchGame();
            if (!result.success) showError("Не удалось запустить игру");
        } catch (error) {
            showError("Не удалось запустить игру: " + error);
        }
    }

    function openLogsFolder() {
        window.backend.openLogsFolder();
    }

    async function changeGamePath() {
        try {
            const newPath = prompt("Введите путь к папке с Wow.exe:");
            if (newPath) {
                window.backend.changeGamePath(newPath);
                window.backend.setGamePath(newPath);
                checkGame();
                loadAddons();
            }
        } catch (error) {
            showError("Не удалось изменить путь: " + error);
        }
    }

    async function openVoiceChat() {
        console.log('[NSQCuT] Opening voice chat');
        
        // 🔥 ПРОВЕРЯЕМ МИКРОФОН ПЕРЕД ОТКРЫТИЕМ
        const hasMedia = await checkMediaSupport();
        if (!hasMedia) {
            console.warn('[NSQCuT] No media support, but opening anyway');
        }
        
        const hasPermission = await requestMediaPermissions();
        if (!hasPermission) {
            console.error('[NSQCuT] No microphone permission!');
            showError('НЕТ ДОСТУПА К МИКРОФОНУ! Проверьте настройки системы.');
        }
        
        isVoiceChatActive = true;
        document.body.classList.add("voice-chat-active");
        if (voiceChatView) voiceChatView.classList.remove("hidden");
        
        if (voiceChatFrame && !voiceChatFrame.dataset.loaded) {
            voiceChatFrame.src = VOICE_CHAT_URL;
            voiceChatFrame.dataset.loaded = "true";
        }
        
        setTimeout(() => {
            if (voiceChatFrame) voiceChatFrame.focus();
        }, 100);
        
        launchBtn.disabled = true;
    }

    function closeVoiceChat() {
        isVoiceChatActive = false;
        document.body.classList.remove("voice-chat-active");
        if (voiceChatView) voiceChatView.classList.add("hidden");
        hideHeader();
        
        // 🔥 ОСВОБОЖДАЕМ МИКРОФОН
        if (mediaStream) {
            mediaStream.getTracks().forEach(track => track.stop());
            mediaStream = null;
            console.log('[NSQCuT] Media stream released');
        }
        
        checkGame();
    }

    function toggleMicrophone() {
        isMicMuted = !isMicMuted;
        if (voiceMicBtn) {
            voiceMicBtn.classList.toggle("muted", isMicMuted);
            voiceMicBtn.title = isMicMuted ? "Включить микрофон" : "Выключить микрофон";
        }
        
        if (isMicMuted) {
            disableMicrophone();
        } else {
            enableMicrophone();
        }
        
        if (voiceChatFrame && voiceChatFrame.contentWindow) {
            try {
                voiceChatFrame.contentWindow.postMessage(
                    { type: "MICROPHONE_TOGGLE", muted: isMicMuted, timestamp: Date.now() },
                    VOICE_CHAT_URL
                );
            } catch (e) {
                console.error('[NSQCuT] postMessage failed:', e);
            }
        }
    }

    launchBtn.addEventListener("click", launchGame);
    logsBtn.addEventListener("click", openLogsFolder);
    voiceBtn.addEventListener("click", openVoiceChat);
    changePathBtn.addEventListener("click", changeGamePath);
    if (backToAddonsBtn) backToAddonsBtn.addEventListener("click", closeVoiceChat);
    if (voiceMicBtn) voiceMicBtn.addEventListener("click", toggleMicrophone);

    function showHeader() {
        if (voiceChatHeader) voiceChatHeader.classList.add("show");
    }

    function hideHeader() {
        if (voiceChatHeader) voiceChatHeader.classList.remove("show");
    }

    if (voiceHoverZone) {
        voiceHoverZone.addEventListener("mouseenter", showHeader);
        voiceHoverZone.addEventListener("mouseleave", hideHeader);
    }

    if (voiceChatHeader) {
        voiceChatHeader.addEventListener("mouseenter", showHeader);
        voiceChatHeader.addEventListener("mouseleave", hideHeader);
    }

    console.log("[NSQCuT] Waiting for backend ready callback...");
});