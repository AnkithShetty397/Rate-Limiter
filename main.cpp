#include<iostream>
#include<vector>
#include<queue>
#include<unordered_map>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<chrono>
#include<boost/asio.hpp>

#define MAX_REQS 10

using namespace std;
using namespace boost::asio;

class ThreadPool{
private:
	vector<thread> threads;
	queue<std::packaged_task<void()>> tasks;
	mutex queue_mutex;
	condition_variable cv;
	bool stop=false;

public:
	ThreadPool(size_t num_threads=thread::hardware_concurrency()){
		for(size_t i=0;i<num_threads;i++){
			cout<<"Thread "<<i+1<<" created"<<endl;
			threads.emplace_back([this]{
				while(true){
				std::packaged_task<void()> task;
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
    template<typename F>
    void enqueue(F&& tcp_connection) {
        {
            unique_lock<mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(tcp_connection));
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

void tcp_connection_handler(ip::tcp::socket client_socket){
    try{
        string client_ip = client_socket.remote_endpoint().address().to_string();
        if(!process_req(client_ip)){
			cout<<"Blocked "<<client_ip<<endl;
            client_socket.send(buffer("HTTP/1.1 429 Too Many Requests\r\n\r\n"));
            return;
        }

    	auto start = std::chrono::steady_clock::now();
		
        // Buffer to hold the incoming request
        boost::asio::streambuf request_buffer;
        boost::asio::read_until(client_socket, request_buffer, "\r\n\r\n"); // Read the HTTP request headers

        // Convert request buffer to string
        std::istream request_stream(&request_buffer);
        std::string request((std::istreambuf_iterator<char>(request_stream)), std::istreambuf_iterator<char>());

        // Connect to the internal server
        ip::tcp::socket internal_socket(client_socket.get_executor());
        ip::tcp::resolver resolver(client_socket.get_executor());
        ip::tcp::resolver::query query("127.0.0.1", "3000"); 
        ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

        boost::asio::connect(internal_socket, endpoint_iterator);

        // Forward the request to the internal server
        boost::asio::write(internal_socket, boost::asio::buffer(request));

        // Buffer to hold the internal server's response
        boost::asio::streambuf response_buffer;
        boost::asio::read_until(internal_socket, response_buffer, "\r\n\r\n"); // Read the response headers

        // Relay the response back to the client
        boost::asio::write(client_socket, response_buffer);

		auto end = std::chrono::steady_clock::now();
		std::chrono::duration<double> total_duration = end - start;
		std::cout << "Total time for request handling: " << total_duration.count()<< " seconds" << std::endl;
        return;
    }  catch (const boost::system::system_error& e) {
        cerr << "Boost Error: " << e.what() << endl;
        client_socket.send(buffer("HTTP/1.1 500 Internal Server Error\r\n\r\n"));
    } catch (const std::exception& e) {
        cerr << "Standard Error: " << e.what() << endl;
        client_socket.send(buffer("HTTP/1.1 500 Internal Server Error\r\n\r\n"));
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
		thread_pool.enqueue([sock = std::move(socket)]() mutable { 
			tcp_connection_handler(std::move(sock)); 
		});
	}
	
	decrement_thread.join();
	return 0;
}

