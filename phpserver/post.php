<?php
session_start();
if(isset($_SESSION['name'])){
    $text = $_POST['msg'];
	
    $text_message = "<div class='msgln'><span class='chat-time'>".date("g:i A")."</span> <b class='user-name'>".$_SESSION['name']."</b> ".stripslashes(htmlspecialchars($text))."<br></div>";
    $file_name = "./log/".$_SESSION['name']."_log.html";
    file_put_contents($file_name, $text_message, FILE_APPEND | LOCK_EX);
   
  
}
?>
