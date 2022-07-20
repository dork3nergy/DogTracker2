#!/usr/bin/python3

import sys
import socket
import selectors
import types
from os import system
from datetime import datetime, timedelta
import time
import message_processor as mp
import mysqlfunc as mysql

sel = selectors.DefaultSelector()
host = '0.0.0.0' #this runs from you server which is why it's all zeros
port = 8090  #Socket Server Port
deviceID = "GW"
rts_array = []


def writeLog(entry):

	f = open("/home/eturd/bin/gateway.log", "a")
	now = datetime.now()
	f.write(str(now)+" : "+entry+"\n")
	f.close()

def process_received(received):
	global lastreceived
	#Is message a duplicate / printable /in range?
	print("Checking Message")
	if(mp.isprintable(received)):
		print("Message Printable")
		if(mp.validateMessage(received)):
			print("Message Passed Validation")
			mysql.storeMessage(received)
			lastreceived = received
		else:
			("Message Failed Validation")
	else:
		#writeLog("Non-Printable characters detected-Skipping")
		print("message nonprintable")

		
def accept_wrapper(sock):
	conn, addr = sock.accept()  # Should be ready to read
	#print("accepted connection from", addr)
	conn.setblocking(False)
	data = types.SimpleNamespace(addr=addr, inb=b"", outb=b"")
	events = selectors.EVENT_READ | selectors.EVENT_WRITE
	sel.register(conn, events, data=data)

def service_connection(key, mask,events):
	sock = key.fileobj
	data = key.data
	if mask & selectors.EVENT_READ:
		try:
			recv_data = sock.recv(1024)  # Should be ready to read
		except OSError as err:
			print("Socket Receive Error:",err)
			recv_data = ""

		if recv_data:
			print(recv_data)
			data.outb += recv_data
			try:
				received = recv_data.decode("ascii")
				print("Received : "+received)
				writeLog(received)
				
			except:
				#writeLog("Decode Error - Skipped")
				received=""
				print("Decode Error")
			

			if (received != ""):	
	
				process_received(received)

				

		else:
			#print("closing connection to", data.addr)
			sel.unregister(sock)
			sock.close()
	if mask & selectors.EVENT_WRITE:
		if data.outb:
			try:
				b = len(data.outb) #Number of bytes in data.outb
				for key,mask in events: #send data to other clients
					s=key.fileobj
					s.send(data.outb)
			except:
				print("Socket Send Error")
				
			data.outb = data.outb[b:] #remove bytes from data.outb

def main():
	lsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	lsock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	lsock.bind((host, port))
	lsock.listen()
	print("listening on", (host, port))
	lsock.setblocking(False)
	sel.register(lsock, selectors.EVENT_READ, data=None)

	try:
		while True:
			#time.sleep(.02)
			events = sel.select(timeout=None)
			for key, mask in events:
				if key.data is None:
					accept_wrapper(key.fileobj)
				else:
					service_connection(key, mask,events)
	except KeyboardInterrupt:
		print("caught keyboard interrupt, exiting")
		sel.close()
		lsock.close()
		exit()
	finally:
		sel.close()
	
if __name__ == "__main__":
	
	lastreceived = ""
	lastSignal = "";
	withinrange = True
	writeLog("----RESTART---")
	main()
