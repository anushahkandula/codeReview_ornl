#include "Graph.h"
#include "Edge.h"
#include "Vertex.h"
#include "DirectedEdge.h"

#include <string>
#include <iostream>
#include <list>

/**
* @return The number of vertices in the Graph
*/
template <class V, class E>
unsigned int Graph<V,E>::numVertices() const
{
  unsigned int result= vertexMap.size();
  return result;
}

template <class V, class E>
V & Graph<V,E>::insertVertex(std::string key) {
  V & v = *(new V(key));
  std::pair<string,V&> temp{key,v};
  vertexMap.insert({key, v});
  std::list<edgeListIter> newEdgeList;
  adjList.insert({key, newEdgeList});
  return v;
}

template <class V, class E>
const std::list<std::reference_wrapper<E>> Graph<V,E>::incidentEdges(const std::string key) const
{
std::list<std::reference_wrapper<E>> edges;
for (edgeListIter ite: adjList.at(key))
{
  edges.push_back(*ite);
}
typedef typename std::list<E_byRef>::const_iterator iterator;
for(iterator it=edgeList.begin(); it != edgeList.end(); ++it)
{
  if (!((*it).get().directed()))
  {
    return edges;
  }
  else if((*it).get().dest().key()==key)
  {
    edges.push_back((*it));
  }
}
return edges;
}

template <class V, class E>
unsigned int Graph<V,E>::degree(const V & v) const
{
  int count = adjList.at(v.key()).size();
  typedef typename std::list<E_byRef>::const_iterator iterator;
  for(iterator it=edgeList.begin(); it != edgeList.end(); ++it)
  {
    if (!((*it).get().directed()))
    {
      return count;
    }
    else if((*it).get().dest().key()==v.key())
    {
      count ++;
    }
  }
  return count;
}

/**
* Removes a given Vertex
* @param v The Vertex to remove
*/
template <class V, class E>
void Graph<V,E>::removeVertex(const std::string & key)
{
std::list<E_byRef> incidentEdgeList = incidentEdges(key);
for (E_byRef e: incidentEdgeList) removeEdge(e.get().source(), e.get().dest());
vertexMap.erase(key);
adjList.erase(key);
}

/**
* Removes an Edge from the Graph
* @param key1 The key of the source Vertex
* @param key2 The key of the destination Vertex
*/
template <class V, class E>
void Graph<V,E>::removeEdge(const string key1, const string key2)
{
	edgeListIter lola = edgeList.end();
	bool dFlag = false;
	typename std::list<edgeListIter>::iterator it = adjList.at(key1).begin();
  while(it != adjList.at(key1).end()) {
		E cur = (*(*it)).get();
    string sour =  cur.source().key();
    string des = cur.dest().key();
		if (sour == key1 && des== key2 ) {
			adjList.at(key1).erase(it);
			lola = *it;
			break;
		}
		if (!dFlag && cur.directed()) dFlag = true;
    it++;
	}
	if (!dFlag) {
	typename std::list<edgeListIter>::iterator it = adjList.at(key2).begin();
  while( it != adjList.at(key2).end()) {
			E cuur = (*(*it)).get();
      string sour =  cuur.source().key();
      string des = cuur .dest().key();
			if ( sour == key1 &&  des == key2 ) {
				adjList.at(key2).erase(it);
				lola = *it;
				break;
			}
      it++;
    }
	}
	if (lola != edgeList.end()) edgeList.erase(lola);
}

/**
* Return whether the two vertices are adjacent to one another
* @param key1 The key of the source Vertex
* @param key2 The key of the destination Vertex
* @return True if v1 is adjacent to v2, False otherwise
*/
template <class V, class E>
bool Graph<V,E>::isAdjacent(const string key1, const string key2) const
{
	for (edgeListIter lola: adjList.at(key1)) {
		E cur = (*lola).get();
    string des = cur.dest().key();
    string sour = cur.source().key();
		if (des == key2 || sour == key2) return true;
	}
	return false;
}

/**
* Inserts an Edge into the adjacency list
* @param v1 The source Vertex
* @param v2 The destination Vertex
* @return The inserted Edge
*/
template <class V, class E>
E & Graph<V,E>::insertEdge(const V & v1, const V & v2)
{
  	E & e = *(new E(v1, v2));
  	edgeList.push_front(e);
  	adjList.at(v1.key()).push_front(edgeList.begin());
  	if (!e.directed()) adjList.at(v2.key()).push_front(edgeList.begin());
  	return e;
}

/**
* Removes an Edge from the adjacency list at the location of the given iterator
* @param it An iterator at the location of the Edge that
* you would like to remove
*/
template <class V, class E>
void Graph<V,E>::removeEdge(const edgeListIter & it)
{
	V vertex1 = (*it).get().source();
	V vertex2 = (*it).get().dest();
	bool directedFlag = false;
	typename std::list<edgeListIter>::iterator iot = adjList.at(vertex1.key()).begin();
  while(it != adjList.at(vertex1.key()).end()) {
		if ( (*iot) == iot) {
			adjList.at(vertex1.key()).erase(iot);
			break;
		}
		if (!directedFlag && (*iot).get().directed()) directedFlag = true;
    iot++;
	}
	if (!directedFlag) {
	typename std::list<edgeListIter>::iterator iot = adjList.at(vertex2.key()).begin();
  while(iot != adjList.at(vertex2.key()).end()) {
			if ( (*iot) == iot) {
				adjList.at(vertex2.key()).erase(iot);
				break;
			}
      iot++;
		}
	}
	edgeList.erase(iot);
}
