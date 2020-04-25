#include <thread>
#include <iostream>
#include <chrono>
#include <functional>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <queue>
#include <mutex>
#include <string>
#include <condition_variable>

class WorkQueue {
	typedef std::function<void(void)> function_ptr_t;

public:
	WorkQueue(size_t thread_cnt = 1);
	~WorkQueue();

	void addWork(const function_ptr_t& op);
	void addWork(function_ptr_t&& op);

	WorkQueue(const WorkQueue& rhs) = delete;
	WorkQueue& operator=(const WorkQueue& rhs) = delete;
	WorkQueue(WorkQueue&& rhs) = delete;
	WorkQueue& operator=(WorkQueue&& rhs) = delete;

private:
	std::mutex lock_;
	std::vector<std::thread> threads_;
	std::queue<function_ptr_t> q_;
	std::condition_variable cv_;
	bool quit_ = false;

	void workThreadHandler(void);
};

WorkQueue::WorkQueue(size_t thread_cnt) :
	threads_(thread_cnt)
{
	for(size_t i = 0; i < threads_.size(); i++)
	{
		threads_[i] = std::thread(&WorkQueue::workThreadHandler, this);
	}
}

WorkQueue::~WorkQueue()
{
	std::unique_lock<std::mutex> lock(lock_);
	quit_ = true;
	lock.unlock();
	cv_.notify_all();

	for(size_t i = 0; i < threads_.size(); i++)
	{
		if(threads_[i].joinable())
		{
			threads_[i].join();
		}
	}
}

void WorkQueue::addWork(const function_ptr_t& op)
{
	std::unique_lock<std::mutex> lock(lock_);
	q_.push(op);
	lock.unlock();
	cv_.notify_all();
}

void WorkQueue::addWork(function_ptr_t&& op)
{
	std::unique_lock<std::mutex> lock(lock_);
	q_.push(std::move(op));
	lock.unlock();
	cv_.notify_all();
}

void WorkQueue::workThreadHandler()
{
	std::unique_lock<std::mutex> lock(lock_);
	do {
		cv_.wait(lock, [this]{
			return (q_.size() || quit_);
		});
		if(!quit_ && q_.size())
		{
			auto op = std::move(q_.front());
			q_.pop();
			lock.unlock();
			op();
			lock.lock();
		}
	} while (!quit_);
}


//handles all file access
//owns encoders and decoders
//owns 
class AsyncCore{
public:
    AsyncCore(){
        file_ = fopen("hej.txt","w");
    }
	~AsyncCore(){
    }

    void execute(){
        //lock_.lock();
        if (!data_.empty()){
          data_[0] += "\n";
          fwrite(data_[0].c_str(),sizeof(char)*data_[0].size(),1,file_);
          //printf((data_[0] + "\n").c_str());
          data_.erase(data_.begin());
        }
        //lock_.unlock();
        
    }
    void add_data(std::string data,std::array<unsigned char,1> data_array){
        //lock_.lock();
        data_.push_back(data + " : " + std::to_string(data_array[0]));
        //lock_.unlock();
    }

	
	AsyncCore(const AsyncCore& rhs) = delete;
	AsyncCore& operator=(const AsyncCore& rhs) = delete;
	AsyncCore(AsyncCore&& rhs) = delete;
    AsyncCore& operator=(AsyncCore&& rhs) = delete;
private:
    std::vector<std::string> data_;
    FILE* file_;
    //std::mutex lock_; needs lock for more than 1 thread
};


//owns only ports and enqueue their callbacks
class SyncCore{
public:
    SyncCore():
    aCore{}, //aCore owned by sync process
    q{1}, // only 1 worker thread as for now... queue owned by sync process
    ctr{0}{
    }

	~SyncCore(){
    }
    // this is how the full sync execution by the platform will look most like
    
    void add_work_and_execute(){
        //fake events adding data:
        // this can be callbacks or monitored cbs
        std::array<unsigned char,1> data_array{0};
        ctr++;
        
        data_array = {1};
        //make sure to capture data as value 
        q.addWork([this,data_array]{aCore.add_data("Event 1 From Cycle: " + std::to_string(ctr),data_array);});
        data_array = {2};
        q.addWork([this,data_array]{aCore.add_data("Event 2 From Cycle: " + std::to_string(ctr),data_array);});
        data_array = {3};
        q.addWork([this,data_array]{aCore.add_data("Event 3 From Cycle: " + std::to_string(ctr),data_array);});

        //async 
        q.addWork([&]{
            aCore.execute();
        });
        std::cout << ctr << "\n";
        
    }
    //in execute only enqueue one execute for the aCore
    void execute(){
        //async 
        q.addWork([&]{
            aCore.execute();
        });
        ctr++;
        std::cout << ctr << "\n";
        
    }
	
	SyncCore(const SyncCore& rhs) = delete;
	SyncCore& operator=(const SyncCore& rhs) = delete;
	SyncCore(SyncCore&& rhs) = delete;
    SyncCore& operator=(SyncCore&& rhs) = delete;
private:
    AsyncCore aCore;
    WorkQueue q;
    uint32_t ctr;
};

int main(void)
{
    SyncCore sCore;
    //Platform :)
    uint32_t execute_cycle = 0;
    while (execute_cycle <= 20){
        execute_cycle++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        sCore.add_work_and_execute();
    }
    while(execute_cycle <= 100){
        execute_cycle++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        sCore.execute();
    }
	return 0;
}