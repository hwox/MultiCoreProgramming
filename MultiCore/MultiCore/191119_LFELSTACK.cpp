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
class CSTACK {
	NODE *top;
	mutex glock;
public:
	CSTACK()
	{
		top = nullptr;
	}
	~CSTACK() {
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
		NODE *e = new NODE(key);
		glock.lock();
		e->next = top;
		top = e;
		glock.unlock();
	}
	int Pop()
	{
		glock.lock();
		if (nullptr == top) {
			glock.unlock();
			return 0;
		}
		int result = top->key;
		NODE *temp = top;
		top = top->next;
		glock.unlock();
		delete temp;
		return result;
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
	NODE * volatile top;
public:
	LFSTACK()
	{
		top = nullptr;
	}
	~LFSTACK() {}

	void Init()
	{
		NODE *ptr;
		while (top != nullptr) {
			ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	bool CAS(NODE * volatile * addr, NODE *old_node, NODE *new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int *>(addr),
			reinterpret_cast<int *>(&old_node),
			reinterpret_cast<int>(new_node));
	}
	void Push(int key)
	{
		NODE *e = new NODE(key);
		while (true) {
			NODE *p = top;
			e->next = p;
			if (true == CAS(&top, p, e)) return;
		}
	}
	int Pop()
	{
		while (true) {
			NODE *p = top;
			if (nullptr == p) return 0;
			NODE *next = p->next;
			int value = p->key;
			if (p != top) continue;
			if (true == CAS(&top, p, next)) return value;
		}
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

class BACKOFF {
	int limit;
	const int minDelay = 100;
	const int maxDelay = 100000;
public:
	BACKOFF() {
		limit = minDelay;
	}
	void do_backoff()
	{
		int delay = 1 + (rand() % limit);
		limit = limit * 2;
		if (limit > maxDelay) limit = maxDelay;
		_asm mov eax, delay
		my_loop :
				_asm dec eax
				_asm jnz my_loop
	}
};

class LFBOSTACK {
	NODE * volatile top;
public:
	LFBOSTACK()
	{
		top = nullptr;
	}
	~LFBOSTACK() {}

	void Init()
	{
		NODE *ptr;
		while (top != nullptr) {
			ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	bool CAS(NODE * volatile * addr, NODE *old_node, NODE *new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int *>(addr),
			reinterpret_cast<int *>(&old_node),
			reinterpret_cast<int>(new_node));
	}
	void Push(int key)
	{
		BACKOFF bo;
		NODE *e = new NODE(key);
		while (true) {
			NODE *p = top;
			e->next = p;
			if (true == CAS(&top, p, e)) return;
			bo.do_backoff();
		}
	}
	int Pop()
	{
		BACKOFF bo;

		while (true) {
			NODE *p = top;
			if (nullptr == p) return 0;
			NODE *next = p->next;
			int value = p->key;
			if (p != top) continue;
			if (true == CAS(&top, p, next)) return value;
			bo.do_backoff();
		}
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

constexpr unsigned int ST_EMPTY = 0;
constexpr unsigned int ST_WAIT = 0x40000000;
constexpr unsigned int ST_BUSY = 0x80000000;

class EXCHANGER {
	volatile int slot;
public:
	EXCHANGER() { slot = 0; }
	bool CAS(unsigned int old_st, int value, unsigned int new_st)
	{
		int old_v = (slot & 0x3FFFFFFF) | old_st;
		int new_v = (value & 0x3FFFFFFF) | new_st;
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_int *>(&slot), &old_v, new_v);
	}
	int exchange(int value, bool *time_out)
	{

		*time_out = false;
		for (int i = 0; i < 100; i++) {
			while (true) {
				switch (slot & 0xC0000000) {// slot의 앞에 두 비트만 상태니까 앞의 2비트만 골라내기
				case ST_EMPTY:
					if (true == CAS(ST_EMPTY, value, ST_WAIT)) {
						for (int i = 0; i < 100; ++i) {
							if ((slot & 0xC0000000) != ST_WAIT) {
								int ret = slot & 0x3FFFFFFF;
								slot = ST_EMPTY;
								return ret;
							}
						}
						//	slot = 0; // 아무도 안왔다고 slot = 0하는 이 순간에 다른 게 올 수도 있으니까 CAS쓴다.
						if (true == CAS(ST_WAIT, 0, ST_EMPTY)) {
							//성공하면 time_out
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
					for (int i = 0; i < 100; i++) {
						// CAS하기 이전의 값을 저장해놔야 하고
						// 이전의 old_value까지 확인하는 CAS가 하나 더 필요하다
						if (true == CAS(ST_WAIT, value, ST_BUSY)) {
							int ret = slot & 0x3FFFFFFF;// 맨 위의 두 비트는 상ㅌㅐ니까 그걸 리턴하면 안되잖아. 
							slot = 0;
							return ret;
						}
						else {
							break;
						}
					}
					break;
				case ST_BUSY:
					break;
				default:
					cout << "Invalid State!\n";
					while (true);
				}
			}
		}
	}
};

constexpr int CAPACITY = 8;

class EL_ARRAY {
	int range;
	EXCHANGER exchanger[CAPACITY];
public:
	EL_ARRAY() {
		range = 1;
	}
	int visit(int value, bool *time_out) {
		int index = rand() % range;
		return exchanger[index].exchange(value, time_out);
	}
};

class LFELSTACK {
	NODE * volatile top;
	EL_ARRAY el_array;
public:
	LFELSTACK()
	{
		top = nullptr;
	}
	~LFELSTACK() {}

	void Init()
	{
		NODE *ptr;
		while (top != nullptr) {
			ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	bool CAS(NODE * volatile * addr, NODE *old_node, NODE *new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int *>(addr),
			reinterpret_cast<int *>(&old_node),
			reinterpret_cast<int>(new_node));
	}
	void Push(int key)
	{
		BACKOFF bo;
		NODE *e = new NODE(key);
		while (true) {
			NODE *p = top;
			e->next = p;
			if (true == CAS(&top, p, e)) return;
			bool time_out;
			int ret = el_array.visit(key, &time_out);
			if (false == time_out) {
				if (0 == ret) return;
			}
		}
	}
	int Pop()
	{
		BACKOFF bo;

		while (true) {
			NODE *p = top;
			if (nullptr == p) return 0;
			NODE *next = p->next;
			int value = p->key;
			if (p != top) continue;
			if (true == CAS(&top, p, next)) return value;
			bool time_out;
			int ret = el_array.visit(0, &time_out);
			if (false == time_out) {
				if (0 != ret) return ret;
			}
		}
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


const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000;

LFELSTACK my_stack;
void ThreadFunc(int num_thread)
{
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		if ((rand() % 2 == 0) || i < 10000 / num_thread) {
			my_stack.Push(i);
		}
		else {
			int key = my_stack.Pop();
		}
	}
}

int main()
{
	for (auto n = 1; n <= 16; n *= 2) {
		my_stack.Init();
		vector <thread> threads;
		auto s = high_resolution_clock::now();
		for (int i = 0; i < n; ++i)
			threads.emplace_back(ThreadFunc, n);
		for (auto &th : threads) th.join();
		auto d = high_resolution_clock::now() - s;
		my_stack.display20();
		//my_queue.recycle_freelist();
		cout << n << "Threads,  ";
		cout << ",  Duration : " << duration_cast<milliseconds>(d).count() << " msecs.\n";
	}
	system("pause");
}


