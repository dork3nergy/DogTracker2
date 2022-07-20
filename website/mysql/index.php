<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0, user-scalable=no">
    <meta charset="utf-8">
    <title>Mysql Monitor</title>
	

	</head>
	<style>

		#container{
			height:100%;
			width:100%;
		}
		

	</style>
	<body>
		<div id = "container">

</html>

<script>

	setInterval (refresh, 5000 );
	refresh();

	function refresh(){
		dlURL = "getdata.php";
		downloadUrl(dlURL, function(data,status) {
			var xml = data.responseXML;
			if(status == 200) {
				var sigdata = xml.documentElement.getElementsByTagName('signaldata');
				document.body.innerHTML = '';

				Array.prototype.forEach.call(sigdata, function(row) {
					var msg = row.getAttribute('message');
					document.write(msg);
					document.write("<br>");

				});
			}
		});
	}


	function downloadUrl(url, callback) {
		var request = window.ActiveXObject ?
		new ActiveXObject('Microsoft.XMLHTTP') :
		new XMLHttpRequest;

		request.onreadystatechange = function() {
		  if (request.readyState == 4) {
			request.onreadystatechange = doNothing;
			callback(request, request.status);
		  }
		};

		request.open('GET', url, true);
		request.send(null);
	}


	function doNothing() {}
      
</script>
		</div>
	</body>



