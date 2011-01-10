#ifndef LLRB_C
#define LLRB_C

#define RED	(1)
#define BLACK	(0)

typedef jit_value rb_key_t;
typedef void * value_t;

typedef struct rb_node {
	struct rb_node * left;
	struct rb_node * right;
	int color;
	rb_key_t key;
	value_t value;
} rb_node;


static inline int is_red(rb_node * n)
{
	if (n == NULL) return 0;
	return (n->color == RED);
}

static inline rb_node * rotate_left(rb_node * h)
{
	rb_node * x = h->right;
	h->right = x->left;
	x->left = h;
	x->color = x->left->color;
	x->left->color = RED;
	return x;
}

static inline rb_node * rotate_right(rb_node * h)
{
	rb_node * x = h->left;
	h->left = x->right;
	x->right = h;
	x->color = x->right->color;
	x->right->color = RED;
	return x;
}

static inline void color_flip(rb_node * h)
{
	h->color = !h->color;
	h->left->color = !h->left->color;
	h->right->color = !h->right->color;
}

static inline rb_node * fixup(rb_node * h)
{
	if (is_red(h->right)) h = rotate_left(h);

	if (is_red(h->left) && (is_red(h->left->left))) {
		h = rotate_right(h);
		color_flip(h);
	}

	if (is_red(h->left) && is_red(h->right)) color_flip(h);

	return h;
}



static inline rb_node * node_new(rb_key_t key, value_t value)
{
	rb_node * res = malloc(sizeof(rb_node));
	res->key = key;
	res->value = value;
	res->color = RED;
	res->left = NULL;
	res->right = NULL;
	return res;
}

static rb_node * node_insert(rb_node * h, rb_key_t key, value_t value, int * found)
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

static rb_node * rb_insert(rb_node * root, rb_key_t key, value_t value, int * found)
{
	if (found) *found = 0;
	root = node_insert(root, key, value, found);
	root->color = BLACK;
	return root;
}


static rb_node * rb_search(rb_node * h, rb_key_t key)
{
	if ((h == NULL) || (h->key == key)) return h;
	if (h->key > key) return rb_search(h->left, key);
	return rb_search(h->right, key);
}
// delete stuff
static inline rb_node * move_red_left(rb_node * h)
{
	color_flip(h);
	if (is_red(h->right->left)) {
		h->right = rotate_right(h->right);
		h = rotate_left(h);
		color_flip(h);
	}
	return h;
}

static inline rb_node * move_red_right(rb_node * h)
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

static inline rb_key_t node_min(rb_node * x)
{
	if (x->left == NULL) return x->key;
	else return node_min(x->left);
}

static rb_node * delete_min(rb_node * h)
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

static rb_node * delete_node(rb_node * h, rb_key_t key, int * found)
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
			h->value = rb_search(h->right, node_min(h->right))->value;
			h->key = node_min(h->right);
			h->right = delete_min(h->right);
			if (found) *found = 1;
		}
		else h->right = delete_node(h->right, key, found);
	}
	return fixup(h);
}

static inline rb_node * rb_delete(rb_node * root, rb_key_t key, int * found)
{ 
	root = delete_node(root, key, found);
	if (root) root->color = BLACK;
	return root;
}


/////////////////

static inline rb_node * rb_addall(rb_node * target, rb_node * n)
{
	if (n == NULL) return target;
	target = rb_addall(target, n->left);
	target = rb_insert(target, n->key, n->value, NULL);
	target = rb_addall(target, n->right);
	return target;
}

static inline rb_node * rb_clone(rb_node * root)
{
	return rb_addall(NULL, root);
}

/////////////////
static inline void rb_print_tree(rb_node * h, int level)
{
	int i;
	if (h == NULL) return;
	for (i = 0; i < level; i++)
		printf(" ");

	printf("%i:%li\n", (int)h->key, (long)h->value);
	rb_print_tree(h->left, level + 1);
	rb_print_tree(h->right, level + 1);
}

static void rb_free(rb_node * h)
{
	if (h == NULL) return;
	rb_free(h->left);
	rb_free(h->right);
	JIT_FREE(h);
}

static int rb_subset(rb_node * root, rb_node * n)
{
	if (n == NULL) return 1;
	return rb_search(root, n->key) && rb_subset(root, n->left) && rb_subset(root, n->right);
}

static int rb_equal(rb_node * r1, rb_node * r2)
{
	return rb_subset(r1, r2) && rb_subset(r2, r1);
}
#endif
