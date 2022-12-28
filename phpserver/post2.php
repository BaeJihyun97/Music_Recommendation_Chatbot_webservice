<?PHP
session_start();
if(isset($_SESSION['name'])){
	//prepare tcp socket
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
	
	//make input data
	$inputText = "";
	$usern = $_SESSION['name']."<///userName///>";

	$file_name_cache = "./log/".$_SESSION['name']."_log_c.txt";
	if (file_exists($file_name_cache)) {
		$fp = fopen($file_name_cache, 'r');

		while(!feof($fp)) {
			$line = (fgets($fp));
			$inputText = trim($line)." ".$inputText;
		}
		fclose($fp);
		
		if(strlen($inputText) <= 1) {
			return;
		}
	}

	$inputText = $usern.$inputText;
	$in = sprintf("%04d2002%s", strlen($inputText), $inputText);
	
	
	//send
	socket_write($sock, $in, strlen($in));
	
	//receive
	$out = socket_read($sock, 8192);
	
	
	//read received data
	$out_data = "empty";
	if(substr($out, 4, 4) == "2002") {
		$out_data = substr($out, 8);
		if($out_data != "success"){
			echo "packet data error\n";
			return;
		}
		$file_name = "./log/".$_SESSION['name']."_log.html";
		$text_message_res = array("chatTime" => date("g:i A"), "userName" => "chatbot", "msg" => stripslashes(htmlspecialchars("음악 리스트 생성 중입니다.")));
		$text_message_res = ",".json_encode($text_message_res, JSON_UNESCAPED_UNICODE);
	    file_put_contents($file_name, $text_message_res, FILE_APPEND | LOCK_EX); 
	}
	else if(strlen($out) != 0) {
		$out_data = $out;
		echo "packet data error\n";
		return;
	}
	socket_close($sock);
	
	//return
	echo "success";
}
?>





