#include <queue>
#include <algorithm>
#include <string>
#include <list>

using namespace std;
/**
 * Returns an std::list of vertex keys that creates any shortest path between `start` and `end`.
 *
 * This list MUST include the key of the `start` vertex as the first vertex in the list, the key of
 * the `end` vertex as the last element in the list, and an ordered list of all vertices that must
 * be traveled along the shortest path.
 *
 * For example, the path a -> c -> e returns a list with three elements: "a", "c", "e".
 *
 * You should use undirected edges. Hint: There are no edge weights in the Graph.
 *
 * @param start The key for the starting vertex.
 * @param end   The key for the ending vertex.
 */
template <class V, class E>
// std::list<std::string> Graph<V,E>::shortestPath(const std::string start, const std::string end)
// {
//
// std::list<std::string> path;
// std::unordered_map<string,string> lolo;
// std::queue<string> que;
// lolo[start]="VISITED";
// que.push(start);
//
// while(!que.empty())
// {
//   string v = que.front();
//   que.pop();
//   auto edges = incidentEdges(v);
//   for(auto & i : edges)
//   {
//     const Edge & edg = i;
//     string The_Present;
//     if(edg.source().key() != v)
//     {
//       The_Present = edg.source().key();
//     }
//     else
//     {
//       The_Present = edg.dest().key();
//     }
//     if(lolo.end()==lolo.find(The_Present))
//     {
//       lolo[The_Present]=v;
//       que.push(The_Present);
//     }
//   }
// }
//
// string s = "abcdefg";
// string ending = end;
// path.push_front(ending);
//
// do
// {
//   string pre = lolo[ending];
//   path.push_front(pre);
//   ending = pre;
// }
// while(start!=ending);
//
// return path;
//
// }

std::list<std::string> Graph<V,E>::shortestPath(const std::string start, const std::string end)
{
unordered_map<string, string> predecessor;
unordered_map<string, int> distances;
for (pair<string, V &> elem: vertexMap)
{
	predecessor.insert(pair<string, string>(elem.first, ""));
	distances.insert(pair<string, int>(elem.first, INT_MAX));
}
queue<string> q;
q.push(start);
distances[start] = 0;
while (!q.empty()) {
string cur = q.front();
q.pop();
for (E_byRef ebr: incidentEdges(cur))
{
	string cur_next = ebr.get().dest().key() == cur ? ebr.get().source().key() : ebr.get().dest().key();
	if (distances[cur_next] > (distances[cur] + 1))
  {
		q.push(cur_next);
		predecessor[cur_next] = cur;
		distances[cur_next] = distances[cur] + 1;
	}
}
}
list<string> path;
string cur = end;
while (cur != "")
{
	path.push_front(cur);
	cur = predecessor[cur];
}
return path;
}
