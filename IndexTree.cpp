/**
 *	@Copyright (c) 2016 joni <joni.kartorz.lee@gmail.com>
 *
 * Distributed under the GNU GENERAL PUBLIC LICENSE, version 3 (GPLv3)
 * (See accompanying file LICENSE.txt or copy at
 * http://www.gnu.org/licenses/gpl.txt)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>

#include "indextree_inner.h"
#include "IndexTreeHelper.h"
#include "IndexTree.h"

#define CACHE_SLICE_MAX  400  /* 100k */

#define MEM_CHARINX_MAX  20*1024*1024 /*M*/

#define INDEXARRY_LEN_MAX  256

IndexTree::IndexTree():
m_chrIndexLoc(INXTREE_INVALID_ADDR),
m_strIndexLoc(INXTREE_INVALID_ADDR),
m_dataLoc(INXTREE_INVALID_ADDR),
m_indexTree(NULL)
{
}

bool IndexTree::load(const string& inxFilePath, int magic)
{
    FILE *F = fopen(inxFilePath.c_str(),"rb");
    return load(F, magic);
}


bool IndexTree::load(FILE *inxFile, int magic)
{
	if (inxFile == NULL) {
		return false;
	}

    m_inxFile = inxFile;
	indextree_helper::ReadFile read;
	size_t size = read(m_inxFile, &m_header, sizeof(struct inxtree_header));
    if (size < sizeof(struct inxtree_header))
        return false;

    if (inxtree_read_u16(m_header.magic) != magic)
        return false;

    m_chrIndexLoc = m_header.loc_chrindex[0];
    m_strIndexLoc = inxtree_read_u32(m_header.loc_strindex);
    m_dataLoc = inxtree_read_u32(m_header.loc_data);
    return loadIndexTree();
}

bool IndexTree::loadIndexTree()
{
    if (m_chrIndexLoc != INXTREE_INVALID_ADDR) {
        indextree_helper::ReadFile read;
        indextree_helper::Malloc maclloc_t;
        address_t len = (m_strIndexLoc - m_chrIndexLoc)*INXTREE_BLOCK;
        void* chrblock = maclloc_t(len);

        fseek(m_inxFile, (m_chrIndexLoc-1)*INXTREE_BLOCK, SEEK_SET);
        read(m_inxFile, chrblock, len);

        struct inxtree_chrindex rootIndex;
        rootIndex = *((struct inxtree_chrindex*)chrblock);
        m_indexTree = new kary_tree2<inxtree_chrindex>(rootIndex);
        if (true/*len <= MEM_CHARINX_MAX*/) {
            loadIndexTree(m_indexTree->root(), chrblock, len);
        } else {
            // Todo: Loading a part of index character.
        }
        return true;
    }

    return false;
}

void IndexTree::loadIndexTree(tree_node<inxtree_chrindex>::treeNodePtr parent,
                              void *chrblock, address_t blksize)
{
	struct inxtree_chrindex& parInx = parent->value();
	address_t loc = inxtree_read_u32(parInx.location);
    u16 len = inxtree_read_u16(parInx.len_content);

    if (len > 5000) {
        printf("w:{loadIndexTree} more than 5000 child need to be loaded, someting wrong?\n");
    }

	//g_sysLog.d("{loadIndexTree} parent loc: (%u-->0x%x), len:(%d)\n", loc, loc, len);
	if ((loc & 0x80000000) == 0 && len > 0) { /* non-leaf */
        if (loc + (len-1) * sizeof(struct inxtree_chrindex) > blksize) {
            printf("e:{loadIndexTree} (loc(%u) --> len(%u) )over char index aread\n", loc, len);
            return;
        }

	    for (u32 i=0; i<len; i++) {
		    struct inxtree_chrindex chrInx;
            address_t off = loc + i*sizeof(struct inxtree_chrindex);
			memcpy(&chrInx,(u8 *)chrblock + off,sizeof(struct inxtree_chrindex));
			parent->insert(chrInx);
            // Recursion
			loadIndexTree((*parent)[i], chrblock, blksize);
		}
	}
}

bool IndexTree::lookup(const string& word, vector<inxtree_dataitem>& items)
{
    struct LookupStat lookupStat;
    lookupStat.advance = word;

    if (lookup((char*)word.c_str(), m_indexTree->root(), lookupStat)) {
        for (int i=0; i<lookupStat.locs.size(); i++) {
            struct inxtree_dataitem item = dataitem(lookupStat.locs[i]);
            items.push_back(item);
        }
        return true;
    }
    return false;
}

