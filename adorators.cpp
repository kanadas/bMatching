#include "blimit.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <set>
#include <map>
#include <tuple>
#include <sstream>
#include <thread>
#include <mutex>
#include <future>
#include <algorithm>
#include <cassert>
#include <condition_variable>

typedef std::pair<int, int> edge_t;

const edge_t NULLEDGE = edge_t(-1, -1);
const edge_t INFEDGE = edge_t(1e9 + 9, 1e9 + 9);
const int NUM_COPY = 5;
const int BREAK_MAIN = 0;

std::map<int, int> in_map;
std::vector<int> out_map;

struct edge_compare 
{
	bool operator ()(edge_t a, edge_t b) 
	{
		if(a.second == b.second) return out_map[a.first] > out_map[b.first];
		return a.second > b.second;
	}
};

std::vector<std::priority_queue<edge_t, std::vector<edge_t >, edge_compare> > S;
std::vector<std::set<int> > T;
std::vector<std::vector<edge_t > > v;
std::vector<std::unique_ptr<std::mutex> > mut;
std::mutex Smut, Qmut, Tmut, ochrona;
std::condition_variable empty_queue, finish;
int working = 0;



void new_verticle(int num, int in_num) 
{
	in_map[in_num] = num;
	out_map.push_back(in_num);
	v.push_back(std::vector<edge_t >());
	S.push_back(std::priority_queue<edge_t, std::vector<edge_t >, edge_compare>());
	T.push_back(std::set<int>());
	mut.push_back(std::make_unique<std::mutex>());
}

edge_t last(int b_method, int x) 
{
	std::lock_guard<std::mutex> lock(Smut);
	//	std::cout << "Last: S(" << x << ") size = " << S[x].size() << " b = " << bvalue(b_method, out_map[x]) << std::endl;
	if(bvalue(b_method, out_map[x]) == 0) return INFEDGE;
	if(S[x].size() == bvalue(b_method, out_map[x])) return S[x].top();
	return NULLEDGE;
}

void insert(int b_method, int u, edge_t edge)
{
	assert(bvalue(b_method, out_map[u]) > 0);
	std::lock_guard<std::mutex> lock(Smut);
	if(S[u].size() == bvalue(b_method, out_map[u])) S[u].pop();
	S[u].push(edge);
	//std::cout << "Insert: S(" << u << ") size = " << S[u].size()  << " b = " << bvalue(b_method, out_map[u]) << std::endl;
}

edge_t find_edge(int b_method, int u, std::set<int>& tempT)
{
	edge_compare cmp;
	edge_t x = NULLEDGE; 
	for(auto it = v[u].begin(); it != v[u].end(); ++it)
		if(cmp(*it, x) && cmp(edge_t(u, it->second), last(b_method, it->first)) && tempT.count(it->first) == 0) x = *it;
	return x;
}

int suitor(std::vector<int>& Q, std::vector<int>& q, const std::vector<int>& b, std::vector<int>& db, int b_method)
{
	std::set<int> tempT{};
	std::vector<std::pair<int, int>> inserted{};
	std::vector<std::pair<int, int>> todelete{};
	int i;
	int res = 0; 
	
	for(auto it = Q.begin(); it != Q.end(); ++it)
	{
		//TODO: wypróbować bez tego
		Tmut.lock();
		tempT = T[*it];
		Tmut.unlock();

		i = 0;
		while(i < b[*it])
		{
			edge_t p = find_edge(b_method, *it, tempT);
			if(p != NULLEDGE)
			{
				std::lock_guard<std::mutex> lock(*mut[p.first]); 
				if(find_edge(b_method, *it, tempT) == p)
				{
					++i;
					edge_t y = last(b_method, p.first);
					insert(b_method, p.first, std::make_pair(*it, p.second));
					tempT.insert(p.first);
					inserted.push_back(std::make_pair(*it, p.first));
					res += p.second;

					if(y != NULLEDGE)
					{
						assert(y.second <= p.second);
						//T[y.first].erase(p.first);
						todelete.push_back(std::make_pair(y.first, p.first));
						//q.push_back(y.first);
						//++db[y.first];
						res -= y.second;
					}
				}
			}
			else break;
		}
	}
	
	ochrona.lock();
	Tmut.lock();
	for(auto it = inserted.begin(); it != inserted.end(); ++it) T[it->first].insert(it->second);
	for(auto it = todelete.begin(); it != todelete.end(); ++it)
	{
		T[it->first].erase(it->second);
		q.push_back(it->first);
		++db[it->first];
	}
	Tmut.unlock();	
	ochrona.unlock();
	return res;
}

