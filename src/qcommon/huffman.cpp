#include "huffman.h"
#include "huffman_wire_codes.h"
#include <universal/assertive.h>
#include <cstdlib>
#include <cstring>

int bloc;

int __cdecl get_bit(const uint8_t *fin)
{
    int t; // [esp+0h] [ebp-4h]

    t = ((int)fin[bloc >> 3] >> (bloc & 7)) & 1;
    ++bloc;
    return t;
}

bool __cdecl Huff_offsetReceive(nodetype *node, int *ch, const uint8_t *fin, int *offset, int maxOffset)
{
    bloc = *offset;
    while (node && node->symbol == 257)
    {
        if (bloc >= maxOffset)
        {
            *ch = 0;
            *offset = bloc;
            return false;
        }
        if (get_bit(fin))
            node = node->right;
        else
            node = node->left;
    }
    if (node)
    {
        *ch = node->symbol;
        *offset = bloc;
        return true;
    }

    *ch = 0;
    *offset = bloc;
    return false;
}

void __cdecl huffman_send(nodetype *node, nodetype *child, uint8_t *fout)
{
    if (node->parent)
        huffman_send(node->parent, node, fout);
    if (child)
    {
        if (node->right == child)
            add_bit(1, fout);
        else
            add_bit(0, fout);
    }
}

void __cdecl add_bit(char bit, uint8_t *fout)
{
    if ((bloc & 7) == 0)
        fout[bloc >> 3] = 0;
    fout[bloc >> 3] |= bit << (bloc & 7);
    ++bloc;
}

int __cdecl huffman_bitCountForNode(nodetype *node, nodetype *child)
{
    int bits; // [esp+0h] [ebp-4h]

    bits = 0;
    if (node->parent)
        bits = huffman_bitCountForNode(node->parent, node);
    if (child)
        ++bits;
    return bits;
}

int __cdecl Huff_bitCount(huff_t *huff, uint32_t ch)
{
    if (ch >= 0x100)
        MyAssertHandler(".\\qcommon\\huffman.cpp", 152, 0, "ch doesn't index 256\n\t%i not in [0, %i)", ch, 256);
    if (!huff->loc[ch])
        MyAssertHandler(".\\qcommon\\huffman.cpp", 153, 0, "%s", "huff->loc[ch] != NULL");
    return huffman_bitCountForNode(huff->loc[ch], 0);
}

void __cdecl Huff_offsetTransmit(huff_t *huff, int ch, uint8_t *fout, int *offset)
{
    bloc = *offset;
    huffman_send(huff->loc[ch], 0, fout);
    *offset = bloc;
}

void __cdecl Huff_Init(huffman_t *huff)
{
    std::memset(huff, 0, sizeof(huffman_t));
    huff->compressDecompress.loc[256] = &huff->compressDecompress.nodeList[huff->compressDecompress.blocNode++];
    huff->compressDecompress.tree = huff->compressDecompress.loc[256];
    huff->compressDecompress.tree->symbol = 256;
    huff->compressDecompress.tree->weight = 0;
    huff->compressDecompress.tree->parent = 0;
    huff->compressDecompress.tree->left = 0;
    huff->compressDecompress.tree->right = 0;
}

nodetype *__cdecl Huff_initNode(huff_t *huff, int ch, int weight)
{
    nodetype *tnode; // [esp+0h] [ebp-4h]

    tnode = &huff->nodeList[huff->blocNode++];
    tnode->symbol = ch;
    tnode->weight = weight;
    tnode->left = 0;
    tnode->right = 0;
    tnode->parent = 0;
    // Internal nodes have symbol 257, outside the 257-entry symbol lookup.
    if (ch < 257)
        huff->loc[ch] = tnode;
    return tnode;
}

int __cdecl nodeCmp(const void *left, const void *right)
{
    const nodetype *a = *static_cast<nodetype *const *>(left);
    const nodetype *b = *static_cast<nodetype *const *>(right);
    return (a->weight > b->weight) - (a->weight < b->weight);
}

void __cdecl Huff_BuildFromData(huff_t *huff, const int *msg_hData)
{
    // Both leaf and internal nodes can tie in weight. The C runtime's qsort
    // tie ordering changes the codebook (Darwin swapped bytes 228 and 231),
    // corrupting snapshot bits and outgoing user commands. Use the wire tree.
    std::memset(huff, 0, sizeof(*huff));
    huff->tree = Huff_initNode(huff, 257, 0);
    for (int symbol = 0; symbol <= 256; ++symbol)
    {
        const HuffWireCode &code = kHuffWireCodes[symbol];
        nodetype *node = huff->tree;
        for (int bit = 0; bit < code.length; ++bit)
        {
            nodetype *&child = (code.bits & (1u << bit)) ? node->right : node->left;
            if (!child)
            {
                child = Huff_initNode(huff, 257, 0);
                child->parent = node;
            }
            node = child;
        }
        node->symbol = symbol;
        node->weight = symbol < 256 ? msg_hData[symbol] : 0;
        huff->loc[symbol] = node;
        for (nodetype *parent = node->parent; parent; parent = parent->parent)
            parent->weight += node->weight;
    }
    iassert(huff->blocNode == 513);
}
