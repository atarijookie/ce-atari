var CosmosEx = CosmosEx || {};

function getSearchResults(searchString)
{
    if(prevSearchString != searchString) {  // if the search string changed, reset current page number to zero
        currentPage = 0;
    }
    prevSearchString = searchString;

    var select_image_list = document.getElementById('select_image_list');
    var list_index = select_image_list.value;

    url = '/download/imagelist'; // base url
    url = url + "?page=" + currentPage + '&list_index=' + list_index; // add page and list_index as request args

    if(searchString.length > 0) {       // some search string was specified?
        url = url + "&search=" + searchString;
    }

    // get list of available online files
    $.ajax({
        url: url,
        type: 'GET',
        dataType: 'json',
        success: function(data){
            // retrieve data from response
            totalPages = data.totalPages;
            currentPage = data.currentPage;
            imageList = data.imageList;
            slots = data.slots;

            // refresh the shown list on screen
            generateSearchResultsTable();
        },
        error: function (xhr) {
            console.log("Error: " + xhr.statusText);
        },
    })
}

function insertStringAtPos(largeString, index, smallString)
{
    return largeString.substr(0, index) + smallString + largeString.substr(index);
}

var IconType = {
  DOWNLOAD: 1,
  INSERT: 2,
  DUMMY: 3
};

function showNoStorageWarning(show)
{
    if(show) {
        var message = "<font color='#ff0000'><b>You have no USB drive or shared drive attached to your device.<br>This function requires it and can't be used without it.</b></font>";
    } else {
        var message = "";
    }

    var warning = document.getElementById('no_storage_warning');
    warning.innerHTML = message;
}

function handleShowOverlay(encoding_ready)
{
    if(typeof(encoding_ready)==='undefined') {  // if don't have variable, hide overlay
        $(".over_convert").hide();
        return false;               // don't check again
    }

    var check_again = false;        // don't check again

    if(encoding_ready) {            // encoding ready?
        $(".over_convert").hide();
    } else {                        // still not ready?
        $(".over_convert").show();
        check_again = true;         // check again in a while!
    }

    if(!prev_encoding_ready && encoding_ready) {    // if we just finished encoding, reload page to show what image is inserted where
        onSearchStringChanged();                // fake search string change to reload the download/insert icons
    }
    prev_encoding_ready = encoding_ready;

    return check_again;             // return if we should check again in a while
}

function handleShowDownloading(downloading_count, downloading_progress)
{
    if(typeof(downloading_count)==='undefined') {  // if don't have variable, hide overlay
        $(".overlay_dwnld").hide();
        return false;               // don't check again
    }

    var check_again = false;        // don't check again

    if(downloading_count == 0) {    // not downloading?
        $(".overlay_dwnld").hide();
    } else {                        // still downloading?
        var msgText = "<h3>Downloading " + downloading_count + " disk images...<br>Current file: " + downloading_progress + " %</h3>";

        var msg = document.getElementById('download_msg');  // find message
        msg.innerHTML = msgText;                            // set content of message

        $(".overlay_dwnld").show();
        check_again = true;         // check again in a while!
    }

    // if we were downloading something, but we arent't downloading anymore
    if(downloading_count == 0 && prevDownloadingCount > 0) {
        onSearchStringChanged();                // fake search string change to reload the download/insert icons
    }
    prevDownloadingCount = downloading_count;   // store current value so we would be able to detect end of downloading

    return check_again;             // return if we should check again in a while or not
}

// check if image is converted / downloaded
function onCheckTimer() {
    $.ajax({
        type: 'GET',
        url: '/download/status',
        success: function(data){
            var check1 = handleShowOverlay(data.encoding_ready);         // encoding?
            var check2 = handleShowDownloading(data.downloading_count, data.downloading_progress);  // downloading?

            do_we_have_storage = data.do_we_have_storage;
            showNoStorageWarning(!data.do_we_have_storage);  // show / hide no storage message

            if(check1 || check2) {              // if we should check again, start timer again
                setTimeout(onCheckTimer, 333);
            }

        },
        error: function(){              // when failed to get info, just hide overlay
            $(".over_convert").hide();
            $(".overlay_dwnld").show();
        }
    });
}

function onInsert(imageName, slotNo)
{
    url = "/download/insert?image=" + imageName + "&slot=" + slotNo;

    $.ajax({
        url: url,
        type: 'GET',
        dataType: 'json',
        success: function(data){
            console.log("Insert Success");
            onSearchStringChanged();        // fake search string change to reload the download/insert icons
        },
        error: function (xhr) {
            console.log("Insert Error: " + xhr.statusText);
        }
    })
}

