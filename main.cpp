#include<iostream>
#include<unordered_map>
#include<mutex>
#include<thread>
#include<chrono>

using namespace std;

#define MAX_REQS 10

unordered_map<string,pair<int,mutex>> req_map;		// [(ip_address,(count,mutex))]

bool process_req(string& ip_addr){
	auto& [count,mutex1] = req_map[ip_addr];
	lock_guard<mutex> guard(mutex1);	// Resource Aquisition  Is Initialized (RAII) here
	/*
		the thread will wait until it gets access to the resource	-aquiring lock
		when the guard object goes out of scope, the guard destructor automatically unlocks the mutex 
	*/
	count++;
	if(count>MAX_REQ)
		return false;	
	return true;	
}

void decrement_count(){
	while(true){
		this_thread::sleep_for(chrono::milliseconds(100));
		for(auto mp =req_map.begin();mp!=req_map.end();){
			auto& [ip_addr, data] = *mp;
			auto& [count, mutex1] = data;
			
			lock_guard<mutex> guard(mutex1);
			
			if(0<count)
				count--;					
			if(count==0)
				mp = req_map.erase(mp);		//this will return the next element in the map
			else
				mp++;					
		}
	}
}

void middleware(){
	/*
    This function should:
    1. Listen to a specific port for incoming requests.
    2. Extract the IP address from the incoming request.
    3. Use the `process_req` function to decide whether to process the request.
    4. If accepted, forward the request to the appropriate service and return the response.
    */

    while (true) {
        // Listen to the port here (simplified pseudo-code below)
        string ip_addr;  // Assume we extract the IP address from the incoming request
        // Example: ip_addr = get_ip_from_request();
        
        if (process_req(ip_addr)) {
            // Forward the request to the target service and get the response
            // Example: string response = forward_request_to_service();
            
            // Send the response back to the original requester
            // Example: send_response_to_client(response);
        } else {
            // Respond with an error or rate limit message
            // Example: send_response_to_client("429 Too Many Requests");
        }

        // Repeat this process in a loop
    }
}

int main(){
	thread decrement_thread(decrement_count);
	thread middleware_thread(middleware); 
	
	middleware_thread.join();
	decrement_thread.join();
	
	return 0;
}
	







