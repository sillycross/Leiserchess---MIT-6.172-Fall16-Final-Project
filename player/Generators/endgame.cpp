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


#ifndef ONLINE_JUDGE
#define debug(x) { cerr<<#x<<" = "<<(x)<<endl; }
#else
#define debug(x) {}
#endif

//0: only two kings
//1: current hand have a pawn in 3rd dim
//2: opponent hand have a pawn in 3rd dim

static const uint8_t TWO_KING = 0;
static const uint8_t CURRENT_HAVE_PAWN = 1;
static const uint8_t OPPONENT_HAVE_PAWN = 2;

struct piece_type
{
	int type;
	int sKing;
	int eKing;
	int pawn;
};

//0=nothing zapped
//1=win
//2=lose
//3=pawn zapped
static const uint8_t NOTHING = 0;
static const uint8_t WIN = 1;
static const uint8_t Lose = 2;
static const uint8_t Zapped = 3;

uint8_t nextState[3][256][256][256];
//reverse graph
vector<int> *A[3][256][256][256];
//1=win
//2=lose
//0=undetermined
static const uint8_t undetermined = 0;

uint8_t dp[3][256][256][256][4];
uint8_t deg[3][256][256][256][4];
uint8_t vis[3][256][256][256][4];
vector<int> q;
int head=0;

int checkCan(int type, int sKing, int eKing, int pawn, int k, int ntype, int nsKing, int neKing, int npawn, int nk)
{
	swap(nsKing,neKing);
	if (ntype) ntype=3-ntype;
	if (k==0 || nk==0) return 1;
	if (type!=ntype) return 1;
	int osKing, oeKing, opawn;
	if (k==1) 
	{
		osKing=eKing; oeKing=sKing; opawn=pawn;
	}
	if (k==2)
	{
		osKing=pawn; oeKing=eKing; opawn=sKing;
	}
	if (k==3)
	{
		osKing=sKing; oeKing=pawn; opawn=eKing;
	}
	if (type==ntype && osKing==nsKing && oeKing==neKing && opawn==npawn) return 0;
	return 1;
}

int compress(int type, int sKing, int eKing, int pawn, int nk)
{
	return type*256*256*256*4+sKing*256*256*4+eKing*256*4+pawn*4+nk;
}

void addEdge(int type, int sKing, int eKing, int pawn, int ntype, int nsKing, int neKing, int npawn, int nk)
{
	swap(nsKing,neKing);
	if (ntype) ntype=3-ntype;
	A[ntype][nsKing][neKing][npawn][nk].push_back(compress(type,sKing,eKing,pawn,0));
	for (int i = 0; i <= 3; i++)
		if (checkCan(type,sKing,eKing,pawn,i,ntype,nsKing,neKing,npawn,nk))
			deg[type][sKing][eKing][pawn][i]++;
}

void addToQueue(int type, int sKing, int eKing, int pawn, int k)
{
	if (vis[type][sKing][eKing][pawn][k]) return;
	vis[type][sKing][eKing][pawn][k]=1;
	q.push_back(compress(type, sKing, eKing, pawn, k));
}

void checkAndAdd(int type, int sKing, int eKing, int pawn, int nsKing, int neKing, int npawn, int nk)
{
	if (nextState[type][nsKing][neKing][npawn]==1 || nextState[type][nsKing][neKing][npawn]==2)
	{
		if (nextState[type][nsKing][neKing][npawn]==1)
		{
			for (int k = 0; k <= 3; k++)
			{
				dp[type][sKing][eKing][pawn][k]=1;
				addToQueue(type,sKing,eKing,pawn,k);
			}
		}
	}
	else
	{
		if (nextState[type][nsKing][neKing][npawn]==0)
			addEdge(type,sKing,eKing,pawn,type,nsKing,neKing,npawn,nk);
		else
			addEdge(type,sKing,eKing,pawn,0,nsKing,neKing,0,0);
	}	
}

int mvx[8]={-1,-1,-1,0,0,1,1,1};
int mvy[8]={-1,0,1,-1,1,-1,0,1};

int exploredNum=0;

int delx[4] = {0, 1, 0, -1};
int dely[4] = {1, 0, -1, 0};