bool IndexTree::lookup(const string& word, vector<inxtree_dataitem>& items, int candidateNum, IndexList& candidate)
{
    struct LookupStat lookupStat;
    lookupStat.advance = word;

    if (lookup((char*)word.c_str(), m_indexTree->root(), lookupStat)) {
        for (int i=0; i<lookupStat.locs.size(); i++) {
            struct inxtree_dataitem item = dataitem(lookupStat.locs[i]);
            items.push_back(item);
        }
        return true;
    }

    if (candidateNum > 0)
        lookupCandidate(lookupStat.currentNode, lookupStat.advance, candidateNum, candidate);

    return false;
}

/* Look up in char index tree,
 * If we can't find the 'target', return a candidate string.
 */
bool IndexTree::lookup(char *strkey, tree_node<inxtree_chrindex>::treeNodePtr parent, struct LookupStat& lookupStat)
{
    int remain = strlen(strkey); /* for lookup candidate, mbrtowc_r will remove some chars */
    /* 'strkey' will be modified, some chars will be removed */
    const u4char_t key = IndexTreeHelper::utf8byteToUCS4Char((const char**)&strkey);
    //printf("lookup, %lc\n", key);
    int cid = bsearch(parent, key, 0, parent->children().size()-1);
	if (cid != -1) {
	    if (strlen(strkey) > 0) {
	        if (parent->child(cid)->children().size() > 0) {
                    return lookup(strkey, (*parent)[cid], lookupStat);
		} else {
            struct inxtree_chrindex chrInx = parent->child(cid)->value();
		    address_t loc = inxtree_read_u32(chrInx.location);
		    int len = inxtree_read_u16(chrInx.len_content);
		    if ((loc & F_LOCSTRINX) == F_LOCSTRINX) {
	                if (lookup(strkey, loc & (~F_LOCSTRINX), len, lookupStat))
                            return true;

                        int len = lookupStat.advance.length() - strlen(strkey); /* it is not the same as 'remain' */
                        lookupStat.advance = lookupStat.advance.substr(0, len);
                        lookupStat.currentNode = (*parent)[cid];
                        return false;
		    } else /* leaf node without a string index */  {
                        int len = lookupStat.advance.length() - strlen(strkey);
                        lookupStat.advance = lookupStat.advance.substr(0, len);
                        lookupStat.currentNode = NULL;
                        return false;
                    }
	        }
	} else { /* advance to the end of strkey */
            int csize = parent->child(cid)->children().size();
		    if (csize > 0) {
                /* Maybe, there are same key with different val.*/
                for (int i=0; i<csize; i++) {
			        struct inxtree_chrindex chrInx = parent->child(cid)->child(i)->value();
				    if (inxtree_read_u32(chrInx.wchr) == 0) {
				        lookupStat.locs.push_back(inxtree_read_u32(chrInx.location));
                    } else {
                        break;
                    }
                }

				if (lookupStat.locs.size() == 0) {
                    lookupStat.currentNode = (*parent)[cid];
                    return false;
			    }
                return true;
			} else {
                struct inxtree_chrindex chrInx = parent->child(cid)->value();
				address_t loc = inxtree_read_u32(chrInx.location);
			    if ((loc & F_LOCSTRINX) == 0) {
				    lookupStat.locs.push_back(loc);
                    return true;
				} else {
                    lookupStat.currentNode = (*parent)[cid];
				    return false;
                }
			}
		}
	}

    int total = lookupStat.advance.length();
    lookupStat.advance = lookupStat.advance.substr(0, total-remain); // get the common string.
    lookupStat.currentNode = parent;
	return false;
}

