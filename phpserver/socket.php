<?PHP
session_start();
if(isset($_SESSION['name'])){
    
	$text = $_POST['msg'];

	$file_name = "./log/".$_SESSION['name']."_log.html";
	$msg = array("chatTime" => date("g:i A"), "userName" => $_SESSION['name'], "msg" => stripslashes(htmlspecialchars($text)));
	$msg = ",".json_encode($msg, JSON_UNESCAPED_UNICODE);
	file_put_contents($file_name, $msg, FILE_APPEND | LOCK_EX);
	
	if(strlen($text) == 0) {
    	$text_message_res = array("chatTime" => date("g:i A"), "userName" => "chatbot", "msg" => "please input data.");
		$text_message_res = ",".json_encode($text_message_res, JSON_UNESCAPED_UNICODE);
		file_put_contents($file_name, $text_message_res, FILE_APPEND | LOCK_EX);
    	exit(0);
	}	

	
	$addr = gethostbyname('127.0.0.1');
	$port = 8082;

	
	$sock = socket_create(AF_INET, SOCK_STREAM, 0);	
	if($sock < 0){
		echo "socket_create() failed: reason: ".socket_strerror($sock)."<br>";
	}
	
	$result = socket_connect($sock, $addr, $port);
	if($result == false) {
		echo "socket_connect() failed: reason: ".socket_strerror($result)."<br>";
	}

	$textArr = array($_POST['msg']);
	$inputText = $_POST['msg'];
	$file_name_cache = "./log/".$_SESSION['name']."_log_c.txt";
	if (file_exists($file_name_cache)) {
		$fp = fopen($file_name_cache, 'r');

		$i = 1;
		while(!feof($fp)) {
			$line = (fgets($fp));
			$inputText = trim($line)." ".$inputText;
			$textArr[$i] = trim($line);
			$i += 1;
		}
		fclose($fp);
	}

	
	
	
	
	//$in = "%04d1001".$text;
		
	//$in = "%04d2001".$inputText;
	$in = sprintf("%04d2001%s", strlen($inputText), $inputText);

	
	$out = "";
	
	
	socket_write($sock, $in, strlen($in));

	
	//broken pipe
	//change to two read way fragmentation (read/write all) 
	$out = socket_read($sock, 8192);

	
	$out_data = "empty";
	if(substr($out, 4, 4) == "2001") {
		$out_data = substr($out, 8);
	}
	else if(strlen($out_data) != 0) {
		$out_data = $out;
		echo "packet data error\n";
	}
	socket_close($sock);
	

	$text_message_res = array("chatTime" => date("g:i A"), "userName" => "chatbot", "msg" => stripslashes(htmlspecialchars($out_data)));
	$text_message_res = ",".json_encode($text_message_res, JSON_UNESCAPED_UNICODE);
    file_put_contents($file_name, $text_message_res, FILE_APPEND | LOCK_EX); 
	
	
	$fp = fopen($file_name_cache, 'w');
	
	$tempOut = "";
	for($i = 0; $i < count($textArr) && $i < 5; $i += 1) {
		$tempOut = $tempOut.$textArr[$i]."\n" ;  
	}
	fwrite($fp, $tempOut);
	fclose($fp);
	
	
	
	
	
}
?>


