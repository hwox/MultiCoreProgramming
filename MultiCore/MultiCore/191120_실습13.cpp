#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>
using namespace std;
using namespace chrono;

class NODE {
public:
	int key;
	NODE* next;

	NODE() : key(0) { next = NULL; }

	NODE(int key_value) {
		next = NULL;
		key = key_value;
	}

	~NODE() {}
};

bool CAS(NODE* volatile* next, NODE* old_v, NODE* new_v) {
	return atomic_compare_exchange_strong(
		reinterpret_cast<atomic_int volatile*>(next),
		reinterpret_cast<int*>(&old_v),
		reinterpret_cast<int>(new_v)
	);
}



class BACKOFF {
	int limit;
	const int minDelay = 1000;
	const int maxDelay = 10000;

public:
	BACKOFF() {
		limit = minDelay;
	}

	void do_backoff() {
		int delay = rand() % limit;
		if (0 == delay) return;
		limit = limit + limit;
		if (limit > maxDelay) limit = maxDelay;

		_asm mov ecx, delay;
	myloop:
		_asm loop myloop;
	}
};

class nullmutex {
public:
	void lock() {}
	void unlock() {}
};

class CSTACK {
	NODE *top;
	mutex glock;
public:
	CSTACK()
	{
		top = nullptr;
	}
	~CSTACK()
	{
		Init();
	}

	void Init()
	{
		while (top != nullptr) {
			NODE *ptr = top;
			top = top->next;
			delete ptr;
		}
	}

	void Push(int key)
	{
		NODE *e = new NODE{ key };
		glock.lock();
		e->next = top;
		top = e;
		glock.unlock();
	}
	int Pop()
	{
		glock.lock();
		if (nullptr == top)
		{
			glock.unlock(); 
			return 0;
		}
		int temp = top->key;
		NODE *ptr = top;
		top = top->next;
		glock.unlock();
		delete ptr;
		return temp;
	}

	void display20()
	{
		int c = 20;
		NODE *p = top;
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
class LFSTACK {
	NODE* volatile top;

public:
	LFSTACK() {
		top = nullptr;
	}

	~LFSTACK() {
		Init();
	}

	void Init() {
		while (top != nullptr) {
			NODE* ptr = top;
			top = top->next;
			delete ptr;
		}
	}

	void Push(int key) {

		NODE* e = new NODE(key);
		while (true) {
			NODE* p = top;
			e->next = p;
			if (true == CAS(&top, p, e)) { return; }
		}
	}

	int Pop() {
		while (true) {
			NODE* p = top;
			if (nullptr == p) { return 0; }
			NODE* next = p->next;
			int value = p->key;
			if (p != top) continue;
			if (true == CAS(&top, p, next)) {
				return value;
			}
		}
	}

	int EMPTY_ERROR() {
		cout << "Stack Empty\n";
		return 0;
	}

	void display20() {
		int c = 20;
		NODE* p = top->next;

		while (p != nullptr) {
			cout << p->key;
			p = p->next;
			c--;
			if (c == 0) break;
			cout << ", ";
		}
		cout << endl;
	}
};

//락프리 BACK OFF 스택
class LFBOSTACK {
	NODE* volatile top;

public:
	LFBOSTACK() {
		top = nullptr;
	}

	~LFBOSTACK() {
		Init();
	}

	void Init() {
		while (top != nullptr) {
			NODE* ptr = top;
			top = top->next;
			delete ptr;
		}
	}

	void Push(int key) {

		BACKOFF bo;
		NODE* e = new NODE(key);
		while (true) {
			NODE* p = top;
			e->next = p;
			if (true == CAS(&top, p, e)) { return; }
			bo.do_backoff();
		}
	}

	int Pop() {

		BACKOFF bo;
		while (true) {
			NODE* p = top;
			if (nullptr == p) { return 0; }
			NODE* next = p->next;
			int value = p->key; 
			if (p != top) continue;
			if (true == CAS(&top, p, next)) {
				return value;
			}
			bo.do_backoff();
		}
	}

	int EMPTY_ERROR() {
		cout << "Stack Empty!\n";
		return 0;
	}

	void display20() {
		int c = 20;
		NODE* p = top->next;

		while (p != nullptr) {
			cout << p->key;
			p = p->next;
			c--;
			if (c == 0) break;
			cout << ", ";
		}
		cout << endl;
	}
};


constexpr unsigned ST_EMPTY = 0;		 //0000 0000 0000 0000 0000 0000 0000 0000
constexpr unsigned ST_WAIT = 0x40000000; //0100 0000 0000 0000 0000 0000 0000 0000 
constexpr unsigned ST_BUSY = 0x80000000; //1000 0000 0000 0000 0000 0000 0000 0000

//0x3FFFFFFF -> 0011 1111 1111 1111 1111 1111 1111 1111
//0xC0000000 -> 1100 0000 0000 0000 0000 0000 0000 0000

class EXCHANGER {
	volatile int slot;

public:
	EXCHANGER() { slot = 0; }
	bool CAS(unsigned int old_st, int value, unsigned int new_st)
	{
		int old_v = (slot & 0x3FFFFFFF) | old_st; 
		int new_v = (value & 0x3FFFFFFF) | new_st;
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(&slot), &old_v, new_v);
	}

	bool CAS(int ytItem, unsigned int old_st, int myItem, unsigned int new_st)
	{
		int old_v = (slot & 0x3FFFFFFF) | old_st; 
		int new_v = (myItem & 0x3FFFFFFF) | new_st; 
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(&slot), &old_v, new_v);
	}

