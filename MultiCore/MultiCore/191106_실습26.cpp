#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>

using namespace std;
using namespace std::chrono;

class NODE {
public:
	int key;
	NODE *next;

	NODE() { next = nullptr; }
	NODE(int key_value) {
		next = nullptr;
		key = key_value;
	}
	~NODE() {}
};
class nullmutex {
public:
	void lock() {}
	void unlock() {}
};
class CQUEUE {
	NODE *head, *tail;
	nullmutex glock;
public:
	CQUEUE()
	{
		head = tail = new NODE(0);
	}
	~CQUEUE() {}

	void Init()
	{
		NODE *ptr;
		while (head->next != nullptr) {
			ptr = head->next;
			head->next = head->next->next;
			delete ptr;
		}
		tail = head;
	}
	void Enq(int key)
	{
		NODE *e = new NODE(key);
		glock.lock();
		tail->next = e;
		tail = e;
		glock.unlock();
	}
	int Deq()
	{
		glock.lock();
		if (nullptr == head->next) {
			cout << "QUEUE EMPTY!!\n";
			while (true);
		}
		int result = head->next->key;
		NODE *temp = head;
		head = head->next;
		glock.unlock();
		delete temp;
		return result;
	}

	void display20()
	{
		int c = 20;
		NODE *p = head->next;
		while (p != nullptr)
		{
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};
class LFQUEUE {
	NODE * volatile head;
	NODE * volatile tail;
public:
	LFQUEUE()
	{
		head = tail = new NODE(0);
	}
	~LFQUEUE() {}

	void Init()
	{
		NODE *ptr;
		while (head->next != nullptr) {
			ptr = head->next;
			head->next = head->next->next;
			delete ptr;
		}
		tail = head;
	}
	bool CAS(NODE * volatile * addr, NODE *old_node, NODE *new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int *>(addr),
			reinterpret_cast<int *>(&old_node),
			reinterpret_cast<int>(new_node));
	}
	void Enq(int key)
	{
		NODE *e = new NODE(key);
		while (true) {
			NODE *last = tail;
			NODE *next = last->next;
			if (last != tail) continue;
			if (next != nullptr) {
				CAS(&tail, last, next);
				continue;
			}
			if (false == CAS(&last->next, nullptr, e)) continue;
			CAS(&tail, last, e);
			return;
		}
	}
	int Deq()
	{
		while (true) {
			NODE *first = head;
			NODE *next = first->next;
			NODE *last = tail;
			NODE *lastnext = last->next;
			if (first != head) continue;
			if (last == first) {
				if (lastnext == nullptr) {
					cout << "EMPTY!!!\n";
					this_thread::sleep_for(1ms);
					return -1;
				}
				else
				{
					CAS(&tail, last, lastnext);
					continue;
				}
			}
			if (nullptr == next) continue;
			int result = next->key;
			if (false == CAS(&head, first, next)) continue;
			first->next = nullptr;
			delete first;
			return result;
		}
	}

	void display20()
	{
		int c = 20;
		NODE *p = head->next;
		while (p != nullptr)
		{
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};

//STAMP_PTR
class SPTR {

public:
	NODE * volatile ptr;
	volatile int stamp;
	SPTR() {
		ptr = nullptr;
		stamp = 0;
	}

	SPTR(NODE* p, int v) {
		ptr = p;
		stamp = v;
	}
};
class SLFQUEUE {
	SPTR head;
	SPTR tail;
public:
	SLFQUEUE()
	{
		head.ptr = tail.ptr = new NODE(0);
	}
	~SLFQUEUE() {}

	void Init()
	{
		NODE *ptr;
		while (head.ptr->next != nullptr) {
			ptr = head.ptr->next;
			head.ptr->next = head.ptr->next->next;
			delete ptr;
		}
		tail = head;
	}
	bool CAS(NODE * volatile * addr, NODE *old_node, NODE *new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int *>(addr),
			reinterpret_cast<int *>(&old_node),
			reinterpret_cast<int>(new_node));
	}

	bool STAMP_CAS(SPTR *addr, NODE *old_node, int old_stamp, NODE *new_node)
	{
		SPTR old_ptr{ old_node, old_stamp };
		SPTR new_ptr{ new_node, old_stamp + 1 };
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_llong *>(addr),
			reinterpret_cast<long long *>(&old_ptr),
			*(reinterpret_cast<long long *>(&new_ptr)));
	}

	void Enq(int key)
	{
		NODE *e = new NODE(key);
		while (true) {
			SPTR last = tail;
			NODE *next = last.ptr->next;
			if (last.stamp != tail.stamp) continue;
			if (next != nullptr) {
				STAMP_CAS(&tail, last.ptr, last.stamp, next);
				continue;
			}
			if (false == CAS(&last.ptr, nullptr, e))
				continue;
			STAMP_CAS(&tail, last.ptr, last.stamp, e);
			return;
		}
	}
	int Deq()
	{
		while (true) {
			SPTR first = head;
			NODE *next = first.ptr->next; //로컬변수라 굳이 SPTR로 할 필요가 없다. next의 스탬프 값을 cas에 쓰지 않음. head tail을 직접 쓸때만 
			SPTR last = tail;
			NODE *lastnext = last.ptr->next;
			if (first.ptr != head.ptr) continue;
			if (last.ptr == first.ptr) { //  if (first.ptr == head.ptr ???
				if (lastnext == nullptr) {
					cout << "EMPTY!!!\n";
					this_thread::sleep_for(1ms);
					return -1;
				}
				else
				{
					STAMP_CAS(&tail, last.ptr, last.stamp, lastnext);
					continue;
				}
			}
			if (nullptr == next) continue;
			int result = next->key;
			if (false == STAMP_CAS(&head, first.ptr, first.stamp, next))
				continue;

			delete first.ptr;
			return result;
		}
	}

	void display20()
	{
		int c = 20;
		NODE *p = head.ptr->next;
		while (p != nullptr)
		{
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};



const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000;

LFQUEUE my_queue;
void ThreadFunc(int num_thread)
{
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		if ((rand() % 2 == 0) || i < 10000 / num_thread) {
			my_queue.Enq(i);
		}
		else {
			int key = my_queue.Deq();
		}
	}
}

int main()
{
	for (auto n = 1; n <= 16; n *= 2) {
		my_queue.Init();
		vector <thread> threads;
		auto s = high_resolution_clock::now();
		for (int i = 0; i < n; ++i)
			threads.emplace_back(ThreadFunc, n);
		for (auto &th : threads) th.join();
		auto d = high_resolution_clock::now() - s;
		my_queue.display20();
		//my_queue.recycle_freelist();
		cout << n << "Threads,  ";
		cout << ",  Duration : " << duration_cast<milliseconds>(d).count() << " msecs.\n";
		cout << endl;
	}
	system("pause");
}