/* Look up in string index area */
bool IndexTree::lookup(char* strkey, address_t off, int len, struct LookupStat& lookupStat)
{
    //printf("lookup2, %s\n", strkey);
    bool result = false;
	int block_nr = off/INXTREE_BLOCK + m_strIndexLoc;
	off = off%INXTREE_BLOCK;
    u8 *buf = (u8 *)getBlock(block_nr) + off; /* Load index has checked if NULL. */

	int len_key = strlen(strkey);
	for (int nr = 0; nr < len; nr++) {
		struct inxtree_strindex *pStrInx = ( struct inxtree_strindex *) buf;
		if (pStrInx->len_str[0] == 0) {
			// Read next block
			buf = (u8 *)getBlock(++block_nr);
			pStrInx = ( struct inxtree_strindex *) buf;
		}

		if (len_key == pStrInx->len_str[0]) {
		    char *strinx = (char *)(pStrInx->keystr);
			bool found = true;
			// compasion from tail to head.
		    for (int i=len_key; i>0; i--) {
				if (strinx[i-1] != strkey[i-1]) {
				    found = false;
					break;
				}
			}
			// found.
			if (found == true) {
				lookupStat.locs.push_back(inxtree_read_u32(pStrInx->location));
                result = true;
                //printf("lookup multi-result %u\n", inxtree_read_u32(pStrInx->location));
			} else if (result){
                /* the dumplicate indexes saved together */
                //printf("lookup same result done\n");
                return true;
            }
	    }
		buf += 5 + pStrInx->len_str[0];
	}
	return result;
}

void IndexTree::lookupCandidate(tree_node<inxtree_chrindex>::treeNodePtr parent,
                                string& header, int candidateNum, IndexList& candidate)
{
    if (parent != NULL) {
        u4char_t index[INDEXARRY_LEN_MAX];
    	memset(index, L'\0', INDEXARRY_LEN_MAX);
        struct IndexStat stat;
        stat.start = 0;
        stat.end = candidateNum;
        stat.number = 0; /* start from 0 */
        //IndexList indexList;
        //string header = candidate;
        //candidate.clear();
        loadIndex(index, 0, &stat, parent, candidate);
        for (int i=0; i<candidate.size(); i++) {
            candidate[i]->index = header + candidate[i]->index;
            //candidate = candidate + index + '\n';
            //addToInxCache(index, candidate[i]->addr);
        }
    }
}


/*
 * [indexList]  return back.
 * [startwith]  prefix of index.
 * [start, end] -1 means all.
 */
int IndexTree::getIndexList(IndexList& indexList, string startwith, int start, int len)
{
    if (!m_indexTree)
        return 0;

    u4char_t index[INDEXARRY_LEN_MAX];
    memset(index, L'\0', INDEXARRY_LEN_MAX);
    int index_star = 0;
    struct IndexStat stat;
    stat.start = start;
    stat.end = len == -1 ? -1 : start + len;
    stat.number = 0; /* start from 0 */

    tree_node<inxtree_chrindex>::treeNodePtr root = m_indexTree->root();
    if (startwith != "") {
        string key = startwith;
        int remain;
        root = findTreeNode((char*)key.c_str(), root, &remain);
        string prefix = startwith.substr(0, startwith.length() - remain);

        if (remain > 0) {
            int num = 0;
            struct inxtree_chrindex chrInx = root->children().size() == 0 ? root->value() : root->child(0)->value();
            IndexList strIndexList;
            loadIndex(prefix, chrInx, &stat, strIndexList);
            for (int i = 0; i < strIndexList.size(); i++) {
                string index = strIndexList[i]->index;
                if (index.length() >= key.length() && index.substr(0, key.length()) == key) {
                    num++;
                    indexList.push_back(strIndexList[i]);
                }
            }
            return num;
        }

        //printf("%s, %s, %d\n", startwith.c_str(), prefix.c_str(), remain);
        char* pkey = (char *)(prefix.c_str());
        size_t u4len;
        u4char_t* u4str = IndexTreeHelper::utf8StrToUcs4Str(pkey, &u4len);
        if (u4len > 0) {
            memcpy(index, u4str, sizeof(u4char_t)*u4len);
            index_star = u4len;
        }
        free((void *)u4str);
    }

    loadIndex(index, index_star, &stat, root, indexList);
    return stat.number - stat.start; /* indexListSize use this feature.*/
}

