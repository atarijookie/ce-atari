var CosmosEx = CosmosEx || {};
CosmosEx.Screencast=function(){
	// Locate "<div id="demo_placeholder" ...>" element in web page
	var element = document.getElementById('demo_placeholder');
	var iCurrentRes=-1;
	// Add an ST mode 0 screen called 'demo' to it.
	//var demoScreen = new AtariScreen(iCurrentRes, element, 'demo');
	var demoScreen=null;
	
	var cnt=new Date().getTime()*10;
	// ... Any screen manipulation goes here ...

	var onLoad=function(e) {
			var dv=new DataView(this.response);
			if( dv.byteLength==32033 ){
				var iRes=dv.getInt8(0);
				if( iRes!=iCurrentRes && dv.byteLength>0 ){
					console.log("rez changed");
					$("#demo_placeholder").html("");
					demoScreen = null;
					demoScreen = new AtariScreen(iRes, element, 'demo');
					iCurrentRes=iRes;			
				}			
				if( demoScreen!=null ){
					demoScreen.ExtractPalette(dv, 1)
					demoScreen.ExtractPlanarScreen(dv,16*2+1);
					// Display screen.
					demoScreen.Display();
				}
			}
			dv=null;
			setTimeout(loadScreen,200);
		};
	
	var onError=function(e) {
			setTimeout(loadScreen,200);
		};
	var loadScreen=function(){
		var xhr = new XMLHttpRequest();
		cnt++;
		xhr.open('GET', '/app/screencast/getscreen?'+cnt, true);
		xhr.responseType = 'arraybuffer';
		xhr.onerror = onError; 
		xhr.onload = onLoad;
		
		xhr.send(null);
	    xhr = null;
	};
	loadScreen();
};  