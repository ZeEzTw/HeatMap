// ajuta ma sa imbunatatesc acest cod

// Team: Artura Loop
#include <iostream>
#include <cstdlib>
#include <utility>
using namespace std;

// Limits per problem statement (extended steps for larger grids)
static const int MAX_G = 16;            // allow up to 16x16 if expanded
static const int MAX_RUNES = 32;        // support more rune pairs
static const int MAX_STEPS = 256;       // generous cap for path length accumulation

int dr[4] = {1, -1, 0, 0};
int dc[4] = {0, 0, 1, -1};
char dch[4] = {'S', 'N', 'E', 'W'};

int rows, cols;
int occ[MAX_G][MAX_G] = {{-1}};         // occupancy: -1 empty, rune id for endpoints, -2 path claimed
int firstPos[MAX_RUNES] = {-1};
int secondPos[MAX_RUNES] = {-1};
int pairCount = 0;
char paths[MAX_RUNES][MAX_STEPS] = {{0}};
int pathLen[MAX_RUNES] = {0};
char bestPaths[MAX_RUNES][MAX_STEPS] = {{0}};
int bestPathLen[MAX_RUNES] = {0};
int solved = 0;

void markPath(int pairIdx, int startPos)
{
    int r = startPos / cols;
    int c = startPos % cols;
    for (int i = 0; i < pathLen[pairIdx]; i++)
    {
        char ch = paths[pairIdx][i];
        switch (ch)
        {
        case 'S': r++; break;
        case 'N': r--; break;
        case 'E': c++; break;
        case 'W': c--; break;
        }
        if (occ[r][c] == -1)
            occ[r][c] = -2;
    }
}

void unmarkPath(int pairIdx, int startPos)
{
    int r = startPos / cols;
    int c = startPos % cols;
    for (int i = 0; i < pathLen[pairIdx]; i++)
    {
        char ch = paths[pairIdx][i];
        switch (ch)
        {
        case 'S': r++; break;
        case 'N': r--; break;
        case 'E': c++; break;
        case 'W': c--; break;
        }
        if (occ[r][c] == -2)
            occ[r][c] = -1;
    }
}

// Forward declarations
void solve(int idx);
bool bfsShortest(int sr,int sc,int er,int ec, int pairIdx, char outPath[], int &outLen);
int countFreeNeighbors(int r,int c);
void orderPairs(int order[]);
bool trySimplePath(int idx);

// Depth-first search with pruning (fallback / branching search)
void dfs(int r, int c, int er, int ec, int pairIdx, int depth, int visited[MAX_G][MAX_G])
{
    if (solved)
        return;

    if (r == er && c == ec)
    {
        pathLen[pairIdx] = depth;
        markPath(pairIdx, firstPos[pairIdx]);
        solve(pairIdx + 1);
        if (!solved)
            unmarkPath(pairIdx, firstPos[pairIdx]);
        return;
    }

    int remainingDist = abs(er - r) + abs(ec - c);
    if (depth + remainingDist >= MAX_STEPS)
        return;

    // Simple heuristic: try directions that reduce Manhattan distance first
    int dirIdx[4] = {0,1,2,3};
    int bestOrder[4];
    int ordCount=0;
    int scores[4];
    for(int i=0;i<4;i++){
        int nr = r + dr[i];
        int nc = c + dc[i];
        int valid = !(nr < 0 || nr >= rows || nc < 0 || nc >= cols || visited[nr][nc]);
        if(!valid){ scores[i]=100000; continue; }
        scores[i] = abs(er - nr) + abs(ec - nc);
    }
    // naive sort 4 elements by score
    for(int i=0;i<4;i++){ bestOrder[i]=i; }
    for(int i=0;i<4;i++) for(int j=i+1;j<4;j++) if(scores[bestOrder[j]] < scores[bestOrder[i]]) std::swap(bestOrder[i], bestOrder[j]);
    for (int oi = 0; oi < 4; oi++)
    {
        int d = bestOrder[oi];
        int nr = r + dr[d];
        int nc = c + dc[d];
        if (nr < 0 || nr >= rows || nc < 0 || nc >= cols)
            continue;
        if (visited[nr][nc])
            continue;

        int isEnd = (nr == er && nc == ec);
        if (!isEnd && occ[nr][nc] != -1)
            continue;

        visited[nr][nc] = 1;
        paths[pairIdx][depth] = dch[d];

        dfs(nr, nc, er, ec, pairIdx, depth + 1, visited);

        visited[nr][nc] = 0;
    }
}

