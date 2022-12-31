# LoRa
from config import *
import machine
from machine import Pin, SoftSPI, deepsleep, ADC, RTC
from sx127x import SX127x
from collections import OrderedDict
from random import seed

import ujson
from ntptime import settime

# Socket Server
import sys
import select
import socket
import time
import os
import urequests
import gc
import wificonnect
import uos
import re
import random

#Settings

deviceID = "G1"

rtc = machine.RTC()	

tz = -4  #TIMEZONE OFFSET
dst = True #USE Daylight Savings

// Address and port of your socket server
server_address = ('192.168.1.155', 8090) 

config_file = 'gateway.cfg'
logfile = 'gateway.log'
valid_bw = [7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000]
valid_voltage = [3.3,3.4,3.5,3.6,3.7,3.8,3.9,4.0,4.1,4.2]
valid_id_prefix = ["G","T","X","R"]

settables = OrderedDict()
settables["bw"]=valid_bw
settables["sf"]=range(6,13,1)
settables["cr"]=range(5,9,1)
settables["sync_word"]=range(1,256,1)
settables["light_sleep"]=range(30,3601,1)
settables["light_sleep_battery"]=valid_voltage
settables["deep_sleep"]=range(30,3601,1)
settables["deep_sleep_battery"]=valid_voltage
settables["scan_time"]=range(30,3601,1)
settables["nightmode_start"]=range(0,25)
settables["nightmode_end"]=range(0,25)
settables["nightmode_sleep"]=range(30,3601,1)
settables["wait_for_signal"]=range(0,600,1)
settables['logging']=range(0,1,1)

signal_detected = False
signal_detected_start = 0


command_mode = False
command_mode_start = 0

ack_mode = True

coffee_mode = False
verbose = False

battery_pin = ADC(Pin(device_config['battery']))
battery_pin.atten(ADC.ATTN_11DB)

garbage_timeout = 600 #collect garbage after 10 minutes

#Resistor values for battery voltage divider
r1 = 117700.0
r2 = 383000.0

# I did a bunch of readings at different voltages to get a curve
# for the battery, then did a linear regression to get the line
# of best fit.  Does a decent job of approximating the battery voltage

m = 0.00069163  # Slope
b = 1.3548		# y-intercept

rts_array = []

device_spi = SoftSPI(baudrate = 10000000, 
        polarity = 0, phase = 0, bits = 8, firstbit = SoftSPI.MSB,
        sck = Pin(device_config['sck'], Pin.OUT, Pin.PULL_DOWN),
        mosi = Pin(device_config['mosi'], Pin.OUT, Pin.PULL_UP),
        miso = Pin(device_config['miso'], Pin.IN, Pin.PULL_UP))

lora = SX127x(device_spi, pins=device_config, parameters=lora_parameters)

socket_connected = False

#Functions

def load_config():
	global config_settings
	f = open(config_file, 'r')
	config_settings = ujson.loads(f.read())

def save_config():
	f = open(config_file, 'w')
	f.write(ujson.dumps(config_settings))
	f.close()	

def use_config():
	print("Setting Config Values")
	lora.setSignalBandwidth(float(config_settings["bw"]))
	lora.setSpreadingFactor(int(config_settings["sf"]))
	lora.setCodingRate(int(config_settings["cr"]))
	
	lora.setSyncWord(int(config_settings["sync_word"]))
	
	#lora.setSignalBandwidth(125E3)
	#lora.setSpreadingFactor(12)
	#lora.setCodingRate(8)

def print_config():
	sendstr = '\n'+"[Current Settings]"+'\n'
	sendstr = sendstr+"------------------\n"
	for key in settables.keys():
		var = key+' = '+str(config_settings[key])
		sendstr = sendstr+var+'\n'
	sendstr = sendstr+"------------------\n"
	sendstr = sendstr
	socket_send(sendstr)

def print_help():
	sendstr = '\n[Available Commands]\n'
	sendstr = sendstr+"-----------------------\n"
	sendstr = sendstr+"ls = List Settings\nreset = Reset ESP\nbattery =  Battery Voltage\nsleep <seconds> : Sleep ESP\ncoffee : Prevents Sleeping\n"
	sendstr = sendstr+"send <message> : Send LoRa Message\nset <config item> <value>: Modify Settings\ncmd : Enter Command Mode\nverbose : Verbose Mode\nsave : Save Config\n\n"
	sendstr = sendstr+"set tx <bw:sf:cr>\n"
	sendstr = sendstr+"\nlog : Toggle Logging\ndelete log : Delete Log File\nverbose : Show More Gateway Info\nfree : Show Free Filespace and Memory\n"
	sendstr = sendstr+"-----------------------\n"
	socket_send(sendstr)

