
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

constexpr int MAXHEIGHT = 10;

class SLNODE {
public:
	int key;
	SLNODE* next[MAXHEIGHT]; 
	int height; 

	SLNODE() {
		key = 0;
		height = MAXHEIGHT;
		for (auto &p : next)
			p = nullptr;
	}

	SLNODE(int _key, int _height)
	{
		key = _key;
		height = _height;
		for (auto &p : next)
			p = nullptr;
	}

	SLNODE(int _key)
	{
		key = _key;
		height = MAXHEIGHT;
		for (auto &p : next)
			p = nullptr;
	}

};

//skip list
class SKLIST {
	SLNODE head, tail;
	mutex glock;

public:

	SKLIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
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
		// ���� �ٴ� �����Ͱ� null�� �ƴϸ� �ϳ��� ���� ��Ű�鼭 ����� �ȴ�.
		SLNODE *ptr;
		while (head.next[0] != &tail) {
			ptr = head.next[0];
			// head�� ptr? tail? ���̿� ��� ��带 delete�ϰ� �Ǵµ� ����ϰ� ������ ����� �Ѵ�. (�ؿ� for)
			head.next[0] = head.next[0]->next[0];
			delete ptr;
		}

		for (auto &p : head.next) p = &tail;
	}

	void Find(int key, SLNODE* preds[MAXHEIGHT], SLNODE* currs[MAXHEIGHT])
	{
		int cl = MAXHEIGHT - 1; 
		while (true) {
			// �ٵ� �ٽ� �˻��ϸ� head���� �˻��ϰ� ��. �׷��ϱ� if()�� �� ����� ���̸� head����
			if (MAXHEIGHT - 1 == cl) 
				preds[cl] = &head; 
			else
				preds[cl] = preds[cl + 1]; 

			currs[cl] = preds[cl]->next[cl];

			while (currs[cl]->key < key) {
				// ã���� �ϴ� Ű������ ������
				// ����
				preds[cl] = currs[cl];
				currs[cl] = currs[cl]->next[cl];
			}
			// current_level�� 0�����̸� �˻��� �� ���� ��
			if (0 == cl) return;
			cl--;

		}
	}
	bool Add(int key)
	{
		SLNODE *preds[MAXHEIGHT], *currs[MAXHEIGHT];

		glock.lock();
		Find(key, preds, currs);

		if (key == currs[0]->key)
		{
			glock.unlock();
			return false;
		}
		else
		{
			int height = 1; 
			while (rand() % 2 == 0)
			{
				height++;
				if (MAXHEIGHT == height) break;
			}

			SLNODE *node = new SLNODE(key, height);


			for (int i = 0; i < height; ++i)
			{
				node->next[i] = currs[i];
				preds[i]->next[i] = currs[i];
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
			for (int i = 0; i < currs[0]->height; ++i)
			{
				preds[i]->next[i] = currs[i]->next[i];
			}

			delete currs[0];
			glock.unlock();
			return true;
		}
		else
		{
			glock.unlock();
			return false;
		}

	}
	bool Contains(int key)
	{
		SLNODE *preds[MAXHEIGHT], *currs[MAXHEIGHT];

		glock.lock();

		Find(key, preds, currs);

		if (key == currs[0]->key)
		{
			glock.unlock();
			return true;
		}
		else
		{
			glock.unlock();
			return false;
		}

	}

	void display20() {

		int c = 20;
		SLNODE *p = head.next[0];

		while (p != &tail) {
			cout << p->key << ", ";
			p = p->next[0]; 
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};


SKLIST sklist;
void ThreadFunc(/*void *lpVoid,*/ int num_thread)
{
	int key;

	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			sklist.Add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			sklist.Remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			sklist.Contains(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}

int main() {

	for (auto n = 1; n <= 16; n *= 2) {
		sum = 0;
		sklist.Init();
		vector <thread> threads;
		auto start_time = high_resolution_clock::now();
		for (int i = 0; i < n; ++i) {
			threads.emplace_back(ThreadFunc, n);
		}
		for (auto &th : threads) th.join();

		auto end_time = high_resolution_clock::now();
		auto exec_time = end_time - start_time;
		int exec_ms = duration_cast<milliseconds>(exec_time).count();
		sklist.display20();
		cout << "Threads[ " << n << " ] , sum= " << sum;
		cout << ", Exec_time =" << exec_ms << " msecs\n";
		cout << endl;
	}

	system("pause");
}