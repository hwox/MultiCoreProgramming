
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

using namespace std;
using namespace std::chrono;

const auto NUM_TEST = 4000000;
const auto KEY_RANGE = 1000;

volatile int sum;

class LFNODE;

class MPTR {
	int value;

public:
	void set(LFNODE* node, bool removed) {
		value = reinterpret_cast<int>(node);
		if (true == value)
			value = value | 0x01;
		else
			value = value & 0xFFFFFFFE;
	}

	LFNODE* getptr() {
		return reinterpret_cast<LFNODE*>(value & 0xFFFFFFFE);
	}

	LFNODE* getptr(bool* removed) {
		int temp = value;
		if (0 == (temp & 0x1)) *removed = false;
		else *removed = true;
		return reinterpret_cast<LFNODE*>(temp & 0xFFFFFFFE);
	}

	bool CAS(LFNODE* old_node, LFNODE* new_node, bool old_removed, bool new_removed)
	{
		int old_value, new_value;

		old_value = reinterpret_cast<int>(old_node);
		if (true == old_removed) old_value = old_value | 0x01;
		else old_value = old_value & 0xFFFFFFFFE;

		new_value = reinterpret_cast<int>(new_node);
		if (true == old_removed) new_value = new_value | 0x01;
		else new_value = new_value & 0xFFFFFFFFE;

		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&value), &old_value, new_value);
	}

	bool TryMarking(LFNODE* old_node, bool new_removed)
	{
		int old_value, new_value;
		old_value = reinterpret_cast<int>(old_node);
		old_value = old_value & 0xFFFFFFFFE;

		new_value = old_value;
		if (true == new_removed) new_value = new_value | 0x01;

		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&value), &old_value, new_value);
	}
};

class LFNODE {
public:
	int key;
	MPTR next;
	//LFNODE* next;
	//LFNODE *next; ���ľ���
	//bool marked;
	//mutex nlock;

	LFNODE() { next.set(nullptr, false); }

	LFNODE(int key_value) {
		next.set(nullptr, false);
		key = key_value;
	}

	~LFNODE() {}
};




class LFLIST {
	LFNODE head, tail;
	LFNODE* freelist;
	LFNODE freetail;
	mutex fl;
public:
	LFLIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next.set(&tail, false);
		freetail.key = 0x7FFFFFFF;
		freelist = &freetail;
	}
	~LFLIST() {}

	void Init()
	{
		LFNODE *ptr;
		while (head.next.getptr() != &tail) {
			ptr = head.next.getptr();
			head.next = head.next.getptr()->next;
			delete ptr;
		}
	}

	//���� ���� �ʴ´�. �� ���� �����ϱ� 
	//bool validate(NODE* pred, NODE* curr) {
	//   return !pred->marked && !curr->marked  &&pred->next == curr;
	//}

	void recycle_freelist()
	{
		LFNODE* p = freelist;
		while (p != &freetail)
		{
			LFNODE* n = p->next.getptr();
			delete p;
			p = n;
		}
		freelist = &freetail;
	}

	//�̷��� ���������� ���� �ٲ��. �����͸� ���۷����� ĳ�����ҋ� �̷��� �ؾ��Ѵ�. 
	void find(int key, LFNODE* (&pred), LFNODE* (&curr))
	{
	retry:
		pred = &head;
		curr = pred->next.getptr();
		while (true)
		{
			bool removed;
			LFNODE* succ = curr->next.getptr(&removed);
			while (true == removed) {
				if (false == pred->next.CAS(curr, succ, false, false))
					goto retry; //�δܰ� ���������� �ϴ� ��ɾ c++ �� �����ϱ�. �ٸ��� �Ҽ��� ������ ����������. 
				curr = succ;
				succ = curr->next.getptr(&removed);
			}

			if (curr->key >= key) return;

			pred = curr;
			curr = curr->next.getptr();
		}
	}

	bool Add(int key)
	{
		LFNODE *pred = NULL, *curr = NULL;

		while (true) {

			find(key, pred, curr);

			if (key == curr->key) {
				return false;
			}
			else {
				LFNODE *node = new LFNODE(key);
				node->next.set(curr, false);

				if (pred->next.CAS(curr, node, false, false))
					return true;
			}
		}
		// ������ ���� unlock
	}
	bool Remove(int key)
	{
		LFNODE *pred, *curr;
		//pred = &head;
		//curr = pred->next.getptr();
		bool snip;
		while (true) {
			find(key, pred, curr);

			if (key != curr->key) {
				return false;
			}
			else {
				LFNODE* succ = curr->next.getptr();
				snip = curr->next.TryMarking(succ, true);
				if (!snip)
					continue;
				pred->next.CAS(curr, succ, false, false);
				return true;
			}
		}

	}
	bool Contains(int key)
	{
		LFNODE *pred, *curr;
		bool marked = false;
		curr = &head;
		while (curr->key < key)
		{
			curr = curr->next.getptr();
			LFNODE* succ = curr->next.getptr(&marked);
		}
		return curr->key == key && !marked;

	}

	void display20() {

		int c = 20;
		LFNODE *p = head.next.getptr();

		while (p != &tail) {
			cout << p->key << ", ";
			p = p->next.getptr();
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}


};

constexpr int MAXHEIGHT = 10;

class SLNODE {
public:
	int key;
	SLNODE *next[MAXHEIGHT];  // �ٵ� ���� key_range�� 1000���ϱ� �ִ�� ���� �þ�� 1000�� ����, ��� �� 500�� ����
	int height;
	SLNODE() {
		key = 0;
	}
	SLNODE(int x, int h) {
		key = x;
		height = h;

		for (auto &p : next) p = nullptr;
	}
	SLNODE(int x) {
		key = x;
		height = MAXHEIGHT;
		for (auto &p : next) p = nullptr;
	}
};


