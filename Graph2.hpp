#include <queue>
#include <algorithm>
#include <string>
#include <list>

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
std::list<std::string> Graph<V,E>::shortestPath(const std::string start, const std::string end)
{

std::list<std::string> path;
std::unordered_map<string,string> lolo;
std::queue<string> que;
lolo[start]="VISITED";
que.push(start);

while(!que.empty())
{
  string v = que.front();
  que.pop();
  auto edges = incidentEdges(v);
  for(auto & i : edges)
  {
    const Edge & edg = i;
    string The_Present;
    if(edg.source().key() != v)
    {
      The_Present = edg.source().key();
    }
    else
    {
      The_Present = edg.dest().key();
    }
    if(lolo.end()==lolo.find(The_Present))
    {
      lolo[The_Present]=v;
      que.push(The_Present);
    }
  }
}

string s = "abcdefg";
string ending = end;
path.push_front(ending);

do
{
  string pre = lolo[ending];
  path.push_front(pre);
  ending = pre;
}
while(start!=ending);

return path;

}
