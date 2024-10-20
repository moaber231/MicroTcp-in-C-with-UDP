MicroTCP - A Custom TCP Protocol
Project Overview
MicroTCP is a lightweight implementation of a custom TCP-like protocol built on top of UDP, designed for teaching and academic purposes. It provides basic TCP functionalities such as connection establishment (three-way handshake), reliable data transfer, flow control, and connection termination, all implemented using UDP functions.

The protocol can be used in client-server models to transfer data, including large files, across the network. The project demonstrates core networking concepts and is useful for understanding how reliable transport protocols work.

Key Features:
Connection Establishment: Implements a 3-way handshake similar to TCP.
Reliable Data Transfer: Ensures data is correctly received using checksums and acknowledgments.
Flow Control: Implements sliding window control to manage data transfer rates.
Connection Termination: Proper closing of the connection, ensuring all data is transmitted before shutdown.
System Requirements
Operating System: Linux or macOS (with networking libraries like arpa/inet.h).
Compiler: GCC or compatible C compiler.
Libraries: Standard C libraries (sys/socket.h, netinet/in.h, errno.h) and a CRC32 checksum library.
Project Structure
```bash

MicroTCP/
├── src/
│   ├── microtcp.c              # Main source file with MicroTCP protocol implementation
│   ├── microtcp.h              # Header file defining structures and function prototypes
│   ├── server.c                # Server-side implementation using MicroTCP
│   ├── client.c                # Client-side implementation using MicroTCP
│   ├── utils/crc32.c           # Utility functions for calculating CRC32 checksum
│   └── utils/crc32.h           # Header file for CRC32 checksum utility
├── bandwidth_test.c            # Test program for measuring data transfer bandwidth
├── Makefile                    # Makefile for building the project
└── README.md                   # This file
```
How to Build the Project
Prerequisites:
GCC Compiler: Make sure GCC is installed. On Linux, you can install it with:
```bash

sudo apt-get install build-essential
```
Make: Ensure make is installed to build the project.
Building the Project:
Clone the repository or download the source files.
Navigate to the project directory and run:
```bash
make
```
This will compile the microtcp, server, and client programs.
How to Run the Project
1. Running the MicroTCP Server
You can start the server using MicroTCP by specifying the port and the file that will be saved with the transferred data:

```bash

./server -s -m -p 8080 -f output.txt
```
-s: Indicates that the program runs in server mode.
-m: Uses the MicroTCP implementation instead of standard TCP.
-p: The port number for the server to listen on.
-f: The file to store the received data.
2. Running the MicroTCP Client
On the client side, you can specify the server IP, port, and the file to be sent:

```bash

./client -a 192.168.1.10 -m -p 8080 -f input.txt
```
-a: IP address of the server.
-m: Uses MicroTCP.
-p: The port number to connect to.
-f: The file to send from the client to the server.
Input Examples
Here is an example of how you can run a bandwidth test by transferring a file (e.g., input.txt) of size several gigabytes using MicroTCP.

Starting the Server:
```bash

./bandwidth_test -s -m -p 8080 -f large_output.txt
```
Running the Client:
```bash
./bandwidth_test -a 192.168.1.10 -m -p 8080 -f large_input.txt
```
This will send a file named large_input.txt from the client to the server and save it as large_output.txt on the server side. During the transfer, the program will print out statistics such as the total data transferred, the time taken, and the throughput.

Testing Bandwidth
The bandwidth_test program included in the project can be used to measure how efficiently data is transferred using MicroTCP. Here's an example of how you can perform the test:

Server-Side:

```bash
./bandwidth_test -s -m -p 9000 -f received_file.txt
```
Client-Side:

```bash
./bandwidth_test -a 192.168.1.20 -m -p 9000 -f large_test_file.txt
```
The test will display results such as:

Data received: The total size of the file transferred.
Transfer time: How long the transfer took.
Throughput achieved: The data transfer speed in MB/s.
Future Improvements
Congestion Control: Extend the protocol to include congestion control mechanisms similar to TCP’s slow start and congestion avoidance.
Packet Reordering: Implement mechanisms to handle out-of-order packet delivery.
Enhanced Error Detection: Improve checksum algorithms or add more advanced error-detection mechanisms for better reliability.
License
This project is licensed under the GNU General Public License (GPLv3). See the LICENSE file for more details.

Acknowledgments
This project was developed for teaching and academic purposes by Manolis Surligas. You can find the original implementation details in the source files.

This README.md provides all the necessary information for running and testing the MicroTCP protocol, including how to compile the project, run the server and client, and perform bandwidth tests with large files.
