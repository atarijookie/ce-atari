/* Zepto v1.1.4 - zepto event ajax form ie - zeptojs.com/license */
var Zepto=function(){function L(t){return null==t?String(t):j[S.call(t)]||"object"}function Z(t){return"function"==L(t)}function $(t){return null!=t&&t==t.window}function _(t){return null!=t&&t.nodeType==t.DOCUMENT_NODE}function D(t){return"object"==L(t)}function R(t){return D(t)&&!$(t)&&Object.getPrototypeOf(t)==Object.prototype}function M(t){return"number"==typeof t.length}function k(t){return s.call(t,function(t){return null!=t})}function z(t){return t.length>0?n.fn.concat.apply([],t):t}function F(t){return t.replace(/::/g,"/").replace(/([A-Z]+)([A-Z][a-z])/g,"$1_$2").replace(/([a-z\d])([A-Z])/g,"$1_$2").replace(/_/g,"-").toLowerCase()}function q(t){return t in f?f[t]:f[t]=new RegExp("(^|\\s)"+t+"(\\s|$)")}function H(t,e){return"number"!=typeof e||c[F(t)]?e:e+"px"}function I(t){var e,n;return u[t]||(e=a.createElement(t),a.body.appendChild(e),n=getComputedStyle(e,"").getPropertyValue("display"),e.parentNode.removeChild(e),"none"==n&&(n="block"),u[t]=n),u[t]}function V(t){return"children"in t?o.call(t.children):n.map(t.childNodes,function(t){return 1==t.nodeType?t:void 0})}function B(n,i,r){for(e in i)r&&(R(i[e])||A(i[e]))?(R(i[e])&&!R(n[e])&&(n[e]={}),A(i[e])&&!A(n[e])&&(n[e]=[]),B(n[e],i[e],r)):i[e]!==t&&(n[e]=i[e])}function U(t,e){return null==e?n(t):n(t).filter(e)}function J(t,e,n,i){return Z(e)?e.call(t,n,i):e}function X(t,e,n){null==n?t.removeAttribute(e):t.setAttribute(e,n)}function W(e,n){var i=e.className,r=i&&i.baseVal!==t;return n===t?r?i.baseVal:i:void(r?i.baseVal=n:e.className=n)}function Y(t){var e;try{return t?"true"==t||("false"==t?!1:"null"==t?null:/^0/.test(t)||isNaN(e=Number(t))?/^[\[\{]/.test(t)?n.parseJSON(t):t:e):t}catch(i){return t}}function G(t,e){e(t);for(var n=0,i=t.childNodes.length;i>n;n++)G(t.childNodes[n],e)}var t,e,n,i,C,N,r=[],o=r.slice,s=r.filter,a=window.document,u={},f={},c={"column-count":1,columns:1,"font-weight":1,"line-height":1,opacity:1,"z-index":1,zoom:1},l=/^\s*<(\w+|!)[^>]*>/,h=/^<(\w+)\s*\/?>(?:<\/\1>|)$/,p=/<(?!area|br|col|embed|hr|img|input|link|meta|param)(([\w:]+)[^>]*)\/>/gi,d=/^(?:body|html)$/i,m=/([A-Z])/g,g=["val","css","html","text","data","width","height","offset"],v=["after","prepend","before","append"],y=a.createElement("table"),x=a.createElement("tr"),b={tr:a.createElement("tbody"),tbody:y,thead:y,tfoot:y,td:x,th:x,"*":a.createElement("div")},w=/complete|loaded|interactive/,E=/^[\w-]*$/,j={},S=j.toString,T={},O=a.createElement("div"),P={tabindex:"tabIndex",readonly:"readOnly","for":"htmlFor","class":"className",maxlength:"maxLength",cellspacing:"cellSpacing",cellpadding:"cellPadding",rowspan:"rowSpan",colspan:"colSpan",usemap:"useMap",frameborder:"frameBorder",contenteditable:"contentEditable"},A=Array.isArray||function(t){return t instanceof Array};return T.matches=function(t,e){if(!e||!t||1!==t.nodeType)return!1;var n=t.webkitMatchesSelector||t.mozMatchesSelector||t.oMatchesSelector||t.matchesSelector;if(n)return n.call(t,e);var i,r=t.parentNode,o=!r;return o&&(r=O).appendChild(t),i=~T.qsa(r,e).indexOf(t),o&&O.removeChild(t),i},C=function(t){return t.replace(/-+(.)?/g,function(t,e){return e?e.toUpperCase():""})},N=function(t){return s.call(t,function(e,n){return t.indexOf(e)==n})},T.fragment=function(e,i,r){var s,u,f;return h.test(e)&&(s=n(a.createElement(RegExp.$1))),s||(e.replace&&(e=e.replace(p,"<$1></$2>")),i===t&&(i=l.test(e)&&RegExp.$1),i in b||(i="*"),f=b[i],f.innerHTML=""+e,s=n.each(o.call(f.childNodes),function(){f.removeChild(this)})),R(r)&&(u=n(s),n.each(r,function(t,e){g.indexOf(t)>-1?u[t](e):u.attr(t,e)})),s},T.Z=function(t,e){return t=t||[],t.__proto__=n.fn,t.selector=e||"",t},T.isZ=function(t){return t instanceof T.Z},T.init=function(e,i){var r;if(!e)return T.Z();if("string"==typeof e)if(e=e.trim(),"<"==e[0]&&l.test(e))r=T.fragment(e,RegExp.$1,i),e=null;else{if(i!==t)return n(i).find(e);r=T.qsa(a,e)}else{if(Z(e))return n(a).ready(e);if(T.isZ(e))return e;if(A(e))r=k(e);else if(D(e))r=[e],e=null;else if(l.test(e))r=T.fragment(e.trim(),RegExp.$1,i),e=null;else{if(i!==t)return n(i).find(e);r=T.qsa(a,e)}}return T.Z(r,e)},n=function(t,e){return T.init(t,e)},n.extend=function(t){var e,n=o.call(arguments,1);return"boolean"==typeof t&&(e=t,t=n.shift()),n.forEach(function(n){B(t,n,e)}),t},T.qsa=function(t,e){var n,i="#"==e[0],r=!i&&"."==e[0],s=i||r?e.slice(1):e,a=E.test(s);return _(t)&&a&&i?(n=t.getElementById(s))?[n]:[]:1!==t.nodeType&&9!==t.nodeType?[]:o.call(a&&!i?r?t.getElementsByClassName(s):t.getElementsByTagName(e):t.querySelectorAll(e))},n.contains=a.documentElement.contains?function(t,e){return t!==e&&t.contains(e)}:function(t,e){for(;e&&(e=e.parentNode);)if(e===t)return!0;return!1},n.type=L,n.isFunction=Z,n.isWindow=$,n.isArray=A,n.isPlainObject=R,n.isEmptyObject=function(t){var e;for(e in t)return!1;return!0},n.inArray=function(t,e,n){return r.indexOf.call(e,t,n)},n.camelCase=C,n.trim=function(t){return null==t?"":String.prototype.trim.call(t)},n.uuid=0,n.support={},n.expr={},n.map=function(t,e){var n,r,o,i=[];if(M(t))for(r=0;r<t.length;r++)n=e(t[r],r),null!=n&&i.push(n);else for(o in t)n=e(t[o],o),null!=n&&i.push(n);return z(i)},n.each=function(t,e){var n,i;if(M(t)){for(n=0;n<t.length;n++)if(e.call(t[n],n,t[n])===!1)return t}else for(i in t)if(e.call(t[i],i,t[i])===!1)return t;return t},n.grep=function(t,e){return s.call(t,e)},window.JSON&&(n.parseJSON=JSON.parse),n.each("Boolean Number String Function Array Date RegExp Object Error".split(" "),function(t,e){j["[object "+e+"]"]=e.toLowerCase()}),n.fn={forEach:r.forEach,reduce:r.reduce,push:r.push,sort:r.sort,indexOf:r.indexOf,concat:r.concat,map:function(t){return n(n.map(this,function(e,n){return t.call(e,n,e)}))},slice:function(){return n(o.apply(this,arguments))},ready:function(t){return w.test(a.readyState)&&a.body?t(n):a.addEventListener("DOMContentLoaded",function(){t(n)},!1),this},get:function(e){return e===t?o.call(this):this[e>=0?e:e+this.length]},toArray:function(){return this.get()},size:function(){return this.length},remove:function(){return this.each(function(){null!=this.parentNode&&this.parentNode.removeChild(this)})},each:function(t){return r.every.call(this,function(e,n){return t.call(e,n,e)!==!1}),this},filter:function(t){return Z(t)?this.not(this.not(t)):n(s.call(this,function(e){return T.matches(e,t)}))},add:function(t,e){return n(N(this.concat(n(t,e))))},is:function(t){return this.length>0&&T.matches(this[0],t)},not:function(e){var i=[];if(Z(e)&&e.call!==t)this.each(function(t){e.call(this,t)||i.push(this)});else{var r="string"==typeof e?this.filter(e):M(e)&&Z(e.item)?o.call(e):n(e);this.forEach(function(t){r.indexOf(t)<0&&i.push(t)})}return n(i)},has:function(t){return this.filter(function(){return D(t)?n.contains(this,t):n(this).find(t).size()})},eq:function(t){return-1===t?this.slice(t):this.slice(t,+t+1)},first:function(){var t=this[0];return t&&!D(t)?t:n(t)},last:function(){var t=this[this.length-1];return t&&!D(t)?t:n(t)},find:function(t){var e,i=this;return e=t?"object"==typeof t?n(t).filter(function(){var t=this;return r.some.call(i,function(e){return n.contains(e,t)})}):1==this.length?n(T.qsa(this[0],t)):this.map(function(){return T.qsa(this,t)}):[]},closest:function(t,e){var i=this[0],r=!1;for("object"==typeof t&&(r=n(t));i&&!(r?r.indexOf(i)>=0:T.matches(i,t));)i=i!==e&&!_(i)&&i.parentNode;return n(i)},parents:function(t){for(var e=[],i=this;i.length>0;)i=n.map(i,function(t){return(t=t.parentNode)&&!_(t)&&e.indexOf(t)<0?(e.push(t),t):void 0});return U(e,t)},parent:function(t){return U(N(this.pluck("parentNode")),t)},children:function(t){return U(this.map(function(){return V(this)}),t)},contents:function(){return this.map(function(){return o.call(this.childNodes)})},siblings:function(t){return U(this.map(function(t,e){return s.call(V(e.parentNode),function(t){return t!==e})}),t)},empty:function(){return this.each(function(){this.innerHTML=""})},pluck:function(t){return n.map(this,function(e){return e[t]})},show:function(){return this.each(function(){"none"==this.style.display&&(this.style.display=""),"none"==getComputedStyle(this,"").getPropertyValue("display")&&(this.style.display=I(this.nodeName))})},replaceWith:function(t){return this.before(t).remove()},wrap:function(t){var e=Z(t);if(this[0]&&!e)var i=n(t).get(0),r=i.parentNode||this.length>1;return this.each(function(o){n(this).wrapAll(e?t.call(this,o):r?i.cloneNode(!0):i)})},wrapAll:function(t){if(this[0]){n(this[0]).before(t=n(t));for(var e;(e=t.children()).length;)t=e.first();n(t).append(this)}return this},wrapInner:function(t){var e=Z(t);return this.each(function(i){var r=n(this),o=r.contents(),s=e?t.call(this,i):t;o.length?o.wrapAll(s):r.append(s)})},unwrap:function(){return this.parent().each(function(){n(this).replaceWith(n(this).children())}),this},clone:function(){return this.map(function(){return this.cloneNode(!0)})},hide:function(){return this.css("display","none")},toggle:function(e){return this.each(function(){var i=n(this);(e===t?"none"==i.css("display"):e)?i.show():i.hide()})},prev:function(t){return n(this.pluck("previousElementSibling")).filter(t||"*")},next:function(t){return n(this.pluck("nextElementSibling")).filter(t||"*")},html:function(t){return 0 in arguments?this.each(function(e){var i=this.innerHTML;n(this).empty().append(J(this,t,e,i))}):0 in this?this[0].innerHTML:null},text:function(t){return 0 in arguments?this.each(function(e){var n=J(this,t,e,this.textContent);this.textContent=null==n?"":""+n}):0 in this?this[0].textContent:null},attr:function(n,i){var r;return"string"!=typeof n||1 in arguments?this.each(function(t){if(1===this.nodeType)if(D(n))for(e in n)X(this,e,n[e]);else X(this,n,J(this,i,t,this.getAttribute(n)))}):this.length&&1===this[0].nodeType?!(r=this[0].getAttribute(n))&&n in this[0]?this[0][n]:r:t},removeAttr:function(t){return this.each(function(){1===this.nodeType&&X(this,t)})},prop:function(t,e){return t=P[t]||t,1 in arguments?this.each(function(n){this[t]=J(this,e,n,this[t])}):this[0]&&this[0][t]},data:function(e,n){var i="data-"+e.replace(m,"-$1").toLowerCase(),r=1 in arguments?this.attr(i,n):this.attr(i);return null!==r?Y(r):t},val:function(t){return 0 in arguments?this.each(function(e){this.value=J(this,t,e,this.value)}):this[0]&&(this[0].multiple?n(this[0]).find("option").filter(function(){return this.selected}).pluck("value"):this[0].value)},offset:function(t){if(t)return this.each(function(e){var i=n(this),r=J(this,t,e,i.offset()),o=i.offsetParent().offset(),s={top:r.top-o.top,left:r.left-o.left};"static"==i.css("position")&&(s.position="relative"),i.css(s)});if(!this.length)return null;var e=this[0].getBoundingClientRect();return{left:e.left+window.pageXOffset,top:e.top+window.pageYOffset,width:Math.round(e.width),height:Math.round(e.height)}},css:function(t,i){if(arguments.length<2){var r=this[0],o=getComputedStyle(r,"");if(!r)return;if("string"==typeof t)return r.style[C(t)]||o.getPropertyValue(t);if(A(t)){var s={};return n.each(A(t)?t:[t],function(t,e){s[e]=r.style[C(e)]||o.getPropertyValue(e)}),s}}var a="";if("string"==L(t))i||0===i?a=F(t)+":"+H(t,i):this.each(function(){this.style.removeProperty(F(t))});else for(e in t)t[e]||0===t[e]?a+=F(e)+":"+H(e,t[e])+";":this.each(function(){this.style.removeProperty(F(e))});return this.each(function(){this.style.cssText+=";"+a})},index:function(t){return t?this.indexOf(n(t)[0]):this.parent().children().indexOf(this[0])},hasClass:function(t){return t?r.some.call(this,function(t){return this.test(W(t))},q(t)):!1},addClass:function(t){return t?this.each(function(e){i=[];var r=W(this),o=J(this,t,e,r);o.split(/\s+/g).forEach(function(t){n(this).hasClass(t)||i.push(t)},this),i.length&&W(this,r+(r?" ":"")+i.join(" "))}):this},removeClass:function(e){return this.each(function(n){return e===t?W(this,""):(i=W(this),J(this,e,n,i).split(/\s+/g).forEach(function(t){i=i.replace(q(t)," ")}),void W(this,i.trim()))})},toggleClass:function(e,i){return e?this.each(function(r){var o=n(this),s=J(this,e,r,W(this));s.split(/\s+/g).forEach(function(e){(i===t?!o.hasClass(e):i)?o.addClass(e):o.removeClass(e)})}):this},scrollTop:function(e){if(this.length){var n="scrollTop"in this[0];return e===t?n?this[0].scrollTop:this[0].pageYOffset:this.each(n?function(){this.scrollTop=e}:function(){this.scrollTo(this.scrollX,e)})}},scrollLeft:function(e){if(this.length){var n="scrollLeft"in this[0];return e===t?n?this[0].scrollLeft:this[0].pageXOffset:this.each(n?function(){this.scrollLeft=e}:function(){this.scrollTo(e,this.scrollY)})}},position:function(){if(this.length){var t=this[0],e=this.offsetParent(),i=this.offset(),r=d.test(e[0].nodeName)?{top:0,left:0}:e.offset();return i.top-=parseFloat(n(t).css("margin-top"))||0,i.left-=parseFloat(n(t).css("margin-left"))||0,r.top+=parseFloat(n(e[0]).css("border-top-width"))||0,r.left+=parseFloat(n(e[0]).css("border-left-width"))||0,{top:i.top-r.top,left:i.left-r.left}}},offsetParent:function(){return this.map(function(){for(var t=this.offsetParent||a.body;t&&!d.test(t.nodeName)&&"static"==n(t).css("position");)t=t.offsetParent;return t})}},n.fn.detach=n.fn.remove,["width","height"].forEach(function(e){var i=e.replace(/./,function(t){return t[0].toUpperCase()});n.fn[e]=function(r){var o,s=this[0];return r===t?$(s)?s["inner"+i]:_(s)?s.documentElement["scroll"+i]:(o=this.offset())&&o[e]:this.each(function(t){s=n(this),s.css(e,J(this,r,t,s[e]()))})}}),v.forEach(function(t,e){var i=e%2;n.fn[t]=function(){var t,o,r=n.map(arguments,function(e){return t=L(e),"object"==t||"array"==t||null==e?e:T.fragment(e)}),s=this.length>1;return r.length<1?this:this.each(function(t,u){o=i?u:u.parentNode,u=0==e?u.nextSibling:1==e?u.firstChild:2==e?u:null;var f=n.contains(a.documentElement,o);r.forEach(function(t){if(s)t=t.cloneNode(!0);else if(!o)return n(t).remove();o.insertBefore(t,u),f&&G(t,function(t){null==t.nodeName||"SCRIPT"!==t.nodeName.toUpperCase()||t.type&&"text/javascript"!==t.type||t.src||window.eval.call(window,t.innerHTML)})})})},n.fn[i?t+"To":"insert"+(e?"Before":"After")]=function(e){return n(e)[t](this),this}}),T.Z.prototype=n.fn,T.uniq=N,T.deserializeValue=Y,n.zepto=T,n}();window.Zepto=Zepto,void 0===window.$&&(window.$=Zepto),function(t){function l(t){return t._zid||(t._zid=e++)}function h(t,e,n,i){if(e=p(e),e.ns)var r=d(e.ns);return(s[l(t)]||[]).filter(function(t){return!(!t||e.e&&t.e!=e.e||e.ns&&!r.test(t.ns)||n&&l(t.fn)!==l(n)||i&&t.sel!=i)})}function p(t){var e=(""+t).split(".");return{e:e[0],ns:e.slice(1).sort().join(" ")}}function d(t){return new RegExp("(?:^| )"+t.replace(" "," .* ?")+"(?: |$)")}function m(t,e){return t.del&&!u&&t.e in f||!!e}function g(t){return c[t]||u&&f[t]||t}function v(e,i,r,o,a,u,f){var h=l(e),d=s[h]||(s[h]=[]);i.split(/\s/).forEach(function(i){if("ready"==i)return t(document).ready(r);var s=p(i);s.fn=r,s.sel=a,s.e in c&&(r=function(e){var n=e.relatedTarget;return!n||n!==this&&!t.contains(this,n)?s.fn.apply(this,arguments):void 0}),s.del=u;var l=u||r;s.proxy=function(t){if(t=j(t),!t.isImmediatePropagationStopped()){t.data=o;var i=l.apply(e,t._args==n?[t]:[t].concat(t._args));return i===!1&&(t.preventDefault(),t.stopPropagation()),i}},s.i=d.length,d.push(s),"addEventListener"in e&&e.addEventListener(g(s.e),s.proxy,m(s,f))})}function y(t,e,n,i,r){var o=l(t);(e||"").split(/\s/).forEach(function(e){h(t,e,n,i).forEach(function(e){delete s[o][e.i],"removeEventListener"in t&&t.removeEventListener(g(e.e),e.proxy,m(e,r))})})}function j(e,i){return(i||!e.isDefaultPrevented)&&(i||(i=e),t.each(E,function(t,n){var r=i[t];e[t]=function(){return this[n]=x,r&&r.apply(i,arguments)},e[n]=b}),(i.defaultPrevented!==n?i.defaultPrevented:"returnValue"in i?i.returnValue===!1:i.getPreventDefault&&i.getPreventDefault())&&(e.isDefaultPrevented=x)),e}function S(t){var e,i={originalEvent:t};for(e in t)w.test(e)||t[e]===n||(i[e]=t[e]);return j(i,t)}var n,e=1,i=Array.prototype.slice,r=t.isFunction,o=function(t){return"string"==typeof t},s={},a={},u="onfocusin"in window,f={focus:"focusin",blur:"focusout"},c={mouseenter:"mouseover",mouseleave:"mouseout"};a.click=a.mousedown=a.mouseup=a.mousemove="MouseEvents",t.event={add:v,remove:y},t.proxy=function(e,n){var s=2 in arguments&&i.call(arguments,2);if(r(e)){var a=function(){return e.apply(n,s?s.concat(i.call(arguments)):arguments)};return a._zid=l(e),a}if(o(n))return s?(s.unshift(e[n],e),t.proxy.apply(null,s)):t.proxy(e[n],e);throw new TypeError("expected function")},t.fn.bind=function(t,e,n){return this.on(t,e,n)},t.fn.unbind=function(t,e){return this.off(t,e)},t.fn.one=function(t,e,n,i){return this.on(t,e,n,i,1)};var x=function(){return!0},b=function(){return!1},w=/^([A-Z]|returnValue$|layer[XY]$)/,E={preventDefault:"isDefaultPrevented",stopImmediatePropagation:"isImmediatePropagationStopped",stopPropagation:"isPropagationStopped"};t.fn.delegate=function(t,e,n){return this.on(e,t,n)},t.fn.undelegate=function(t,e,n){return this.off(e,t,n)},t.fn.live=function(e,n){return t(document.body).delegate(this.selector,e,n),this},t.fn.die=function(e,n){return t(document.body).undelegate(this.selector,e,n),this},t.fn.on=function(e,s,a,u,f){var c,l,h=this;return e&&!o(e)?(t.each(e,function(t,e){h.on(t,s,a,e,f)}),h):(o(s)||r(u)||u===!1||(u=a,a=s,s=n),(r(a)||a===!1)&&(u=a,a=n),u===!1&&(u=b),h.each(function(n,r){f&&(c=function(t){return y(r,t.type,u),u.apply(this,arguments)}),s&&(l=function(e){var n,o=t(e.target).closest(s,r).get(0);return o&&o!==r?(n=t.extend(S(e),{currentTarget:o,liveFired:r}),(c||u).apply(o,[n].concat(i.call(arguments,1)))):void 0}),v(r,e,u,a,s,l||c)}))},t.fn.off=function(e,i,s){var a=this;return e&&!o(e)?(t.each(e,function(t,e){a.off(t,i,e)}),a):(o(i)||r(s)||s===!1||(s=i,i=n),s===!1&&(s=b),a.each(function(){y(this,e,s,i)}))},t.fn.trigger=function(e,n){return e=o(e)||t.isPlainObject(e)?t.Event(e):j(e),e._args=n,this.each(function(){"dispatchEvent"in this?this.dispatchEvent(e):t(this).triggerHandler(e,n)})},t.fn.triggerHandler=function(e,n){var i,r;return this.each(function(s,a){i=S(o(e)?t.Event(e):e),i._args=n,i.target=a,t.each(h(a,e.type||e),function(t,e){return r=e.proxy(i),i.isImmediatePropagationStopped()?!1:void 0})}),r},"focusin focusout load resize scroll unload click dblclick mousedown mouseup mousemove mouseover mouseout mouseenter mouseleave change select keydown keypress keyup error".split(" ").forEach(function(e){t.fn[e]=function(t){return t?this.bind(e,t):this.trigger(e)}}),["focus","blur"].forEach(function(e){t.fn[e]=function(t){return t?this.bind(e,t):this.each(function(){try{this[e]()}catch(t){}}),this}}),t.Event=function(t,e){o(t)||(e=t,t=e.type);var n=document.createEvent(a[t]||"Events"),i=!0;if(e)for(var r in e)"bubbles"==r?i=!!e[r]:n[r]=e[r];return n.initEvent(t,i,!0),j(n)}}(Zepto),function(t){function l(e,n,i){var r=t.Event(n);return t(e).trigger(r,i),!r.isDefaultPrevented()}function h(t,e,i,r){return t.global?l(e||n,i,r):void 0}function p(e){e.global&&0===t.active++&&h(e,null,"ajaxStart")}function d(e){e.global&&!--t.active&&h(e,null,"ajaxStop")}function m(t,e){var n=e.context;return e.beforeSend.call(n,t,e)===!1||h(e,n,"ajaxBeforeSend",[t,e])===!1?!1:void h(e,n,"ajaxSend",[t,e])}function g(t,e,n,i){var r=n.context,o="success";n.success.call(r,t,o,e),i&&i.resolveWith(r,[t,o,e]),h(n,r,"ajaxSuccess",[e,n,t]),y(o,e,n)}function v(t,e,n,i,r){var o=i.context;i.error.call(o,n,e,t),r&&r.rejectWith(o,[n,e,t]),h(i,o,"ajaxError",[n,i,t||e]),y(e,n,i)}function y(t,e,n){var i=n.context;n.complete.call(i,e,t),h(n,i,"ajaxComplete",[e,n]),d(n)}function x(){}function b(t){return t&&(t=t.split(";",2)[0]),t&&(t==f?"html":t==u?"json":s.test(t)?"script":a.test(t)&&"xml")||"text"}function w(t,e){return""==e?t:(t+"&"+e).replace(/[&?]{1,2}/,"?")}function E(e){e.processData&&e.data&&"string"!=t.type(e.data)&&(e.data=t.param(e.data,e.traditional)),!e.data||e.type&&"GET"!=e.type.toUpperCase()||(e.url=w(e.url,e.data),e.data=void 0)}function j(e,n,i,r){return t.isFunction(n)&&(r=i,i=n,n=void 0),t.isFunction(i)||(r=i,i=void 0),{url:e,data:n,success:i,dataType:r}}function T(e,n,i,r){var o,s=t.isArray(n),a=t.isPlainObject(n);t.each(n,function(n,u){o=t.type(u),r&&(n=i?r:r+"["+(a||"object"==o||"array"==o?n:"")+"]"),!r&&s?e.add(u.name,u.value):"array"==o||!i&&"object"==o?T(e,u,i,n):e.add(n,u)})}var i,r,e=0,n=window.document,o=/<script\b[^<]*(?:(?!<\/script>)<[^<]*)*<\/script>/gi,s=/^(?:text|application)\/javascript/i,a=/^(?:text|application)\/xml/i,u="application/json",f="text/html",c=/^\s*$/;t.active=0,t.ajaxJSONP=function(i,r){if(!("type"in i))return t.ajax(i);var f,h,o=i.jsonpCallback,s=(t.isFunction(o)?o():o)||"jsonp"+ ++e,a=n.createElement("script"),u=window[s],c=function(e){t(a).triggerHandler("error",e||"abort")},l={abort:c};return r&&r.promise(l),t(a).on("load error",function(e,n){clearTimeout(h),t(a).off().remove(),"error"!=e.type&&f?g(f[0],l,i,r):v(null,n||"error",l,i,r),window[s]=u,f&&t.isFunction(u)&&u(f[0]),u=f=void 0}),m(l,i)===!1?(c("abort"),l):(window[s]=function(){f=arguments},a.src=i.url.replace(/\?(.+)=\?/,"?$1="+s),n.head.appendChild(a),i.timeout>0&&(h=setTimeout(function(){c("timeout")},i.timeout)),l)},t.ajaxSettings={type:"GET",beforeSend:x,success:x,error:x,complete:x,context:null,global:!0,xhr:function(){return new window.XMLHttpRequest},accepts:{script:"text/javascript, application/javascript, application/x-javascript",json:u,xml:"application/xml, text/xml",html:f,text:"text/plain"},crossDomain:!1,timeout:0,processData:!0,cache:!0},t.ajax=function(e){var n=t.extend({},e||{}),o=t.Deferred&&t.Deferred();for(i in t.ajaxSettings)void 0===n[i]&&(n[i]=t.ajaxSettings[i]);p(n),n.crossDomain||(n.crossDomain=/^([\w-]+:)?\/\/([^\/]+)/.test(n.url)&&RegExp.$2!=window.location.host),n.url||(n.url=window.location.toString()),E(n);var s=n.dataType,a=/\?.+=\?/.test(n.url);if(a&&(s="jsonp"),n.cache!==!1&&(e&&e.cache===!0||"script"!=s&&"jsonp"!=s)||(n.url=w(n.url,"_="+Date.now())),"jsonp"==s)return a||(n.url=w(n.url,n.jsonp?n.jsonp+"=?":n.jsonp===!1?"":"callback=?")),t.ajaxJSONP(n,o);var j,u=n.accepts[s],f={},l=function(t,e){f[t.toLowerCase()]=[t,e]},h=/^([\w-]+:)\/\//.test(n.url)?RegExp.$1:window.location.protocol,d=n.xhr(),y=d.setRequestHeader;if(o&&o.promise(d),n.crossDomain||l("X-Requested-With","XMLHttpRequest"),l("Accept",u||"*/*"),(u=n.mimeType||u)&&(u.indexOf(",")>-1&&(u=u.split(",",2)[0]),d.overrideMimeType&&d.overrideMimeType(u)),(n.contentType||n.contentType!==!1&&n.data&&"GET"!=n.type.toUpperCase())&&l("Content-Type",n.contentType||"application/x-www-form-urlencoded"),n.headers)for(r in n.headers)l(r,n.headers[r]);if(d.setRequestHeader=l,d.onreadystatechange=function(){if(4==d.readyState){d.onreadystatechange=x,clearTimeout(j);var e,i=!1;if(d.status>=200&&d.status<300||304==d.status||0==d.status&&"file:"==h){s=s||b(n.mimeType||d.getResponseHeader("content-type")),e=d.responseText;try{"script"==s?(1,eval)(e):"xml"==s?e=d.responseXML:"json"==s&&(e=c.test(e)?null:t.parseJSON(e))}catch(r){i=r}i?v(i,"parsererror",d,n,o):g(e,d,n,o)}else v(d.statusText||null,d.status?"error":"abort",d,n,o)}},m(d,n)===!1)return d.abort(),v(null,"abort",d,n,o),d;if(n.xhrFields)for(r in n.xhrFields)d[r]=n.xhrFields[r];var S="async"in n?n.async:!0;d.open(n.type,n.url,S,n.username,n.password);for(r in f)y.apply(d,f[r]);return n.timeout>0&&(j=setTimeout(function(){d.onreadystatechange=x,d.abort(),v(null,"timeout",d,n,o)},n.timeout)),d.send(n.data?n.data:null),d},t.get=function(){return t.ajax(j.apply(null,arguments))},t.post=function(){var e=j.apply(null,arguments);return e.type="POST",t.ajax(e)},t.getJSON=function(){var e=j.apply(null,arguments);return e.dataType="json",t.ajax(e)},t.fn.load=function(e,n,i){if(!this.length)return this;var a,r=this,s=e.split(/\s/),u=j(e,n,i),f=u.success;return s.length>1&&(u.url=s[0],a=s[1]),u.success=function(e){r.html(a?t("<div>").html(e.replace(o,"")).find(a):e),f&&f.apply(r,arguments)},t.ajax(u),this};var S=encodeURIComponent;t.param=function(t,e){var n=[];return n.add=function(t,e){this.push(S(t)+"="+S(e))},T(n,t,e),n.join("&").replace(/%20/g,"+")}}(Zepto),function(t){t.fn.serializeArray=function(){var n,e=[];return t([].slice.call(this.get(0).elements)).each(function(){n=t(this);var i=n.attr("type");"fieldset"!=this.nodeName.toLowerCase()&&!this.disabled&&"submit"!=i&&"reset"!=i&&"button"!=i&&("radio"!=i&&"checkbox"!=i||this.checked)&&e.push({name:n.attr("name"),value:n.val()})}),e},t.fn.serialize=function(){var t=[];return this.serializeArray().forEach(function(e){t.push(encodeURIComponent(e.name)+"="+encodeURIComponent(e.value))}),t.join("&")},t.fn.submit=function(e){if(e)this.bind("submit",e);else if(this.length){var n=t.Event("submit");this.eq(0).trigger(n),n.isDefaultPrevented()||this.get(0).submit()}return this}}(Zepto),function(t){"__proto__"in{}||t.extend(t.zepto,{Z:function(e,n){return e=e||[],t.extend(e,t.fn),e.selector=n||"",e.__Z=!0,e},isZ:function(e){return"array"===t.type(e)&&"__Z"in e}});try{getComputedStyle(void 0)}catch(e){var n=getComputedStyle;window.getComputedStyle=function(t){try{return n(t)}catch(e){return null}}}}(Zepto);//quick hack to have a jquery var albeit using zepto
var jQuery=Zepto;/*!
 * actual 0.2.0+201402061122
 * https://github.com/ryanve/actual
 * MIT License 2014 Ryan Van Etten
 */
!function(a,b,c){"undefined"!=typeof module&&module.exports?module.exports=c():a[b]=c()}(this,"actual",function(){function a(b,c,d,e){var f,g,h,i,j=a.mq;for(c="string"==typeof c?c:"",d=d>0?c?+d:d>>0:1,e=e>0?+e:0>e?-e:"px"==c?256:c?32:1,b+=":",c+=")",i=d;e&&i>=0;i+=e){if(h=j("(min-"+b+i+c),g=j("(max-"+b+i+c),h&&g)return j("("+b+(i>>0)+c)?i>>0:i;null==f?e=(f=!g)?h&&e:-e:(g?f:!f)&&(f=!f,e=-e/2)}return 0}function b(b){return function(c){return a(c,b)}}function c(b){return function(c){return a(b,c)}}var d="matchMedia",e="undefined"!=typeof window&&window;return a.actual=a,a.feature=c,a.as=b,a.mq=e[d]||e[d="msMatchMedia"]?function(a){return!!e[d](a).matches}:function(){return!1},a});//browser keycodes to linux keycodes
var keylinux=[
0,  //KEY_RESERVED            0
27, //KEY_ESC                 1
49, //KEY_1                   2
50, //KEY_2                   3
51, //KEY_3                   4
52, //KEY_4                   5
53, //KEY_5                   6
54, //KEY_6                   7
55, //KEY_7                   8
56, //KEY_8                   9
57, //KEY_9                   10
48, //KEY_0                   11
219,//KEY_LEFTBRACE           12
221,//KEY_RIGHTBRACE          13
8,  //KEY_BACKSPACE           14
9,  //KEY_TAB                 15
81, //KEY_Q                   16
87, //KEY_W                   17
69, //KEY_E                   18
82, //KEY_R                   19
84, //KEY_T                   20
90, //KEY_Z                   21
85, //KEY_U                   22
73, //KEY_I                   23
79, //KEY_O                   24
80, //KEY_P                   25
186,//KEY_SEMICOLON           26
187,//KEY_EQUAL               27
13, //KEY_ENTER               28
17, //KEY_LEFTCTRL            29
65, //KEY_A                   30
83, //KEY_S                   31
68, //KEY_D                   32
70, //KEY_F                   33
71, //KEY_G                   34
72, //KEY_H                   35
74, //KEY_J                   36
75, //KEY_K                   37
76, //KEY_L                   38
192,//KEY_GRAVE               39
222,//KEY_APOSTROPHE          40
192,//KEY_SLASH               53
16, //KEY_LEFTSHIFT           42
220,//KEY_BACKSLASH           43
89, //KEY_Y                   44
88, //KEY_X                   45
67, //KEY_C                   46
86, //KEY_V                   47
66, //KEY_B                   48
78, //KEY_N                   49
77, //KEY_M                   50
188,//KEY_COMMA               51
190,//KEY_DOT                 52
189,//KEY_MINUS               41
16, //KEY_RIGHTSHIFT          54
106,//KEY_KPASTERISK          55
18, //KEY_LEFTALT             56
32, //KEY_SPACE               57
20, //KEY_CAPSLOCK            58
112,//KEY_F1                  59
113,//KEY_F2                  60
114,//KEY_F3                  61
115,//KEY_F4                  62
116,//KEY_F5                  63
117,//KEY_F6                  64
118,//KEY_F7                  65
119,//KEY_F8                  66
120,//KEY_F9                  67
121,//KEY_F10                 68
144,//KEY_NUMLOCK             69
145,//KEY_SCROLLLOCK          70
103,//KEY_KP7                 71
104,//KEY_KP8                 72
105,//KEY_KP9                 73
109,//KEY_KPMINUS             74
100,//KEY_KP4                 75
101,//KEY_KP5                 76
102,//KEY_KP6                 77
107,//KEY_KPPLUS              78
97, //KEY_KP1                 79
98, //KEY_KP2                 80
99, //KEY_KP3                 81
96, //KEY_KP0                 82
110,//KEY_KPDOT               83
0,	//						  84   						
0,  //KEY_ZENKAKUHANKAKU      85
0,  //KEY_102ND               86
0,  //KEY_F11                 87
0,  //KEY_F12                 88
0,  //KEY_RO                  89
0,  //KEY_KATAKANA            90
0,  //KEY_HIRAGANA            91
0,  //KEY_HENKAN              92
0,  //KEY_KATAKANAHIRAGANA    93
0,  //KEY_MUHENKAN            94
0,  //KEY_KPJPCOMMA           95
13, //KEY_KPENTER             96
17, //KEY_RIGHTCTRL           97
111,//KEY_KPSLASH             98
0,  //KEY_SYSRQ               99
18, //KEY_RIGHTALT            100
0,  //KEY_LINEFEED            101
36, //KEY_HOME                102
38, //KEY_UP                  103
33, //KEY_PAGEUP              104
37, //KEY_LEFT                105
39, //KEY_RIGHT               106
35, //KEY_END                 107
40, //KEY_DOWN                108
34, //KEY_PAGEDOWN            109
45, //KEY_INSERT              110
46, //KEY_DELETE              111
0,  //KEY_MACRO               112
0,  //KEY_MUTE                113
0,  //KEY_VOLUMEDOWN          114
0,  //KEY_VOLUMEUP            115
0,  //KEY_POWER               116     /* SC System Power Down */
12, //KEY_KPEQUAL             117
0,  //KEY_KPPLUSMINUS         118
19, //KEY_PAUSE               119
0,  //KEY_SCALE               120
];/* Modernizr 2.8.3 (Custom Build) | MIT & BSD
 * Build: http://modernizr.com/download/#-backgroundsize-prefixed-testprop-testallprops-domprefixes-pointerlock_api
 */
;window.Modernizr=function(a,b,c){function w(a){i.cssText=a}function x(a,b){return w(prefixes.join(a+";")+(b||""))}function y(a,b){return typeof a===b}function z(a,b){return!!~(""+a).indexOf(b)}function A(a,b){for(var d in a){var e=a[d];if(!z(e,"-")&&i[e]!==c)return b=="pfx"?e:!0}return!1}function B(a,b,d){for(var e in a){var f=b[a[e]];if(f!==c)return d===!1?a[e]:y(f,"function")?f.bind(d||b):f}return!1}function C(a,b,c){var d=a.charAt(0).toUpperCase()+a.slice(1),e=(a+" "+m.join(d+" ")+d).split(" ");return y(b,"string")||y(b,"undefined")?A(e,b):(e=(a+" "+n.join(d+" ")+d).split(" "),B(e,b,c))}var d="2.8.3",e={},f=b.documentElement,g="modernizr",h=b.createElement(g),i=h.style,j,k={}.toString,l="Webkit Moz O ms",m=l.split(" "),n=l.toLowerCase().split(" "),o={},p={},q={},r=[],s=r.slice,t,u={}.hasOwnProperty,v;!y(u,"undefined")&&!y(u.call,"undefined")?v=function(a,b){return u.call(a,b)}:v=function(a,b){return b in a&&y(a.constructor.prototype[b],"undefined")},Function.prototype.bind||(Function.prototype.bind=function(b){var c=this;if(typeof c!="function")throw new TypeError;var d=s.call(arguments,1),e=function(){if(this instanceof e){var a=function(){};a.prototype=c.prototype;var f=new a,g=c.apply(f,d.concat(s.call(arguments)));return Object(g)===g?g:f}return c.apply(b,d.concat(s.call(arguments)))};return e}),o.backgroundsize=function(){return C("backgroundSize")};for(var D in o)v(o,D)&&(t=D.toLowerCase(),e[t]=o[D](),r.push((e[t]?"":"no-")+t));return e.addTest=function(a,b){if(typeof a=="object")for(var d in a)v(a,d)&&e.addTest(d,a[d]);else{a=a.toLowerCase();if(e[a]!==c)return e;b=typeof b=="function"?b():b,typeof enableClasses!="undefined"&&enableClasses&&(f.className+=" "+(b?"":"no-")+a),e[a]=b}return e},w(""),h=j=null,e._version=d,e._domPrefixes=n,e._cssomPrefixes=m,e.testProp=function(a){return A([a])},e.testAllProps=C,e.prefixed=function(a,b,c){return b?C(a,b,c):C(a,"pfx")},e}(this,this.document),Modernizr.addTest("pointerlock",!!Modernizr.prefixed("pointerLockElement",document));(function($){
 
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
 
})(jQuery);function AtariScreen(M,N,O,z){function D(){var a=l.getContext("2d");w?(l.width=640,l.height=400,a.setTransform(1,0,0,1,0,0),a.scale(x,y),a.imageSmoothingEnabled=!1):(l.width=p,l.height=r);return a}function A(a){a="undefined"===typeof a?0:a;a=0>a||2<a?0:a;switch(a){case 0:n=4;y=x=2;p=320;r=200;q(new Uint16Array([4095,3840,240,4080,15,3855,255,1365,819,3891,1011,4083,831,3903,1023,0]));break;case 1:n=2;x=1;y=2;p=640;r=200;q(new Uint16Array([4095,3840,240,0]));break;case 2:y=x=n=1,p=640,r=400,q(new Uint16Array([0,
4095]))}var b=D();b.fillStyle=B[0];b.fillRect(0,0,p,r);return E=a}function F(){if(u){u=!1;var a=D(),b=a.createImageData(p,r),c=s.length/n,d=0,e=0,C,f,m,t,g,h=Array(n);for(C=0;C<c;C++){for(f=0;f<n;f++)h[f]=s[e++];for(f=15;-1<f;--f){t=0;g=1<<f;for(m=0;m<n;m++)h[m]&g&&(t+=1<<m);b.data[d++]=k[t][0];b.data[d++]=k[t][1];b.data[d++]=k[t][2];b.data[d++]=255}}a.putImageData(b,0,0);w&&a.drawImage(l,0,0);u=!0}return u}function q(a){var b=a.length;v=Array(b);B=Array(b);k=Array(b);g=Array(b);for(var c=0;c<b;c++)G(c,
a[c]);this.palette=v}function G(a,b){var c=(((b&1792)>>7)+((b&2048)>>11)).toString(16),d=(((b&112)>>3)+((b&128)>>7)).toString(16),e=(((b&7)<<1)+((b&8)>>3)).toString(16);v[a]=b;B[a]="#"+c+c+d+d+e+e;k[a]=[parseInt(c+c,16),parseInt(d+d,16),parseInt(e+e,16)]}function H(a,b){for(var c=v.length,d=new Uint16Array(c),e=0;e<c;e++)d[e]=a.getUint16(b),b+=2;q(d)}function I(a,b){for(var c=s.length,d=0;d<c;d++)s[d]=a.getUint16(b),b+=2;return b}function J(a,b){var c=160/n,d=c/2,e,g,f,m,k,h,l;e=new ArrayBuffer(160);
var p=new DataView(e),q=0;for(e=0;e<r;e++){for(g=m=0;g<n;g++)for(k=0;k<c;)if(h=a.getUint8(b++),0<(h&128))for(l=256-h+1,h=a.getUint8(b++),f=0;f<l;f++)p.setUint8(m++,h),k++;else for(h++,f=0;f<h;f++)p.setUint8(m++,a.getUint8(b++)),k++;for(g=m=0;g<d;g++){for(f=0;f<n;f++)s[q+f]=p.getUint16(m+f*c);m+=2;q+=n}}return b}function K(a){var b=!1;if(0<=a&&3>=a){var c=h[a];if(c.on){a=b=c.left_colour;var d=c.right_colour,e=c.position,e=e-c.direction;e>d&&(e=a);e<a&&(e=d);c.position=e;for(var c=c.length,l=0;l<c;l++)e>
d&&(e=b),k[a++]=[g[e][0],g[e][1],g[e++][2]];b=!0}}return b}function L(a){var b=-1;if(0<=a&&3>=a){var c=h[a];0!=c.direction&&(b=c.animationId=setInterval(function(){K(a);requestAnimationFrame(function(){F()})},c.delay),c.animating=!0)}return b}void 0===z&&(z=!0);var v,B,k,g,p,r,E,n,x,y,w=z,u=!0,s=new Uint16Array(16E3),h=[],l=N.appendChild(document.createElement("canvas"));l.id=O;l.width=640;l.height=400;this.width=function(){return p};this.height=function(){return r};this.planes=function(){return n};
this.mode=function(){return E};this.ready=function(){return u};this.palette=v;this.screen_memory=s;this.cycles=h;Object.defineProperty(this,"scale",{get:function(){return w},set:function(a){w=a}});this.SetMode=A;this.Display=F;this.SetPalette=q;this.SetPaletteValue=G;this.ExtractDegasElite=function(a){var b=new DataView(a),c=b.getUint16(0);A(c&255);H(b,2);c=0<(c&32768)?J(b,34):I(b,34);if(32==a.byteLength-c){h=[];for(a=0;4>a;a++){var d={};d.left_colour=b.getUint16(c+2*a);d.right_colour=b.getUint16(c+
2*a+8);d.direction=b.getUint16(c+2*a+16)-1;d.delay=Math.round(1E3*(128-b.getUint16(c+2*a+24))/60);d.position=0;d.length=d.right_colour-d.left_colour+1;d.on=!1;d.animating=!1;h.push(d)}this.cycles=h}};this.ExtractPalette=H;this.ExtractPlanarScreen=I;this.ExtractRLEData=J;this.StartCycle=function(a,b){void 0===b&&(b=!1);var c=!1;if(0<=a&&3>=a){var d=h[a];if(0!=d.direction){c=k.length;g=Array(c);for(var e=0;e<c;e++)g[e]=[k[e][0],k[e][1],k[e][2]];d.position=0;d.length=d.right_colour-d.left_colour+1;d.on=
!0;b&&L(a);c=!0}}return c};this.GetNextCycle=K;this.StartCycleAnimation=L;this.StopCycle=function(a){var b=!1;if(0<=a&&3>=a&&(a=h[a],a.on)){a.animating&&clearInterval(a.animationId);a.animating=!1;b=g.length;k=Array(b);for(var c=0;c<b;c++)k[c]=[g[c][0],g[c][1],g[c][2]];a.on=!1;b=!0}return b};A(M)};var CosmosEx = CosmosEx || {};
CosmosEx.Screencast=function(c){
	var config = c || {reloadms:200};
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
			if( config.reloadms>0 ){
				setTimeout(loadScreen,config.reloadms);
			}
		};
	
	var onError=function(e) {
			if( config.reloadms>0 ){
				setTimeout(loadScreen,config.reloadms);
			}
		};
	var loadScreen=function(){
		var xhr = new XMLHttpRequest();
		cnt++;
		xhr.open('GET', '/screencast/getscreen?'+cnt, true);
		xhr.responseType = 'arraybuffer';
		xhr.onerror = onError; 
		xhr.onload = onLoad;
		
		xhr.send(null);
	    xhr = null;
	};
	loadScreen();
	return {
		loadScreen:loadScreen
	}
};/*================================================================
  CosmosEx simple remote control
  ================================================================
  Just a quick usage example for the CosmosEx HTTP Mouse/Keybard API
  sends keyboard events as linux keycodes to the HTTP API as well
  as relative mouse packets
  ----------------------------------------------------------------
  RESTlike interface, using json

  /hid/mouse 	POST
	Mousemove: 
	{ "type":"relative", "x": 8, "y": 21 }
	Mouse click:
	{ "type": "buttonleft", "state": "down" }
	{ "type": "buttonright", "state": "up" }

  /hid/keyboard 	POST 
	{ "type": "pc", "code": 40, "state": "down" };
	{ "type": "pc", "code": 40, "state": "up" };
	
	(keycodes are linux keycodes, can be converted from browser keycodes via array in keylinux.js)

  ----------------------------------------------------------------*/
//TODO: 
//		ANDROID:
//		- Chrome 31 on android does always send code 0 on key events (see https://code.google.com/p/chromium/issues/detail?id=118639)
//		  at least Chrome 35 is needed, but even then certain special keys won't work....
// 		POINTERLOCK:
// 		- when pointer is locked, the user needs an alternative for the ESC key on native keyboard, which is unusable in that mode (as it leaves pointer lock mode)
// 		ST KEYBOARD:
//		- st keyboard on android lets the mouse move - event bubble up for some odd reason
//		- multitouch for st keyboard
//		- send st keycodes
//    TOUCH DEVICES:
//    - mouse drag
//    - better orientation support

var CosmosEx = CosmosEx || {};
CosmosEx.Remote=function(){
    	//Mouse position states
    	var iMouseX=null;
    	var iMouseY=null;
    	var iTouchMouseX=null;
    	var iTouchMouseY=null;
    	var iCurrentDX=null;
    	var iCurrentDY=null;
    	//touch devices keyboard status
    	var bShowKeyboard=false;
    	//Help display status
    	var bShowHelp=false;
    	//ST imagemap-keyboard display state
    	var bShowStKeyboard=false;
    	//a simple "queue" - gather only one mouse delta per timeslice
    	var currentMouseMove=null;
    	//helper canvas for pointerlock (grabbing the pointer in the window), only works on Chrome/FF/Safari/IE, not used on touch devices 
    	var canvas=null;
    	//pointerlock states
    	var bHasPointerLock=false;
    	var bPointerLocked=null;
    	var iLastSendKeyDown=null;
    	/* ==========================
    	 * 			private functions
    	 */
    	var onMouseMove=function(e){
				//console.log("mousemove");
				//console.log(e);
				//store initial position
				if( iMouseX==null && iMouseY==null ){
					iMouseX=e.clientX;
					iMouseY=e.clientY;
				}
				//add this delta to the 1-entry queue. will be dscarded when mouse is moved further, the delta is recalculated until it is send
				(function(){
					var iDX=e.clientX-iMouseX;
					var iDY=e.clientY-iMouseY;
					//we might have had succes locking the pointer onto the canvas
					if( bPointerLocked ){
						//use the delta values given, but use a sum - the fixed sending time slice would otherwise miss deltas
						iCurrentDX+=e.movementX ||
			                		e.mozMovementX ||
			                		e.webkitMovementX ||
                					0;
						iCurrentDY+=e.movementY ||
					                e.mozMovementY ||
					                e.webkitMovementY ||
		                			0;
		               	iDX=iCurrentDX;
		               	iDY=iCurrentDY;
					}
					var iX=e.clientX;
					var iY=e.clientY;
					currentMouseMove=function(){
						sendMouseRelative(iDX,iDY)
						//update last position with the one we send
						iMouseX=iX;
						iMouseY=iY;
						iCurrentDX=0;
						iCurrentDY=0;
					};
				})();
			};
		var onTouchStart=function(e){
				//console.log("onTouchStart");			
				//currentMouseMove=null;
				iTouchMouseX=null;
				iTouchMouseY=null;
			};
		var onTouchEnd=function(e){
				//console.log("onTouchEnd");			
				//currentMouseMove=null;
				iTouchMouseX=null;
				iTouchMouseY=null;
			};
		var onTouchMove=function(e){
				currentMouseMove=null;
				e.preventDefault();
				var touch = e.touches[0]; // || e.changedTouches[0];
				//store initial position
				if( iTouchMouseX==null && iTouchMouseY==null ){
					iTouchMouseX=touch.pageX;
					iTouchMouseY=touch.pageY;
				}
				(function(){
					var iDX=touch.pageX-iTouchMouseX;
					var iDY=touch.pageY-iTouchMouseY;
					var iX=touch.pageX;
					var iY=touch.pageY;
					currentMouseMove=function(){
						//update last position with the one we are sending
						iTouchMouseX=iX;
						iTouchMouseY=iY;
						sendMouseRelative(iDX,iDY)
					};
				})();

			};
		var sendMouseRelative=function(iDX,iDY){
				var transmit=function(iDX,iDY){
					$.ajax({
						type: 'POST',
						url: '/hid/mouse',
						data: JSON.stringify({ "type":"relative", "x": iDX, "y": iDY }),
						contentType: 'application/json'
					});
				}
				//we only can transmit movement distance of -128 to 127 in one go, so slice this into several packets if necessary
				if( !(iDX>=-128 && iDX<128 && iDY>=-128 && iDY<128) ){
					//slice DX
					while(iDX>127 || iDX<-128){
						//depending on delta negative or positive, transmit maximum packet until we reach a value lower than the maximum trnsmittable value
						if( iDX>=0 ){
							transmit(127,0);
							iDX-=127
						}else{
							transmit(-128,0);
							iDX+=128
						}
					}
					//slice DY
					while(iDY>127 || iDY<-128){
						if( iDY>=0 ){
							transmit(0,127);
							iDY-=127
						}else{
							transmit(0,-128);
							iDY+=128
						}
					}
				}
				//transmit rest if any movement left
				if( iDX!=0 || iDY!=0 ){
					transmit(iDX,iDY);
				}

			};	
    	var onMouseDown=function(e){
    			if( e.which==1 ){
    				e.preventDefault();
	    			sendMouseKey("buttonleft","down");
	    			return false;
    			}
    			if( e.which==3 ){
    				e.preventDefault();
	    			sendMouseKey("buttonright","down");
	    			return false;
    			}
			};
    	var onMouseUp=function(e){
    			if( e.which==1 ){
    				e.preventDefault();
	    			sendMouseKey("buttonleft","up");
	    			return false;
    			}
    			if( e.which==3 ){
    				e.preventDefault();
	    			sendMouseKey("buttonright","up");
	    			return false;
    			}
			};
		//touch device - double tap triggers mouse click
    	var onDoubleTap=function(e){
    			sendMouseKey("buttonleft","down");
    			setTimeout(function(){sendMouseKey("buttonleft","up")},100);
			};
		//touch device - double tap with 2 fingers triggers double click
		var onDoubleTap2Fingers=function(e){
    			sendMouseKey("buttonleft","down");
    			setTimeout(function(){
    				sendMouseKey("buttonleft","up")
	    			setTimeout(function(){
	    				sendMouseKey("buttonleft","down")
		    			setTimeout(function(){
		    				sendMouseKey("buttonleft","up")
		    			},100);
	    			},100);
    			},100);
			};
	    var sendMouseKey=function(sType,sState){
				$.ajax({
					type: 'POST',
					url: '/hid/mouse',
					data: JSON.stringify({ "type": sType, "state": sState }),
					contentType: 'application/json'
				});
			};
    	var onKeyDown=function(e){
    			var iLinuxCode=$.inArray(e.keyCode,keylinux);
    			//console.log("Key ",e.keyCode);
    			if( iLinuxCode>=0 ){
    				e.preventDefault();
	    			sendKey(iLinuxCode,"down");
	    			iLastSendKeyDown=new Date().getTime();
    			} else{
    				//console.log("Key not found",e.keyCode);
    			}
	    	};
    	var onKeyUp=function(e){
    			var iLinuxCode=$.inArray(e.keyCode,keylinux);
    			//console.log("Key ",e.keyCode);
    			if( iLinuxCode>=0 ){
    				e.preventDefault();
    				//some touch device keyboard (i.e. iOS) send keydown/keyup 1-4ms apart - thats a bit too fast. 
    				//Keydown/up even could race each other, resulting in keyup arriving first. So we better add a delay if necessary.
	    			var iNow=new Date().getTime();
    				if( iNow-iLastSendKeyDown>20 ){
		    			sendKey(iLinuxCode,"up");
    				} else{
    					window.setTimeout(function(){
    						sendKey(iLinuxCode,"up");
    					},50);
    				}
    			} else{
    				//console.log("Key not found",e.keyCode);
    			}
	    	};
	    var handleStKeyboardKey=function(xThis,e,sState,iX,iY){
        		var iKeysInRow=xThis.data("count") || 1;
        		var lsLinuxKeyCodes=xThis.data("linuxcodes")+"";
        		var asKeyCodes=lsLinuxKeyCodes.split(",");
				if( iKeysInRow==1 ){
					sendKey(asKeyCodes[0]|0,sState); // //important: cast keycode to int
				}else{
					var offset = $("#keyboard").offset();
					var iImageX = offset.left - $(window).scrollLeft();
					var iImageY = offset.top - $(window).scrollTop();
	        		iX-=iImageX;
					iY-=iImageY;
					var liCoords=xThis.attr("coords");
					var aiCoords=liCoords.split(",");
					iX-=aiCoords[0];
					iY-=aiCoords[1];
					var iW=aiCoords[2]-aiCoords[0];
					var iKeyInRow=Math.floor(iX/(iW/iKeysInRow));
					var iKeyCode=asKeyCodes[iKeyInRow]|0; //important: cast keycode to int
					sendKey(iKeyCode,sState);
				}
        	};

	    var sendKey=function(iCode,sState){
	    		var xData={ "type": "pc", "code": iCode, "state":sState };
				//console.log(xData);
				//return;	    		
				$.ajax({
					type: 'POST',
					url: '/hid/keyboard',
					data: JSON.stringify(xData),
					contentType: 'application/json'
				});
			};
		var initPointerLock=function(){
			/* Rresize the canvas to occupy the full page, 
			   by getting the widow width and height and setting it to canvas*/
			 
			//check if we have pointerlock available
			var canvas=$("#canvas").get()[0];
			rpl = 	canvas.requestPointerLock ||
                	canvas.mozRequestPointerLock ||
                	canvas.webkitRequestPointerLock || 
                	null;
            if( rpl!=null ){
            	//register the request function under a normalized name
				canvas.requestPointerLock = rpl;
				// register the callback when a pointerlock event occurs
	        	document.addEventListener('pointerlockchange', onPointerLockchange, false);
	        	document.addEventListener('mozpointerlockchange', onPointerLockchange, false);
	        	document.addEventListener('webkitpointerlockchange', onPointerLockchange, false);   

            	bHasPointerLock=true;
            }else{
            	//no pointerlock - set mock function and disable capability
            	canvas.requestPointerLock=function(){};
            	bHasPointerLock=false;
            }
		};
		//ask browser to ask user to allow hiding of the cursor
		var activatePointerLock=function(){
			//only if we have pointerlock available
			if( bHasPointerLock ){
				var canvas=$("#canvas").get()[0];
				canvas.requestPointerLock();
			}
		};
		//keep track of pointerlock status - mosemove gets different eventobjects when enabled
		var onPointerLockchange=function(e){
			var ple=e.target.pointerLockElement || e.target.mozPointerLockElement || e.target.webkitPointerLockElement || null;
			if( ple==null){
				bPointerLocked=false;
			}else{
				bPointerLocked=true;
			}
		};
		var ignoreEvent=function(e){
        		e.preventDefault();
        		e.stopPropagation();
				return false;	        		
        	};
        var toggleSystemKeyboard=function(e){
        		e.preventDefault();
        		e.stopPropagation();
        		bShowKeyboard=!bShowKeyboard;
        		if(bShowKeyboard){
	        		$("#keyboardtrigger").focus();
        		}else{
	        		$("#keyboardtrigger").blur();
        		}
        		return false;
        	};
        var toggleStKeyboard=function(e){
        		e.preventDefault();
        		e.stopPropagation();
        		bShowStKeyboard=!bShowStKeyboard;
        		if(bShowStKeyboard){
	        		$("#keyboardlayer").show();
        		}else{
	        		$("#keyboardlayer").hide();
        		}
        		return false;
        	};
        var toggleHelp=function(e){
        		e.preventDefault();
        		e.stopPropagation();
        		bShowHelp=!bShowHelp;
        		if(bShowHelp){
	        		$(".alert.help.current").show();
        		}else{
	        		$(".alert.help.current").hide();
        		}
        		return false;
        	};
        var closeAlert=function(e){
         		e.preventDefault();
         		e.stopPropagation();
            $(this).parent().hide();
          };
        //bind mouse click to an element in a way, that's not sending an ST click
        var bindClickHelper=function(selector,funcEvent){
            var $element=$(selector);
				    $element.bind('touchstart', funcEvent);
				    $element.bind('touchend', ignoreEvent);
	        	$element.mousedown(funcEvent);
	        	$element.mouseup(ignoreEvent);
	        	$element.click(ignoreEvent);
          };
    	/* ==========================
    	 * 			 public functions
    	 */
    	return {
	        init: function(){
            //remove background if not supported properly
            if( !Modernizr.backgroundsize ){
              $("body").removeClass("background");
            }
	        	//ST keyboard
	        	$("area").mousedown(function(e){
	        		e.preventDefault();
	        		e.stopPropagation();
	        		handleStKeyboardKey($(this),e,"down",e.pageX,e.pageY);
	        	});
	        	$("area").bind("touchstart",function(e){
	        		e.preventDefault();
	        		e.stopPropagation();
	        		handleStKeyboardKey($(this),e,"down",e.changedTouches[0].pageX,e.changedTouches[0].pageY);
	        	});
	        	$("area").mouseup(function(e){
	        		e.preventDefault();
	        		e.stopPropagation();
	        		handleStKeyboardKey($(this),e,"up",e.pageX,e.pageY);
	        	});
	        	$("area").bind("touchend",function(e){
	        		e.preventDefault();
	        		e.stopPropagation();
	        		handleStKeyboardKey($(this),e,"up",e.changedTouches[0].pageX,e.changedTouches[0].pageY);
	        	});
	        	//input pad
	        	$(window).mousemove(onMouseMove);
				    $(document).bind('touchstart', onTouchStart);
				    $(document).bind('touchend', onTouchEnd);
				    $(document).bind('touchmove', onTouchMove);
	        	$(window).mousedown(onMouseDown);
	        	$(window).mouseup(onMouseUp);
	        	$(window).keydown(onKeyDown);
	        	$(window).keyup(onKeyUp);
	        	$(window).doubletap(onDoubleTap,onDoubleTap2Fingers,300);

	        	var bHasTouch = /android|iphone|ipad/i.test(navigator.userAgent.toLowerCase());

            //only if canvas is supported
            var bFeatureMissing=false;
	       	  initPointerLock();
            if( !bHasPointerLock ){
                bFeatureMissing=true;
            }         
            if( bHasTouch ){
                bFeatureMissing=false;
            } 
            if( bFeatureMissing ){
              $("#info-features").removeClass("hidden");
            }

	        	//timer for sending mouse movements every 1/10s
	        	window.setInterval(function(){
	        		if( currentMouseMove!=null ){
	        			//empty queue before we send, to make sure we don't call the queue with the old value again if this operation takes >50ms for some reason
	        			var x=currentMouseMove;
	        			currentMouseMove=null;
	        			x();
	        		}
	        	}, 50);

	        	//UI
            //Toggle System Keyboard Button
            bindClickHelper("button#showkeyboard",toggleSystemKeyboard);
            //Toggle ST Keyboard Button
            bindClickHelper("button#showstkeyboard",toggleStKeyboard);
            //Help! Button
            bindClickHelper("button#help",toggleHelp);
            //Send CTRL+ALT+DEL
            bindClickHelper("button#keyctrlaltdel",function(e){
            		e.preventDefault();
					e.stopPropagation();
					if( confirm("Really send CTRL-ALT-DEL to the ST?") ){
						sendKey(29,"down");
						sendKey(56,"down");
						sendKey(111,"down");
						setTimeout(function(){
							sendKey(29,"up");
							sendKey(56,"up");
							sendKey(111,"up");
						},100);
					}
                });
            //Start recieving screencast from ST
            bindClickHelper("button#screencast",function(e){
					if( typeof CosmosEx.Screencast!="undefined" && !$(this).hasClass("disabled") ){
						$(this).attr("disabled", "disabled");
						$(this).addClass("disabled");
						$(this).html('Screncast running...');
						CosmosEx.Screencast();
					}
                });

            //close feature bubble on x
            bindClickHelper("#info-features a.close",closeAlert);
            //close help
            bindClickHelper(".alert.help a.close",toggleHelp);
            //Grab mouse Button
            bindClickHelper("button#lockpointer",function(e){
	        		e.preventDefault();
	        		e.stopPropagation();
		        	activatePointerLock();
	        	});
				    if( !bHasPointerLock ){
		          $("button#lockpointer").hide();
				    }
            //links shouldn't send an ST event 
            var $element=$("a");
				    $element.bind('touchstart', function(e){
              $(this).click();
            });
				    $element.bind('touchend', ignoreEvent);
	        	$element.mousedown(ignoreEvent);
	        	$element.mouseup(ignoreEvent);
	        	$element.click(function(e){
	        		e.stopPropagation();
            });

	        	//hide Keyboard toggle on devices!=android/ios
		        if( !bHasTouch ){
		        	$("#showkeyboard").hide();
		        	$("#keyboardtrigger").hide();
		        	$("#keyboardlayer").hide();
		        	bShowStKeyboard=false;
		        }else{
		        	$("button#lockpointer").hide();
		        	bShowStKeyboard=true;
		        }
            if( bHasTouch ){
                $(".alert.touch").removeClass("hidden");
                $(".alert.touch").addClass("current");
            } else{
                $(".alert.desktop").removeClass("hidden");
                $(".alert.desktop").addClass("current");
            } 
            bShowHelp=true;

	        	//various browser UI fixes
				    $(document).bind("contextmenu",function(e){
				        return false;
				    });
            //orientation change - prevent the keyboard from getting to large by adjusting the viewport accordingly
            window.addEventListener("orientationchange", function() {
              var w=actual('device-width', 'px');
              if (w < 768) { $('meta[name=viewport]').attr('content','initial-scale='+w/768+', maximum-scale='+w/768+', user-scalable=0'); }
            }, false);
	        },
			onMouseMove: onMouseMove
		}
    }();        
