<?php
$response = false;
$file_name = $_GET['file']; 
if( file_exists($file_name) ) {
    unlink($file_name);
    $response = true;
}

// Send JSON Data to AJAX Request
echo $response;
?>


