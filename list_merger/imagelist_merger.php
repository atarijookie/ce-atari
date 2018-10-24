<?php
header("Content-type: text/plain");
header('Cache-Control: no-cache');          // recommended to prevent caching of event data.

ini_set('output_buffering', 'off');         // Turn off output buffering
ini_set('zlib.output_compression', false);  // Turn off PHP output compression

ini_set('implicit_flush', true);            // Implicitly flush the buffer(s)
ob_implicit_flush(true);

$date = date('Y-m-d H:i:s');
echo "Started at: $date\n\n";

function downloadFile($url) {
    $start = microtime(true);
    echo "downloading: $url\n";

    flush();
    ob_flush();

    $handle = curl_init();

    $fileHandle = tmpfile();    // create temp file, don't close it yet as that deletes the file

    curl_setopt_array($handle,
        array(
            CURLOPT_URL => $url,
            CURLOPT_FILE => $fileHandle,
        ));

    $data = curl_exec($handle); // download and write to file

    curl_close($handle);

    $elapsed = round(microtime(true) - $start, 2);
    echo "downloading took $elapsed s\n\n";

    return $fileHandle;         // return file handle, don't close yet as that deletes the file
}

function getHashOfFile($filename)
{
    $start = microtime(true);
    echo "hashing: $filename\n";

    flush();
    ob_flush();

    $hash = md5_file($filename);        // calculate hash

    $elapsed = round(microtime(true) - $start, 2);
    echo "hashing took $elapsed s\n\n";

    return $hash;
}

function loadValueFromFile($key, $defaultValue)
{
    $filename = "./imagelist_merger_$key";

    if(!file_exists($filename)) {       // if the file doesn't exist, just return the default value
        echo "File: $filename doesn't exist, returning default value $defaultValue\n";
        return $defaultValue;
    }

    $handle = fopen($filename, "r");    // open file

    if($handle === FALSE) {             // couldn't open file? quit
        echo "Couldn't open file: $filename, returning default value $defaultValue\n";
        return $defaultValue;
    }

    $value = fgets($handle);            // get value
    fclose($handle);                    // close file

    return $value;                      // return value
}

function saveValueToFile($key, $value)
{
    $filename = "./imagelist_merger_$key";
    $handle = fopen($filename, "w");    // open file

    if($handle === FALSE) {             // couldn't open file? quit
        echo "Failed to save to file: $filename\n";
        return;
    }

    fputs($handle, $value);             // store value
    fclose($handle);                    // close file
}

function saveMergedFileWithDate($merged)
{
    $new_list = fopen("imagelist.csv", "w");    // open file

    if($new_list !== FALSE) {           // couldn't open file? quit
        $date = date('Y-m-d');

        fputs($new_list, $date);        // current date
        fputs($new_list, "\n");         // new line char

        rewind($merged);                // merged file to start
        while(!feof($merged)) {
            $line = fgets($merged);     // get line from merged file
            fputs($new_list, $line);    // write line to merged file with date
        }

        fclose($new_list);              // close new_list

        echo "New list saved!\n";
        return TRUE;
    }

    // when failed to open file
    echo "Failed to save new list\n";
    return FALSE;
}

function checksum($filename)
{
  $size   = filesize($filename);
  $handle = fopen($filename, "rb");

  $cs = 0;

  for($i=0; $i < $size; $i += 2) {
    $data = "";

    if(($i + 1) < $size) {
      $data  = fread($handle, 2);

      $array = unpack("Chi/Clo", $data);
      $hi    = $array["hi"];
      $lo    = $array["lo"];
    } else {
      $data  = fread($handle, 1);

      $array = unpack("Chi", $data);
      $hi    = $array["hi"];
      $lo    = 0;
    }

    $word = ($hi << 8) | $lo;
    $cs += $word;
    $cs = $cs & 0xffff;
  }

  if(($size & 1) == 0) {        // even number of bytes?
    $cs += 0xff00;
  } else {                      // odd number of bytes?
    $cs += 0x00ff;
  }

  fclose($handle);

  $cs = $cs & 0xffff;
  $hexcs = sprintf("0x%04x", $cs);

  return $hexcs;
}

