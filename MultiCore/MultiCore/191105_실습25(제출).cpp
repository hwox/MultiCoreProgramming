#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>

using namespace std;
using namespace std::chrono;

void EMPTY_ERROR() {
	cout << "empty_error" << endl;
	system("pause");
	exit(1);
}

class NODE {
public:
	int key;
	NODE *next;

	NODE() { next = NULL; }
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
	mutex glock;
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
		while (true) {
			NODE *last = tail;
			NODE *next = last->next;
			if (last != tail) continue;
			if (NULL == next) {
				if (CAS(&(last->next), NULL, e)) {
					CAS(&tail, last, e);
					return;
				}
			}
			else {
				CAS(&tail, last, next);
			}
		}
	}
	// return atomic_compare_exchange_strong(reinterpret_cast<volitile atomic int> (&addr),  reinterpret_cast<int*> (&old_node), reinterpret_cast<int>(new_node))
	int Deq()
	{
		while (true) {
			NODE *first = head;
			NODE *last = tail;
			NODE *next = first->next;
			if (first != head) continue;
			if (first == last) {
				if (next == NULL) {
					EMPTY_ERROR();
					CAS(&tail, last, next);
					continue;
				}
			}
			int value = next->key;
			if (false == CAS(&head, first, next)) continue;
			delete first;
			return value;
		}
	}

	bool CAS(NODE* volatile *addr, NODE* exp, NODE* up)
	{
		return atomic_compare_exchange_strong((atomic_uintptr_t*)(addr), reinterpret_cast<uintptr_t *>(&exp),
			reinterpret_cast<uintptr_t>(up));
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

const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000;

CQUEUE my_queue;
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
	}
	system("pause");
}