void expand(int type, int sKing, int eKing, int pawn)
{
	if (sKing/4==eKing/4) return;
	if (type && sKing/4==pawn/4) return;
	if (type && eKing/4==pawn/4) return;
	exploredNum++;
	//null move
	if (nextState[type][sKing][eKing][pawn]!=0)
	{
		if (nextState[type][sKing][eKing][pawn]<=2)
		{
			if (nextState[type][sKing][eKing][pawn]==1)
			{
				for (int k = 0; k <= 3; k++)
				{
					dp[type][sKing][eKing][pawn][k]=1;
					addToQueue(type,sKing,eKing,pawn,k);
				}
			}
		}
		else
		{
			addEdge(type,sKing,eKing,pawn,0,sKing,eKing,0,0);
		}
	}
	int sKingP=sKing/4;
	int sKingD=sKing%4;
	int sKingX=sKingP/8;
	int sKingY=sKingP%8;
	int eKingD=eKing%4;
	int eKingP=eKing/4;
	int eKingX=eKingP/8;
	int eKingY=eKingP%8;
	int pawnP=pawn/4;
	int pawnX=pawnP/8;
	int pawnY=pawnP%8;
	int pawnD=pawnP%4;
	//king move
	for (int i = 0; i <= 7; i++)
	{
		int nsKing, neKing, npawn, nk=0;
		
		int nx=sKingX+mvx[i], ny=sKingY+mvy[i];
		if (nx<0 || nx>7 || ny<0 || ny>7) continue;
		//check swap
		if (nx==eKingX && ny==eKingY)
		{
			nsKing=eKing; neKing=sKing; npawn=pawn; nk=1;
		}
		else if (type && nx==pawnX && ny==pawnY)
		{
			nsKing=pawn; neKing=eKing; npawn=sKing; nk=2;
		}
		else
		{
			nsKing=(nx*8+ny)*4+sKingD; neKing=eKing; npawn=pawn;
		}
		checkAndAdd(type,sKing,eKing,pawn,nsKing,neKing,npawn,nk);
	}
	//king rotate
	for (int i = 0; i <= 3; i++)
	{
		if (i==sKingD) continue;
		int nsKing=sKingP*4+i, neKing=eKing, npawn=pawn, nk=0;
		checkAndAdd(type,sKing,eKing,pawn,nsKing,neKing,npawn,nk);
	}
	if (type==1)
	{
		int ineyesight=0;
		if (eKingD==0 && eKingX==pawnX && eKingY<pawnY) ineyesight=1;
		if (eKingD==1 && eKingY==pawnY && eKingX<pawnX) ineyesight=1;
		if (eKingD==2 && eKingX==pawnX && eKingY>pawnY) ineyesight=1;
		if (eKingD==3 && eKingY==pawnY && eKingX>pawnX) ineyesight=1;
		if (!ineyesight)
		{
			//pawn move
			for (int i = 0; i <= 7; i++)
			{
				int nsKing, neKing, npawn, nk=0;
				
				int nx=pawnX+mvx[i], ny=pawnY+mvy[i];
				if (nx<0 || nx>7 || ny<0 || ny>7) continue;
				//check swap
				if (nx==sKingX && ny==sKingY)
				{
					nsKing=pawn; neKing=eKing; npawn=sKing; nk=2;
				}
				else if (nx==eKingX && ny==eKingY)
				{
					nsKing=sKing; neKing=pawn; npawn=eKing; nk=3;
				}
				else
				{
					nsKing=sKing; neKing=eKing; npawn=(nx*8+ny)*4+pawnD;
				}
				checkAndAdd(type,sKing,eKing,pawn,nsKing,neKing,npawn,nk);
			}
			//pawn rotate
			for (int i = 0; i <= 3; i++)
			{
				if (i==pawnD) continue;
				int nsKing=sKing, neKing=eKing, npawn=pawnD*4+i, nk=0;
				checkAndAdd(type,sKing,eKing,pawn,nsKing,neKing,npawn,nk);
			}
		}
	}
}
	
typedef enum {
  NN,
  EE,
  SS,
  WW
} king_ori_t;

typedef enum {
  NW,
  NE,
  SE,
  SW
} pawn_ori_t;


int reflect[4][4] = {
  //  NW  NE  SE  SW
  { -1, -1, 1, 3},   // NN
  { 0, -1, -1, 2},   // EE
  { 3, 1, -1, -1 },  // SS
  { -1, 0, 2, -1 }   // WW
};

