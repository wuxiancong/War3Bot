#ifndef CM_STACK_H_INCLUDED
#define CM_STACK_H_INCLUDED 1

typedef struct cm_stack_node {
	void* value;
	struct cm_stack_node* next;
} cm_stack_node_t;

typedef struct cm_stack {
	unsigned int size;
	cm_stack_node_t* top;
} *cm_stack_t;

cm_stack_t cm_stack_create();
void cm_stack_destroy(cm_stack_t stack);
void cm_stack_push(cm_stack_t stack, void* item);
void* cm_stack_pop(cm_stack_t stack);
void* cm_stack_peek(cm_stack_t stack);
unsigned int cm_stack_size(cm_stack_t stack);

#endif
