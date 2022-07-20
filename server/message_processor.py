import mysql.connector
import string
from datetime import datetime

# This could easily be integrated into the main program.
# I was just pissing around with splitting some of the code up.

unknown_log = "odd_messages.txt"

valid_id_prefix = ["T","R"]

def valid_tracker_id(tid):
	if tid == "" :
		return False
	if tid[0] in valid_id_prefix:
		if int(tid[1:]) in range(1,999):
			return True
		else:
			return False


def storeOddMessage(m):
	# find a place to store weird messages
	f = open("/home/youruserid/bin/odd_messages.log", "a")
	now = datetime.now()
	f.write(str(now)+" : "+m+"\n")
	f.close()

def validateMessage(m):
	#Is this a duplicate message?
	message = m.split(',')
	
	# Standard format string <ID>,<Dest>,<data>,<route>,<packetID>

	fields = len(message)
	if (fields != 7):  #Something's not right. Store Message
		#storeOddMessage(m);
		return False

	if not valid_tracker_id(message[0]):
		print("IDPattern Fail")
		return False
		
	return True;

def isprintable(received):
	stringok = True;
	for i in received:
		if i in string.printable:
			pass
		else:
			stringok = False
			print("Message Unprintable")
			
	return stringok

def inRange(new,old):
	if (old == ""):
		return True
		
	newArray = new.split(',')
	oldArray = old.split(',')

	if ((oldArray[2] == "") or (oldArray[3] =="")):
		return True

	if ((newArray[2] == "") or (newArray[3] == "")):
		return True
		
	newlat = abs(float(newArray[2]))
	newlng = abs(float(newArray[3]))

		
	oldlat = abs(float(oldArray[2]))
	oldlng = abs(float(oldArray[3]))

	if ((newlat < oldlat - .05) or (newlat > oldlat + .05)):
		return False

	if((newlng < oldlng -.09) or (newlng > oldlng + .09)):
		return False

	return True
