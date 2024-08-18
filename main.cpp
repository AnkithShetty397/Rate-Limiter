#include<iostream>
#include<vector>
#include<queue>
#include<unordered_map>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<chrono>
#include<cmath>
#include<boost/asio.hpp>

#define MAX_REQS 10

using namespace std;
using namespace boost::asio;

class RateLimiterEMA{
private:
	unordered_map<string,double> ema_map;
	unordered_map<string,std::chrono::steady_clock::time_point> last_update_map;
	double alpha;		//smoothing factor	(0-1)
	double threshold;	//rate limiter threashold (req/sec)
	mutex mtx;

public:
	RateLimiterEMA(double alpha,double threshold){
		this->alpha=alpha;
		this->threshold=threshold;
	}
	
	RateLimiterEMA(){
		alpha=0.5;
		threshold=10;
	}
	
	bool process_req(string& ip_addr){
		auto now = std::chrono::steady_clock::now();

		lock_guard<mutex> lock(mtx);

		auto& last_update = last_update_map[ip_addr];

		std::chrono::duration<double> time_diff = now - last_update;
		last_update = now;

		double request_rate = 1.0 / time_diff.count();  // Calculate the instantaneous request rate

		// Cap the maximum rate to avoid spikes due to very small time_diff
		request_rate = std::min(request_rate, threshold * 2);

		if(ema_map.find(ip_addr) == ema_map.end()){
			ema_map[ip_addr] = request_rate;  // Initialize EMA with the calculated rate
			return true;
		}

		double& current_ema = ema_map[ip_addr];
		current_ema = alpha * request_rate + (1 - alpha) * current_ema;

		std::cout << "IP: " << ip_addr 
				<< " | Time Diff: " << time_diff.count() 
				<< " | Request Rate: " << request_rate 
				<< " | Current EMA: " << current_ema 
				<< " | Threshold: " << threshold 
				<< std::endl;

		if(current_ema > threshold){
			std::cout << "Blocked " << ip_addr << std::endl;
			return false;
		} else {
			std::cout << "Allowed " << ip_addr << std::endl;
			return true;
		}
	}
};

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

void tcp_connection_handler(ip::tcp::socket client_socket,RateLimiterEMA* rate_limiter){
    try{
        string client_ip = client_socket.remote_endpoint().address().to_string();
        if(!rate_limiter->process_req(client_ip)){
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
	auto rate_limiter=make_unique<RateLimiterEMA>(0.5,5);

	io_service io_service;
	ip::tcp::acceptor acceptor(io_service, ip::tcp::endpoint(ip::tcp::v4(), 8080));
	ThreadPool thread_pool(16);
	while(true){
		ip::tcp::socket socket(io_service);
		acceptor.accept(socket);
		thread_pool.enqueue([sock = std::move(socket),rate_limiter=rate_limiter.get()]() mutable { 
			tcp_connection_handler(std::move(sock),rate_limiter); 
		});
	}
	
	return 0;
}

