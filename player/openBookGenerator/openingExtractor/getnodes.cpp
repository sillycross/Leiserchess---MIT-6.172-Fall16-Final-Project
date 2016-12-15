#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include <cstring>

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

struct node
{
	int depth;
	int count;
	map<string, node*> child;
	node() {}
	node(int d)
	{
		depth = d; count = 0;
		child.clear();
	}
};

node *root;

int DLIM = 20;

string p1[1000], p2[1000], p[1000];
char buf[1000];
int cnt[1000], cnt2[1000];

void insert(int len)
{
	node *cur = root;
	rep(i,1,len)
	{
		if (!cur->child.count(p[i]))
		{
			cur->child[p[i]]=new node(i);
			cnt[i]++;
		}
		cur = cur->child[p[i]];
		cur->count++;
	}
}

int cnt3[1000];

void verify(int len)
{
	node *cur = root;
	rep(i,1,len)
	{
		if (!cur->child.count(p[i])) return;
		cur = cur->child[p[i]];
		cnt3[i]++;
	}
}

string t[1000];
int uid=0;
int d9=0, d10=0, d11=0;
void dfs(node *cur)
{
	if (cur!=root)
	{
		int flag=0, depth=10;
		if (cur->count>=2) 
		{
			flag=1; 
			if (cur->depth<=7) 
				depth=11; 
			else if (cur->depth<=15)
				depth=10;
			else if (cur->depth<=18)
				depth=9;
		}
		else if (cur->depth<=15)
		{
			flag=1;
			depth=9;
		}
		//flag=0;
		//if (cur->depth<=20) flag=1, depth=max(depth,10);
		//if (depth==11) depth=10;
		if (flag)
		{
			uid++;
			printf("INSERT IGNORE INTO `tasks` VALUES (NULL,%d,%d,'",cur->depth,depth);
			rep(i,1,cur->depth) printf("%s ",t[i].c_str());
			printf("','',0);\n");
			if (depth==10)
			{
				printf("UPDATE `tasks` SET depth='10',status='0' WHERE moves='");
				rep(i,1,cur->depth) printf("%s ",t[i].c_str());
				printf("' AND depth='9';\n");
			}
			if (depth==9) d9++; else if (depth==10) d10++; else d11++;
		}
	}
	if (cur->depth>=21) return;
	rept(it,cur->child)
	{
		t[cur->depth+1]=it->first;
		dfs(it->second);
	}
}

void lemon()
{
	root = new node(0);
	int wrongdata = 0, emptydata = 0, correctdata = 0;
	int cas=-1;
	while (1)
	{
		cas++;
		int flag=1;
		int n1;
		scanf("%d",&n1);
		if (n1==-1) break;
		rep(i,1,n1) 
		{
			scanf("%s",buf);
			p1[i]=buf;
			rep(j,0,int(p1[i].length())-1)
				if (p1[i][j]=='|') flag=0;
		}
		int n2;
		scanf("%d",&n2);
		rep(i,1,n2)
		{
			scanf("%s",buf);
			p2[i]=buf;
			rep(j,0,int(p2[i].length())-1)
				if (p2[i][j]=='|') flag=0;
		}
		if (flag==0) continue;
		if (n1-n2!=0 && n1-n2!=1)
		{
			//printf("warning: wrong data on cas %d, n1 = %d, n2 = %d, ignoring..\n", cas, n1, n2);
			wrongdata++; continue;
		}
		if (n1==0 || n2==0)
		{
			//printf("empty: %d\n",cas);
			emptydata++; continue;
		}
		int all=0;
		rep(i,1,max(n1,n2))
		{
			all++; p[all]=p1[i];
			if (i<=n2) { all++; p[all]=p2[i]; }
		}
		correctdata++;
		insert(all);
	}
	fprintf(stderr,"Good Data = %d, Wrong Data = %d, Empty Data = %d\n", correctdata, wrongdata, emptydata);
	dfs(root);
	fprintf(stderr,"d9=%d, d10=%d, d11=%d\n",d9,d10,d11);
	/*
	rep(i,1,50)
	{
		printf("Node count #%d = %d\n",i,cnt[i]);
	}
	*/
	
}

int main()
{
	ios::sync_with_stdio(true);
	#ifndef ONLINE_JUDGE
		freopen("T1.txt","r",stdin);
	#endif
	lemon();
	return 0;
}