function onDownload(imageName)
{
    url = "/download/download?image=" + imageName;

    $.ajax({
        url: url,
        type: 'GET',
        dataType: 'json',
        success: function(data){
            console.log("Download Success");

            prevDownloadingCount++;       // downloading at least one thing (helps to refresh on fast downloads where timer doesn't happen before download still running)
            handleShowDownloading(prevDownloadingCount, 0);
            setTimeout(onCheckTimer, 333); // check every while
        },
        error: function (xhr, textStatus, errorThrown) {
            console.log("Download Error: " + xhr.statusText + ", " + textStatus + ", " + errorThrown);
        }
    })
}

function onGoToPage(pageNumber)
{
    currentPage = pageNumber;   // set new current page
    onSearchStringChanged();    // get new page content
}

function generatePageSwitchButton(prevNotNext)
{
    var pageNumber = 0;
    var buttonSymbol = "X";
    var buttonClass = "btnDownload btnHidden";
    var onClickFunction = "";

    if(prevNotNext) {                           // prev page?
        if(currentPage != 0) {                  // we're not on first page?
            pageNumber = currentPage - 1;
            onClickFunction = "onGoToPage(" + pageNumber + ");"

            buttonSymbol = "&laquo;";
            buttonClass = "btnDownload";
        }
    } else {                                    // next page?
        if(currentPage < (totalPages - 1)) {    // we're not on the last page?
            pageNumber = currentPage + 1;
            onClickFunction = "onGoToPage(" + pageNumber + ");"

            buttonSymbol = "&raquo;";
            buttonClass = "btnDownload";
        }
    }

    return "<a href='#' class='" + buttonClass + "' onclick=\"" + onClickFunction + "\"\>" + buttonSymbol + "</a>";
}

function getFileNameWithoutExtension(fileNameWithExt)
{
    if(fileNameWithExt.length < 3) {  // if filename too short, just return it
        return fileNameWithExt;
    }

    var output = fileNameWithExt.split('.');   // split to filename and extension
    return output[0];                 // return 0th path (before dot)
}

function generateActionButton(iconType, slotNo, imageName)
{
    // this dummy is just a place holder with size of normal button
    var dummy = "<a href='#' class='btnDownload btnHidden' onclick=\"\"\>X</a>";

    if(do_we_have_storage == false) {           // no storage? no action button
        return dummy;
    }

    if(iconType == IconType.INSERT) {           // insert button?
        // if this imageName is already inserted in this slotNo, highlight button, disable button click
        var isLoadedStyle = "";                 // not loaded by default
        var isLoaded = false;

        if(slots.length == 3) {                 // if we do have slots contents
            var imageInSlot = slots[slotNo];    // get name of image in this slot
            imageInSlot = getFileNameWithoutExtension(imageInSlot);
            imageNameWOext = getFileNameWithoutExtension(imageName);

            if(imageInSlot == imageNameWOext) {      // if the name matches what we're showing here, it's already loaded
                isLoadedStyle = "btnInserted";  // not loaded by default
                isLoaded = true;
            }
        }

        var onInsertFunction = "";
        if(!isLoaded) {             // allow click when not loaded
            onInsertFunction = "onInsert('" + imageName + "', " + slotNo + ");";
        }

        slotNoPlusOne = slotNo + 1;
        return "<a href='#' class='btnDownload btnInsert " + isLoadedStyle + "' onclick=\"" + onInsertFunction + "\"\>" + slotNoPlusOne + "</a>";
    } else if(iconType == IconType.DOWNLOAD) {  // download button?
        return "<a href='#' class='btnDownload' onclick=\"onDownload('" + imageName + "');\"\>V</a>";
    }

	return dummy;                               // other button?
}