int calcNextState(int type, int sKing, int eKing, int pawn)
{
	int sKingP=sKing/4;
	int sKingD=sKing%4;
	int sKingX=sKingP/8;
	int sKingY=sKingP%8;
	int eKingP=eKing/4;
	int eKingX=eKingP/8;
	int eKingY=eKingP%8;
	int pawnP=pawn/4;
	int pawnX=pawnP/8;
	int pawnY=pawnP%8;
	int pawnD=pawnP%4;
	int x=sKingX, y=sKingY;
	int d=sKingD;
	int pawnZapped=0;
	while (1)
	{
		x+=delx[d]; y+=dely[d];
		if (x<0 || x>7 || y<0 || y>7) break;
		if (x==sKingX && y==sKingY) return 2;
		if (x==eKingX && y==eKingY) return 1;
		if (type && x==pawnX && y==pawnY && !pawnZapped)
		{
			int nd=reflect[d][pawnD];
			if (nd==-1)  
				pawnZapped=1;
			else	d=nd;
		}
	}
	if (pawnZapped) return 3; else return 0;
}

void decompress(int v, int *type, int *sKing, int *eKing, int *pawn, int *k)
{
	*k=v%4;
	v/=4;
	*pawn=v%256;
	v/=256;
	*eKing=v%256;
	v/=256;
	*sKing=v%256;
	v/=256;
	*type=v;
	assert(*type<=2 && *sKing<=255 && *eKing<=255 && *pawn<=255 && *k<=3); 
}

uint8_t result[3][256][256][256];

void step1() {
	for (int i = 0; i <= 2; i++)
		for (int j = 0; j <= 255; j++)
			for (int k = 0; k <= 255; k++)
			{
				int ml;
				if (i==0) ml=0; else ml=255;
				for (int l = 0; l <= ml; l++)
					A[i][j][k][l]=new vector<int>[4];
			}
			
	for (int i = 0; i <= 2; i++)
		for (int j = 0; j <= 255; j++)
			for (int k = 0; k <= 255; k++)
			{
				int ml;
				if (i==0) ml=0; else ml=255;
				for (int l = 0; l <= ml; l++)
					nextState[i][j][k][l]=calcNextState(i,j,k,l);
			}
	fprintf(stderr,"step 1 done\n");
}


void step2() {
	for (int i = 0; i <= 2; i++)
		for (int j = 0; j <= 255; j++)
		{
			fprintf(stderr,"%d %d\n",i,j);
			for (int k = 0; k <= 255; k++)
			{
				int ml;
				if (i==0) ml=0; else ml=255;
				for (int l = 0; l <= ml; l++)
				{
					expand(i,j,k,l);
				}
			}
		}
		
	fprintf(stderr,"step 2 done exploredNum=%d\n",exploredNum);
}

void step3() {

	
	while (head<q.size())
	{
		int x=q[head]; head++;
		int type, sKing, eKing, pawn, k;
		decompress(x, &type, &sKing, &eKing, &pawn, &k);
		int res=dp[type][sKing][eKing][pawn][k];
		if (res==0)
		{
			fprintf(stderr,"error %d %d %d %d %d %d\n",type, sKing, eKing, pawn, k, res);
			exit(0);
		}
		for (typeof((A[type][sKing][eKing][pawn][k]).begin()) it=(A[type][sKing][eKing][pawn][k]).begin(); 
						it!=(A[type][sKing][eKing][pawn][k]).end(); it++)
		{
			int otype, osKing, oeKing, opawn, tmp;
			decompress(*it, &otype, &osKing, &oeKing, &opawn, &tmp);
			for (int oldk = 0; oldk <= 3; oldk ++)
				if (checkCan(otype, osKing, oeKing, opawn, oldk, type, sKing, eKing, pawn, k))
				{
					if (dp[otype][osKing][oeKing][opawn][oldk]!=0) continue;
					if (res==1)
					{
						deg[otype][osKing][oeKing][opawn][oldk]--;
						if (deg[otype][osKing][oeKing][opawn][oldk]==0)
						{
							dp[otype][osKing][oeKing][opawn][oldk]=2;
							addToQueue(otype, osKing, oeKing, opawn, oldk);
						}
					}
					if (res==2)
					{
						dp[otype][osKing][oeKing][opawn][oldk]=1;
						addToQueue(otype, osKing, oeKing, opawn, oldk);
					}
				}
		}
	}
	
	fprintf(stderr,"step 3 done q.size=%d\n",q.size());
	
}

