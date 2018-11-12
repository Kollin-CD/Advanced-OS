# CS6210: Distributed Service using gRPC - Project 3

## Overview
This is a store which receives requests from different users, querying the prices off by the different registered vendors.

## To Run:

### Terminal 1:
- run `make` command
- ./store $num_threads $port_number $vendor_file
	- Note:
	- Number of threads is maxed at 20 and default is 4.
	- Port number must not be also in the vendor IP addresses or else it will fail
	- Port number is defaulted to '50057' 
	- Vendor file is defaulted to vendor_addresses.txt

### Terminal 2:
- ./test/run_vendors ../src/vendor_addresses.txt

### Terminal 3:
- ./test/run_tests $port_number $nthreads
	-Note:
	- Port number here should be like 'localhost:50057'

### Description

This code structure heavily takes from gRPC's asynchronous server and client model (Found at: https://github.com/grpc/grpc/tree/master/examples/cpp/helloworld). 

Originally, built the single threaded version and then added a pool of threads that end up handling the actual RPC. Once a task is put on the queue for the pool, a thread/worker will assign itself to make the async RPC call to the vendor, wait and respond to the client with the results. I was able to see that the server could handle multiple clients concurrently this way. 