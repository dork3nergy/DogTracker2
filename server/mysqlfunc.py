import re
import mysql.connector
from datetime import datetime, timedelta

newtrailtime = 31 #Elapsed minutes between trails to make a new trail

GPSPattern = re.compile("^-?[0-9]\d*(\.\d+)?$")

# Find a palce to store your logs
mysqllog = "/home/<youruserid>/bin/mysql.log"

def connectDB():
	global trackerdb
	try:
		trackerdb = mysql.connector.connect(
		  host="localhost",
		  user="<YOUR MYSQL USERNAME>",
		  password="<YOUR MYSQL PASSWORD>",
		  database="tracker"
		)
		return True
	except:
		print("Failed to Open DB Connection");
		return False

def disconnectDB():
	trackerdb.commit()
	try:
		trackerdb.close();
	except:
		writeLog("Failed to close DB Connection") 
		print("Failed to close DB Connection");

def writeLog(m):
	f = open(mysqllog, "a")
	now = datetime.now()
	f.write(str(now)+" : "+m+"\n")
	f.close()


def getDatetime():
	now = datetime.now() # current date and time
	dt = now.strftime("%Y-%m-%d %H:%M:%S")
	return dt


def isGPSData(data):
	datafield = data.split('&')
	print(datafield)
	try:
		if(GPSPattern.match(datafield[0].replace(" ","")) and GPSPattern.match(datafield[1].replace(" ",""))):
			return True	
		else:
			return False
	except:
		return False


def mysql_command(mcommand,mdata):
	try:
		connectDB()
		mcursor = trackerdb.cursor()
		mcursor.execute(mcommand, mdata)
		mcursor.close()

		disconnectDB()
	except:
		print("Mysql INSERT failed")
		writeLog("Failed: " + mcommand)
	
def mysql_select(mcommand,mdata):
	connectDB();
	mcursor = trackerdb.cursor()
	mcursor.execute(mcommand, mdata)
	mresults = mcursor.fetchall()
	mcursor.close()
	disconnectDB()
	return mresults

		
def storeMessage(m):
	# Get DateTime and format it
	
	data = m.split(',')
	dt = getDatetime()
	trackermessage = ""
	trackermessage = data[2].strip(" ");
	
	# Standard format string <ID>,<Dest>,<data>,<route>,<packetID>,<rssi>,<snr>
	
	print("storing message")
	#insert message into database
	
	if (isGPSData(data[2])):
		print("GPS Data")
		datafield = trackermessage.split('&')
		lat = datafield[0].replace(" ","")
		lng = datafield[1].replace(" ","")
		insertstring = 'INSERT INTO markers (ts,trackerid,destination,lat,lng,route,rssi,snr) VALUES (%s,%s,%s,%s,%s,%s,%s,%s)'
		insertdata = (dt, data[0],data[1],lat,lng,data[3],data[5],data[6])

	else:
		print("MSG Data")
		insertstring = 'INSERT INTO markers (ts,trackerid,destination,message,route,rssi,snr) VALUES (%s,%s,%s,%s,%s,%s,%s)'
		insertdata = (dt, data[0],data[1],trackermessage,data[3],data[5],data[6])

	

	mysql_command(insertstring, insertdata)
	newTrackCheck(data[0])

def newTrackCheck(tracker_id):

	# Get Last Entry in MARKERS for trackerid
	selectstring = "SELECT ts FROM markers WHERE trackerid = %s ORDER BY ts DESC LIMIT 1"
	selectdata = (tracker_id,)
	mresult = mysql_select(selectstring,selectdata)
	last_marker_ts = mresult[0][0]

	# Get Last Entry in TRAILS for trackerid
	selectstring = "SELECT start, end FROM trails WHERE trackerid = %s ORDER BY start DESC LIMIT 1"
	selectdata = (tracker_id,)
	mresult = mysql_select(selectstring,selectdata)
	if(mresult):
		#A previous track exists for this tracker
		last_trail_start = mresult[0][0]
		last_trail_end = mresult[0][1]

		#does this new point indicate a new trail?
		elapsed = last_marker_ts - last_trail_end
		seconds_in_day = 24*60*60
		elapsed_minutes= divmod(elapsed.days * seconds_in_day + elapsed.seconds,60)
		#If time elapsed since previous trail is greater than newtrailtime, start a new trail
		if (elapsed_minutes[0] > newtrailtime):
			insertstring="INSERT into trails (trackerid, start,end) VALUES (%s,%s,%s)"
			insertdata = (tracker_id, last_marker_ts, last_marker_ts)
			mysql_command(insertstring,insertdata)
		else: # MODIFY current trail in trails database with new end timestamp
			commandstring="UPDATE trails SET end = %s WHERE end = %s AND trackerid = %s"
			commanddata=(last_marker_ts, last_trail_end, tracker_id)
			mysql_command(commandstring, commanddata)
	else:
		#Trails database is empty
		insertstring = "INSERT INTO trails (trackerid, start, end) VALUES (%s,%s,%s)"
		insertdata=(tracker_id, last_marker_ts, last_marker_ts)
		mysql_command(insertstring, insertdata)
