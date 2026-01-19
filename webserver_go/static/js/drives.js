// for each combo box add a on-change handler
function bindHandlersGEMDrives() {
    for (let i=2; i<16; i++) {
        let letter = String.fromCharCode(65 + i);

        var elemToBind = document.getElementById(`TRANS_DRIVE_${letter}`);
        elemToBind.onchange = function () { onComboChanged(i); }
    }
}

// get ACSI IDs and paths from backend and update UI
function getDrivesConfig() {
    // Get MAC address from URL parameters
    function getURLParameter(name) {
        name = name.replace(/[\[]/, '\\[').replace(/[\]]/, '\\]');
        var regex = new RegExp('[\\?&]' + name + '=([^&#]*)');
        var results = regex.exec(location.search);
        return results === null ? '' : decodeURIComponent(results[1].replace(/\+/g, ' '));
    }
    
    var mac = getURLParameter('mac');
    if (!mac) {
        console.log("Error: No MAC address in URL");
        return;
    }
    
    // Fetch from the translated endpoint: /hdd/{mac}/translated
    $.ajax({
        url: '/hdd/' + encodeURIComponent(mac) + '/translated',
        type: 'GET',
        dataType: 'json',
        success: function (data) {
            paths = data.paths;
            driveTypes = data.drive_types;

            console.log("hdd/" + mac + "/translated - data: " + JSON.stringify(data) + ", paths: " + paths + ", driveTypes: " + driveTypes);
            updateIdsFromData();
        },
        error: function (xhr) {
            console.log("Error fetching device config: " + xhr.statusText);
        },
    })
}

// for the specified path fetch the dir content from backend, then fill 
// the received dirs and files into the file selector, attaching all the handlers
function getDirContent(new_path) {
    selectedFile = "";
    dir = new_path;
    document.getElementById('currentPath').textContent = dir;

    $.ajax({
        url: '/host/dir?path=' + encodeURIComponent(new_path),
        type: 'GET',
        dataType: 'json',
        success: function (data) {
            var dirs = data.dirs;
            var files = data.files;

            const itemList = document.getElementById('itemList');
            itemList.innerHTML = '';

            dirs.forEach(item => {
                var fullpath = (new_path == "/") ? ("/" + item) : (new_path + "/" + item);
                const div = document.createElement('div');
                div.className = 'item';
                div.textContent = "[" + item + "]";
                div.onclick = () => getDirContent(fullpath);
                itemList.appendChild(div);
            });

            files.forEach(item => {
                var fullpath = (new_path == "/") ? ("/" + item) : (new_path + "/" + item);
                const div = document.createElement('div');
                div.className = 'item';
                div.textContent = item;
                div.onclick = () => onFileSelected(fullpath);
                itemList.appendChild(div);
            });
        },
        error: function (xhr) {
            console.log("Error: " + xhr.statusText);
        },
    })
}

// go through all the lines in file selector, select/unselect line which matches 
// the file specified in fullFilePath
function selectByText(fullFilePath, selectNotUnselect) {
    var items = document.getElementsByClassName("item");
    var fileName = fullFilePath.split('/').pop();

    for (x = 0; x < items.length; x++) {
        item = items[x];

        if (item.textContent == fileName) {
            if (selectNotUnselect) {
                item.style.backgroundColor = "#cccccc";
            } else {
                item.style.backgroundColor = "#ffffff";
            }
            break;
        }
    }
}

// on file clicked, select the file on 1st click, unselect it on the 2nd click
function onFileSelected(filePath) {
    if (filePath == selectedFile) {  // 2nd click - unselect
        selectByText(selectedFile, false);
        selectedFile = "";
    } else {                        // 1st click - select
        selectByText(selectedFile, false);
        selectedFile = filePath;
        selectByText(filePath, true);
    }
}

// Check if the selected configuration is valid and show warning if it isn't.
// If config is valid, save it.
function onIdsSave() {
    var configDriveFound = 0;

    for (var i = 2; i < 16; i++) {
        let letter = String.fromCharCode(65 + i);

        if (driveTypes[i] == 1 && !paths[i]) {
            alert(`The drive ${letter} does not have any host path selected. Please select path before saving.`);
            return;
        }

        if (driveTypes[i] == 2) {
            configDriveFound++;
        }
    }

    if (configDriveFound == 0) {
        alert("Config drive was not assigned to any GEM drive letter. Please select one drive as CONFIG drive before saving.");
        return;
    }

    if (configDriveFound > 1) {
        alert("Multiple drives selected as config drives. Please select only one drive as CONFIG drive before saving.");
        return;
    }

    putDrivesConfig();
}

// save the configured paths and drive drives
function putDrivesConfig() {
    // Get MAC address from URL parameters
    function getURLParameter(name) {
        name = name.replace(/[\[]/, '\\[').replace(/[\]]/, '\\]');
        var regex = new RegExp('[\\?&]' + name + '=([^&#]*)');
        var results = regex.exec(location.search);
        return results === null ? '' : decodeURIComponent(results[1].replace(/\+/g, ' '));
    }
    
    var mac = getURLParameter('mac');
    if (!mac) {
        alert("Error: No MAC address in URL");
        return;
    }
    
    // Save to the translated endpoint: /hdd/{mac}/translated
    $.ajax({
        url: '/hdd/' + encodeURIComponent(mac) + '/translated',
        type: 'PUT',
        contentType: 'application/json',
        data: JSON.stringify({ 'paths': paths, 'drive_types': driveTypes }),
        success: function (data) {
            alert("Config saved.");
        },
        error: function (xhr) {
            console.log("Error saving config: " + xhr.statusText);
            alert("Error saving config: " + xhr.statusText);
        },
    })
}

// go through all the rows, switch combo boxes to selected values and show paths on screen
function updateIdsFromData() {
    for (let i = 2; i < 16; i++) {
        let letter = String.fromCharCode(65 + i);
        var elem = document.getElementById(`TRANS_DRIVE_${letter}`);
        elem.value = driveTypes[i].toString();
        onComboChanged(i);
    }
}

// for the specified index show the button and path or nothing
function onComboChanged(index) {
    let letter = String.fromCharCode(65 + index);
    var devtype = document.getElementById(`TRANS_DRIVE_${letter}`).value;
    var td = document.getElementById(`PATH_${letter}`);

    driveTypes[index] = Number(devtype);   // store the new device type to array

    switch (devtype) {
        case "0": td.innerHTML = ""; break;
        case "1": td.innerHTML = `<button type="button" onclick="handleSelectClick(${index});">Select host dir</button> &nbsp; ${paths[index]}`; break;
        case "2": td.innerHTML = "[CONFIG DRIVE]"; break;
    }
}

// on 'Select file' button clicked, get the dir content and show the file selector
function handleSelectClick(index) {
    selectingFileForIndex = index;
    $("[name=file-selector]").show();

    dir = paths[index];

    const parts = dir.split('/');
    if (parts.length > 1) {
        parts.pop();
    }

    dir = parts.join('/');

    if (!dir) {
        dir = "/home";
    }

    getDirContent(dir);
}

// go to parent directory in the file selector
function goUp() {
    const parts = dir.split('/');
    if (parts.length > 1) {
        parts.pop();
    }

    var new_path = parts.join('/');
    if (new_path == "") {
        new_path = "/";
    }

    getDirContent(new_path);
}

// when 'OK' button is clicked on the file selector
function onOK() {
    paths[selectingFileForIndex] = dir;
    onComboChanged(selectingFileForIndex);

    $("[name=file-selector]").hide();
}

// when 'Cancel' button is clicked on the file selector
function onCancel() {
    $("[name=file-selector]").hide();
}