/* @return : false -> abort */
bool IndexTree::loadIndex(u4char_t *str, int inx, struct IndexStat *stat,
                               tree_node<inxtree_chrindex>::treeNodePtr parent,
                               IndexList& indexList)
{
    int children_size = parent->children().size();
	if (children_size > 0) {
	    for (int i=0; i<children_size; i++) {
	        struct inxtree_chrindex chrInx = parent->child(i)->value();
            if (inx < INDEXARRY_LEN_MAX -1) {
                bool ret;
                if (chrInx.wchr == 0) { //being 0, is a special zero-node for 'index being a word'
                    ret = loadIndex(str, inx, stat, parent->child(i), indexList);
                } else {
                    str[inx] = inxtree_read_u32(chrInx.wchr);
                    ret = loadIndex(str, inx+1, stat, parent->child(i), indexList);
                }
                if (ret == false)
                    return false;
            } else {
                printf("d: {loadIndex} index is too long, should't happen\n");
                return false;
            }
        }
        return true;
	}

    // Leaf node
    string strparent;
    {
        str[inx] = L'\0';
        char* pinx = IndexTreeHelper::ucs4StrToUTF8Str(str);
        //char* pinx = IndextreeHelper::wcsrtombs_r(str);
        if (pinx == NULL) {
            printf("e: {loadIndex} invalid wcstring\n");
            return false;
        }
        strparent = string(pinx);
        //printf("strparent %s\n", strparent.c_str());
        free(pinx);
    }

    struct inxtree_chrindex chrInx = parent->value();
	address_t loc = inxtree_read_u32(chrInx.location);
	int length = inxtree_read_u16(chrInx.len_content);
	if ((loc & F_LOCSTRINX) == 0) {
        if (stat->number >= stat->start) {
            if (stat->end == -1 || stat->number < stat->end) {
                iIndexItem* item = new iIndexItem();
                item->index = strparent;
                item->d = dataitem(loc);
                indexList.push_back(item);
            } else {
                return false;
            }
        }
        ++stat->number;
        return true;
	}

    return loadIndex(strparent, chrInx, stat, indexList);
}

/* load from string index area */
bool IndexTree::loadIndex(string startwith,
                          struct inxtree_chrindex& chrInx,
                          struct IndexStat *stat,
                          IndexList& indexList)
{
	address_t loc = inxtree_read_u32(chrInx.location);
	int length = inxtree_read_u16(chrInx.len_content);
    if ((loc & F_LOCSTRINX) != F_LOCSTRINX)
        return false;

    loc = loc & (~F_LOCSTRINX);
    int bk_off = loc/INXTREE_BLOCK;
    int addr_off = loc%INXTREE_BLOCK;
    int block_nr = m_strIndexLoc+bk_off;
    u8 *buf_start, *buf_end;
    if (bk_off > m_dataLoc - m_strIndexLoc) {
        printf("e: {loadIndex} a invalid addr (%x\n", loc);
        return false;
    }

    if ((buf_start = (u8 *)getBlock(block_nr)) == NULL)
        return false;
    buf_end = buf_start + INXTREE_BLOCK;

    buf_start += addr_off; // move buf_start

    for (int item = 0; item < length; item++) {
        struct inxtree_strindex *pStrInx;
        if (buf_end <= buf_start || buf_end - buf_start < sizeof(struct inxtree_strindex))
            goto READ_NEXT_BLOCK;

        pStrInx = (struct inxtree_strindex *) buf_start;
        if (pStrInx->len_str[0] != 0)
            goto LOADING;

    READ_NEXT_BLOCK:
        // Read next block
        ++block_nr;
        if ((buf_start = (u8 *)getBlock(block_nr)) == NULL)
            return false;
        buf_end = buf_start + INXTREE_BLOCK;
        pStrInx = (struct inxtree_strindex *) buf_start;

        // Check pStrInx.
        if (pStrInx->len_str[0] == 0) {
            printf("e: {loadIndex} read a invalid data area \n");
            return false;
        }

    LOADING:
        if (stat->number >= stat->start) {
            if (stat->end == -1 || stat->number < stat->end) {
                string strinx = string((char*)(pStrInx->keystr), pStrInx->len_str[0]);
                //printf("strinx %s, %d\n", strinx.c_str(), pStrInx->len_str[0]);
                iIndexItem* item = new iIndexItem();
                item->index = startwith + strinx;
                item->d = dataitem(inxtree_read_u32(pStrInx->location));
                indexList.push_back(item);
            } else {
                return false;
            }
        }
        ++stat->number;
        buf_start += 5 + pStrInx->len_str[0];
    }
    return true;
}

/*
 * [return] the start pos of the invalid part of the index.
 *     - 0: The whole 'index' is invalid.
 *     - length of index: valid,
 */
