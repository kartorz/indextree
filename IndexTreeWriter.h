/**
 *	@Copyright (c) 2016 joni <joni.kartorz.lee@gmail.com>
 *
 * Distributed under the GNU GENERAL PUBLIC LICENSE, version 3 (GPLv3)
 * (See accompanying file LICENSE.txt or copy at
 * http://www.gnu.org/licenses/gpl.txt)
 *
 */

#ifndef _INDEXTREEWRITER_H_
#define _INDEXTREEWRITER_H_

#include <unistd.h>

#include "kary_tree/kary_tree.hpp"
#include "IndexTreeHelper.h"
#include "IndexTree.h"
//using namespace ktree;
using namespace std;

class IndexTreeWriter : public IndexTree {
public:
    IndexTreeWriter(bool online = false);

    ~IndexTreeWriter();

    void setStrinxThreshold(int wordsMax, int depthMax);
    void open();
    bool load(const string& inxFilePath, int magic, bool online = false);
    bool add(u32 *key, int keylen, void *dataPtr, int dataLen, bool bVariedLength=true);
    bool write(string output="");

    virtual struct inxtree_dataitem dataitem(address_t loc);


    unsigned int getTotalEntry() { return m_totalEntry; }

private:
    unsigned int m_totalEntry;
    bool m_duplicateIndexFlag;
    bool m_bOnLineRW;

    int m_strinxWordsMax;
    int m_strinxDepthMax;
    string m_inxFilePath;
    FILE *m_outputFile;

    FILE *m_dataTmpFile;
    FILE *m_strinxTmpFile;

    int m_inxDataLen;
    int m_totalChrindex;

    void addToIndextree(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent,
                        const off_t d_off, u32 *keyStartPtr, u32 *keyEndPtr);

    int bsearch(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent,u32 key, int min, int max);    

    void trimIndexTree(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent, int depth, FILE* sinxfile);

    bool isInStringIndex(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent, int words, int depth);

    void writeStringIndex(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent,
                              int len_inx, int* total, off_t *start, FILE* file);

    void writeCharIndex(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent, FILE* cinxfile);
};
#endif
