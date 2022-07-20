
<?php


	error_reporting(E_ALL);ini_set('display_errors',1);

	require_once('/var/www/html/post/dbconnect.php');
	
	if (!isset($_POST['id'])) {

	//get the data
	$json = file_get_contents("php://input");

	//convert the string of data to an array
	$data = json_decode($json, true);
	$trackerid = $data["id"];
	$current_timestamp = $data["ts"];
	

	//place data into database


	$sql = "INSERT INTO markers (ts,trackerid,destination,lat,lng,message) VALUES ('".$current_timestamp."','".$data['id']."','".$data['destination']."','".$data['lat']."','".$data['lng']."','".$data['message']."')";
	if ($connection->query($sql) === FALSE) {
		writelog("Error creating new marker record\n");
	}


	//check if this is a new trail
	
	$sql = "SELECT ts FROM markers WHERE trackerid = '".$trackerid."' ORDER BY ts DESC LIMIT 1";
	$result = $connection->query($sql);
	if ($result->num_rows > 0) {

		$row = $result->fetch_assoc();
		$lastmarker_ts = $row["ts"];
		
		
		$sql = "SELECT start, end FROM trails WHERE trackerid = '".$trackerid."' ORDER BY start DESC LIMIT 1";
		$result = $connection->query($sql);
		if ($result->num_rows > 0) {
			$row = $result->fetch_assoc();
			$lasttrail_start = $row["start"];
			$lasttrail_end = $row["end"];
			
			
			$elapsed_seconds = strtotime($lastmarker_ts) - strtotime($lasttrail_end);
			if ($elapsed_seconds > 1860) {
				// create new trail
				create_new_trail($connection,$trackerid,$current_timestamp,$current_timestamp);
			} else {

				// update trail with current end point
				$sql="UPDATE trails SET end = '".$current_timestamp."' WHERE end = '".$lasttrail_end."' AND trackerid = '".$trackerid."'";
				if ($connection->query($sql) === FALSE) {
					writelog("Error updating trail record\n");
				}
			
			}
			
		} else {
			create_new_trail($connection,$trackerid,$current_timestamp,$current_timestamp);
			
		}

	}

	$connection->close();


}

function writelog($msg) {
	
	$path = $_SERVER['DOCUMENT_ROOT'] . '/post/post.log';
	$myfile = fopen($path, "a");
	fwrite($myfile, $msg);
	fclose($myfile);
	
}

function create_new_trail($connection,$tid,$start,$end) {
	
	$sql="INSERT into trails (trackerid, start,end) VALUES ('".$tid."','".$start."','".$end."')";
	
	if ($connection->query($sql) === FALSE) {
		writelog("Error creating new trail\n");
	}

}

?>
