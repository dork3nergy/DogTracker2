<?php
	$starttime = $_GET[starttime];
	header("Access-Control-Allow-Origin: *");

	error_reporting(E_ALL);ini_set('display_errors',1);

	$config = parse_ini_file('/var/www/private/mysqlcred.php'); 
	$connection = mysqli_connect($config['servername'],$config['username'],$config['password'],$config['dbname']);

	// Change the Tracker your want to monitor here :
	$query = "SELECT start FROM trails WHERE trackerid='T1' ORDER BY end DESC LIMIT 1";
	
	$result = mysqli_query($connection, $query);
	if (!$result) {
	  die('Invalid query');
	}
	while ($row = @mysqli_fetch_assoc($result)){
		$starttrail = $row['start'];
	}
	
	// And Here
	$query = "SELECT lat,lng FROM markers WHERE trackerid='T1' AND ts > '".$starttrail."' AND lat <> 'NULL' ORDER BY ts ASC";
	
	$result = mysqli_query($connection, $query);
	if (!$result) {
	  die('Invalid Squery');
	}

	// Iterate through the rows, adding XML nodes for each
	header("Content-type: text/xml");
	// Start XML file, echo parent node
	echo "<?xml version='1.0' ?>";
	echo("<markers>");

	while ($row = @mysqli_fetch_assoc($result)){
	  // Add to XML document node
		echo('<marker ');
		echo('lat="' . $row['lat'] . '" ');
		echo('lng="' . $row['lng'] . '" ');
		echo('/>');
	}
	echo('</markers>');

	$connection -> close();
?>
