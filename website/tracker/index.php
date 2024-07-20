<DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0, user-scalable=no">
    <meta charset="utf-8">
    <title>DorkEnergy Dog Tracker</title>
	

	</head>
	<style>

		a:link {
			decoration:none;
		}
		a:visited {
			decoration:none;
		}
	
		#container{
			height:100%;
			width:100%;
		}
		
		#map {
			height: 88%;
			border: 4px solid black;
		}

		.button{
		  background-color: #4CAF50; /* Green */
		  border: none;
		  color: white;
		  padding: 10px 10px;
		  text-align: center;
		  text-decoration: none;
		  display: inline-block;
		  font-size: 16px;
		}

		.button2 {background-color: #008CBA;} /* Blue */
		.button3 {background-color: #f44336;} /* Red */ 

		#centerbutton{
			text-align:center;
			padding-top:10px;
		}

	</style>
	<body>
		<div id = "container">
			<div id="map"></div>
			<div id="centerbutton">
				<button class="button button2" onclick="recenter()">Recenter</button>
				<button class="button" onclick="resetmap()">Reset</button>

			</div>
		</div>
	</body>

</html>

    <script>
		var map;
		var flightPath;
		var currentmarker;
		var startTime = getDatetime();
		var home = {lat: 49.265314,lng:-72.834560};
		var activity = "now";
		var lastLocation = {lat: 49.265314,lng:-72.834560};
		var prev_markers = 0;
		var dot = 'images/marker2.png';
		
		
//		var startTime = getDate();
	
		console.log(startTime);
		
        function initMap() {
			map = new google.maps.Map(document.getElementById('map'), {
				zoom: 17
			});
			

			var marker = new google.maps.Marker({
				map: map,
			});
			map.setCenter(home);
			refreshMap();
			
		}
		setInterval (refreshMap, 5000 );

		function recenter(){
			map.setCenter(lastLocation);
			map.setZoom(17);
			refreshMap();

		}
		function resetmap(){
			activity="now";
			startTime = getDatetime();
			refreshMap();
		}

		function todaymap(){
			activity = "today";
			refreshMap();
		}

		function refreshMap(){
			
			console.log("Refreshing\n");
			var flightPlanCoordinates = [];
			
			if (activity == "now"){
				dlURL = "getpoints.php?starttime="+startTime
			}
			//if (activity == "today"){
				//dlURL = 'getpoints.php?starttime="'+getDate()+'"'
			//}
			var markers;
			var curr_markers = 0;
			downloadUrl(dlURL, function(data,status) {
				var xml = data.responseXML;
				if(status == 200) {
					if (xml != null) {
						var markers = xml.documentElement.getElementsByTagName('marker');
						curr_markers = markers.length
					}
					
					if(curr_markers != prev_markers){

						Array.prototype.forEach.call(markers, function(markerElem) {
							var lat = markerElem.getAttribute('lat');
							var lng = markerElem.getAttribute('lng');

							var point = new google.maps.LatLng(
							parseFloat(markerElem.getAttribute('lat')),
							parseFloat(markerElem.getAttribute('lng')));

							flightPlanCoordinates.push(point);
							lastCoordinate = point;
						});
						
						if (flightPath != null) {
							//clear current flightpath
							flightPath.setMap(null);
						}
						
						flightPath = new google.maps.Polyline({
							  path: flightPlanCoordinates,
							  geodesic: true,
							  strokeColor: '#FF0000',
							  strokeOpacity: 1.0,
							  strokeWeight: 2
						});
						if (currentmarker){
							currentmarker.setMap(null);
						}
						currentmarker = new google.maps.Marker({
							position: lastCoordinate,
							map: map,
							icon: dot,
							//animation: google.maps.Animation.BOUNCE

						});


						map.setCenter(lastCoordinate);
						lastLocation = lastCoordinate;
						flightPath.setMap(map);
						prev_markers = markers.length;
					}
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
		
		function getDatetime(){
			dt = new Date();
			dt.setHours(dt.getHours()-4); 

			dt = dt.toISOString().split('.')[0];
			d = dt.split('T')[0];
			t = dt.split('T')[1];
			return d +' '+t;
		}	
		function getDate(){
			dt = new Date();
			dt.setHours(dt.getHours()-4); 

			dt = dt.toISOString().split('.')[0];
			d = dt.split('T')[0];
			return d;
		}	

		function doNothing() {}
      
</script>

<script async defer
src="https://maps.googleapis.com/maps/api/js?key=<YOUR_MAPS_API_KEY&callback=initMap">
</script>