function updateTheUpdateList()
{
    echo "-----------------------------------------------------------\n";
    echo "Updating updatelist.csv\n\n";

    $update_list = file_get_contents("updatelist.csv");    // read this file
    $rows = explode("\n", $update_list);                   // split into rows

    $new_rows = array();

    foreach($rows as $row) {            // go through the rows
        $cols = explode(",", $row);     // split row into columns

        if($cols[0] == "imglist") {     // this is the row with imglist
            $date = date('Y-m-d');
            $checksum = checksum("imagelist.csv");  // calc our custom checksum
            $new_row = "imglist,$date,http://joo.kie.sk/cosmosex/update/imagelist.csv,$checksum";

            array_push($new_rows, $new_row);        // add this modified row to new rows
        } else {                        // not imglist row, just append as is
            array_push($new_rows, $row);
        }
    }

    array_push($new_rows, " ");             // empty row at the end
    $new_csv = implode("\n", $new_rows);    // join new rows together into new csv

    file_put_contents("updatelist.csv", $new_csv);  // save to file
}

///////////////////////////////////////////////////////////////////////////////////////

$list_of_lists = array(
    "http://joo.kie.sk/cosmosex/update/automation_exxos.csv",
    "http://stonish.net/CosmosEx.csv");

set_time_limit(60);                     // max execution time

$files = array();

$merged = tmpfile();                    // create temp file
$meta = stream_get_meta_data($merged);  // determine file name
$merged_path = $meta['uri'];

echo "-----------------------------------------------------------\n";

// download all those files
foreach ($list_of_lists as $url) {
    // flush output to browser...
    $fileName = basename($url);
    $fileHandle = downloadFile($url);   // download file

    $meta = stream_get_meta_data($fileHandle);  // determine file name
    $path = $meta['uri'];

    $hash = getHashOfFile($path);       // calculate hash

    $one_file = array("name" => $fileName, "path" => $path, "handle" => $fileHandle, "hash" => $hash);

    array_push($files, $one_file);

    echo "-----------------------------------------------------------\n";
}

echo "\n\nMerging files...\n";

// go through all the files
foreach ($files as $file) {
    $name = $file["name"];      // get file name
    $hash = $file["hash"];
    $handle = $file["handle"];

    rewind($handle);            // to start of file

    while (!feof($handle)) {    // while not EOF
        $line = fgets($handle);

        if($line === FALSE) {   // if coudln't read line, quit
            break;
        }

        $line = str_replace("<br>", "\n", $line);   // if file uses html newlines, replace them with C newlines

        $len = strlen($line);   // get line length
        if($len < 15) {         // if line probably doesn't contain enough data to be valid, skip it
            continue;
        }

        fputs($merged, $line);  // write line to merged file
    }
}

echo "-----------------------------------------------------------\n";

// check if the new file is other than what the old file was
$prev_hash = loadValueFromFile("prev_merged_hash", 0);

$new_hash = getHashOfFile($merged_path);    // calculate hash of new merged file

if($prev_hash != $new_hash) {               // hash changed
    echo "hash of new merged file is different than what we had before!\n";

    $good = saveMergedFileWithDate($merged);

    if($good) {     // if succeeded to save merged file with date
        saveValueToFile("prev_merged_hash", $new_hash);  // store this hash to file
        updateTheUpdateList();
        saveValueToFile("last_run_message", "runned at $date, merged is different, updated the list :)");
    } else {
        saveValueToFile("last_run_message", "runned at $date, merged is different, failed to save merged file :(");
    }
} else {
    echo "hash of new merged file are the same\n";
    saveValueToFile("last_run_message", "runned at $date, merged is same, not updating the list");
}

echo "-----------------------------------------------------------\n";
echo "Closing files...\n";

// close all the open files
foreach ($files as $file) {
    fclose($file["handle"]);    // this also deletes the file
}

fclose($merged);    // this also deletes the file

echo "-----------------------------------------------------------\n";
$date = date('Y-m-d H:i:s');
echo "\n\nEnded at: $date\n\n";

?>