void step4() {
	
	int cnt=0;
	for (int i = 0; i <= 2; i++)
		for (int j = 0; j <= 255; j++)
			for (int k = 0; k <= 255; k++)
			{
				int ml;
				if (i==0) ml=0; else ml=255;
				for (int l = 0; l <= ml; l++)
				{
					int last=-1, lastt=-1, abnormal=0;
					for (int t = 0; t <= 3; t++)
					{
						if (!vis[i][j][k][l][t]) continue;
						if (last==-1)
							last=dp[i][j][k][l][t], lastt=t;
						else
						{
							if (last!=dp[i][j][k][l][t])
							{
								abnormal=1;
								//printf("Abnormality found! at %d %d %d %d t = %d -> %d, t = %d -> %d\n", i,j,k,l,lastt,last,t,dp[i][j][k][l][t]);
							}	
						}
					}
					if (abnormal) 
						result[i][j][k][l]=3;
					else
					{
						if (last==-1) cnt++;
						if (last==-1) last=0;
						result[i][j][k][l]=last;
					}
				}
			}
	fprintf(stderr,"Abnormality check finished\n");
	fprintf(stderr,"True illegal state = %d\n",cnt);
	
}

void MainJob()
{
	step1();
	step2();
	step3();
	step4();

	//0: not explored (should be an illegal state)
	//1: win
	//2: lose
	//3: abnormal (previous move and KO rule determines result)
	
	
	for (int i = 0; i <= 2; i++)
	{
		int cnt1=0, cnt2=0, cnt3=0, cnt4=0;
		for (int j = 0; j <= 255; j++)
			for (int k = 0; k <= 255; k++)
			{
				int ml;
				if (i==0) ml=0; else ml=255;
				for (int l = 0; l <= ml; l++)
				{
					if (result[i][j][k][l]==0)
						cnt1++;
					if (result[i][j][k][l]==1)
						cnt2++;
					if (result[i][j][k][l]==2)
						cnt3++;
					if (result[i][j][k][l]==3)
						cnt4++;
				}
			}
		fprintf(stderr,"i=%d Illegal/Draw state = %d, Win State = %d, Lose State = %d Abnormal state = %d\n",i,cnt1,cnt2,cnt3,cnt4);
	}
	
	printf("#include <stdint.h>\n#include \"closebook.h\"\n\n");
	printf("uint64_t closebook0[%d]={\n",256*256/32);
	for (int i = 0; i <= 255; i++)
	{
		for (int j = 0; j <= 7; j++)
		{
			uint64_t t=0;
			for (int k = j*32+31; k >= j*32; k --)
				t=t*4+result[0][i][k][0];
			printf("0x%llx",t);
			if (i==255 && j==7) 
				printf("}");
			else  printf(",");
		}
		printf("\n");
	}
	printf("\n");
	
	printf("uint64_t closebook1[%d]={\n",256*256*256/32);
	for (int i = 0; i <= 255; i++)
	{
		for (int z = 0; z <= 255; z++)
		{
			for (int j = 0; j <= 7; j++)
			{
				uint64_t t=0;
				for (int k = j*32+31; k >= j*32; k --)
					t=t*4+result[1][i][z][k];
				printf("0x%llx",t);
				if (i==255 && z==255 && j==7) 
					printf("};");
				else  printf(",");
			}
			printf("\n");
		}
	}
	printf("\n");
	
	printf("uint64_t closebook2[%d]={\n",256*256*256/32);
	for (int i = 0; i <= 255; i++)
	{
		for (int z = 0; z <=255; z++)
		{
			for (int j = 0; j <= 7; j++)
			{
				uint64_t t=0;
				for (int k = j*32+31; k >= j*32; k --)
					t=t*4+result[2][i][z][k];
				printf("0x%llx",t);
				if (i==255 && z==255 && j==7) 
					printf("};");
				else  printf(",");
			}
			printf("\n");
		}
	}
}

int main()
{
	ios::sync_with_stdio(true);
	#ifndef ONLINE_JUDGE
		freopen("","r",stdin);
	#endif
	MainJob();
	return 0;
}

