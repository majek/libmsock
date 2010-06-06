#ifndef _UMAP_H
#define _UMAP_H

struct umap_root;

typedef unsigned long ulong;

DLL_LOCAL struct umap_root *umap_new(size_t map_sz, ulong max_counter);
DLL_LOCAL void umap_free(struct umap_root *root);
DLL_LOCAL ulong umap_add(struct umap_root *root, void *ptr);
DLL_LOCAL void umap_del(struct umap_root *root, ulong no);
DLL_LOCAL void *umap_get(struct umap_root *root, ulong no);


#endif // _UMAP_H