def logit(message):
	print(message.strip('\n'))
	if config_settings['logging'] == 1:
		#Open Log file in append mode
		log_entry = return_time()+" - "+message
		log_file =  open(logfile, "a+")
		log_file.write(log_entry)
		log_file.close()
		freespace = uos.statvfs("/")
		if freespace[3] < 50:
			socket_send("Running out of Log Space")
		if freespace[3] < 10:
			socket_send("Deleting Log")
			delete_log()

def delete_log(cmd2):
	if cmd2 == 'log':	
		os.remove(logfile)
		socket_send("Log File Deleted")

def toggle_logging():
	if config_settings['logging'] == 0:
		config_settings['logging'] = 1;
		socket_send("---- [Logging Enabled] ----")

	else:
		config_settings['logging'] = 0
		socket_send("---- [Logging Disabled] ----")

	save_config();
		
def get_valid_range(param):
	if param in settables:
		value_range = settables[param]
		return value_range

def print_value_error(param,valid_values):
	vmin=valid_values[0]
	vmax=valid_values[len(valid_values)-1]
	r = str(vmin)+" - "+str(vmax)
	socket_send("Error. "+param+" Valid Range is "+r)

def set_config_value(param,value):
	if param in settables:
		valid_range = get_valid_range(param)
		if param == "light_sleep_battery" or param == "deep_sleep_battery":
			try:
				value = float(value)
			except:
				print_value_error(param,valid_range)
				return False
			if value in valid_range:
				config_settings[param] = value
				socket_send(param+" Set to "+str(value))
				return True
			else:
				print_value_error(param,valid_range)
				return False

		else:
			try:
				value = int(value)
			except:
				print_value_error(param,valid_range)
				return False
			if value in valid_range:
				config_settings[param] = value
				socket_send(param+" Set to "+str(value))
				use_config()
				return True
			else:
				print_value_error(param,valid_range)
				return False
	else:
		socket_send("Not a settable parameter")

def set_tx(command):
	cmd = command.split(":")
	bw = cmd[0]
	sf = cmd[1]
	cr = cmd[2]
	updateOK = True
	
	if not set_config_value("bw",bw):
		updateOK = False
	if not set_config_value("sf",sf):
		updateOK = False
	if not set_config_value("cr",cr):
		updateOK = False
	use_config()


def parse_socket_data(data):

	global command_mode
	global command_mode_start
	global verbose
	global coffee_mode
	global ack_mode
	

	command = data.split(" ")
	sendstr = ""
	
	if command[0] == "set":
		if command[1] != "tx":
			set_config_value(command[1],command[2])
		else:
			set_tx(command[2])

	if command[0] == 'send':
		print("sending Message:",end="")
		message = data.lstrip(command[0]+" ")
		print(message)
		lora_write(message)
		print("message sent")
		
	if command[0] == 'ls':
		print_config()
	
	if command[0] == 'log':
		toggle_logging()
	
	if command[0] == 'reset':
		machine.reset()
		
	if command[0] == 'battery':
		v = battery_check()
		socket_send(str(round(v,3))+" V")
		
	if command[0] == 'sleep':
		naptime(int(command[1]))
	
	if command[0] == 'delete':
		delete_log(command[1])

	if command[0] == 'free':
		freemem = gc.mem_free()
		freespace = uos.statvfs("/")
		socket_send(str(freespace[3])+" FS Blocks / "+str(freemem)+" Bytes Memory")

	if command[0] == 'ack':
		if ack_mode == False:
			ack_mode = True
			socket_send("---- [ACK Mode ON] ----")
		else:	
			sendstr = sendstr+"------------------\n"
			socket_send("---- [ACK Mode OFF] ----")
			ack_mode = False


	if command[0] == 'cmd':
		if command_mode == False:
			command_mode = True
			socket_send("---- [Command Mode ON] ----")
			command_mode_start = time.time()
		else:	
			sendstr = sendstr+"------------------\n"
			socket_send("---- [Command Mode OFF] ----")
			command_mode = False
			
	if command[0] == 'verbose':
		if verbose == False:
			verbose = True
			socket_send("---- [Verbose Mode ON] ----")
		else:
			verbose = False
			socket_send("---- [Verbose Mode OFF] ----")
		
	if command[0] == 'save':
		save_config()
		socket_send("Config Saved")
	if command[0] == 'coffee':
		if coffee_mode == False:
			coffee_mode = True
			socket_send("---- [Coffee Mode ON] ----")
		else:
			coffee_mode = False
			socket_send("---- [Coffee Mode OFF] ----")

	if command[0] == '?':
		print_help()

