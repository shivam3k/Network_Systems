----------------------------------------------- README.txt --------------------------------------------------
COURSE : Network Systems (CSCI 5273)
SUBMITTED BY : Shivam Khandelwal

1. FILE STRUCTURE :

	The UDP folder contains these files - 
			
			A. SERVER - 
				1. foo1 (50 KB jpeg file)
				2. foo2 (10 KB jpeg file)
				3. foo3 (50 MB jpeg file)
				4. uftp_server.c
				5. Makefile
				6. server_output_files
			
			B. CLIENT - 
				1. foo1 (50 KB jpeg file)
				2. foo2 (10 KB jpeg file)
				3. foo3 (50 MB jpeg file)
				4. uftp_client.c
				5. Makefile
				6. client_output_files
				
-------------------------------------------------------------------------------------------------------------
			
2.	MAKEFILE COMMANDS - 
	
			A. SERVER - 
				1. make : generates output file - server
				2. make clean : removes output file - server 
				
			B. CLIENT - 
				1. make : generates output file - client
				2. make clean : removes output file - client
				
-------------------------------------------------------------------------------------------------------------

3. FILE OPERATIONS - 
		
		1. gt <file name> : Get the file specified by user at client from the server if found.
		2. pt <file name> : Put the file specified by user in server if file exits in client directory.
		3. dl <file name> : Delete the file specified by user from server directory if found.
		4. ls		  : Fetch the current list of files in server directory.
		5. ex		  : Exit the server gracefully.
		
-------------------------------------------------------------------------------------------------------------

4. DATA PACKET TYPES - 

		To ensure reliable transfer, certain packet types were created with different fields. 
		These fields are :
		
		a. Packet type 		  	- 	Byte 0
		b. Packet Sequence Number 	- 	Byte 1 - 6
		c. Data length	(d)		- 	Byte 7 - 12
		d. Command type			-	Byte 13
		e. Data				-	Byte 14 - (14 + d)
		
		The different types of packets defined are - 
		
		1. Command Packet 			(C) : Send command to server to perform specified file operation
		2. Data Packet 	  			(D) : Send data to/from server from/to client
		3. Acknowledgment Packet		(A) : Send acknowledgement in response to data / command packet.
		4. File Command Packet			(F) : Send file size from/to server to/from client.
		5. File Size ACK Packet     		(K) : Send ACK in response to file size packet.

-------------------------------------------------------------------------------------------------------------		

4. UDP SERVER DESCRIPTION - 

	-	The server responds when it receives a packet from the client. It continuously wait in a loop 
		for any packet after which it open the packet and determines the packet type (open_packet_server()).
		
	-	To initiate any file operation, the client sends a command packet (C) and the server sends 
		acknowledgment packet (A) as response back to the client.
		
	-	When a data packet is received from client, server sends ACK (A) packet with the same 
		sequence number as that of the received data packet.
		
	-	When a data packet is sent by server to client, server waits for ACK (A) of the same sequence 
		number as the data packet.
		
	-	When exit command is received from client, server closes the socket, exits from the loop and 
		the program is terminated.

-------------------------------------------------------------------------------------------------------------

5. UDP CLIENT DESCRIPTION - 

	-	The client takes input command and file name from user to determine the file operation to be 
		performed.
		
	-	To initiate any file operation, the client sends a command packet (C) and the server sends 
		acknowledgment packet (A) as response back to the client.
		
	-	When a data packet is received from server, client sends ACK (A) packet with the same 
		sequence number as that of the received data packet.
		
	-	When a data packet is sent by client to server, client waits for ACK (A) of the same seqeunce 
		number as the data packet.
		
	-	When exit command is sent to server, client closes the socket, exits from the loop and 
		the program is terminated.

-------------------------------------------------------------------------------------------------------------

6. RELIABLE TRANSFER - 

	-	To ensure reliable transfer, timeout period for receive operation at client is set to 2 seconds.
	
	-	Data packets and acknowledgment packets are sent/received with a sequence number. Data packet from
		client/server is only sent when acknowledgment packet with previous sequence number is received. Thus,
		the reliability method used is of BLOCKING type.
		
-------------------------------------------------------------------------------------------------------------
