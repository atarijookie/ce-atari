function fillFloppyImagepaths()
{
  for(var i=0; i<8; i++) {
      fillFloppyImagePath(i);
  }
}

// on 'Select file' button clicked, get the dir content and show the file selector
function handleFloppyImageSelectClick(index) {
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

function fillFloppyImagePath(index) {
  var td = document.getElementById("path" + index);
  td.innerHTML = "<button type=\"button\" onclick=\"handleFloppyImageSelectClick(" + index + ");\">Select file</button> &nbsp; " + paths[index];
}

// get last used floppy images
function getSlots() {
    $.ajax({
        url: '/floppy/get_slots',
        type: 'GET',
        dataType: 'json',
        success: function (data) {
            paths = data.paths;
            max_clients = data.max_clients;

            console.log("floppy/get_slots - paths: " + paths + ", max_clients: " + max_clients);
            fillFloppyImagepaths();
        },
        error: function (xhr) {
            console.log("Error: " + xhr.statusText);
        },
    })
}

function saveSlotPath(slotIndex, image_path) {
    $.ajax({
        url: '/floppy/' + slotIndex,
        type: 'PUT',
        dataType: 'json',
        data: JSON.stringify({ 'path': image_path, 'slot_no': slotIndex }),
        success: function (data) {
          fillFloppyImagePath(slotIndex);
        },
        error: function (xhr) {
            console.log("Error: " + xhr.statusText);
        },
    })
}

// when 'OK' button is clicked on the file selector
function onSelectorOK() {
    if (!selectedFile || selectingFileForIndex == -1) {
        return;
    }

    var ext = selectedFile.split('.').pop().toLowerCase();
    if(ext != 'msa' && ext != 'st') {
      alert("File extension '" + ext + "' is not supported floppy image format - .msa / .st");
      return;
    }

    paths[selectingFileForIndex] = selectedFile;
    saveSlotPath(selectingFileForIndex, selectedFile);

    $("[name=file-selector]").hide();
}

// when 'Cancel' button is clicked on the file selector
function onSelectorCancel() {
    $("[name=file-selector]").hide();
}

function getFddClients() {
        $.ajax({
        url: '/floppy/get_clients',
        type: 'GET',
        dataType: 'json',
        success: function (data) {
            console.log("floppy/get_clients - data: " + data);
            fillFloppyClients(data);
        },
        error: function (xhr) {
            console.log("Error: " + xhr.statusText);
        },
    })
}

function fillFloppyClients(data) {
  for(var i=0; i<8; i++) {
        var key = i.toString();
        var td = document.getElementById("ip" + i);

        if(key in data) {
            var info = data[key];
            td.innerHTML = "name: " + info['name'] + "<br>mac: " + info['mac'] + "<br>ip: " + info['ip'];
        } else {
            td.innerHTML = "";
        }
    }
}
