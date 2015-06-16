(function($){
 
var hasTouch = /android|iphone|ipad/i.test(navigator.userAgent.toLowerCase());
 
/**
 * Bind an event handler to the "double tap" JavaScript event.
 * @param {function} doubleTapHandler
 * @param {number} [delay=300]
 */
$.fn.doubletap = function(doubleTapHandler, doubleTap2Fingerhandler, delay){
    if( !hasTouch ){
        return;
    }
    delay = (delay == null) ? 300 : delay;
    var lastTouchEnd=null;
    var lastTouchStart=null;
    var lastLastTouchStart=null;
    this.bind("touchstart",function(event){
        lastLastTouchStart=lastTouchStart;
        lastTouchStart=new Date().getTime();
    });
    this.bind("touchend", function(event){
        var now = new Date().getTime();
 
        // the first time this will make delta a negative number
        var lt = lastTouchEnd || now + 1;
        var delta = now - lt;
        if(delta < delay && 0 < delta && (lastTouchEnd-lastLastTouchStart) < delay){
            // After we detct a doubletap, start over
            lastTouchEnd=null;
            
            if( event.changedTouches.length==1 ){
                if(doubleTapHandler !== null && typeof doubleTapHandler === 'function'){
                    doubleTapHandler(event);
                }
            } else if( event.changedTouches.length==2 ){
                if(doubleTap2Fingerhandler !== null && typeof doubleTap2Fingerhandler === 'function'){
                    doubleTap2Fingerhandler(event);
                }
            }
        }else{
            lastTouchEnd=now;
        }
    });
};
 
})(jQuery);