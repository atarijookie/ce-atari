<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">
    <head>
        <meta http-equiv="content-type" content="text/html; charset=utf-8">
	    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=0"/>
        <title>{{title}}</title>
        <!-- 
        <link rel="stylesheet" type="text/css" href="css/bootstrap.light.css">
        <link rel="stylesheet" type="text/css" href="css/style.css">
		    <script type="text/javascript" src="js/modernizr.custom.50426.js"></script> 
        -->
        <script>
        var version="140810";
        document.write('<link rel="stylesheet" type="text/css" href="/css/combined.css?'+version+'">');
        //unfortunately we have to support IE<10, so decide on zepto/jquery here
		document.write('<script type="text/javascript" src=/js/' +
		('__proto__' in {} ? 'zepto' : 'jquery') +
		'.js?'+version+'><\/script>');
		document.write('<script type="text/javascript"">var jQuery=Zepto;<\/script>');
		</script>
    </head>
    <body>
    <div class="container">
        <div class="row">
            <div class="span10">
                <ul class="nav nav-pills">
                    <li class="{{activeHome}}"><a href="/?v=5">Home</a></li>
                    <li><a href="http://joo.kie.sk/?page_id=415">Manual</a></li>
                    <li><a href="/floppy.html?v=5">Floppy</a></li>
                    <li><a href="/download.html?v=5">Download</a></li>
                    <li><a href="/remote.html?v=5">Remote</a></li>
                    <li><a href="/screenshots.html?v=5">Screenshots</a></li>
                    <li><a href="/config.html?v=5">Config</a></li>
                    <li class="{{activeStatus}}"><a href="/app/status/?v=5">Status</a></li>
                </ul>
        <h3>{{title}}</h3>
        <p>{{info}}</p>
            </div>
            <div class="span2">
                <img class="logo" src="/img/ce_logo.png">
            </div>
        </div>     
    {{content}}
    </div>
    </body>
</html>
