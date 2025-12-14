/**
 * @file stack.cpp
 * @brief 通用栈结构
 * 作用：
 * 实现了一个基于链表的通用“栈”（Stack）数据结构。
 * 主要功能：
 * 在项目中的用途： 这是一个辅助工具文件，主要被 pe.cpp 依赖。因为 PE 文件的资源目录是一个树状结构（目录套子目录），pe.cpp 在解析时使用这个栈来非递归地遍历目录树，防止深层递归导致栈溢出。
*/
#include <bncsutil/stack.h>
#include <cstdlib>

/*
 *创建一个新的栈
 */
cm_stack_t cm_stack_create()
{
    cm_stack_t stack = (cm_stack_t) calloc(1, sizeof(struct cm_stack));
    if (!stack)
        return nullptr;
    return stack;
}

/*
 *销毁栈并释放所有内存
 */
void cm_stack_destroy(cm_stack_t stack)
{
    cm_stack_node_t *node;
    cm_stack_node_t *next;

    if (!stack)
        return;

    node = stack->top;

    // 遍历链表释放每个节点
    while (node) {
        next = node->next;
        free(node); // 释放节点内存
        node = next;
    }

    free(stack); // 释放栈结构体内存
}

/*
 *入栈：将元素压入栈顶
 */
void cm_stack_push(cm_stack_t stack, void *item)
{
    cm_stack_node_t *new_node;

    if (!stack || !item)
        return;

    // 分配新节点内存
    new_node = (cm_stack_node_t*) malloc(sizeof(cm_stack_node_t));
    if (!new_node)
        return;

    // 链表插入操作
    new_node->next = stack->top;
    new_node->value = item;

    stack->size++;

    stack->top = new_node;
}

/*
 *出栈：移除并返回栈顶元素
 */
void *cm_stack_pop(cm_stack_t stack)
{
    cm_stack_node_t *next;
    void *value;

    if (!stack || !stack->top)
        return nullptr;

    next = stack->top->next;
    value = stack->top->value;

    // 释放栈顶节点，但不释放 value 指向的数据（由调用者负责）
    free(stack->top);

    stack->top = next;
    stack->size--;
    return value;
}

/*
 *查看栈顶元素（不移除）
 */
void *cm_stack_peek(cm_stack_t stack)
{
    if (!stack || !stack->top)
        return nullptr;

    return stack->top->value;
}

/*
 *获取栈的大小
 */
unsigned int cm_stack_size(cm_stack_t stack)
{
    if (!stack)
        return 0;

    return stack->size;
}
