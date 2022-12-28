<?php
 
session_start();

 
if(isset($_GET['logout'])){    
     
    //Simple exit message
    $file_name = "./log/".$_SESSION['name']."_log.html";
    $outmsg = "You has left this session ".date("g:i A").".";
    $logout_message = array("chatTime" => date("g:i A"), "userName" => "-1", "msg" => $outmsg);
	$logout_message	 = ",".json_encode($logout_message, JSON_UNESCAPED_UNICODE);
	file_put_contents($file_name, $logout_message, FILE_APPEND | LOCK_EX);

        
    session_destroy();
    header("Location: index.php"); //Redirect the user
}

 
?>
 
<!DOCTYPE html>
<html lang="en">
    <head>
        <meta charset="utf-8" />
 
        <title>AI Chat Application</title>
        <meta name="description" content="AI Chat Application" />
        <link rel="stylesheet" href="style.css?after28" />
    </head>
    <body>
    <?php
    if(!isset($_SESSION['name'])){
    ?>
        <div id="loginform">
			<p>Please enter your name to continue!</p>
			<form action="index2.php" method="post">
				<label for="name">Name &mdash;</label>
				<input type="text" name="name" id="name" />
				<input type="submit" name="enter" id="enter" value="Enter" />				
			</form>
			<span id="errorMessage" name="errorMessage"></span>
		</div>
		
		<script type="text/javascript" src="https://cdnjs.cloudflare.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>
        <script type="text/javascript">
        	$(document).ready(function () {
        		function idsearch() {        			
        			var id = $("#name").val();
        			
        			$.ajax({
                		type:"POST",
                		url:"index2.php",
                		cache: false,
                		async: false,
                		data:{ name: id, enter: "enter" },
                		success: function () {
                			//alert("success!");
                		}
                		
                	}).done(function(data) {
                		if(data[0] == "n") {
                			alert('success!!');
					    	location.reload();	
                		}
                		else {
                			alert('answer some question!');
                			location.href='./question.php';
                		}
     			
					})
                	.fail(function(xhr, status, errorThrown) {
					    alert('fail');
					    var msg= JSON.parse(xhr.responseText.trim());
					    $("#errorMessage").html(msg.error.msg);
					})
        		}
        		
        		
        		$("#enter").click(function (){
    				idsearch();
    				return false;
        		});
        	});        	
        </script>
        
	<?php
    }
    else {
    ?>
        <div id="wrapper">
        	<div class="music">
        		<div class="musicinputArea">
        			<p class="musicRT"><b>추천 음악</b></p>
        			<button name="submitMusic" type="submit" id="submitMusic" value="Music">Music</button>
        		</div>
        		<div class="musicList" id="musicList">
        			
        		</div>
        	</div>
        	<div class="chat">
        		<div id="menu">
	                <p class="welcome">Welcome, <b><?php echo $_SESSION['name']; ?></b></p>
	                <p class="logout"><a class="buttonS" id="exit" href="#">Exit Chat</a></p>
	                <p class="logDelete"><a class="buttonS" id="deleteLog" href="#">Delete Log</a></p>
	            </div>
	 
	            <div id="chatbox">
	                    
	            </div>
	            
	 			<div class="inputArea">
	 				<form name="message" action="">
		                <p><textarea name="usermsg" id="usermsg" cols="50" rows="2" autofocus></textarea></p>
		                <div class="buttonArea">
		                	<input name="submitmsg" type="submit" id="submitmsg" value="Send" />
		                </div>
	            	</form>
	            	
	 			</div>
        	</div>
        	
            
            
        </div>
        <script type="text/javascript" src="https://cdnjs.cloudflare.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>
        <script type="text/javascript">
            // jQuery Document
            
            
            $(document).ready(function () {
                
                window.onload = loadLog();
      
                function loadLog() {
                    var oldscrollHeight = $("#chatbox")[0].scrollHeight - 20; //Scroll height before the request
                    var file_name = "<?php echo $_SESSION['name'];?>";
                    file_name = "./log/" + file_name + "_log.html";

 
                    $.ajax({
                        url: file_name,
                        cache: false,
                        datatype: "text",
                        success: function (msgData) {

                        	html_data = "";
                        	var msg = msgData.substr(1);
                        	msg = "[" + msg + "]";

                        	$.each(JSON.parse(msg), function(i, item) {
                        		if(item.userName == "-1") {
                        			html_data += "<div class='msgln2'><span class='left-info'>" + item.msg + "</span><br></div>";
                        		}
                        		else if(item.userName == "chatbot"){
                        			
                        			html_data += "<div class='msgln'><div class='user-name'><b >" + item.userName + "</b></div> " + "<div class='msgbox'>" + item.msg +"</div>"+ "<div class='chat-time'>" + item.chatTime + "</div><br></div>";
                        		}
                        		else {
                        			html_data += "<div class='msgln3'><div class='msgln3_1'><div class='chat-time'>" + item.chatTime + "</div>" + "<div class='msgbox2'>" + item.msg + "</div>" +"<br></div></div>";
                        		}
                        		
                        	});
                        	
                            $("#chatbox").html(html_data); //Insert chat log into the #chatbox div
 
                            //Auto-scroll           
                            var newscrollHeight = $("#chatbox")[0].scrollHeight - 20; //Scroll height after the request
                            if(newscrollHeight > oldscrollHeight){
                                $("#chatbox").animate({ scrollTop: newscrollHeight }, 'normal'); //Autoscroll to bottom of div
                            }   
                        }
                    });
                }
                
                function sendMsgF() {
           			var clientmsg = $("#usermsg").val();
           			
           			if(clientmsg == "" || clientmsg == "\n") {
           				$("#usermsg").val("");
           				return false;
           			}
           			
                	$.ajax({
                		type:"POST",
                		url:"socket.php",
                		cache: false,
                		data:{ msg: clientmsg },
                		success: function () {
                			//console.log("성공");
                		}
                		
                	}).done(loadLog());
                	
                	
                    $("#usermsg").val("");
                }
                
                
                function sendMusicF() {         			
                	$.ajax({
                		type:"POST",
                		url:"post2.php",
                		cache: false,              		
                	}).done(function(data) {
                		console.log(data);
     			
					});
                }
                
                function receiveMusicF() {
                	$.ajax({
                		type:"POST",
                		url:"post3.php",
                		cache: false,               		
                	}).done(function(data) {
                		console.log(data);
     					$("#musicList").html(data);
					});
                }
                
                            
                $("#submitmsg").click(function(){
                	
                	sendMsgF();
                    return false;   	
                });
                
                
                $("#submitMusic").click(function(){
                	sendMusicF();
                	receiveMusicF();
                	return false;
                });
                
                document.getElementById('usermsg').addEventListener('keydown',function(event){
			        if(event.keyCode ==13){
			        event.preventDefault();
			            sendMsgF();
			        }		        
		    	});
                
                function deleteLogF(temp) {
                	var file_name = "<?php echo $_SESSION['name'];?>";
                	const url_name = "./logDelete.php";
                	var deleteLog = true;
                	if(temp == "temp") {
                    	file_name = "./log/" + file_name + "_log_c.txt";
                    	$.ajax({
	                        url: url_name,
	                        data: {'file' : file_name },
					    	method: 'GET',
	                        success: function (response) {
					        	//
					        	console.log("cache");
					    	}
	                    });	
                    }
                    else if(temp == "all"){
                    	file_name = "./log/" + file_name + "_log.html";	 
                    	deleteLog = confirm("Are you sure you want to delete the all session data?");
                    	if (deleteLog == true) {						
	                    $.ajax({
	                        url: url_name,
	                        data: {'file' : file_name },
					    	method: 'GET',
	                        success: function (response) {
					        	if( response == true ) {
					            	alert('File Deleted!');
					            	console.log("whole log");
					        	}
					         	else alert('Something Went Wrong!');
					    	}
	                    });	                    
	            	}
                    }
                	
                    
	                window.location.reload();
                }
                
                
                
                //$.when($.ajax("socket.php")).done(loadLog());
 
                $("#deleteLog").click(function () {
                    deleteLogF("temp");  
                    deleteLogF("all");                  
                    return false;                    
                });
 
                setInterval (loadLog, 1000);
                
                
 
                $("#exit").click(function () {
                    var exit = confirm("Are you sure you want to end the session?");
                    if (exit == true) {
                    	deleteLogF("temp");
                    	window.location = "index.php?logout=true";
                    }
                    return false;
                });            
                
            });
        </script>
    </body>
</html>
<?php
}
?>
