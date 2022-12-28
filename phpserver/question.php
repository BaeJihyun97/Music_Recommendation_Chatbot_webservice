<?php
session_start();
try {
	if(isset($_SESSION['name'])){
		$con = mysqli_connect("localhost","echatbot","echatbot2016","echatbotdb") or Exception('DB connect error', 9003);
	    mysqli_set_charset($con, "utf8");
		
		if(isset($_POST['enter'])){
			$uname = $_SESSION['name'];
		    $query = "insert into user (name, sad, angry, nervous, hurt, surprise, happy) values('".$uname."', ".(int)$_POST['sad'].", ".(int)$_POST['angry'].", ".(int)$_POST['nervous'].", ".(int)$_POST['hurt'].", ".(int)$_POST['surprise'].", ".(int)$_POST['happy'].")";
			$ires = mysqli_query($con, $query) or Exception('DB inseart error', 9006);
			if(!$ires) {
				throw new Exception('DB inseart error', 9007);
			}
			
			//done
			$_SESSION['name'] = stripslashes(htmlspecialchars($uname));
			$file_name = "./log/".$_SESSION['name']."_log.html";
			$myfile = fopen($file_name, "w"); fclose($file_name);
		    echo "<script>location.href='./index.php'</script>";
		}

			
	
		
	}
	else {
		throw new Exception('session error', 9008);
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

<!DOCTYPE html>
<html lang="ko">
<head>
	<meta charset="UTF-8">
	<title>questionnaire!</title>
    <meta name="description" content="AI Chat Application" />
</head>
<body>
	<div id="loginform">
		<p>Please fill out the questionnaire!</p>
		<form action="question.php" method="post">
		<div id="qustionText">
			<label id="qustionText">나는 슬플 때 &mdash; 노래를 듣는다.</label>
			<div class="optionA">
				<div class="optionB"><label>발라드</label><input type="radio" name="sad" value="1"/></div>
				<div class="optionB"><label>댄스</label><input type="radio" name="sad" value="2"/></div>
				<div class="optionB"><label>랩/힙합 </label><input type="radio" name="sad" value="3"/></div>
				<div class="optionB"><label>락/매탈</label><input type="radio" name="sad" value="4"/></div>
				<div class="optionB"><label>R&amp;B</label><input type="radio" name="sad" value="5"/></div>
				<div class="optionB"><label>인디</label><input type="radio" name="sad" value="6"/></div>
				<div class="optionB"><label>팝</label><input type="radio" name="sad" value="7"/></div>
				<div class="optionB"><label>클래식</label><input type="radio" name="sad" value="8"/></div>
				<div class="optionB"><label>재즈</label><input type="radio" name="sad" value="9"/></div>
			</div>
		</div><br/><br/>
		<div>
			<label id="qustionText">나는 화날 때 &mdash; 노래를 듣는다.</label>
			<div class="optionA">
				<div class="optionB"><label>발라드</label><input type="radio" name="angry" value="1"/></div>
				<div class="optionB"><label>댄스</label><input type="radio" name="angry" value="2"/></div>
				<div class="optionB"><label>랩/힙합 </label><input type="radio" name="angry" value="3"/></div>
				<div class="optionB"><label>락/매탈</label><input type="radio" name="angry" value="4"/></div>
				<div class="optionB"><label>R&amp;B</label><input type="radio" name="angry" value="5"/></div>
				<div class="optionB"><label>인디</label><input type="radio" name="angry" value="6"/></div>
				<div class="optionB"><label>팝</label><input type="radio" name="angry" value="7"/></div>
				<div class="optionB"><label>클래식</label><input type="radio" name="angry" value="8"/></div>
				<div class="optionB"><label>재즈</label><input type="radio" name="angry" value="9"/></div>
			</div>
		</div><br/><br/>
		<div>
			<label id="qustionText">나는 불안할 때 &mdash; 노래를 듣는다.</label>
			<div class="optionA">
				<div class="optionB"><label>발라드</label><input type="radio" name="nervous" value="1"/></div>
				<div class="optionB"><label>댄스</label><input type="radio" name="nervous" value="2"/></div>
				<div class="optionB"><label>랩/힙합 </label><input type="radio" name="nervous" value="3"/></div>
				<div class="optionB"><label>락/매탈</label><input type="radio" name="nervous" value="4"/></div>
				<div class="optionB"><label>R&amp;B</label><input type="radio" name="nervous" value="5"/></div>
				<div class="optionB"><label>인디</label><input type="radio" name="nervous" value="6"/></div>
				<div class="optionB"><label>팝</label><input type="radio" name="nervous" value="7"/></div>
				<div class="optionB"><label>클래식</label><input type="radio" name="nervous" value="8"/></div>
				<div class="optionB"><label>재즈</label><input type="radio" name="nervous" value="9"/></div>
			</div>
		</div><br/><br/>
		<div>
			<label id="qustionText">나는 상처 받을 때 &mdash; 노래를 듣는다.</label>
			<div class="optionA">
				<div class="optionB"><label>발라드</label><input type="radio" name="hurt" value="1"/></div>
				<div class="optionB"><label>댄스</label><input type="radio" name="hurt" value="2"/></div>
				<div class="optionB"><label>랩/힙합 </label><input type="radio" name="hurt" value="3"/></div>
				<div class="optionB"><label>락/매탈</label><input type="radio" name="hurt" value="4"/></div>
				<div class="optionB"><label>R&amp;B</label><input type="radio" name="hurt" value="5"/></div>
				<div class="optionB"><label>인디</label><input type="radio" name="hurt" value="6"/></div>
				<div class="optionB"><label>팝</label><input type="radio" name="hurt" value="7"/></div>
				<div class="optionB"><label>클래식</label><input type="radio" name="hurt" value="8"/></div>
				<div class="optionB"><label>재즈</label><input type="radio" name="hurt" value="9"/></div>
			</div>
		</div><br/><br/>
		<div>
			<label id="qustionText">나는 당황할 때 &mdash; 노래를 듣는다.</label>
			<div class="optionA">
				<div class="optionB"><label>발라드</label><input type="radio" name="surprise" value="1"/></div>
				<div class="optionB"><label>댄스</label><input type="radio" name="surprise" value="2"/></div>
				<div class="optionB"><label>랩/힙합 </label><input type="radio" name="surprise" value="3"/></div>
				<div class="optionB"><label>락/매탈</label><input type="radio" name="surprise" value="4"/></div>
				<div class="optionB"><label>R&amp;B</label><input type="radio" name="surprise" value="5"/></div>
				<div class="optionB"><label>인디</label><input type="radio" name="surprise" value="6"/></div>
				<div class="optionB"><label>팝</label><input type="radio" name="surprise" value="7"/></div>
				<div class="optionB"><label>클래식</label><input type="radio" name="surprise" value="8"/></div>
				<div class="optionB"><label>재즈</label><input type="radio" name="surprise" value="9"/></div>
			</div>
		</div><br/><br/>
		<div >
			<label id="qustionText">나는 기쁠 때 &mdash; 노래를 듣는다.</label>
			<div class="optionA">
				<div class="optionB"><label>발라드</label><input type="radio" name="happy" value="1"/></div>
				<div class="optionB"><label>댄스</label><input type="radio" name="happy" value="2"/></div>
				<div class="optionB"><label>랩/힙합 </label><input type="radio" name="happy" value="3"/></div>
				<div class="optionB"><label>락/매탈</label><input type="radio" name="happy" value="4"/></div>
				<div class="optionB"><label>R&amp;B</label><input type="radio" name="happy" value="5"/></div>
				<div class="optionB"><label>인디</label><input type="radio" name="happy" value="6"/></div>
				<div class="optionB"><label>팝</label><input type="radio" name="happy" value="7"/></div>
				<div class="optionB"><label>클래식</label><input type="radio" name="happy" value="8"/></div>
				<div class="optionB"><label>재즈</label><input type="radio" name="happy" value="9"/></div>
			</div>
		</div>
			
			
			<input type="submit" name="enter" id="enter" value="Enter" />
		</form>
	</div>
</body>
<style>
	
	input[type=radio] { 
		width:1rem;
		height:1rem;
	}
	
	#qustionText{
		font-size: 1.5rem;
	}

	
 #loginform {
    margin: 0 auto;
    padding-bottom: 1rem;
    background: #B2C7D9;
    width: 600px;
    max-width: 100%;
    border: 2px solid #212121;
    border-radius: 4px;
  }
	
#loginform {
    padding-top: 1rem;
    text-align: center;
  }

#loginform p {
    padding: 15px 25px;
    font-size: 1.4rem;
    font-weight: bold;
  }
	
#enter{
    background: #FFEC42;
    padding: 4px 10px;
    font-weight: bold;
    border-radius: 4px;
    border: none;
    color: #706850;
    margin-top: 1rem;
	border: 1px solid #FFEC42;
  }
  
  .optionB {
  	display: flex;
  	flex-direction: column;
  	font-size:1rem;
  	margin: 0.1rem;
  	width: 11%;
  	font-weight: bold;
  }
  
  .optionB input {
  	margin: auto;
  }
  
  .optionA {
  	margin-top: 0.8rem;
  	display: flex;
  	flex-direction: row;
  }
  
  </style>