def socket_connect():

	global s
	global socket_connected
	retry_timer = time.time()
	
	if not socket_connected:
		print("Connecting Socket...",end='')
		logit("Connecting Socket\n")
		while not socket_connected and time.time() < retry_timer +20: # Attempt socket connection for 20 seconds
			try:
				s = socket.socket()
				s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
				s.connect(server_address)
				print("[OK]")
				logit("Socket Connected\n")
				socket_connected = True
			except Exception:
				print(".",end='')
				time.sleep(1)
				socket_disconnect()
		if not socket_connected:
			logit("Socket Connection Failed...Rebooting\n")
			machine.reset()

def socket_disconnect():

	msg("Socket Disonnect")

	global s
	global socket_connected
	s.close()
	socket_connected = False
	logit("Socket Disconnected\n")

def socket_read():
	global socket_connected
	global s

	connect_wifi()
	
	if not socket_connected:
		socket_connect()

	socket_list = [sys.stdin, s]		 
	# Get the list sockets which are readable
	ready_to_read,ready_to_write,in_error = select.select(socket_list , [], [],0)
	for sock in ready_to_read:
		if sock == s:
			
			try:
				data = s.recv(4096)
				
				if data:
					data = data.decode('ascii')
					parse_socket_data(data)
				else:
					socket_disconnect()

			except Exception as e:
				socket_disconnect()
				logit("Error During socket_read:"+str(e))
					
		
def socket_send(outgoing):

	global socket_connected
	global command_mode_start
	
	if not socket_connected:
		socket_connect()
	try:
		s.send(outgoing)

	except Exception as e:
		socket_disconnect()
		logit("Socket Send Failed - "+str(e)+"\n")

	command_mode_start = time.time()

def valid_device_id(tid):
	if tid[0] in valid_id_prefix:
		if int(tid[1:]) in range(1,999):
			return True
		else:
			return False

def lora_read():
	global signal_detected
	global signal_detected_start
	if lora.receivedPacket():
		print("packet received")
		try:
			logit("packet available\n")
			payload = lora.readPayload()
			print(payload)
			payload = payload.decode('ascii')
			logit("packet decoded : "+payload+"\n")

			message = payload.split(",")
			sig = str(lora.packetRssi())+","+str(lora.packetSnr())
			tmp = payload.split(",")
			if not command_mode or (command_mode and tmp[2] == ""):
				socket_send(payload+","+sig)
			signal_detected = True
			signal_detected_start = time.time()
			send_ACK()
			
		except Exception as e:
			print("Error:"+str(e))
			socket_disconnect()


def lora_write(message):
	parsed = message.split(" ",1)
	elements = len(parsed)
	if elements > 1:
		dest = parsed[0]
		if valid_device_id(dest):
			message = parsed[1]
			# Format : <ID>,<Dest>,<data>,<route>,<packetID>
			packetID = random.randint(1000,9999)
			route = "99" #99 means no known route..Outoing messages are like that.
			outbound = deviceID+","+dest+","+message+","+route+","+str(packetID)
			print(outbound)

			lora_read() # Make sure there is not an incoming message
			sendtimer = time.ticks_ms()
			
			lora.println(outbound)
			endtimer = time.ticks_ms()
			elapsed = endtimer - sendtimer;
			socket_send("Message Transmitted ("+str(elapsed)+" ms)")
		else:
			socket_send("Invalid Device ID")

def send_ACK():
	if(ack_mode):
		lora.println("ACK")

def is_nightmode():
	
	t = time.localtime()
	h = t[3]
	m = t[4]

	if h >= int(config_settings["nightmode_start"]) and h < int(config_settings["nightmode_end"]):
		return True
	else:
		return False

