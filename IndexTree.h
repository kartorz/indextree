/** 
 *	@Copyright (c) 2016 joni <joni.kartorz.lee@gmail.com>
 *
 * Distributed under the GNU GENERAL PUBLIC LICENSE, version 3 (GPLv3)
 * (See accompanying file LICENSE.txt or copy at
 * http://www.gnu.org/licenses/gpl.txt)
 *
 */

#ifndef _INDEXTREE_H_
#define _INDEXTREE_H_

#include <stdlib.h>
#include <stdio.h>

#include <map>
#include <string>

#include "kary_tree/kary_tree.hpp"
#include "indextree_inner.h"
#include "IndexTreeHelper.h"

using namespace ktree;
using namespace std;

class iIndexItem {
public:
    iIndexItem():opaque(NULL),data_i(0),data_s(""),data_f(0.0)
	{
        index = "";  // fixed: macro "index" requires 2 arguments
	}
    ~iIndexItem()
    {
        if (opaque)
		    free(opaque);
	}
	string index; /* utf-8 bytes */
    inxtree_dataitem d;

    // user data.
    int    data_i;
    string data_s;
    double data_f;
	void *opaque;
};

typedef vector<iIndexItem*> IndexList;

class IndexTree {
public:
    IndexTree();
    virtual ~IndexTree();

    virtual bool load(FILE *inxFile, int magic);
    virtual struct inxtree_dataitem dataitem(address_t loc);

    bool load(const string& inxFilePath, int magic, bool r=true);
    bool lookup(const string& word, vector<inxtree_dataitem>& items);
    bool lookup(const string& word, vector<inxtree_dataitem>& items, int candidateNum, IndexList& candidate);

    // 'start' from 0.
    // 'end' specified -1 meas all items.
    int getIndexList(IndexList& indexList, string startwith="", int start=0, int len=-1);
    int validLen(string  key);

    struct inxtree_header m_header;

protected:
    struct IndexStat {
        int start;
        int end;
        int number;
    };

    struct LookupStat {
        string advance;
        tree_node<inxtree_chrindex>::treeNodePtr currentNode;
        vector<address_t> locs;
    };

    bool loadIndexTree();

    void loadIndexTree(tree_node<inxtree_chrindex>::treeNodePtr parent,
                  void *chrblock, address_t blksize);

    bool lookup(char *strkey, tree_node<inxtree_chrindex>::treeNodePtr parent, struct LookupStat& lookupStat);

    bool lookup(char* strkey, address_t off, int len, struct LookupStat& lookupStat);

    void lookupCandidate(tree_node<inxtree_chrindex>::treeNodePtr parent,
                         string& header, int candidateNum, IndexList& candidate);

    bool loadIndex(u4char_t *str, int inx, struct IndexStat *stat,
                   tree_node<inxtree_chrindex>::treeNodePtr parent,
                   IndexList& indexList);

    bool loadIndex(string startwith,
                   struct inxtree_chrindex& chrInx,
                   struct IndexStat *stat,
                   IndexList& indexList);

    struct inxtree_dataitem  dataitem(FILE *datafile, off_t off);

    tree_node<inxtree_chrindex>::treeNodePtr
    findTreeNode(char *strkey, tree_node<inxtree_chrindex>::treeNodePtr parent, int* remain);

    int bsearch(tree_node<inxtree_chrindex>::treeNodePtr parent,
                u4char_t key, int min, int max);

    void* getBlock(int blk);

    FILE *m_inxFile;
    kary_tree<inxtree_chrindex> *m_indexTree;
    address_t m_chrIndexLoc;
    address_t m_strIndexLoc;
    address_t m_dataLoc;
    IndexList m_indexList;
    int m_indexStart;
    int m_indexEnd;
    int m_indexNumber;
    map<int, void*> m_blkCache;
    indextree::MutexCriticalSection m_cs;
};

#endif