int main(int argc, char* argv[]) 
{
   if (argc != 4) 
	{
      std::cerr << "usage: "<<argv[0]<<" thread-count inputfile b-limit"<< std::endl;
      return 1;
   }

   int thread_count = std::stoi(argv[1]);
   int b_limit = std::stoi(argv[3]);
   std::string input_filename{argv[2]};
	std::ifstream infile;
	infile.open(input_filename);
	int n_verticles = 0;
	std::string line;
	std::istringstream iss; 
	while(std::getline(infile, line)) 
	{
		int a,b,w;
		if(line[0] != '#')
		{ 
			iss = std::istringstream(line);
			if(iss >> a >> b >> w) 
			{
				if(in_map.count(a) == 0) new_verticle(n_verticles++, a);
				if(in_map.count(b) == 0) new_verticle(n_verticles++, b);
				v[in_map[a]].push_back(std::make_pair(in_map[b], w));
				v[in_map[b]].push_back(std::make_pair(in_map[a], w));
			}
			else
			{
				std::cerr << "Incorrect input. Error in: " << line << std::endl;
				return 0;
			}
		}
	}

	for (int b_method = 0; b_method < b_limit + 1; b_method++) 
	{
		std::vector<std::thread> threads;
		std::vector<int> b,nb;
		std::vector<int> Q,q;
		int res = 0;
		
		for(int i = 0; i < n_verticles; ++i) 
		{
			b.push_back(bvalue(b_method, out_map[i]));
			nb.push_back(0);
			Q.push_back(i);
		}

		for(int i = 1; i < thread_count; ++i)
			threads.push_back(std::thread([&res, &Q, &q, &b, &nb, b_method]
			{
				std::vector<int> toprocess{};
				while(true)
				{
					std::unique_lock<std::mutex> lk(Qmut);
					empty_queue.wait(lk, [Q]{return !Q.empty();});
					for(int i = 0; i < NUM_COPY; ++i)
					{
						toprocess.push_back(Q.back());
						Q.pop_back();
						if(Q.empty()) break;
					}
					lk.unlock();
					++working;
					res += suitor(toprocess, q, b, nb, b_method);
					--working;
				}
			}));

		while(!Q.empty())
		{
			std::cout << Q.size() << std::endl;
			std::vector<int> toprocess{};
			//TODO: nie jestem pewien czy to przyspieszy
			while(!Q.empty())
			{
				std::unique_lock<std::mutex> lk(Qmut);
				if(Q.empty()) break;
				for(int i = 0; i < NUM_COPY; ++i)
				{
					toprocess.push_back(Q.back());
					Q.pop_back();
					if(Q.empty()) break;
				}
				lk.unlock();
				res += suitor(toprocess, q, b, nb, b_method);
			}
			std::unique_lock<std::mutex> lock(ochrona);
			finish.wait(lock, []{return working == 0;});
			//TODO: Nie jestem pewien czy to działa
			for(int i = 0; i < n_verticles; ++i)
			{
				b[i] = nb[i];
				nb[i] = 0;
			}
			std::sort(q.begin(), q.end());
			if(!q.empty()) Q.push_back(q.front());
			for(size_t i = 1; i < q.size(); ++i)
				if(q[i] != q[i - 1]) Q.push_back(q[i]);
			q.clear();
		}
		std::cout << res/2 << std::endl;
		res = 0;
		for(int i = 0; i < n_verticles; ++i)
		{
			S[i] = std::priority_queue<edge_t, std::vector<edge_t >, edge_compare>();
			T[i].clear();
		}
   }
	return 0;
}
