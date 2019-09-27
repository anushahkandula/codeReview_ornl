# search.py
# ---------------
# Licensing Information:  You are free to use or extend this projects for
# educational purposes provided that (1) you do not distribute or publish
# solutions, (2) you retain this notice, and (3) you provide clear
# attribution to the University of Illinois at Urbana-Champaign
#
# Created by Michael Abir (abir2@illinois.edu) on 08/28/2018

"""
This is the main entry point for MP1. You should only modify code
within this file -- the unrevised staff files will be used for all other
files and classes when code is run, so be careful to not modify anything else.
"""
# Search should return the path.
# The path should be a list of tuples in the form (row, col) that correspond
# to the positions of the path taken by your search algorithm.
# maze is a Maze object based on the maze from the file specified by input filename
# searchMethod is the search method specified by --method flag (bfs,dfs,astar,astar_multi,extra)

from collections import defaultdict
import copy

def search(maze, searchMethod):
    return {
        "bfs": bfs,
        "dfs": dfs,
        "astar": astar,
        "astar_multi": astar_multi,
        "extra": extra,
    }.get(searchMethod)(maze)


def bfs(maze):
    """
    Runs BFS for part 1 of the assignment.

    @param maze: The maze to execute the search on.

    @return path: a list of tuples containing the coordinates of each state in the computed path
    """
    # TODO: Write your code here
    start= maze.getStart()
    goal = maze.getObjectives()[0]
    queue = [start]
    path = {start: None}
    visited = set([start])
    while (len(queue) != 0):
        front = queue.pop(0)
        if front == goal:
            p = []
            temp = goal
            while (temp != None):
                p.insert(0, temp)
                temp = path[temp]
            return p
        r, c = front
        for neighbor in maze.getNeighbors(r, c):
            if neighbor not in visited:
                queue.append(neighbor)
                path[neighbor] = front
                visited.add(neighbor)
    return []

def dfs(maze):
    """
    Runs DFS for part 1 of the assignment.

    @param maze: The maze to execute the search on.

    @return path: a list of tuples containing the coordinates of each state in the computed path
    """
    # TODO: Write your code here
    start= maze.getStart()
    goal = maze.getObjectives()[0]
    stack = [start]
    path = {start: None}
    visited = set([start])
    while(len(stack)>0):
        front = stack.pop(len(stack)-1)
        if front == goal:
            p = []
            temp = goal
            while (temp != None):
                p.insert(0, temp)
                temp = path[temp]
            return p
        r, c = front
        for neighbor in maze.getNeighbors(r, c):
            if neighbor not in visited:
                stack.append(neighbor)
                path[neighbor] = front
                visited.add(neighbor)
    return []

def reconstruct_path(cameFrom, current):
    totalPath = [current]
    while current in cameFrom.keys():
        current = cameFrom[current]
        totalPath.insert(0,current)
    return totalPath

def astar(maze):
    """
    Runs A star for part 1 of the assignment.

    @param maze: The maze to execute the search on.

    @return path: a list of tuples containing the coordinates of each state in the computed path
    """
    # TODO: Write your code here
    start= maze.getStart()
    goal = maze.getObjectives()[0]
    openSet = set()
    openSet.add(start)
    closedSet = set()
    cameF = {}
    gScore = defaultdict(lambda: float('inf'))
    gScore[start] = 0
    fScore = defaultdict(lambda: float('inf'))
    r,c=start
    r1,c1=goal
    fScore[start] = (abs(r - r1) + abs(c-c1))
    while (len(openSet)>0):
        min = float('inf')
        current = start
        for i in openSet:
            if fScore[i]<min:
                min=fScore[i]
                current=i
        openSet.remove(current)
        closedSet.add(current)
        if (current == goal):
            return reconstruct_path(cameF, current)
        (x,y)=current
        neighbors=maze.getNeighbors(x,y)
        for y in neighbors:
            if y in closedSet:
                continue
            tentative_gScore = gScore[current] + 1
            if tentative_gScore < gScore[y]:
                cameF[y] = current
                gScore[y] = tentative_gScore
                (a,b)=y
                fScore[y] = gScore[y] + (abs(a - r1) + abs(b-c1))
                if y not in openSet:
                    openSet.add(y)
    return []

 def extra(maze):
    """
    Runs extra credit suggestion.

    @param maze: The maze to execute the search on.

    @return path: a list of tuples containing the coordinates of each state in the computed path
    """
    # TODO: Write your code here

    start= maze.getStart()
    r1,c1=start
    goals = maze.getObjectives()
    current = (r1, c1)
    distances = []
    for i in goals :
        (a, b) = i
        min_dist = (abs(a - r1) + abs(b-c1))
        distances.append(min_dist)
    minval = min(list)
    astar_with_startend(maze, current, i)

    return []

def astar_with_startend(maze, start, goal):
    """
    Runs A star for part 1 of the assignment.

    @param maze: The maze to execute the search on.

    @return path: a list of tuples containing the coordinates of each state in the computed path
    """
    # TODO: Write your code here
    openSet = set()
    openSet.add(start)
    closedSet = set()
    cameF = {}
    gScore = defaultdict(lambda: float('inf'))
    gScore[start] = 0
    fScore = defaultdict(lambda: float('inf'))
    r,c=start
    r1,c1=goal
    fScore[start] = (abs(r - r1) + abs(c-c1))
    while (len(openSet)>0):
        min = float('inf')
        current = start
        for i in openSet:
            if fScore[i]<min:
                min=fScore[i]
                current=i
        openSet.remove(current)
        closedSet.add(current)
        if (current == goal):
            return reconstruct_path(cameF, current)
        (x,y)=current
        neighbors=maze.getNeighbors(x,y)
        for y in neighbors:
            if y in closedSet:
                continue
            tentative_gScore = gScore[current] + 1
            if tentative_gScore < gScore[y]:
                cameF[y] = current
                gScore[y] = tentative_gScore
                (a,b)=y
                fScore[y] = gScore[y] + (abs(a - r1) + abs(b-c1))
                if y not in openSet:
                    openSet.add(y)
    return []