	int exchange(int value, bool* time_out)
	{
		*time_out = false;
		int count = 0;
		while (count < 10) {
			switch (slot & 0xC0000000) 
			{
			case ST_EMPTY:
				if (true == CAS(ST_EMPTY, value, ST_WAIT)) {
					// 성공하면 기다리기
//유한한 횟수만큼 돌아야 하니까 for문
					for (int i = 0; i < 100; ++i) {
						if ((slot & 0xC0000000) != ST_WAIT) { 		
							// 다른 게 오ㅏ서 바꿨으면 성공적으로 교환이 일어난 것
							int ret = slot & 0x3FFFFFFF;// 맨 위의 두 비트는 상ㅌㅐ니까 그걸 리턴하면 안되잖아. 
							slot = ST_EMPTY;
							return ret;
						}
					}
					//	slot = 0; // 아무도 안왔다고 slot = 0하는 이 순간에 다른 게 올 수도 있으니까 CAS쓴다.
					if (true == CAS(ST_WAIT, 0, ST_EMPTY)) {
						*time_out = true;
						return 0;
					}
					else {
						int ret = slot & 0x3FFFFFFF;
						slot = ST_EMPTY;
						return ret;
					}
				}
				break;

			case ST_WAIT:
			{
				if (true == CAS(ST_WAIT, value, ST_BUSY)) 
				{
					int ret = slot & 0x3FFFFFFF;  
					return ret;
				}
				break;
			}
			case ST_BUSY:
				break;

			default:
				cout << "Invalid State! " << (slot & 0xC0000000) << "\n";
				while (true);
			}
			count++;
		}

		*time_out = true;
		return 0;
	}
};

constexpr int CAPACITY = 8; 

class EL_ARRAY {
public:
	int range;
	EXCHANGER exchanger[CAPACITY];

	EL_ARRAY() {
		range = 1;
	}
	int visit(int value, bool* time_out) {
		int index = rand() % range;
		return exchanger[index].exchange(value, time_out);
	}
};

class LFELSTACK {
	NODE* volatile top;
	EL_ARRAY el_array;

public:
	LFELSTACK() {
		top = nullptr;
	}

	~LFELSTACK() {
		Init();
	}

	void Init() {
		while (top != nullptr) {
			NODE* ptr = top;
			top = top->next;
			delete ptr;
		}
	}

	void Push(int key) {

		BACKOFF bo;
		NODE* e = new NODE(key);
		while (true) {
			NODE* p = top;
			e->next = p;
			if (true == CAS(&top, p, e)) { return; }
			bool time_out;
			int ret = el_array.visit(key, &time_out);
			if (false == time_out) {
				if (0 == ret) return;
			}
			else
			{
				el_array.range++;
			}
		}
	}

	int Pop() {

		BACKOFF bo;
		while (true) {
			NODE* p = top;
			if (nullptr == p) { return 0; }
			NODE* next = p->next;
			int value = p->key;
			if (p != top) continue;
			if (true == CAS(&top, p, next)) {
				return value;
			}
			bool time_out;
			int ret = el_array.visit(0, &time_out);
			if (false == time_out) {
				if (0 != ret)
					return ret;
			}
			else
			{
				if (el_array.range > 1)
					el_array.range--;
			}
		}
	}

	int EMPTY_ERROR() {
		cout << "Stack Empty!\n";
		return 0;
	}

	void display20() {
		int c = 20;
		NODE* p = top->next;

		while (p != nullptr) {
			cout << p->key;
			p = p->next;
			c--;
			if (c == 0) break;
			cout << ", ";
		}
		cout << endl;
	}
};


const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000;

LFELSTACK mystack;

void ThreadFunc(int num_thread) {
	for (int i = 0; i < NUM_TEST / num_thread; ++i)
	{
		if (rand() % 2 == 0 || i < 10000 / num_thread)
		{
			mystack.Push(i);
		}
		else {
			mystack.Pop();
		}
	}
}

int main()
{
	for (int num_threads = 1; num_threads <= 16; num_threads *= 2)
	{
		mystack.Init();
		vector<thread> threads;

		auto start_time = high_resolution_clock::now();

		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(ThreadFunc, num_threads);

		for (auto& thread : threads)
			thread.join();

		threads.clear();

		auto end_time = high_resolution_clock::now();
		auto exec_time = end_time - start_time;
		int exec_ms = duration_cast<milliseconds>(exec_time).count();

		mystack.display20();

		cout << "Threads [ " << num_threads << "] "
			<< " Exec_time  = " << exec_ms << "ms\n\n";
		cout << endl;
	}

	system("pause");
}
