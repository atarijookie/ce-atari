var CosmosEx = CosmosEx || {};

function get_loglevel()
{
    $.ajax({
        url: "/debug/loglevel",
        type: 'GET',
        dataType: 'json',
        success: function(data){
            console.log("ajax Success");
            var select_loglevel = document.getElementById('select_loglevel');
            select_loglevel.value = data.loglevel;
        },
        error: function (xhr, textStatus, errorThrown) {
            console.log("ajax Error: " + xhr.statusText + ", " + textStatus + ", " + errorThrown);
        }
    })
}

function on_loglevel_select_changed()
{
    console.log("on_loglevel_select_changed");

    var select_loglevel = document.getElementById('select_loglevel');
    var loglevel = select_loglevel.value;

    $.ajax({
        type: 'POST',
        url: '/debug/loglevel',
        data: JSON.stringify({ "loglevel": loglevel }),
        contentType: 'application/json'
    });
}

function syntaxHighlight(json) {
    json = json.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    return json.replace(/("(\\u[a-zA-Z0-9]{4}|\\[^u]|[^\\"])*"(\s*:)?|\b(true|false|null)\b|-?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?)/g, function (match) {
        var cls = 'number';
        if (/^"/.test(match)) {
            if (/:$/.test(match)) {
                cls = 'key';
            } else {
                cls = 'string';
            }
        } else if (/true|false/.test(match)) {
            cls = 'boolean';
        } else if (/null/.test(match)) {
            cls = 'null';
        }
        return '<span class="' + cls + '">' + match + '</span>';
    });
}

function getStatus()
{
    $.ajax({
        url: '/debug/status',
        type: 'GET',
        dataType: 'json',
        success: function(data){
            console.log("ajax Success");
            var pre_status = document.getElementById('pre_status');
            var status_str = JSON.stringify(data.status, undefined, 4);
            pre_status.innerHTML = syntaxHighlight(status_str);
        },
        error: function (xhr, textStatus, errorThrown) {
            console.log("ajax Error: " + xhr.statusText + ", " + textStatus + ", " + errorThrown);
        }
    })
}

