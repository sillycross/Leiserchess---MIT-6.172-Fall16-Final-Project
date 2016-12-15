#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include <cstring>
#include <cassert>

using namespace std;

typedef long long LL;
typedef unsigned long long ULL;

#define SIZE(x) (int((x).size()))
#define rep(i,l,r) for (int i=(l); i<=(r); i++)
#define repd(i,r,l) for (int i=(r); i>=(l); i--)
#define rept(i,c) for (__typeof((c).begin()) i=(c).begin(); i!=(c).end(); i++)

#ifndef ONLINE_JUDGE
#define debug(x) { cerr<<#x<<" = "<<(x)<<endl; }
#else
#define debug(x) {}
#endif

char buf1[10000], buf2[10000];

struct node
{
	string result;
	map<string, node*> child;
	node() {}
};

node *root;

void insert(string s, string val)
{
	string t="";
	int i=0;
	node *cur=root;
	while (i<s.length())
	{
		while (s[i]!=' ') t+=s[i], i++;
		if (!cur->child.count(t))
			cur->child[t]=new node();
		cur=cur->child[t];
		t="";
		i++;
	}
	cur->result=val;
}

string moves[1000000], result[1000000];
int leftc[1000000], rightc[1000000];
node *q[1000000];

void lemon()
{
	root=new node();
	root->result="h4g5";
	int cnt=0;
	while (1)
	{
		if (!gets(buf1)) break;
		if (!gets(buf2)) break;
		string s1=buf1;
		assert(s1[s1.length()-1]!=' ');
		s1=s1+" ";
		string s2=buf2;
		//assert(3<=s2.length() && s2.length()<=4);
		insert(s1,s2);
		cnt++;
	}
	fprintf(stderr,"%d states processed\n",cnt);
	int head=1, tail=2;
	q[head]=root; moves[head]="";
	while (head<tail)
	{
		node *cur=q[head];
		leftc[head]=tail;
		rept(it,cur->child)
		{
			q[tail]=it->second;
			moves[tail]=it->first;
			tail++;
		}
		rightc[head]=tail;
		head++;
	}
	
	printf("const char* const moves[] = {\n  ");
	rep(i,1,tail-1)
	{
		printf("\"%s\"",moves[i].c_str());
		if (i<tail-1) printf(",");
		if (i%10==0) printf("\n  ");
	}
	printf("\n};\n\n");
	printf("const int childrange[][2] = {\n  ");
	int maxb=0;
	rep(i,1,tail-1)
	{
		printf("{%d,%d}",leftc[i]-1,rightc[i]-1);
		if (rightc[i]-leftc[i]>maxb) maxb=rightc[i]-leftc[i];
		if (i<tail-1) printf(",");
		if (i%5==0) printf("\n  ");
	}
	printf("\n};\n\n");
	printf("const char* const results[] = {\n  ");
	rep(i,1,tail-1)
	{
		printf("\"%s\"",q[i]->result.c_str());
		if (i<tail-1) printf(",");
		if (i%10==0) printf("\n  ");
	}
	printf("\n};\n\n");
	fprintf(stderr,"max branch = %d\n",maxb);
}

int main()
{
	ios::sync_with_stdio(true);
	#ifndef ONLINE_JUDGE
		freopen("D.txt","r",stdin);
	#endif
	lemon();
	return 0;
}

