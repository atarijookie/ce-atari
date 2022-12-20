/*================================================================
  CosmosEx config terminal emulator
  ================================================================
  ----------------------------------------------------------------
  RESTlike interface, using json

  /stream/config 	POST
  Send Key
  
  /stream/config 	GET
  Get current screen as VT100 sequence

  ----------------------------------------------------------------*/

var term = term || new Terminal({
    cols: 80,
    rows: 24,
    useStyle: true,
    screenKeys: true,
    cursorBlink: false
});

function term_init()
{
    var lock=false;
    term.open($("#term").get(0));

    term.on('data', function(data_from_kbd) {
        //naive lock; CE might take a while to digest the last command, so don't flood it
        if(lock) {
            return false;
        }

        lock=true;

        $.ajax({
            type: "POST",
            contentType: "application/json",
            url: "/stream/config",
            data: JSON.stringify({data: data_from_kbd, len: data_from_kbd.length}),
            success: function(data) {
                lock=false;
            },
            error: function(data) {
              lock=false;
            }
        });
    });

    term_get_data();
}


function term_get_data()
{
    $.ajax({
        url: "/stream/config",
    }).done(function(data_from_app) {
        term.write(data_from_app);
    }).always(function() {
        setTimeout(term_get_data, 50);
    });
}


var CosmosEx = CosmosEx || {};

CosmosEx.configInit=term_init
CosmosEx.configGetData=term_get_data
