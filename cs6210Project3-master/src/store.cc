#include "threadpool.h"

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <chrono>
#include <future>
#include <algorithm>

#include <grpcpp/grpcpp.h>
#include "store.grpc.pb.h"
#include "vendor.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerAsyncResponseWriter;
using grpc::ClientAsyncResponseReader;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::ServerCompletionQueue;
using grpc::CompletionQueue;
using grpc::Status;
using grpc::Channel;

using store::Store;
using store::ProductQuery;
using store::ProductReply;
using store::ProductInfo;

using vendor::BidQuery;
using vendor::BidReply;
using vendor::Vendor;

class StoreClient;
vendor::BidReply run_client(const std::string& server_addr, const std::string& product_name);
std::vector<std::string> vendors;
std::vector<std::future<int>> results;

class StoreServiceImpl final {
	public:
		~StoreServiceImpl() {
			server_->Shutdown();
			cq_->Shutdown();
		}

	// There is no shutdown handling in this code.
	void RunServer(std::string portNum, int num_threads) {
		// Default is "0.0.0.0:50053"
		std::string server_address("0.0.0.0:" + portNum);
	
		ServerBuilder builder;

		// Listen on the given address without any authentication mechanism.
		builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
		// Register "service" as the instance through which we'll communicate with
		// clients. In this case it corresponds to a *synchronous* service.
		// builder.RegisterService(&service_);
		builder.RegisterService(&service_);
		// Get hold of the completion queue used for the asynchronous communication
		// with the gRPC runtime.
		cq_ = builder.AddCompletionQueue();
		// Finally assemble the server.
		server_ = builder.BuildAndStart();
		std::cout << "Server listening on " << server_address << std::endl;
		// Create the pool of threads
		pool = new threadpool(num_threads);

		HandleRpcs();
	}

	private:
		// Class encompassing the state and logic needed to serve a request.
		class CallData {
			public:
				// Take in the "service" instance (in this case representing an asynch server) 
				// and the completion "cq" used for asynch comm with the gRPC runtime.
				// called an initialization list
				CallData(Store::AsyncService* service, ServerCompletionQueue* cq) 
					: service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
					// Invoke the serving logic right away
					Proceed();
				}

			void Proceed() {
				if (status_ == CREATE) {
					std::cout << "Creating!!!" << std::endl;
					// Make this instance progress to the PROCESS state.
					status_ = PROCESS;

					// As part of the initial CREATE state, we *request* that the system
					// start processing ProductQuery requests. In this request, "this" acts are
					// the tag uniquely identifying the request (so that different CallData
					// instances can serve different requests concurrently), in this case
					// the memory address of this CallData instance.
					service_->RequestgetProducts(&ctx_, &request_, &responder_, cq_, cq_,
												this);
				} else if (status_ == PROCESS) {
					// Spawn a new CallData instance to serve new clients while we process
					// the one for this CallData. The instance will deallocate itself as
					// part of its FINISH state.
					// Upon receiving a client request, store will assign a thread the
					// thread pool to the incoming request for processing
					new CallData(service_, cq_);

					// The actual processing
					std::string product = request_.product_name();
					// std::vector<Bid> vBid;
					// std::cout << "product: " << product << std::endl;
					// Client to Vendor
					// Build a Queue of products?
					// Here is where the work should be passed to a thread worker
					std::thread::id t_id = std::this_thread::get_id();
					// std::cout << "T_id: " << t_id << std::endl;
					for (int i = 0; i < vendors.size(); ++i)
					{
						BidReply bid;
						bid = run_client(vendors[i], product);
						// std::cout << "Bid Price: " << bid.price() << std::endl;
						ProductInfo* product_info = reply_.add_products();
						product_info->set_price(bid.price());
						product_info->set_vendor_id(bid.vendor_id());
					}
					
					
					// And we are done! Let the gRPC runtime know we've finished, using the
					// memory address of this instance as the uniquely identifying tag for
					// the event.
					status_ = FINISH;
					responder_.Finish(reply_, Status::OK, this);
				} else {
					GPR_ASSERT(status_ == FINISH);
					// Once in the FINISH state, deallocate ourselves (CallData).
					delete this;
				}
			}

		private:
			// The means of communication with the gRPC runtime for an async server.
			Store::AsyncService* service_;
			// The producer-consumer queue where for asynchronous server notifications.
			ServerCompletionQueue* cq_;
			// Context for the rpc, allowing to tweak aspects of it such as the use of
			// compression, authentication, as well as to send metadata back to the client.
			ServerContext ctx_;
			// What we get from the client.
			ProductQuery request_;
			// What we send back to the client.
			ProductReply reply_;
			// The means to get back to the client.
			ServerAsyncResponseWriter<ProductReply> responder_;
			// The threadpool used
			threadpool* pool_;
			// Let's implement a tiny state machine with the following states.
			enum CallStatus
			{
				CREATE, PROCESS, FINISH
			};

			CallStatus status_; // The current serving state.
		};

		// This can be run in multiple threads if needed. 
		void HandleRpcs() {
			// Spawn a new CallData instance to serve new clients.
			new CallData(&service_, cq_.get());
			void* tag; // uniquely identifies a request.
			bool ok;

			while (true) {
				GPR_ASSERT(cq_->Next(&tag, &ok));
				GPR_ASSERT(ok);
				pool->enqueue([tag](){ 
				
				/*
				/ Block waiting to read the next event from the completion queue. The 
				/ event is uniquely identified by its tag, which in this case is the 
				/ memory address of a CallData instance. 
				/ The return value of Next should always be checked. this return value
				/ tells us whether there is any kind of event or cq_ is shutting down.
				*/
				
				static_cast<CallData*>(tag)->Proceed();
				// the above call to proceed can be given to a thread in the threadpool
				});
			}
		}

		std::unique_ptr<ServerCompletionQueue> cq_;
		Store::AsyncService service_;
		std::unique_ptr<Server> server_;
		threadpool* pool;

};