// generate one row from this matching item
function generateSearchResultRow(item, searchString)
{
    // use right image for showing download / is downloaded
    var downloadIcon = "";          // download action / is downloaded notification
    var insertIcons = "";           // nothing / insert into slot

    if(item.haveIt) {               // if file is downloaded
        downloadIcon = generateActionButton(IconType.DUMMY, 0, "");

        for(var i=0; i<3; i++) {    // insert into slot 1, 2, 3
            insertIcons = insertIcons.concat(generateActionButton(IconType.INSERT, i, item.filename));
        }
    } else {                        // if file is not downloaded yet
        downloadIcon = generateActionButton(IconType.DOWNLOAD, 0, item.url);

        for(var i=0; i<3; i++) {    // just 3 empty placeholders
            insertIcons = insertIcons.concat(generateActionButton(IconType.DUMMY, 0, ""));
        }
    }

    // create a string containing download + insert operations if possible
    var imageOps = downloadIcon.concat(insertIcons);

    content = item.content;             // modified content with possible highlighting

    if(searchString.length > 0) {       // if the searchstring was supplied
        contentLC = content.toLowerCase();
        start = contentLC.indexOf(searchString);

        if(start != -1) {               // if the searchstring was found in the content
            end = start + searchString.length;
            content = insertStringAtPos(content, end, "</font></b>");
            content = insertStringAtPos(content, start, "<b><font color='#008000' style='text-decoration: underline;'>");
        }
    }

    // generate html table row
    var row = "<tr><td style='width: 20%;'>";
    row = row.concat(item.filename);
    row = row.concat("</td><td style='width: 20%;'>");
    row = row.concat(imageOps);
    row = row.concat("</td><td>");
    row = row.concat(content);
    row = row.concat("</td></tr>");
    return row;
}

// render the image list on screen
function generateSearchResultsTable()
{
    var resultsTable = "<table class=\"tableDownload\">";

    var searchString = document.getElementById('search_input').value;   // get search string
    searchString = searchString.toLowerCase();     // search string to lower case

    // code for page switching
    var btnPrev = generatePageSwitchButton(true);
    var btnNext = generatePageSwitchButton(false);

    var pagesButtons = "";
    if(totalPages > 0) {        // if something was found, show page switching buttons, otherwise don't show them
        var pagesButtons = "<div class='pageSwitcher'>Pages: " + btnPrev + "&nbsp; " + (currentPage + 1) + " &nbsp; / &nbsp; " + totalPages + " &nbsp; " + btnNext + "</div>";
    }

    var header = "<thead><tr><th class='col_filename'>Image name</th><th class='col_actions'>Download / Insert</th><th class='col_content'>Content" + pagesButtons + "</th></tr></thead> <tbody>";
    resultsTable = resultsTable.concat(header);

    // nothing found?
    if(totalPages == 0) {
        resultRow = "<tr><td colspan=\"3\">nothing matches your search input</td></tr></tbody>";
        resultsTable = resultsTable.concat(resultRow);
    } else {
        for(var i=0; i<imageList.length; i++) {         // go through input list
            var item = imageList[i];

            // if the matching item fits to the right page, output it
            resultRow = generateSearchResultRow(item, searchString);        // generate one row from this matching item
            resultsTable = resultsTable.concat(resultRow);
        }

        resultsTable = resultsTable.concat("</tbody>");
    }

    resultsTable = resultsTable.concat("</table>");

    var results = document.getElementById('search_results');    // find table
    results.innerHTML = resultsTable;                           // set content of the results table
}

function onSearchStringChanged()
{
    // read search string
    var searchString = document.getElementById('search_input').value;   // get search string
    getSearchResults(searchString);     // get list of files after applying search string
}

function onDelayedSearchStringChanged()
{
    setTimeout(function(){ onSearchStringChanged(); }, 100);
}

function imageListLoadAndShow()
{
    url = "/download/list_of_lists";

    $.ajax({
        url: url,
        type: 'GET',
        dataType: 'json',
        success: function(data){
            console.log("Download Success");
            var select_image_list = document.getElementById('select_image_list');

            // remove any existing options in select
            for(i=0; i<select_image_list.options.length; i++) {
                select_image_list.remove(0);
            }

            // fill select with retrieved image lists
            for(var i=0; i<data.length; i++) {          // go through input list
                var idx = data[i]['index']              // fetch index and name
                var name = data[i]['name']

                var opt = document.createElement("option");     // create option
                opt.value = idx;
                opt.innerHTML = name;
                select_image_list.append(opt);                  // append option
            }

            select_image_list.value = 0;                // select item with index 0
        },
        error: function (xhr, textStatus, errorThrown) {
            console.log("Download Error: " + xhr.statusText + ", " + textStatus + ", " + errorThrown);
        }
    })
}

function on_image_list_select_changed()
{
    console.log("on_image_list_select_changed");
    onGoToPage(0);
}