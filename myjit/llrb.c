/*
 * MyJIT 
 * Copyright (C) 2010, 2015 Petr Krajca, <petr.krajca@upol.cz>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef LLRB_C
#define LLRB_C

#define RED	(1)
#define BLACK	(0)

typedef jit_value jit_tree_key;
typedef void * jit_tree_value;

typedef struct jit_tree {
	struct jit_tree * left;
	struct jit_tree * right;
	int color;
	jit_tree_key key;
	jit_tree_value value;
} jit_tree;


static inline int is_red(jit_tree * n)
{
	if (n == NULL) return 0;
	return (n->color == RED);
}

static inline jit_tree * rotate_left(jit_tree * h)
{
	jit_tree * x = h->right;
	h->right = x->left;
	x->left = h;
	x->color = x->left->color;
	x->left->color = RED;
	return x;
}

static inline jit_tree * rotate_right(jit_tree * h)
{
	jit_tree * x = h->left;
	h->left = x->right;
	x->right = h;
	x->color = x->right->color;
	x->right->color = RED;
	return x;
}

static inline void color_flip(jit_tree * h)
{
	h->color = !h->color;
	h->left->color = !h->left->color;
	h->right->color = !h->right->color;
}

static inline jit_tree * fixup(jit_tree * h)
{
	if (is_red(h->right)) h = rotate_left(h);

	if (is_red(h->left) && (is_red(h->left->left))) {
		h = rotate_right(h);
		color_flip(h);
	}

	if (is_red(h->left) && is_red(h->right)) color_flip(h);

	return h;
}



static inline jit_tree * node_new(jit_tree_key key, jit_tree_value value)
{
	jit_tree * res = malloc(sizeof(jit_tree));
	res->key = key;
	res->value = value;
	res->color = RED;
	res->left = NULL;
	res->right = NULL;
	return res;
}

static jit_tree * node_insert(jit_tree * h, jit_tree_key key, jit_tree_value value, int * found)
{
	if (h == NULL) return node_new(key, value);
	if (is_red(h->left) && is_red(h->right)) color_flip(h);


	if (h->key == key) {
		h->value = value;
		if (found) *found = 1;
	} else if (h->key > key) h->left = node_insert(h->left, key, value, found);
	else h->right = node_insert(h->right, key, value, found);

	//if (is_red(h->right) && !is_red(h->left)) h = rotate_left(h);
	//if (is_red(h->left) && is_red(h->left->left)) h = rotate_right(h);
	////if (is_red(h->right)/* && !is_red(h->left)*/) h = rotate_left(h);
	//if (is_red(h->right)/* && !is_red(h->left)*/) h = rotate_left(h);
	return fixup(h);
}

static jit_tree * jit_tree_insert(jit_tree * root, jit_tree_key key, jit_tree_value value, int * found)
{
	if (found) *found = 0;
	root = node_insert(root, key, value, found);
	root->color = BLACK;
	return root;
}


static jit_tree * jit_tree_search(jit_tree * h, jit_tree_key key)
{
	if ((h == NULL) || (h->key == key)) return h;
	if (h->key > key) return jit_tree_search(h->left, key);
	return jit_tree_search(h->right, key);
}
// delete stuff
static inline jit_tree * move_red_left(jit_tree * h)
{
	color_flip(h);
	if (is_red(h->right->left)) {
		h->right = rotate_right(h->right);
		h = rotate_left(h);
		color_flip(h);
	}
	return h;
}

static inline jit_tree * move_red_right(jit_tree * h)
{
//	if (!h->left) return h; // workaround not present in the sedgewick's implementation;
				// fixing seg. fault while deleting nodes
	color_flip(h);
	if (is_red(h->left->left)) {
		h = rotate_right(h);
		color_flip(h);
	}
	return h;
}

static inline jit_tree_key node_min(jit_tree * x)
{
	if (x->left == NULL) return x->key;
	else return node_min(x->left);
}

static jit_tree * delete_min(jit_tree * h)
{ 
	if (h->left == NULL) {
		JIT_FREE(h);
		return NULL;
	}

	if ((!is_red(h->left)) && (!is_red(h->left->left)))
		h = move_red_left(h);

	h->left = delete_min(h->left);

	return fixup(h);
}

static jit_tree * delete_node(jit_tree * h, jit_tree_key key, int * found)
{
	if (h == NULL) {
		if (found) *found = 0; 
		return NULL;
	}

	if (key < h->key) {
		// XXX: if ((!is_red(h->left)) && (!is_red(h->left->left)))
		if ((!is_red(h->left)) && (h->left) && (!is_red(h->left->left)))
			h = move_red_left(h);
		h->left = delete_node(h->left, key, found);
	} else {
		if (is_red(h->left)) h = rotate_right(h);
		if ((key == h->key) && (h->right == NULL)) {
			JIT_FREE(h);
			if (found) *found = 1;
			return NULL;
		}

		// XXX: if (!is_red(h->right) && !is_red(h->right->left)) h = move_red_right(h);
		if (!is_red(h->right) && (h->right) && !is_red(h->right->left)) h = move_red_right(h);
		if (key == h->key) {
			h->value = jit_tree_search(h->right, node_min(h->right))->value;
			h->key = node_min(h->right);
			h->right = delete_min(h->right);
			if (found) *found = 1;
		}
		else h->right = delete_node(h->right, key, found);
	}
	return fixup(h);
}

static inline jit_tree * jit_tree_delete(jit_tree * root, jit_tree_key key, int * found)
{ 
	root = delete_node(root, key, found);
	if (root) root->color = BLACK;
	return root;
}


/////////////////

static inline jit_tree * jit_tree_addall(jit_tree * target, jit_tree * n)
{
	if (n == NULL) return target;
	target = jit_tree_addall(target, n->left);
	target = jit_tree_insert(target, n->key, n->value, NULL);
	target = jit_tree_addall(target, n->right);
	return target;
}

static inline jit_tree * jit_tree_clone(jit_tree * root)
{
	return jit_tree_addall(NULL, root);
}

/////////////////

static void jit_tree_walk(jit_tree *h, void (*func)(jit_tree_key key, jit_tree_value value, void *thunk), void *thunk)
{
        if (!h) return;
	jit_tree_walk(h->left, func, thunk);
        func(h->key, h->value, thunk);
	jit_tree_walk(h->right, func, thunk);
}

static inline void jit_print_tree(jit_tree * h, int level)
{
	if (h == NULL) return;
	for (int i = 0; i < level; i++)
		printf(" ");

	printf("%i:%li\n", (int)h->key, (long)h->value);
	jit_print_tree(h->left, level + 1);
	jit_print_tree(h->right, level + 1);
}

static void jit_tree_free(jit_tree * h)
{
	if (h == NULL) return;
	jit_tree_free(h->left);
	jit_tree_free(h->right);
	JIT_FREE(h);
}

static int jit_tree_subset(jit_tree * root, jit_tree * n)
{
	if (n == NULL) return 1;
	return jit_tree_search(root, n->key) && jit_tree_subset(root, n->left) && jit_tree_subset(root, n->right);
}

static int jit_tree_equal(jit_tree * r1, jit_tree * r2)
{
	return jit_tree_subset(r1, r2) && jit_tree_subset(r2, r1);
}

#endif
