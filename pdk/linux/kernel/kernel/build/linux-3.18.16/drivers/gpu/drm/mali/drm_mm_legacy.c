/**************************************************************************
 *
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 **************************************************************************/

/*
 * Generic simple memory manager implementation. Intended to be used as a base
 * class implementation for more advanced memory managers.
 *
 * Note that the algorithm used is quite simple and there might be substantial
 * performance gains if a smarter free list is implemented. Currently it is just an
 * unordered stack of free regions. This could easily be improved if an RB-tree
 * is used instead. At least if we expect heavy fragmentation.
 *
 * Aligned allocations can also see improvement.
 *
 * Authors:
 * Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include <drm/drmP.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/export.h>
#include "drm_mm_legacy.h"

#define MM_UNUSED_TARGET 4

static struct drm_mm_legacy_node *drm_mm_legacy_kmalloc(struct drm_mm_legacy *mm, int atomic)
{
	struct drm_mm_legacy_node *child;

	if (atomic)
		child = kzalloc(sizeof(*child), GFP_ATOMIC);
	else
		child = kzalloc(sizeof(*child), GFP_KERNEL);

	if (unlikely(child == NULL)) {
		spin_lock(&mm->unused_lock);
		if (list_empty(&mm->unused_nodes))
			child = NULL;
		else {
			child =
			    list_entry(mm->unused_nodes.next,
				       struct drm_mm_legacy_node, node_list);
			list_del(&child->node_list);
			--mm->num_unused;
		}
		spin_unlock(&mm->unused_lock);
	}
	return child;
}

static inline unsigned long drm_mm_legacy_hole_node_start(struct drm_mm_legacy_node *hole_node)
{
	return hole_node->start + hole_node->size;
}

static inline unsigned long drm_mm_legacy_hole_node_end(struct drm_mm_legacy_node *hole_node)
{
	struct drm_mm_legacy_node *next_node =
		list_entry(hole_node->node_list.next, struct drm_mm_legacy_node,
			   node_list);

	return next_node->start;
}

static void drm_mm_legacy_insert_helper(struct drm_mm_legacy_node *hole_node,
				 struct drm_mm_legacy_node *node,
				 unsigned long size, unsigned alignment)
{
	struct drm_mm_legacy *mm = hole_node->mm;
	unsigned long tmp = 0, wasted = 0;
	unsigned long hole_start = drm_mm_legacy_hole_node_start(hole_node);
	unsigned long hole_end = drm_mm_legacy_hole_node_end(hole_node);

	BUG_ON(!hole_node->hole_follows || node->allocated);

	if (alignment)
		tmp = hole_start % alignment;

	if (!tmp) {
		hole_node->hole_follows = 0;
		list_del_init(&hole_node->hole_stack);
	} else
		wasted = alignment - tmp;

	node->start = hole_start + wasted;
	node->size = size;
	node->mm = mm;
	node->allocated = 1;

	INIT_LIST_HEAD(&node->hole_stack);
	list_add(&node->node_list, &hole_node->node_list);

	BUG_ON(node->start + node->size > hole_end);

	if (node->start + node->size < hole_end) {
		list_add(&node->hole_stack, &mm->hole_stack);
		node->hole_follows = 1;
	} else {
		node->hole_follows = 0;
	}
}

static void drm_mm_legacy_remove_node(struct drm_mm_legacy_node *node)
{
	struct drm_mm_legacy *mm = node->mm;
	struct drm_mm_legacy_node *prev_node;

	BUG_ON(node->scanned_block || node->scanned_prev_free
				   || node->scanned_next_free);

	prev_node =
	    list_entry(node->node_list.prev, struct drm_mm_legacy_node, node_list);

	if (node->hole_follows) {
		BUG_ON(drm_mm_legacy_hole_node_start(node)
				== drm_mm_legacy_hole_node_end(node));
		list_del(&node->hole_stack);
	} else
		BUG_ON(drm_mm_legacy_hole_node_start(node)
				!= drm_mm_legacy_hole_node_end(node));

	if (!prev_node->hole_follows) {
		prev_node->hole_follows = 1;
		list_add(&prev_node->hole_stack, &mm->hole_stack);
	} else
		list_move(&prev_node->hole_stack, &mm->hole_stack);

	list_del(&node->node_list);
	node->allocated = 0;
}

static int check_free_hole(unsigned long start, unsigned long end,
			   unsigned long size, unsigned alignment)
{
	unsigned wasted = 0;

	if (end - start < size)
		return 0;

	if (alignment) {
		unsigned tmp = start % alignment;
		if (tmp)
			wasted = alignment - tmp;
	}

	if (end >= start + size + wasted) {
		return 1;
	}

	return 0;
}


struct drm_mm_legacy_node *drm_mm_legacy_get_block(struct drm_mm_legacy_node *parent,
						   unsigned long size,
						   unsigned alignment)
{
    struct drm_mm_legacy_node *node;

	node = drm_mm_legacy_kmalloc(parent->mm, 0);
	if (unlikely(node == NULL))
		return NULL;

	drm_mm_legacy_insert_helper(parent, node, size, alignment);

	return node;
}
EXPORT_SYMBOL(drm_mm_legacy_get_block);

/*
 * Remove a memory node from the allocator and free the allocated struct
 * drm_mm_node. Only to be used on a struct drm_mm_node obtained by one of the
 * drm_mm_get_block functions.
 */
