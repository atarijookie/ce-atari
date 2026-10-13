// Background check that the backend is running and responding.
// Pings /ping periodically; when the server doesn't respond, an overlay box with
// 'Waiting for server to come back' is shown, and hidden again once the server responds.
// If the server is back but the session is gone (e.g. after server restart), go to the login page.
(function () {
    var PING_URL = '/ping';
    var INTERVAL_OK = 5000;             // check interval while the server responds
    var INTERVAL_DOWN = 2000;           // check interval while the server is down
    var TIMEOUT = 3000;                 // how long to wait for the ping response
    var FAILS_BEFORE_OVERLAY = 2;       // don't show the overlay on a single failed ping

    var isLoginPage = window.location.pathname === '/login.html';
    var fails = 0;
    var timer = null;
    var overlay = null;

    function createOverlay() {
        overlay = document.createElement('div');
        overlay.id = 'server-check-overlay';
        overlay.style.cssText = 'display: none; position: fixed; top: 0; left: 0; right: 0; bottom: 0; ' +
            'z-index: 10000; background: rgba(0,0,0,0.4); align-items: center; justify-content: center;';

        var box = document.createElement('div');
        box.style.cssText = 'background: white; border-radius: 12px; padding: 30px 40px; text-align: center; ' +
            'box-shadow: 0 4px 20px rgba(0,0,0,0.3); font-family: "Helvetica Neue",Helvetica,Arial,sans-serif; ' +
            'font-size: 17px; color: #333; max-width: 80%;';

        var spinner = document.createElement('div');
        spinner.style.cssText = 'width: 32px; height: 32px; margin: 0 auto 16px auto; border: 4px solid #ddd; ' +
            'border-top-color: #3baf16; border-radius: 50%; animation: server-check-spin 1s linear infinite;';

        var style = document.createElement('style');
        style.textContent = '@keyframes server-check-spin { to { transform: rotate(360deg); } }';

        box.appendChild(spinner);
        box.appendChild(document.createTextNode('Waiting for server to come back'));
        overlay.appendChild(box);
        document.head.appendChild(style);
        document.body.appendChild(overlay);
    }

    function showOverlay(show) {
        if (!overlay) {
            if (!show) {
                return;
            }
            createOverlay();
        }
        overlay.style.display = show ? 'flex' : 'none';
    }

    function schedule(delay) {
        clearTimeout(timer);
        timer = setTimeout(check, delay);
    }

    function onResponse(status) {
        fails = 0;
        showOverlay(false);

        // server is back, but our session is gone? log in again
        if (status === 401 && !isLoginPage) {
            window.location.replace('/login.html');
            return;
        }
        schedule(INTERVAL_OK);
    }

    function onFailure() {
        fails++;
        if (fails >= FAILS_BEFORE_OVERLAY) {
            showOverlay(true);
        }
        schedule(INTERVAL_DOWN);
    }

    function check() {
        // don't ping from hidden tabs, check again when the tab is shown
        if (document.hidden) {
            return;
        }

        var controller = window.AbortController ? new AbortController() : null;
        var abortTimer = setTimeout(function () {
            if (controller) {
                controller.abort();
            }
        }, TIMEOUT);

        fetch(PING_URL, {
            method: 'GET',
            cache: 'no-store',
            credentials: 'include',
            signal: controller ? controller.signal : undefined
        })
        .then(function (response) {
            clearTimeout(abortTimer);
            if (response.ok || response.status === 401) {
                onResponse(response.status);
            } else {
                onFailure();        // e.g. 502 from a proxy in front of the server
            }
        })
        .catch(function () {
            clearTimeout(abortTimer);
            onFailure();
        });
    }

    document.addEventListener('visibilitychange', function () {
        if (!document.hidden) {
            check();
        }
    });

    function start() {
        schedule(INTERVAL_OK);
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', start);
    } else {
        start();
    }
})();
