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
#define rept(i,c) for (typeof((c).begin()) i=(c).begin(); i!=(c).end(); i++)

#ifndef ONLINE_JUDGE
#define debug(x) { cerr<<#x<<" = "<<(x)<<endl; }
#else
#define debug(x) {}
#endif

char s[10000];

void lemon()
{
	while (gets(s))
	{
		if (strlen(s)<12) exit(0);
		if (s[13]=='|') s[13]=0; else s[12]=0;
		printf("%s\n",s+9);
	}
}

int main(int argc, char **argv)
{
	ios::sync_with_stdio(true);
	#ifndef ONLINE_JUDGE
		freopen(argv[1],"r",stdin);
		freopen(argv[2],"w",stdout);
	#endif
	lemon();
	return 0;
}

