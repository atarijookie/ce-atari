<?php

$mainapp     = $_POST["mainapp"];
$distro      = $_POST["distro"];
$rpirevision = $_POST["rpirevision"];
$rpiserial   = $_POST["rpiserial"];

include 'dblogin.php';

if($rpiserial == "") {
  echo "rpiserial is required, fail";
  return;
}

$mainapp     = $db->real_escape_string($mainapp);
$distro      = $db->real_escape_string($distro);
$rpirevision = $db->real_escape_string($rpirevision);
$rpiserial   = $db->real_escape_string($rpiserial);

$sql    = "SELECT rpiserial FROM ce_versions WHERE rpiserial='$rpiserial'";
$result = $db->query($sql) or die("Error on existence check: " . mysql_error());

if ($result->num_rows > 0) {   // got this RPi serial in the table? UPDATE
  $sql    = "UPDATE ce_versions SET mainapp='$mainapp', distro='$distro', rpirevision='$rpirevision', reportdate=NOW() WHERE rpiserial='$rpiserial'";
  $result = $db->query($sql) or die("Error on UPDATE: " . mysql_error());
  echo "UPDATE OK";
} else {                       // don't have this RPi serial in the table? INSERT
  $sql    = "INSERT INTO ce_versions (mainapp, distro, rpirevision, rpiserial, reportdate) VALUES ('$mainapp', '$distro', '$rpirevision', '$rpiserial', NOW())";
  $result = $db->query($sql) or die("Error on INSERT: " . mysql_error());
  echo "INSERT OK";
}

?>
