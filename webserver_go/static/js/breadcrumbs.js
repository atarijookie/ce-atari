// Breadcrumb navigation component
function renderBreadcrumbs() {
    var path = window.location.pathname;
    var params = new URLSearchParams(window.location.search);
    var mac = params.get('mac');
    
    var breadcrumbs = [];
    
    var currentPage = getCurrentPageName(path);
    
    // If we're on index.html, just show "Devices" as active
    if (path === '/index.html' || path === '/') {
        breadcrumbs.push({
            text: 'Devices',
            url: null // Current page, no link
        });
    } else {
        // Always start with Devices (Home)
        breadcrumbs.push({
            text: 'Devices',
            url: '/index.html'
        });
        
        // Add device breadcrumb if we have a MAC
        if (mac) {
            // Use name parameter if available, otherwise use MAC
            var name = params.get('name');
            var deviceName = name || formatMACForDisplay(mac);
            
            // Build URL with both mac and name parameters
            var deviceUrl = '/device.html?mac=' + encodeURIComponent(mac);
            if (name) {
                deviceUrl += '&name=' + encodeURIComponent(name);
            }
            
            breadcrumbs.push({
                text: deviceName,
                url: deviceUrl
            });
        }
        
        // Add current page breadcrumb (skip if we're on device.html without a page name)
        if (currentPage && currentPage !== 'Devices') {
            breadcrumbs.push({
                text: currentPage,
                url: null // Current page, no link
            });
        }
    }
    
    // Render breadcrumbs
    var container = document.getElementById('breadcrumbs-container');
    if (!container) {
        console.error('breadcrumbs-container element not found');
        return;
    }
    
    var breadcrumbHtml = '<ul class="nav nav-pills" style="margin-bottom: 20px; display: flex; align-items: center; list-style: none; padding: 0;">';
    for (var i = 0; i < breadcrumbs.length; i++) {
        var crumb = breadcrumbs[i];
        if (i === breadcrumbs.length - 1) {
            // Last item - active, no link
            breadcrumbHtml += '<li class="active" style="display: inline-block; margin-right: 5px; margin-bottom: 5px;"><span style="display: inline-block; padding: 6.5px 10.4px; border-radius: 5px;">' + escapeHtml(crumb.text) + '</span></li>';
        } else {
            // Not last item - link with separator
            breadcrumbHtml += '<li style="display: inline-block; margin-right: 5px; margin-bottom: 5px;"><a href="' + escapeHtml(crumb.url) + '" style="display: inline-block; padding: 6.5px 10.4px; border-radius: 5px; text-decoration: none;">' + escapeHtml(crumb.text) + '</a></li>';
            breadcrumbHtml += '<li style="display: inline-block; color: #999; padding: 6.5px 5px; margin-right: 5px; margin-bottom: 5px; vertical-align: middle;">→</li>';
        }
    }
    breadcrumbHtml += '</ul>';
    
    container.innerHTML = breadcrumbHtml;
}

function formatMACForDisplay(mac) {
    // Remove colons if present, then format
    mac = mac.replace(/:/g, '');
    if (mac.length === 12 && /^[0-9a-fA-F]{12}$/.test(mac)) {
        return mac.substring(0,2) + ':' + mac.substring(2,4) + ':' + mac.substring(4,6) + ':' + 
               mac.substring(6,8) + ':' + mac.substring(8,10) + ':' + mac.substring(10,12);
    }
    return mac;
}

function getCurrentPageName(path) {
    var pageNames = {
        '/index.html': 'Devices',
        '/device.html': null, // Will be replaced by MAC address in breadcrumb
        '/acsi_ids.html': 'ACSI IDs',
        '/gem_drives.html': 'GEM Drives',
        '/floppy.html': 'Floppy',
        '/floppy': 'Floppy', // Handle both with and without .html
        '/ikbd.html': 'IKBD',
        '/ikbd': 'IKBD', // Handle both with and without .html
        '/status.html': 'Status',
        '/login.html': 'Login'
    };
    
    // Check exact match first
    if (pageNames[path]) {
        return pageNames[path];
    }
    
    // Check if path contains any of the keys
    for (var key in pageNames) {
        if (path.indexOf(key) !== -1 && pageNames[key]) {
            return pageNames[key];
        }
    }
    
    return null;
}

function escapeHtml(text) {
    var div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// Auto-render when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', renderBreadcrumbs);
} else {
    renderBreadcrumbs();
}