// Attempt BFS shortest path first; if unique or very tight, commit, else DFS explore
void solve(int idx)
{
    if (solved)
        return;

    if (idx == pairCount)
    {
        // CONDIȚIE: toate celulele trebuie ocupate
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                if (occ[r][c] == -1)
                    return;

        solved = 1;
        for (int i = 0; i < pairCount; i++)
        {
            bestPathLen[i] = pathLen[i];
            for (int k = 0; k < pathLen[i]; k++)
                bestPaths[i][k] = paths[i][k];
        }
        return;
    }

    if (secondPos[idx] == -1)
        return;

    int sr = firstPos[idx] / cols;
    int sc = firstPos[idx] % cols;
    int er = secondPos[idx] / cols;
    int ec = secondPos[idx] % cols;

    if (trySimplePath(idx))
        return;

    // Try BFS shortest path in current occupancy
    char candidate[MAX_STEPS];
    int candLen = 0;
    bool bfsOk = bfsShortest(sr,sc,er,ec,idx,candidate,candLen);
    if (bfsOk && candLen > 0) {
        // If candidate length is small or cell choices are forced (heuristic), use it directly
        if (candLen < 2* (abs(sr-er)+abs(sc-ec)+2)) {
            // Commit candidate
            pathLen[idx] = candLen;
            for(int i=0;i<candLen;i++) paths[idx][i]=candidate[i];
            markPath(idx, firstPos[idx]);
            solve(idx+1);
            if(!solved) unmarkPath(idx, firstPos[idx]);
            if(solved) return;
        }
    }

    int visited[MAX_G][MAX_G];
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            visited[i][j] = 0;

    visited[sr][sc] = 1;

    dfs(sr, sc, er, ec, idx, 0, visited);
}

int findIndex(int id, int runeIds[], int count)
{
    for (int i = 0; i < count; i++)
        if (runeIds[i] == id)
            return i;
    return -1;
}

// Count free (unclaimed) neighbors around position
int countFreeNeighbors(int r,int c){
    int cnt=0; for(int i=0;i<4;i++){ int nr=r+dr[i], nc=c+dc[i]; if(nr>=0&&nr<rows&&nc>=0&&nc<cols && occ[nr][nc]==-1) cnt++; }
    return cnt;
}

// Ordering pairs to reduce branching: endpoints with few exits first
void buildOrder(int order[]){
    for(int i=0;i<pairCount;i++) order[i]=i;
    // compute score: sum free neighbors at endpoints + manhattan distance
    int score[MAX_RUNES];
    for(int i=0;i<pairCount;i++){
        if(secondPos[i]==-1){ score[i]=100000; continue; }
        int sr = firstPos[i]/cols, sc= firstPos[i]%cols;
        int er = secondPos[i]/cols, ec= secondPos[i]%cols;
        score[i] = countFreeNeighbors(sr,sc) + countFreeNeighbors(er,ec)*2 + (abs(sr-er)+abs(sc-ec));
    }
    for(int i=0;i<pairCount;i++) for(int j=i+1;j<pairCount;j++) if(score[order[j]] < score[order[i]]) std::swap(order[i],order[j]);
}

static bool buildStraightCandidate(int sr,int sc,int er,int ec,char candidate[],int &len){
    if (sr == er)
    {
        int step = (ec > sc) ? 1 : -1;
        for (int c = sc + step; c != ec; c += step)
        {
            if (occ[sr][c] != -1)
                return false;
        }
        len = abs(ec - sc);
        for (int i = 0; i < len; i++)
            candidate[i] = (step == 1) ? 'E' : 'W';
        return true;
    }
    if (sc == ec)
    {
        int step = (er > sr) ? 1 : -1;
        for (int r = sr + step; r != er; r += step)
        {
            if (occ[r][sc] != -1)
                return false;
        }
        len = abs(er - sr);
        for (int i = 0; i < len; i++)
            candidate[i] = (step == 1) ? 'S' : 'N';
        return true;
    }
    return false;
}

