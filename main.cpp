#include<iostream>
#include<vector>
#include<queue>
#include<unordered_map>
#include<functional>
#include<mutex>
#include <condition_variable>
#include<thread>
#include<chrono>
#include<boost/asio.hpp>

#define MAX_REQS 10

using namespace std;
using namespace boost::asio;

class ThreadPool{
private:
	vector<thread> threads;
	queue<function<void()>> tasks;
	mutex queue_mutex;
	condition_variable cv;
	bool stop=false;

public:
	ThreadPool(size_t num_threads=thread::hardware_concurrency()){
		for(size_t i=0;i<num_threads;i++){
			threads.emplace_back([this]{
				while(true){
				function<void()> task;
				{
					unique_lock<mutex> lock(queue_mutex);
					cv.wait(lock,[this]{ return !tasks.empty()||stop;});
					if(stop && tasks.empty()) return;
					task = std::move(tasks.front());
					tasks.pop();
				}
				task();
			}
			});
		}
	}
	~ThreadPool(){
		{
			unique_lock<mutex> lock(queue_mutex);
			stop=true;
		}
		cv.notify_all();
		for(auto& thread:threads){
			thread.join();
		}
	}
	void enqueue(function<void()> tcp_connection){
		{
			unique_lock<mutex> lock(queue_mutex);
			tasks.emplace(std::move(tcp_connection));
		}
		cv.notify_one();
	}
};

unordered_map<string,pair<int,mutex>> req_map;		// [(ip_address,(count,mutex))]

bool process_req(string& ip_addr){
	auto& [count,mutex1] = req_map[ip_addr];
	lock_guard<mutex> guard(mutex1);	// Resource Aquisition  Is Initialized (RAII) here
	/*
		the thread will wait until it gets access to the resource	-aquiring lock
		when the guard object goes out of scope, the guard destructor automatically unlocks the mutex 
	*/
	count++;
	if(count>MAX_REQS)
		return false;
	return true;
}

void decrement_count(){
	while(true){
		this_thread::sleep_for(std::chrono::milliseconds(100));
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

void tcp_connection_handler(ip::tcp::socket socket){
	try{
		string client_ip = socket.remote_endpoint().address().to_string();
		if(!process_req(client_ip)){
			//Send back 429 Too Many Requests status code response
			return;
		}

		// Handle the request

		return;
	}catch(std::exception& e){
		cerr<<"Error: "<<e.what()<<endl;
	}
}

int main(){
	io_service io_service;
	ip::tcp::acceptor acceptor(io_service, ip::tcp::endpoint(ip::tcp::v4(), 8080));
	thread decrement_thread(decrement_count);

	ThreadPool thread_pool(16);

	while(true){
		ip::tcp::socket socket(io_service);
		acceptor.accept(socket);
		
		thread_pool.enqueue([sock=std::move(socket)]() mutable{ tcp_connection_handler(std::move(sock));});
	}

	decrement_thread.join();
	return 0;
}