int IndexTree::validLen(string  key)
{
    if (!m_indexTree || key == "")
        return 0;

    tree_node<inxtree_chrindex>::treeNodePtr root = m_indexTree->root();

    int remain;
    root = findTreeNode((char*)key.c_str(), root, &remain);
    string prefix = key.substr(0, key.length() - remain);
    if (remain > 0) {
        struct inxtree_chrindex chrInx = root->children().size() == 0 ? root->value() : root->child(0)->value();
        IndexList strIndexList;
        struct IndexStat stat;
        if (loadIndex(prefix, chrInx, &stat, strIndexList)) {
            int max = 0;
            string strrst = key.substr(key.length() - remain, remain);
            for (int i = 0; i < strIndexList.size(); i++) {
                string strinx = strIndexList[i]->index;
                int len = IndexTreeHelper::strComLen(strinx.c_str(), strrst.c_str());
                if (max < len)
                    max = len;
            }
            remain -= max;
        }
    }

    return key.length() - remain;
}

tree_node<inxtree_chrindex>::treeNodePtr
IndexTree::findTreeNode(char *strkey, tree_node<inxtree_chrindex>::treeNodePtr parent, int* remain)
{
    *remain = strlen(strkey);
    if (*strkey != '\0') {
    /* 'strkey' will be modified, some chars will be removed */
	const u4char_t key = IndexTreeHelper::utf8byteToUCS4Char((const char**)&strkey);

        if (parent->children().size() > 0) {
            int i = bsearch(parent, key, 0, parent->children().size()-1);
            if (i != -1) {
                struct inxtree_chrindex chrInx = parent->child(i)->value();
                u4char_t chr = inxtree_read_u32(chrInx.wchr);
                if (chr == key) {
                    return findTreeNode(strkey, (*parent)[i], remain);
                }
	        }
        }
    }
	return parent;
}


int IndexTree::bsearch(tree_node<inxtree_chrindex>::treeNodePtr parent,
                            u4char_t key, int min, int max)
{
    int mid = (min + max) / 2;
    u32 chr = inxtree_read_u32(parent->child(mid)->value().wchr);
    //printf("bsearch mid(%d), min(%d), max(%d): %d-->%d\n", mid, min, max, chr, key);
    if (min == max) {
        if (chr == key)
            return min;
        return -1;
    }
	if (chr == key)
        return mid;
    if (chr < key)
	    return bsearch(parent, key, mid+1, max);

    if (mid > min)
        return bsearch(parent, key, min, mid-1);
    else
        return -1;
}

void* IndexTree::getBlock(int blk)
{
    map<int, void*>::iterator iter = m_blkCache.find(blk);
    if(iter == m_blkCache.end()) {
        indextree_helper::MutexLock lock(m_cs);
        indextree_helper::ReadFile read;
        void *ptr = malloc(INXTREE_BLOCK);
        memset(ptr, 0, INXTREE_BLOCK);
        if (ptr != NULL) {
            if (fseek(m_inxFile, (blk-1)*INXTREE_BLOCK, SEEK_SET) == 0) {
                read(m_inxFile, ptr, INXTREE_BLOCK);
                if(m_blkCache.size() > CACHE_SLICE_MAX) {
                    free(m_blkCache.begin()->second);
                    m_blkCache.erase(m_blkCache.begin());
                }
                m_blkCache[blk] = ptr;
                return ptr;
            }
            free(ptr);
            return NULL;
        }
        printf("e: getBlock can't malloc(INXTREE_BLOCK)\n");
        return NULL;
    } else {
       return iter->second;
    }
}


struct inxtree_dataitem
IndexTree::dataitem(address_t loc)
{
    struct inxtree_dataitem d;
	memset(&d, 0, sizeof(struct inxtree_dataitem));
	if (loc != INXTREE_INVALID_ADDR) {
		indextree_helper::ReadFile read;
    	fseek(m_inxFile, (m_dataLoc-1)*INXTREE_BLOCK + loc, SEEK_SET);

		u8 *buf = (u8 *)read(m_inxFile, 2);
		d.len_data = inxtree_read_u16(buf);
		if (d.len_data > 0) {
			d.ptr_data = (u8 *)malloc(d.len_data);
			buf = (u8 *)read(m_inxFile, d.len_data);
			memcpy(d.ptr_data, buf, d.len_data);
            if (d.len_data > 0x8000) {
                printf("w: dataitem the length of data is larger than 0x8000\n");
            }
		}
	}
	return d;
}