class VendorClient
{
	public:
		VendorClient(std::shared_ptr<Channel> channel);
		vendor::BidReply AsyncAskBid(const std::string&);

	private:
		std::unique_ptr<Vendor::Stub> stub_;
};

vendor::BidReply run_client(const std::string& server_addr, const std::string& product_name) {
	VendorClient vendor_client(grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()));
	// std::cout << "Asking the following vendor: " << server_addr << std::endl;
	return vendor_client.AsyncAskBid(product_name);
}

VendorClient::VendorClient(std::shared_ptr<Channel> channel)
	: stub_(Vendor::NewStub(channel))
	{}


// Assembles the client's payload, sends it and presents the response back from the server
vendor::BidReply VendorClient::AsyncAskBid(const std::string& product_name) {
	// Data sending to vendor
	BidQuery request;
	// std::cout << "Product: " << product_name << std::endl;
	request.set_product_name(product_name);

	// Container for the data we expect from the vendor
	BidReply reply;

	// Context for the client. It could be used to convey extra information to
	// the server and/or tweak certain RPC behaviors.
	ClientContext context;

	// The producer-consumer queue we use to communicate asynchronously with 
	// the gRPC runtime. 
	CompletionQueue cq;

	// Storage for the status of the RPC upon completion.
	Status status;

	// stub_->PrepareAsyncSayHello() creates an RPC object, returning
	// an instance to store in "call" but does not actually start the RPC
	// Because we are using the asynchronous API, we need to hold on to
	// the "call" instance in order to get updates on the ongoing RPC
	std::unique_ptr<ClientAsyncResponseReader<BidReply> > rpc(
		stub_->PrepareAsyncgetProductBid(&context, request, &cq));

	// StartCall initiates the RPC Call
	rpc->StartCall();

	// Request that, upon completion of the RPC, "reply" be updated with the
	// server's response; "status" with the indication of whether the operation
	// was successful. Tag the request with the integer 1. 
	rpc->Finish(&reply, &status, (void*)1); 
	void* got_tag;
	bool ok = false;
	
	// Block until the next result is available in the completion queue "cq"
	// The return value of Next should always be checked. This return value
	// tells us whether there is any kind of event of the cq_ is shutting down.
	GPR_ASSERT(cq.Next(&got_tag, &ok));

	//Verify that the result from "cq" corresponds, by its tag, our previous
	// request.
	GPR_ASSERT(got_tag == (void*)1);

	//...and that the request jwas completed successfully. Note that "ok"
	// corresponds solely to the request for updates introduced by Finish();
	GPR_ASSERT(ok);

	if(status.ok()) {
		// std::cout << "Reply received - vendor ID: " << reply.vendor_id() << std::endl;
		// std::cout << "Reply received - price: " << reply.price() << std::endl;
		return reply;
	} else {
		std::cout << "RPC Failed" << std::endl;
		return reply;
	}
}

// Get the vendors
std::vector<std::string> getVendors(std::string filename) {
	std::vector<std::string> ip_addrresses;
  	int addr_index = -1;
	std::ifstream myfile (filename);
  	if (myfile.is_open()) {
    	std::string ip_addr;
    	while (getline(myfile, ip_addr)) {
      		if (addr_index == -1) {
        		ip_addrresses.push_back(ip_addr);
      		} else if (addr_index == 0) {
        		ip_addrresses.push_back(ip_addr);
        		break;
      		} else {
        		--addr_index;
      		}

    	}
    	myfile.close();
    	return ip_addrresses;
  	} else {
    	std::cerr << "Failed to open file " << filename << std::endl;
    	return {};
  	}
}

int main(int argc, char** argv) {
	// Parse arguments then pass it to the store
	int num_threads;
	std::string vendorFile, portNum;
	if (argc == 4) {
		num_threads = std::min( 20, std::max(0,atoi(argv[1])));
		portNum = argv[2];
		vendorFile = std::string(argv[3]);
	} else if (argc == 3) {
		num_threads = std::min( 20, std::max(0,atoi(argv[1])));
		portNum = argv[2];
		vendorFile = "vendor_addresses.txt";
	} else if (argc == 2) {
		num_threads = std::min( 20, std::max(0,atoi(argv[1])));
		portNum = "50057";
		vendorFile = "vendor_addresses.txt";
	} else {
		num_threads = 4;
		portNum = "50057";
		vendorFile = "vendor_addresses.txt";
	}
	// Get the vendors
	vendors = getVendors(vendorFile);
	/*
	for (int i = 0; i < vendors.size(); ++i)
	{
		std::cout << "Vendor ID: " << vendors[i] << std::endl;
	}
	*/
	// Instantiate the client portion of the store.
	// Test the pool
	/*
	for (int i = 0; i < 8; ++i)
	{
		results.emplace_back(pool->enqueue([i] {
			// std::cout << "hello" << i << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(2));
			// std::cout << "world" << i << std::endl;
			return i*i;
		}));
	};

	for (auto && result: results)
		std::cout << result.get() << ' '; 
	std::cout << std::endl;
	*/
	StoreServiceImpl server;
	server.RunServer(portNum, num_threads);

	return 0;
}

