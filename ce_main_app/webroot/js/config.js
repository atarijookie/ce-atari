/*================================================================
  CosmosEx config terminal emulator
  ================================================================
  ----------------------------------------------------------------
  RESTlike interface, using json

  /api/v1/config/terminal 	POST
  Send Key
  
  /api/v1/config/terminal 	GET
  Get current screen as VT100 sequence

  ----------------------------------------------------------------*/
//TODO:
  
var CosmosEx = CosmosEx || {};

CosmosEx.ConfigTerminal=function(){
  return {
    init:function(){
	  var term = new Terminal({
        cols: 80,
        rows: 24,
        useStyle: true,
        screenKeys: true,
        cursorBlink: false
      });
      var lock=false;
      term.on('data', function(data) {
      	//naive lock; CE might take a while to digest the last command, so don't flood it
      	if( lock ){
      	  return false;
		}
        var stkeycode=0;
        console.log(data.charCodeAt(0));
        if(data.charCodeAt(0)==27 && data.length==1 ){
          stkeycode=27; //ESC
        } else if(data.charCodeAt(0)==27 && data.length>0 ){
          switch( data ){
            case '\033[A':   //up
              stkeycode=130; //0x48;
              break;
            case '\033[D':   //left
              stkeycode=132; //0x4b;
              break;
            case '\033[B':   //down
              stkeycode=131; //0x50;
              break;
            case '\033[C':   //right
              stkeycode=133; //0x4d;
              break;
            case '\033[15~': //F5
              $.ajax({
                url: "/api/v1/config/terminal",
              }).done(function(data) {
                term.write(data);        
              });
              return;
              break;
/*              
            case 'OP': //F1
              stkeycode=150;
              break;
            case '\033OQ': //F2
              stkeycode=151;
              break;
            case '\033OR': //F3
              stkeycode=152;
              break;
            case '\033OS': //F4
              stkeycode=153;
              break;
            case '\033[15~': //F5
              stkeycode=154;
              break;
            case '\033[17~': //F6
              stkeycode=155;
              break;
            case '\033[18~': //F7
              stkeycode=156;
              break;
            case '\033[19~': //F8
              stkeycode=157;
              break;
            case '\033[20~': //F9
              stkeycode=158;
              break;
            case '\033[21~': //F10
              stkeycode=159;
              break;
*/              
            case '\033[3~': //DEL
              stkeycode=142;
              break;
            case '\033[2~': //INSERT
              stkeycode=145;
              break;
            case '\033OH': //HOME
              stkeycode=146;
              break;
          }
        } else if(data.charCodeAt(0)==13){
              stkeycode=140;
        } else if(data.charCodeAt(0)==127){
              stkeycode=143; //BACKSPACE
        } else if(data.charCodeAt(0)==9){
              stkeycode=144; //TAB
        } else if(data.charCodeAt(0)>=32 && data.charCodeAt(0)<127){
              stkeycode=data.charCodeAt(0);
        }
        if( stkeycode!=0 ){
          lock=true;
          $.ajax({
            type: "POST",
            url: "/api/v1/config/terminal",
            data: JSON.stringify({key:stkeycode}),
            success: function(data){
              term.write(data);        
	          lock=false;
            },
            error: function(data){
	          lock=false;
            }
          });
        }
        //console.log(data);
      });
      /*
      term.on('title', function(title) {
        document.title = title;
      });
      */
      term.open($("#term").get(0));
      $.ajax({
        url: "/api/v1/config/terminal",
      }).done(function(data) {
        term.write(data);        
      });
    } 
  };
}();