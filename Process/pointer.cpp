#include<bits/stdc++.h>

using namespace std;

class Notice{
	
	public:
		int val1, val2, val3, val4;
		static int counter;
		Notice(int val11, int val21) {
			counter++;
			val1 = val11;
			val2 = val21;
		}
		
		void get_val4()
		{
			val4++;
			cout << "VAL4:: " << val4 << endl;
		}
		void set_val3()
		{
			val3 += (counter*val1);
			get_val4();
		}
		void get_values()
		{
			cout << "VAL1:: " << val1 << " VAL2:: " << val2 << " VAL3:: " << val3 << endl;
		}

};


int Notice::counter = 0;

int main()
{
	for (int i = 0; i < 2; i++) {
		Notice* n = new Notice(i, i + 1);
		n->set_val3();
		n->get_values();
		delete(n);
	}

	return 0;
}
