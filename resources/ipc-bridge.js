(function() {
    var backend = null;
    var channelReady = false;
    var eventListeners = {};

    window.__TAURI__ = window.__TAURI__ || {};
    window.__TAURI__.core = window.__TAURI__.core || {};
    window.__TAURI__.event = window.__TAURI__.event || {};
    window.__TAURI__.dialog = window.__TAURI__.dialog || {};
    window.__TAURI__.path = window.__TAURI__.path || {};

    function emitEvent(eventName, payload) {
        var listeners = eventListeners[eventName];
        if (listeners) {
            for (var i = 0; i < listeners.length; i++) {
                try {
                    listeners[i]({ payload: payload });
                } catch (e) {
                    console.error('[NSQCuT] Event error:', e);
                }
            }
        }
    }

    function initWebChannel() {
        console.log('[NSQCuT] initWebChannel called');

        if (typeof qt === 'undefined') {
            console.warn('[NSQCuT] qt object not available, retrying in 100ms...');
            setTimeout(initWebChannel, 100);
            return;
        }
        if (typeof qt.webChannelTransport === 'undefined') {
            console.warn('[NSQCuT] qt.webChannelTransport not available, retrying in 100ms...');
            setTimeout(initWebChannel, 100);
            return;
        }
        if (typeof QWebChannel === 'undefined') {
            console.error('[NSQCuT] QWebChannel not loaded');
            return;
        }

        console.log('[NSQCuT] Creating QWebChannel...');
        new QWebChannel(qt.webChannelTransport, function(channel) {
            console.log('[NSQCuT] QWebChannel connected');
            console.log('[NSQCuT] Channel objects:', Object.keys(channel.objects));

            backend = channel.objects.backend;
            if (!backend) {
                console.error('[NSQCuT] Backend object NOT found in channel!');
                console.log('[NSQCuT] Available objects:', Object.keys(channel.objects));
                return;
            }

            console.log('[NSQCuT] Backend methods:', Object.keys(backend));

            if (backend.signalProgress) {
                backend.signalProgress.connect(function(payload) {
                    console.log('[NSQCuT] Signal progress:', payload);
                    emitEvent('progress', payload);
                });
            }
            if (backend.signalOperationFinished) {
                backend.signalOperationFinished.connect(function(payload) {
                    console.log('[NSQCuT] Signal operation finished:', payload);
                    emitEvent('operation-finished', payload);
                });
            }
            if (backend.signalOperationError) {
                backend.signalOperationError.connect(function(payload) {
                    console.error('[NSQCuT] Signal operation error:', payload);
                    emitEvent('operation-error', payload);
                });
            }
            if (backend.signalAddonInstallStarted) {
                backend.signalAddonInstallStarted.connect(function(payload) {
                    console.log('[NSQCuT] Signal install started:', payload);
                    emitEvent('addon-install-started', payload);
                });
            }
            if (backend.signalAddonInstallFinished) {
                backend.signalAddonInstallFinished.connect(function(payload) {
                    console.log('[NSQCuT] Signal install finished:', payload);
                    emitEvent('addon-install-finished', payload);
                });
            }
            if (backend.signalLaunchButtonState) {
                backend.signalLaunchButtonState.connect(function(payload) {
                    console.log('[NSQCuT] Signal launch button state:', payload);
                    emitEvent('launch-button-state', payload);
                });
            }

            channelReady = true;
            console.log('[NSQCuT] Bridge ready');

            window.backend = backend;
            console.log('[NSQCuT] window.backend set:', window.backend !== null);

            if (window.loadAddonsAfterReady) {
                console.log('[NSQCuT] Calling deferred loadAddons');
                window.loadAddonsAfterReady();
                delete window.loadAddonsAfterReady;
            }

            if (window.onBackendReady) {
                console.log('[NSQCuT] Calling onBackendReady callback');
                window.onBackendReady();
            }
        });
    }

    window.__TAURI__.event.listen = function(eventName, callback) {
        console.log('[NSQCuT] event.listen:', eventName);
        return new Promise(function(resolve) {
            if (!eventListeners[eventName]) {
                eventListeners[eventName] = [];
            }
            eventListeners[eventName].push(callback);
            resolve({ id: eventName });
        });
    };

    window.__TAURI__.dialog.open = function(options) {
        options = options || {};
        return new Promise(function(resolve, reject) {
            if (!channelReady) {
                reject(new Error('QtBridge not ready'));
                return;
            }
            if (!backend || !backend.openFileDialog) {
                reject(new Error('openFileDialog not available'));
                return;
            }
            var title = options.title || 'Select file';
            var filter = 'All Files (*.*)';
            if (options.filters && options.filters.length > 0) {
                filter = options.filters[0].name + ' (*.' + options.filters[0].extensions.join(' *.') + ')';
            }
            console.log('[NSQCuT] Opening dialog:', title, filter);
            backend.openFileDialog(String(title), String(filter))
                .then(function(path) {
                    console.log('[NSQCuT] Dialog result:', path);
                    resolve(path);
                })
                .catch(function(err) {
                    console.error('[NSQCuT] Dialog error:', err);
                    reject(err);
                });
        });
    };

    window.__TAURI__.path.dirname = function(filePath) {
        var lastSlash = String(filePath).lastIndexOf('/');
        var lastBackslash = String(filePath).lastIndexOf('\\');
        var sep = lastSlash > lastBackslash ? lastSlash : lastBackslash;
        return sep > 0 ? String(filePath).substring(0, sep) : '';
    };

    console.log('[NSQCuT] ipc-bridge.js loaded');

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initWebChannel);
    } else {
        initWebChannel();
    }
})();