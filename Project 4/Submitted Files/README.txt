Name: Jahan Kuruvilla Cherian
UID: 104436427
Email: jcherian@ucla.edu
TA: Tuan Le
Prof: Mark Kampe
Project: Embedded Systems Project

Lab4-B-1:

We see the client communicating with the server by sending messages and receiving
messages about the patient's heart rate. The communication seems to be set up with
no particular form of security encryption and just uses a basic socket connection
to the specified IP address. Thus there seem to be no official security measures 
being used in the UDP data transfer. The privacy also seems to be lacking in this
stage of the lab, as all the data can be seen.

Lab4-B-2:

The packet sniffer intercepted the messages in the UDP communication. We see that
it receives the byte information of the packet being sent for a certain window.
The lack of security and privacy is shown by this part of the lab, as we can clearly
see that the byte information we intercept is not encrypted at all, and that we can
clearly see the information being decoded as the original message. This shows tat
no encryption was used during the transmission and that there is no privacy in communication.
We can see the data in its entirety with the packet header information in the two
way communication between the server and client.

Lab4-B-3:

The output is slightly different from that seen in LAb4-B-1, because we used the
open communication to change the rate of message transfer by using set rate.
The output shows that the server received the new rate and then relayed that informaiton
back to the client, which shows the change of rate (in seconds) from 3 to 1 in the case submitted.
From this point onwards because the rate is shorter we get the output of the received
and sent message as being much quicker. Note that the rate determined how quickly
the client sent messages to the server.

Lab4-B-4:

This output is somewhat analogous to Lab4-B-3 in that upon starting the attack, we
notice the rate change, but this time the rate (in seconds) increased to a fixed rate of 
15 (from the previously set rate of 1). This meant that the output of the 
received and sent message was a lot slower. Note that the rate determined how quickly
the client sent messages to the server.

Lab4-D-1:

The output of this section of the lab was different to previous outputs because now
we see security measures being implemented amongst the TLS communication. The
SSL handshake seemed to be much more prominent in this setup as we see the encryption
type being printed out and we see the server's certificate as well. The output
of the client sending and receiving messages to and fro the server do not change
in this current output, but we can be rest assured that the communication is a lot
more secure and the privacy actually being implemented. Note that this output
essentially shows the authentication between the client and the server in the beginning
to emphasize that the communication is only trusted between the client and the server.

Lab4-D-2:

The output of this section of the lab was very different from the corresponding
output in part B. This is because the TLS communication actually used encryption
to make the communication private between the server and client. Thus the packet
sniffer (man in the middle essentially) only received malformed packets with the
date being encrypted to make it unreadable. This packet sniffer was also able to 
recieve the ack sent by the server to the client. The output of this part of the
lab truly shows us the security and privacy ensured by the TLS communication, along
with the reliability over the UDP communication as we see the presence of acknowledgements.

Lab4-D-3:

The output shows the data being sent and received by the client to the server over
the TLS comunication at a rate of 3, but upon setting the new rate down to 1, the
server receives the request (just as it did in the case of UDP) but now the data
being sent and received is no longer coherent. The heart rate of the patient is
completely mismatched after setting the new rate. We also notice that the output
of the data is incorrect and we see the client trying to send another packet before
the second instance of he rate change is received. Because of this incorrect sending
and receiving, all subsequent data communication is incorrect and not synced.
It should be noted the reason we are receiving duplicate messages is because of the 
buffering of the information on the client side. We notice that because we use SSL
read and write which has its own buffer. Thus we see that due to the set rate we are
receiving two sets of new rate information in the SSL buffer. Thus we print out
two sets of new rates.

Lab4-D-4:

Upon trying the attack we notice no change in output from Lab4-D-3, because of the
secure communication between the client and server. This reinforces the idea that
the TLS communication is set up only between the client and the server, and thus 
trying a man in the middle attack by spoofing the client. Due to there being an
authentication required, due to the encryption set up, the information that the server
receives from the start attack is not authenticated and thus is just simply ignored and 
thus the message is never sent to the client through SSL and thus the information is
not updated.

Lab4-E-1:

By spawning a new thread we notice that we continue to receive the message of the
new rate twice, because the SSL buffer still receives the information twice over, and
thus the client reads it twice over, but this time the information is no longer 
mismatched

Lab4-E-1:

By spawning a new thread we notice that we continue to receive the message of the
new rate twice, because the SSL buffer still receives the information twice over, and
thus the client reads it twice over, but this time the information is no longer 
mismatched. This works because by placing the reading into a new thread, we can
receive the messages asynchronously, and thus we no longer rely on a sequential mode
of reading which allow for the messages to be read out of order if the SSL buffer
had information that was out of order. This way we get synced up informatino.

------README------

*.png:
	All the png files are the respective screenshots of each of the cases described
	by the webpage specifications, and their results are described above.

README:
	File describing the other files in the submission, along with containing the
	respective answers to the questions asked in the specification.

tls_client.c:
	Modified C source file to set up the tls client communication with the server
	based on the sample code provided. Note that because there is no makefile submitted
	with this project, that the Makefile used to compile this C module should use
	the -pthread flag to allow for multi-threading with POSIX threads.

Lab4-E-2.txt:
	The log file containing all the received information from the TLS server during
	my final testing of the code in PART E of the lab.