bool trySimplePath(int idx)
{
    if (secondPos[idx] == -1)
        return false;

    int sr = firstPos[idx] / cols;
    int sc = firstPos[idx] % cols;
    int er = secondPos[idx] / cols;
    int ec = secondPos[idx] % cols;

    char candidate[MAX_STEPS];
    int candLen = 0;
    bool hasCandidate = false;

    int manhattan = abs(sr - er) + abs(sc - ec);
    if (manhattan == 1)
    {
        hasCandidate = true;
        if (er == sr + 1)
            candidate[0] = 'S';
        else if (er == sr - 1)
            candidate[0] = 'N';
        else if (ec == sc + 1)
            candidate[0] = 'E';
        else
            candidate[0] = 'W';
        candLen = 1;
    }
    else
    {
        hasCandidate = buildStraightCandidate(sr, sc, er, ec, candidate, candLen);
    }

    if (!hasCandidate || candLen <= 0)
        return false;

    pathLen[idx] = candLen;
    for (int i = 0; i < candLen; i++)
        paths[idx][i] = candidate[i];

    markPath(idx, firstPos[idx]);
    solve(idx + 1);

    if (solved)
        return true;

    unmarkPath(idx, firstPos[idx]);
    pathLen[idx] = 0;
    return false;
}

// BFS shortest path avoiding occupied cells except end
bool bfsShortest(int sr,int sc,int er,int ec, int pairIdx, char outPath[], int &outLen){
    static int vis[MAX_G][MAX_G];
    static int pr[MAX_G][MAX_G];
    static int pc[MAX_G][MAX_G];
    for(int r=0;r<rows;r++) for(int c=0;c<cols;c++){ vis[r][c]=0; pr[r][c]=-1; pc[r][c]=-1; }
    struct Node{int r,c;};
    Node q[MAX_G*MAX_G]; int qs=0,qe=0;
    q[qe++]={sr,sc}; vis[sr][sc]=1;
    bool found=false;
    while(qs<qe){ Node cur=q[qs++]; if(cur.r==er && cur.c==ec){ found=true; break; }
        for(int d=0;d<4;d++){ int nr=cur.r+dr[d], nc=cur.c+dc[d]; if(nr<0||nr>=rows||nc<0||nc>=cols) continue; if(vis[nr][nc]) continue; if(!(nr==er && nc==ec) && occ[nr][nc]!=-1) continue; vis[nr][nc]=1; pr[nr][nc]=cur.r; pc[nr][nc]=cur.c; q[qe++]={nr,nc}; }
    }
    if(!found){ outLen=0; return false; }
    // reconstruct
    int pathR[MAX_STEPS]; int pathC[MAX_STEPS]; int len=0; int cr=er, cc=ec; while(!(cr==sr && cc==sc)){ pathR[len]=cr; pathC[len]=cc; int tr=pr[cr][cc]; int tc=pc[cr][cc]; cr=tr; cc=tc; len++; if(len>=MAX_STEPS) break; }
    if(!(cr==sr && cc==sc)) { outLen=0; return false; }
    // reverse
    int rr=sr, rc=sc; outLen=0;
    for(int i=len-1;i>=0;i--){ int nr=pathR[i], nc=pathC[i]; if(nr==rr+1 && nc==rc) outPath[outLen++]='S'; else if(nr==rr-1 && nc==rc) outPath[outLen++]='N'; else if(nr==rr && nc==rc+1) outPath[outLen++]='E'; else if(nr==rr && nc==rc-1) outPath[outLen++]='W'; rr=nr; rc=nc; }
    return true;
}

int main()
{
    int n_entries;
    cin >> rows >> cols >> n_entries;

    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            occ[i][j] = -1;

    int runeIds[MAX_RUNES];

    for (int i = 0; i < n_entries; i++)
    {
        int rune, pos;
        cin >> rune >> pos;
        int idx = findIndex(rune, runeIds, pairCount);
        if (idx == -1)
        {
            idx = pairCount;
            runeIds[idx] = rune;
            firstPos[idx] = pos;
            pairCount++;
        }
        else
        {
            secondPos[idx] = pos;
        }
        int rr = pos / cols;
        int cc = pos % cols;
        occ[rr][cc] = rune;
    }

    // Build order and remap rune arrays (simple permutation apply)
    int order[MAX_RUNES]; buildOrder(order);
    // We won't physically reorder arrays (keep original output indexing), but solve uses sequential index.
    // For simplicity, proceed with original ordering first; advanced: could apply permutation.
    solve(0);

    cout << pairCount << "\n";
    for (int i = 0; i < pairCount; i++)
    {
        cout << firstPos[i] << " " << bestPathLen[i];
        for (int k = 0; k < bestPathLen[i]; k++)
            cout << " " << bestPaths[i][k];
        cout << "\n";
    }

    return 0;
}

