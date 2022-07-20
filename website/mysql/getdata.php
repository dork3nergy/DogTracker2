<?php

	header("Access-Control-Allow-Origin: *");

	error_reporting(E_ALL);ini_set('display_errors',1);

	$config = parse_ini_file('/var/www/private/mysqlcred.php'); 
	$connection = mysqli_connect($config['servername'],$config['username'],$config['password'],$config['dbname']);

	$query = "SELECT message FROM markers WHERE trackerid='X01' order by ts DESC LIMIT 10";
	$result = mysqli_query($connection, $query);
	if (!$result) {
	  die('Invalid query');
	}

	// Iterate through the rows, adding XML nodes for each
	header("Content-type: text/xml");
	// Start XML file, echo parent node
	echo "<?xml version='1.0' ?>";
	echo("<signaldata>");

	while ($row = @mysqli_fetch_assoc($result)){
	  // Add to XML document node
		echo('<signaldata ');
		echo('message="' . $row['message'] . '" ');
		echo('/>');
	}
	echo('</signaldata>');

	$connection -> close();
?>
