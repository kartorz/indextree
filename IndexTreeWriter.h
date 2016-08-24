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
//using namespace ktree;
using namespace std;

class IndexTreeWriter {
public:
    IndexTreeWriter();

    ~IndexTreeWriter();

    void setStrinxThreshold(int wordsMax, int depthMax);

    bool add(u32 *key, int keylen, void *dataPtr, int dataLen);
    bool write(const string& output, struct inxtree_header& header);

    unsigned int getTotalEntry() { return m_totalEntry; }

private:
    unsigned int m_totalEntry;
    bool m_duplicateIndexFlag;
    int m_strinxWordsMax;
    int m_strinxDepthMax;

    FILE *m_outputFile;

    FILE *m_dataTmpFile;
    FILE *m_strinxTmpFile;

    int m_totalChrindex;

    ktree::kary_tree<inxtree_chrindex> *m_indexTree;

    void addToIndextree(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent,
                            off_t d_off, u32 *keyStartPtr, u32 *keyEndPtr);

    int bsearch(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent,u32 key, int min, int max);    

    void trimIndexTree(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent, int depth, FILE* sinxfile);

    bool isInStringIndex(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent, int words, int depth);

    void writeStringIndex(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent,
                              int len_inx, int* total, off_t *start, FILE* file);

    void writeCharIndex(ktree::tree_node<inxtree_chrindex>::treeNodePtr parent, FILE* cinxfile);
};
#endif
