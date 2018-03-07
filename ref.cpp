#include <cstdio>

struct A {

};

struct B : A {

};

struct C : B {

};

void p(A& a) { printf("A\n"); }
void p(B& a) { printf("B\n"); }

int main() {
    C c;
    p(c);
}