def battery_check():
	tot = 0
	for x in range(6):
		a = battery_pin.read()
		tot = tot +a
	a = tot/6
	v = ((3.19*a)/4095) * ((r1+r2)/r2)
	v = (m*a)+b
	msg("analog-read="+str(a))
	return v


def check_timers():
	global command_mode
	global signal_detected
	global signal_detected_start
	global garbage_timer

	if time.time() > garbage_timer + garbage_timeout:
		socket_disconnect()
		msg("collecting garbage")
		gc.collect()
		freemem = gc.mem_free()
		logit("Free Memory = "+str(freemem)+"\n")
		freespace = uos.statvfs("/")
		logit("Free File Blocks = "+str(freespace[3])+"\n")

		socket_connect()
		garbage_timer = time.time()

	if command_mode:
		if time.time() > command_mode_start+60:
			socket_send("---- [Command Mode OFF] ----")
			command_mode = False
	
	if time.time() > signal_detected_start + config_settings["wait_for_signal"]:
		signal_detected = False
	
def do_sleep():
	global starttime
	v = battery_check()
	if coffee_mode:
		msg("Coffee Mode - Skipping Sleep")
		starttime = time.time()
		return
		
	if command_mode:
		msg("Command Mode - Skipping Sleep")
		starttime = time.time()
		return
	if signal_detected:
		if (v < float(config_settings["light_sleep_battery"])):
			msg("Signal Detected - Skipping Sleep")
			starttime = time.time()
			return
	if is_nightmode():
		msg("Nightmode Active")
		naptime(int(config_settings["nightmode_sleep"] * 60))
			
	#time to sleep, but how long?
	if v < 3.35:
		# Battery Critical.  Sleep for an hour
		socket_send("Battery Critical, Sleeping 1 Hour")
		naptime(3600)
	if v < float(config_settings["deep_sleep_battery"]):
		msg("Entering Deep Sleep")
		naptime(int(config_settings["deep_sleep"]))
	if (v < float(config_settings["light_sleep_battery"])):
		msg("Entering Light Sleep")
		naptime(int(config_settings["light_sleep"]))
	starttime = time.time()

def naptime(t):
	try:
		int(t)
	except:
		socket_send("Usage: sleep <time in seconds>")
		return
	if t > 0:
		message = "[SYSTEM OFFLINE : "+str(t)+"s]"
		socket_send(message)
		socket_disconnect()
		lora.sleep()
		machine.deepsleep(t*1000)
	else:
		socket_send("Usage: sleep <time in seconds>")
	
def connect_wifi():
	global socket_connected
	if wificonnect.connect() == False:
		print("Connecting to Wifi...",end="")
		while not wificonnect.connect():
			print('.',end="")
			socket_connected = False
			time.sleep(1)
			
		print("[OK]")
			
def add_zero(inval):
	if inval < 10:
		v = '%02d' % inval
	else:
		v = str(inval)
	return str(v)
	
def adjust_tz(h):

	if tz < 0 :
		if h < abs(tz):
			h = 12 - (abs(h-tz))
		else:
			h = h + tz 
	else:
		if h > (12-tz):
			h = (h + tz) -12
		else:
			h = h + tz

	return h

def adjust_dst(h,m,d):
	if (m >= 3 and d < 8) and (m < 11):
		return h
	if (m >= 11) or (m <= 3 and d < 8):
		return h - 1
	
def return_time():

	t = time.localtime()
	h = t[3]
	m = t[4]
	s = t[5]
	return add_zero(h)+":"+add_zero(m)+":"+add_zero(s)

def set_time():
	try:
		settime()
		(year, month, mday, week_of_year, hour, minute, second, milisecond)=RTC().datetime()
		RTC().init((year, month, mday, week_of_year, hour-5, minute, second, milisecond))
	except:
		logit("Failed to set time\n")
		machine.reset()

	
def msg(instring):
	if verbose == True:
		socket_send(return_time()+" - "+instring)
 	print(instring)
	

# MAIN PROGRAM #

print("Starting LoRa Receiver")

connect_wifi()


load_config()
use_config()
socket_connect()


set_time()

starttime = time.time()
print("Time Now : " + return_time())
socket_send("[SYSTEM ONLINE]")
logit("--System Restart--\n")

garbage_timer = time.time()
seed(241) #Initialize Random Number Generator



while True:
	if time.time() > starttime + int(config_settings["scan_time"]):
		do_sleep()
	socket_read()
	lora_read()
	check_timers()

