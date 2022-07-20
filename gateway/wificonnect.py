def connect():
	import network
	from config import wifi_config


	station = network.WLAN(network.STA_IF)
	if station.isconnected() == True:
		return True

	station.active(True)
	
	while station.isconnected() == False:
		try:
			station.connect(wifi_config['ssid'], wifi_config['password'])
		except:
			print("",end='')
			return False
			

#	while station.isconnected() == False:
#		pass

	
