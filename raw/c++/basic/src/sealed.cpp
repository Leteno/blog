
#include <iostream>

using namespace std;

class Base {
public:
	virtual void Virt() {
		cout << "Base::Virt" << endl;
	}
};

class Derived : public Base {
public:
	virtual void Virt() {
		cout << "Derived::Virt" << endl;
	}
};

class SealedDerived : public Base {
public:
	virtual void Virt() sealed {
		cout << "Derived::Virt" << endl;
	}
};

int Function1() {
	Derived* der = new Derived();
	der->Virt();
	delete der;
	return 0;
}

int Function2() {
	SealedDerived* der = new SealedDerived();
	der->Virt();
	delete der;
	return 0;
}

int Function3() {
	Base* b = new Base();
	b->Virt();
	delete b;
	return 0;
}

int main() {
	Function1();
	Function2();
	Function3();
	return 0;
}