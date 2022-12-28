<?PHP
session_start();
if(isset($_SESSION['name'])){
	try {
		$con = mysqli_connect("localhost","echatbot","echatbot2016","echatbotdb") or Exception('DB connect error', 9003);
	    mysqli_set_charset($con, "utf8");
	    
	    $uname = $_SESSION['name'];
	    $sql = "select music from user where name='".$uname."'";
	    $res = mysqli_query($con, $sql) or Exception('DB search error', 9004);
	    $resA = mysqli_fetch_array($res, MYSQLI_ASSOC) or Exception('Data fetch error', 9004);
	    while($resA["music"] == "") {
	    	$sql = "select music from user where name='".$uname."'";
	    	$res = mysqli_query($con, $sql) or Exception('DB search error', 9004);
	    	$resA = mysqli_fetch_array($res, MYSQLI_ASSOC) or Exception('Data fetch error', 9004);
	    	sleep(1);
	    }
	    
	    $sql = "UPDATE user SET music='' where name='".$uname."'";
	    $res = mysqli_query($con, $sql) or Exception('DB search error', 9004);
	    mysqli_close($con);
		echo $resA["music"];
		
	}catch (Exception $e) {
		header('HTTP/1.1 500 Internal Server Error');	//make error response to ajax
		echo json_encode(array(
	        'error' => array(
	            'msg' => $e->getMessage(),
	            'code' => $e->getCode(),
	        ),
	    ));
	    exit(0);
	}
	
	
	
	
}
?>




