# An Iterative and a Cuncurrent file transfer TCP Server and Client implementations

Mid-course project for the course "Programmazione Distribuita I" @Politecnico di Torino. 

<br>

It can be compiled with the following commands:
```
gcc -std=gnu99 -o server server1/*.c *.c -Iserver1 -lpthread -lm
gcc -std=gnu99 -o server server2/*.c *.c -Iserver2 -lpthread -lm
gcc -std=gnu99 -o client client1/*.c *.c -Iclient1 -lpthread -lm
```

<br>

### Server1
A TCP sequential server (listening to the port specified as the first parameter of the command line, as a decimal integer) that, after having established a TCP connection with a client, accepts file transfer requests from the client and sends the requested files back to the client, following the protocol specified below. The files available for being sent by the server are the ones accessible in the server file system from the working directory of the server.

### Client1
A client that can connect to a TCP server (to the address and port number specified as first and second command-line parameters, respectively). After having established the connection, the client requests the transfer of the files whose names are specified on the command line as third and subsequent parameters, and stores them locally in its working directory. After having transferred and saved locally a file, the client prints a message to the standard output about the performed file transfer, including the file name, followed by the file size (in bytes, as a decimal number) and timestamp of last modification (as a decimal number).
Any timeouts used by client and server to avoid infinite waiting were set to 15 seconds.

### Protocol
The protocol for file transfer works as follows: to request a file the client sends to the server the three ASCII characters “GET” followed by the ASCII space character and the ASCII characters of the file name, terminated by the ASCII carriage return (CR) and line feed (LF).
```
GET ...filename...CRLF
```
(Note: the command includes a total of 6 ASCII characters, i.e. 6 bytes, plus the characters of the file name). The server responds by sending:
```
+OKCRLFB1B2B3B4FileContents...T1T2T3T4
```
Note that this message is composed of 5 characters followed by the number of bytes of the requested file (a 32-bit unsigned integer in network byte order - bytes B1 B2 B3 B4 in the figure), followed by the bytes of the requested file contents, and then by the timestamp of the last file modification (Unix time, i.e. number of seconds since the start of epoch, represented as a 32-bit unsigned integer in network byte order - bytes T1 T2 T3 T4 in the figure).
<br><br>
The client can request more files using the same TCP connection, by sending several GET commands, one after the other. When it has finished sending commands on the connection, it starts the procedure for closing the connection. Under normal conditions, the connection should be closed gracefully, i.e. the last requested file should be transferred completely before the closing procedure terminates.
In case of error (e.g. illegal command, non-existing file) the server always replies with:
```
-ERRCRLF
```
(6 characters) and then it starts the procedure for gracefully closing the connection with the client.

### Server2
A concurrent TCP server (listening to the port specified as the first parameter of the command line, as a decimal integer) that, after having established a TCP connection with a client, accepts file transfer requests from the client and sends the requested files back to the client, following the same protocol. The server creates processes on demand (a new process for each new TCP connection).
<br><br>
By using the same client (client1), it is possible to run more clients concurrently and verify that each one can establish a connection and transfer files. 
<br>
By forcefully terminating a client (by pressing CTRL+C in its window), it is possible to verify that the (parent) server is still alive and able to reply to other clients.
<br>
