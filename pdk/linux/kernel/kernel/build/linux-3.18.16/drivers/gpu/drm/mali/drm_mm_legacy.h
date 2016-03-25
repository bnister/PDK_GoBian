/**************************************************************************
 *
 * Copyright 2006-2008 Tungsten Graphics, Inc., Cedar Park, TX. USA.
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
 * Authors:
 * Thomas Hellstrom <thomas-at-tungstengraphics-dot-com>
 */

#ifndef _DRM_MM_LEGACY_H_
#define _DRM_MM_LEGACY_H_

/*
 * Generic range manager structs
 */
#include <linux/list.h>

struct drm_mm_legacy_node {
	struct list_head node_list;
	struct list_head hole_stack;
	unsigned hole_follows : 1;
	unsigned scanned_block : 1;
	unsigned scanned_prev_free : 1;
	unsigned scanned_next_free : 1;
	unsigned scanned_preceeds_hole : 1;
	unsigned allocated : 1;
	unsigned long start;
	unsigned long size;
	struct drm_mm_legacy *mm;
};

struct drm_mm_legacy {
	/* List of all memory nodes that immediately precede a free hole. */
	struct list_head hole_stack;
	/* head_node.node_list is the list of all memory nodes, ordered
	 * according to the (increasing) start address of the memory node. */
	struct drm_mm_legacy_node head_node;
	struct list_head unused_nodes;
	int num_unused;
	spinlock_t unused_lock;
	unsigned int scan_check_range : 1;
	unsigned scan_alignment;
	unsigned long scan_size;
	unsigned long scan_hit_start;
	unsigned scan_hit_size;
	unsigned scanned_blocks;
	unsigned long scan_start;
	unsigned long scan_end;
	struct drm_mm_legacy_node *prev_scanned_node;
};


extern int drm_mm_legacy_init(struct drm_mm_legacy *mm, unsigned long start,
		       unsigned long size);

extern void drm_mm_legacy_takedown(struct drm_mm_legacy *mm);

extern struct drm_mm_legacy_node *drm_mm_legacy_search_free(const struct drm_mm_legacy *mm,
					      unsigned long size,
					      unsigned alignment,
					      int best_match);


extern void drm_mm_legacy_put_block(struct drm_mm_legacy_node *cur);

extern struct drm_mm_legacy_node *drm_mm_legacy_get_block(struct drm_mm_legacy_node *parent,
						   unsigned long size,
						   unsigned alignment);

#endif
