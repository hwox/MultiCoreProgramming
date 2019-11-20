#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>

void ThreadFunc(int threadNum);
using namespace std;
using namespace std::chrono;
const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000;

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


class BACKOFF {
public:
	int limit;
	const int minDelay = 10;
	const int maxDelay = 100000;
	BACKOFF() {
		limit = minDelay;
	}
	void wait()
	{
		int delay = 1 + (rand() % limit);
		if (delay == 0) return;
		limit = limit + limit;
		if (limit > maxDelay) limit = maxDelay;

		_asm mov eax, delay;
	myloop:
		_asm dec eax
		_asm jnz myloop;
	}
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
		NODE *ptr;
		while (top != nullptr)
		{
			ptr = top;
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

		if (nullptr == top)
		{

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
		NODE *p = top->next;
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
	NODE* top;
	mutex glock;
public:
	LFSTACK()
	{
		top = nullptr;
	}
	~LFSTACK() {}

	void Init()
	{
		NODE *ptr;
		while (top != nullptr)
		{
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
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = top;
			if (last != top) continue;
			e->next = last;
			// top이 top이면 e로 바꾸기? 다른 스레드가 와서 
			// top을 쭉 날려서 top이 저 밑으로 내려갔다? 
			// 그러면 top이 전혀 엉뚱한 노드를 가리키게 됨. 잘하면 오동작 잘못되면 사망
			// CAS할 때 그래서 last에 복사해 놓은 다음에 CAS를 하는 것
			// 성공하면 return 실패하면 처음부터 다시 읽는것
			if (CAS(&top, last, e)) {
				return;
			}
		}
	}
	int Pop()
	{
		while (true) {
			//if (top == nullptr) return 0;
			//top이 null인지 보면 안되고 top을 저장해놓은 걔가 null인지 봐야 한다.
			// 왜? 그 와중에 밑으로 내려오면서 바뀔 수가 있으니까
			NODE* first = top;
			if (first == nullptr) return 0;
			NODE* last = first->next;
			int value = first->key;
			if (first != top)
				continue;

			if (false == CAS(&top, first, last))
				continue;
			return value;
		}
		return 0;
	}

	void display20()
	{
		int c = 20;
		NODE *p = top->next;
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

class LFBOSTACK {
	NODE* top;
	mutex glock;
public:
	LFBOSTACK()
	{
		top = nullptr;
	}
	~LFBOSTACK() {}

	void Init()
	{
		NODE *ptr;
		while (top != nullptr)
		{
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
		// 성공했을 땐 그냥 끝나는데 실패하면?
		BACKOFF bo;
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = top;
			if (last != top) continue;
			e->next = last;

			if (CAS(&top, last, e)) {
				return;
			}
			bo.wait(); // 호출해줘야 함. 실패하면
		}
	}
	int Pop()
	{
		BACKOFF bo;
		while (true) {

			NODE* first = top;
			if (first == nullptr) return 0;
			NODE* last = first->next;
			int value = first->key;
			if (first != top)
				continue;

			if (false == CAS(&top, first, last))
				continue;
			bo.wait();
			return value;

		}
	}

	void display20()
	{
		int c = 20;
		NODE *p = top->next;
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
	bool CAS(unsigned int old_status, int value, unsigned int new_status)
	{
		// CAS할 때 그냥 하면 안되고 다른 게 바꼈는지 안바뀌엇는지 확인하면서 해야 함.
		int old_v = (slot & 0x3FFFFFFF) | old_status; // 슬롯의 두 비트를 지우고 
		int new_v = (value & 0x3FFFFFFF) | new_status;

		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_int*>(&slot), &old_v, new_v);
	}
	int exchange(int value, bool *time_out)
	{
		*time_out = false;
		for (int i = 0; i < 100; i++) {
			while (true) {
				switch (slot && 0xC0000000) // slot의 앞에 두 비트만 상태니까 앞의 2비트만 골라내기
				{
				case ST_EMPTY:
				{
					int cur_value = slot;
					if (true == CAS(ST_EMPTY, value, ST_WAIT)) {
						// 성공하면 기다리기
						//유한한 횟수만큼 돌아야 하니까 for문
						for (int i = 0; i < 100; i++) {
							if ((slot && 0xC0000000) != ST_WAIT) {
								// 다른 게 오ㅏ서 바꿨으면 성공적으로 교환이 일어난 것
								int ret = slot & 0x3FFFFFFF;// 맨 위의 두 비트는 상ㅌㅐ니까 그걸 리턴하면 안되잖아. 
								slot = 0;
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
							return slot & 0x3FFFFFFF;
						}
					}
				}
				break;
				case ST_WAIT:
					for (int i = 0; i < 100; i++) {
						if (true == CAS(ST_WAIT, value, ST_BUSY)) {
							int ret = slot & 0x3FFFFFFF;// 맨 위의 두 비트는 상ㅌㅐ니까 그걸 리턴하면 안되잖아. 
							slot = 0;
							return ret;
						}
					}

					break;
				case ST_BUSY:
					break;
				default:
					cout << "Invalid State! \n";
					while (true);
				}
			}

			*time_out = true;
			return 0;
		}
	}


};

constexpr int CAPACITY = 8;

class EL_ARRAY {

	int range;
	EXCHANGER exchanger[CAPACITY];

public:
	EL_ARRAY() {
		range = 0;
	}
	int visit(int value, bool *time_out)
	{
		int index = rand() % range;
		range += 1;
		return exchanger[index].exchange(value, time_out);
	}

	int range_change() {
		range -= 1;
	}

};

class LFELSTACK {
	NODE *volatile top;
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
		while (top != nullptr)
		{
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
		// 성공했을 땐 그냥 끝나는데 실패하면?
		BACKOFF bo;
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = top;
			if (last != top) continue;
			e->next = last;

			if (CAS(&top, last, e)) {
				return;
			}
			bool time_out;
			int ret = el_array.visit(key, &time_out);
			if (false == time_out) {
				// true이면 다시 시도해줘야 함
				// false일때만 성공했나 봐야 한다.
				if (0 == ret) return; // 성공적으로 교환한 것이니까 교환
			}
			// 아니면 다시 위에서부터

		}
	}
	int Pop() {

		while (true) {
			NODE* p = top;
			if (nullptr == p) { return 0; }
			NODE* next = p->next;
			int value = p->key; //cas하고 읽는거 의미없다. 미리 읽어둬야한다.
			if (p != top) continue;
			if (true == CAS(&top, p, next)) {
				//delete first;
				return value;
			}
			bool time_out;
			int ret = el_array.visit(0, &time_out);
			if (false == time_out) {
				if (0 != ret) return ret; //아니면 처음부터 다시 
			}
		}
	}

	void display20()
	{
		int c = 20;
		NODE *p = top->next;
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


LFELSTACK my_stack;

int main()
{
	for (auto n = 1; n <= 16; n *= 2) {
		my_stack.Init();
		vector <thread> threads;
		auto s = high_resolution_clock::now();
		for (int i = 0; i < n; ++i)
			threads.emplace_back(ThreadFunc, n);
		for (auto &th : threads) th.join();
		//my_stack.recycle_freelist();
		auto d = high_resolution_clock::now() - s;
		my_stack.display20();
		cout << n << "Threads,  ";
		cout << ",  Duration : " << duration_cast<milliseconds>(d).count() << " msecs.\n";
	}
}

void ThreadFunc(int num_thread)
{
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		if ((rand() % 2 == 0) || 10000 / num_thread) {
			my_stack.Push(i);
		}
		else {
			int key = my_stack.Pop();
		}
	}
}