class SKLIST {

	SLNODE head, tail;
	mutex glock;
public:
	SKLIST()
	{
		head.key = 0x80000000; 
		tail.key = 0x7FFFFFFF; 

		//head, tail�� �����͸� �������ְ� 
		head.height = tail.height = MAXHEIGHT;
		for (auto &p : head.next) p = &tail;
		// head.next[0] = &tail.next[0]; head.next[1] = &tail.next[1];
		// Ȥ�� head.next[0] = tail.next[0];  head.next[1] = tail.next[1];
		//�̷������� �ϸ� �ȵȴ�.
		// �׷��� next�� null�� ����Ų��. tail�� next�� �� null�̴ϱ� head�� next�� �� null�� ����Ű�� ��.
	}
	~SKLIST() {
		Init();
	}

	void Init()
	{
		SLNODE *ptr;
		// ���� �ٴ� �����Ͱ� null�� �ƴϸ� �ϳ��� ���� ��Ű�鼭 ����� �ȴ�.
		while (head.next[0] != &tail) {
			ptr = head.next[0];
			// head�� ptr? tail? ���̿� ��� ��带 delete�ϰ� �Ǵµ� ����ϰ� ������ ����� �Ѵ�. (�ؿ� for)
			head.next[0] = head.next[0]->next[0];
			delete ptr;
		}
		for (auto &p : head.next) p = &tail;
	}

	void Find(int key, SLNODE *preds[MAXHEIGHT], SLNODE *currs[MAXHEIGHT]) {
		int cl = MAXHEIGHT - 1;
		while (true) {
			// �ٵ� �ٽ� �˻��ϸ� head���� �˻��ϰ� ��. �׷��ϱ� if()�� �� ����� ���̸� head����
			if(MAXHEIGHT - 1 == cl ) preds[cl] = &head;
			else {
				preds[cl] = preds[cl + 1];
			}
			currs[cl] = preds[cl]->next[cl];

			while (currs[cl]->key < key) 
			{
				// ã���� �ϴ� Ű������ ������
				// ����
				preds[cl] = currs[cl];
				currs[cl] = currs[cl]->next[cl];
			}

			if (0 == cl) {
				// current_level�� 0�����̸� �˻��� �� ���� ��
				return;
			}
			cl--; 

		}
	}

	bool Add(int key)
	{
		SLNODE *preds[MAXHEIGHT], *currs[MAXHEIGHT];

		glock.lock();
		Find(key, preds, currs);

		if (key == currs[0]->key) {
			glock.unlock(); 
			return false;
		}
		else {
			int height = 1; // ���̰� 0���� ���� ������ ��� �Ѱ����� �����ϱ� 1.
			while (rand() % 2 == 0)
			{
				height++;
				if (MAXHEIGHT == height) break;
			}
			//while (rand() % 2 == 0) height++;
			// �� while�� �ؿ����� ���� �ǹ�. �׷��� 100���� ���� �ȵǴϱ� ������������ break. 
			//if (rand() % 2 == 0)
			//{ 
			//	height++; 
			//	if (rand() % 2 == 0) {
			//		// �ٵ� ���⼭ �� Ȯ���� ���̸� ���� ������ �� ����.
			//		height++;
			//	}

			//}
		
			SLNODE *node = new SLNODE(key, height);

			// ��� �������� ���� �����ϱ�
			if (key == currs[0]->key)
			{
				preds[1] = preds[0];
				preds[0]->next[1] = preds[0]->next[0];
				preds[0]->next[0]--;
	
			}

			glock.unlock();
			return true;
		}

	}
	bool Remove(int key)
	{
		SLNODE *preds[MAXHEIGHT], *currs[MAXHEIGHT];

		glock.lock();
		Find(key, preds, currs);
		
		if (key == currs[0]->key)
		{
			//����
			SLNODE *p = 

			delete currs[0];
			glock.unlock();
			return true;
		}
		else {
			glock.unlock(); 
			return false;
		}

	}
	bool Contains(int key)
	{
		SLNODE *preds[MAXHEIGHT], *currs[MAXHEIGHT];
		glock.lock();
	
		Find(key, preds, currs);

		if (key == currs[0]->key) {
			glock.unlock(); return true;
		}
		else {
			glock.unlock(); return false;
		}

	}

	void display20() {

		int c = 20;
		SLNODE *p = head.next[0];

		while (p != &tail) {
			cout << p->key << ", ";
			// ���ʴ�� �ϴ°Ŵϱ� �ؿ� �������� �������
			p = p->next[0];
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};






SKLIST list;
void ThreadFunc(/*void *lpVoid,*/ int num_thread)
{
	int key;

	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			list.Add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			list.Remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			list.Contains(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}

int main() {

	for (auto n = 1; n <= 8; n *= 2) {
		sum = 0;
		list.Init();
		vector <thread> threads;
		auto start_time = high_resolution_clock::now();
		for (int i = 0; i < n; ++i) {
			threads.emplace_back(ThreadFunc, n);
		}
		for (auto &th : threads) th.join();

		auto end_time = high_resolution_clock::now();
		auto exec_time = end_time - start_time;
		int exec_ms = duration_cast<milliseconds>(exec_time).count();
		list.display20();
		//list.recycle_freelist();
		cout << "Threads[ " << n << " ] , sum= " << sum;
		cout << ", Exec_time =" << exec_ms << " msecs\n";
		cout << endl;
	}

	system("pause");
}