void drm_mm_legacy_put_block(struct drm_mm_legacy_node *node)
{

	struct drm_mm_legacy *mm = node->mm;

	drm_mm_legacy_remove_node(node);

	spin_lock(&mm->unused_lock);
	if (mm->num_unused < MM_UNUSED_TARGET) {
		list_add(&node->node_list, &mm->unused_nodes);
		++mm->num_unused;
	} else
		kfree(node);
	spin_unlock(&mm->unused_lock);
}

EXPORT_SYMBOL(drm_mm_legacy_put_block);


struct drm_mm_legacy_node *drm_mm_legacy_search_free(const struct drm_mm_legacy *mm,
				       unsigned long size,
				       unsigned alignment, int best_match)
{
	struct drm_mm_legacy_node *entry;
	struct drm_mm_legacy_node *best;
	unsigned long best_size;

	BUG_ON(mm->scanned_blocks);

	best = NULL;
	best_size = ~0UL;

	list_for_each_entry(entry, &mm->hole_stack, hole_stack) {
		BUG_ON(!entry->hole_follows);
		if (!check_free_hole(drm_mm_legacy_hole_node_start(entry),
				     drm_mm_legacy_hole_node_end(entry),
				     size, alignment))
			continue;

		if (!best_match)
			return entry;

		if (entry->size < best_size) {
			best = entry;
			best_size = entry->size;
		}
	}

	return best;
}
EXPORT_SYMBOL(drm_mm_legacy_search_free);

int drm_mm_legacy_clean(struct drm_mm_legacy * mm)
{
	struct list_head *head = &mm->head_node.node_list;

	return (head->next->next == head);
}

EXPORT_SYMBOL(drm_mm_legacy_clean);

int drm_mm_legacy_init(struct drm_mm_legacy * mm, unsigned long start, unsigned long size)
{
	INIT_LIST_HEAD(&mm->hole_stack);
	INIT_LIST_HEAD(&mm->unused_nodes);
	mm->num_unused = 0;
	mm->scanned_blocks = 0;
	spin_lock_init(&mm->unused_lock);

	/* Clever trick to avoid a special case in the free hole tracking. */
	INIT_LIST_HEAD(&mm->head_node.node_list);
	INIT_LIST_HEAD(&mm->head_node.hole_stack);
	mm->head_node.hole_follows = 1;
	mm->head_node.scanned_block = 0;
	mm->head_node.scanned_prev_free = 0;
	mm->head_node.scanned_next_free = 0;
	mm->head_node.mm = mm;
	mm->head_node.start = start + size;
	mm->head_node.size = start - mm->head_node.start;
	list_add_tail(&mm->head_node.hole_stack, &mm->hole_stack);

	return 0;
}
EXPORT_SYMBOL(drm_mm_legacy_init);

void drm_mm_legacy_takedown(struct drm_mm_legacy * mm)
{
	struct drm_mm_legacy_node *entry, *next;

	if (!list_empty(&mm->head_node.node_list)) {
		DRM_ERROR("Memory manager not clean. Delaying takedown\n");
		return;
	}

	spin_lock(&mm->unused_lock);
	list_for_each_entry_safe(entry, next, &mm->unused_nodes, node_list) {
		list_del(&entry->node_list);
		kfree(entry);
		--mm->num_unused;
	}
	spin_unlock(&mm->unused_lock);

	BUG_ON(mm->num_unused != 0);
}
EXPORT_SYMBOL(drm_mm_legacy_takedown);


