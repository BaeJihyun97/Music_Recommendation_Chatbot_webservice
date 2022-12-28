<?php
session_start();

try {
	if(isset($_POST['enter'])){
		$uname = $_POST['name'];
	    if($uname != ""){
	        
	        $con = mysqli_connect("localhost","echatbot","echatbot2016","echatbotdb") or Exception('DB connect error', 9003);
	        mysqli_set_charset($con, "utf8");
	        
	        $sql = "select * from user where name='".$uname."'";
	        $res = mysqli_query($con, $sql) or Exception('DB search error', 9004);
			if (mysqli_num_rows($res) > 0) {
				$resA = mysqli_fetch_array($res, MYSQLI_ASSOC) or Exception('Data fetch error', 9004); 
				if($resA["name"] != $uname) throw new Exception('data currupt', 9005);		
				$_SESSION['name'] = stripslashes(htmlspecialchars($uname));
				$file_name = "./log/".$_SESSION['name']."_log.html";
				file_put_contents($file_name, "", FILE_APPEND | LOCK_EX);
				echo "n";	
			}
			else {
				echo "q";
				//$query = "insert into user (name, sad, angry, nervous, hurt, surprise, happy) values('".$uname."', 2, 2, 2, 2, 2, 2)";
				//$ires = mysqli_query($con, $query) or Exception('DB inseart error', 9006);
				//if(!$ires) {
				//	throw new Exception('DB inseart error', 9007);
				//}	
			}
			
			//done
			$_SESSION['name'] = stripslashes(htmlspecialchars($uname));
	        echo "<script>location.href='./index.php'</script>";
	        mysqli_close($con);
	    }
	    else{
	        throw new Exception('empty data', 9000);
	    }
	}
	else {
		throw new Exception('invalid', 9001);
	}
}catch (Exception $e) {
	header('HTTP/1.1 500 Internal Server Error');	//make error response to ajax
	//echo "<script>location.href='./index.php'</script>";
	echo json_encode(array(
        'error' => array(
            'msg' => $e->getMessage(),
            'code' => $e->getCode(),
        ),
    ));
    
}

?>



