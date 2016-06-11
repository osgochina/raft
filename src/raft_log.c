/**
 * Copyright (c) 2013, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @file
 * @brief ADT for managing Raft log entries (aka entries)
 * @author Willem Thiart himself@willemthiart.com
 * @version 0.1
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "raft.h"
#include "raft_private.h"
#include "raft_log.h"

#define INITIAL_CAPACITY 10
#define in(x) ((log_private_t*)x)

/**
 * 日志条目结构体
 */
typedef struct
{
    /* size of array */
    int size;//条目大小

    /* the amount of elements in the array */
    int count;//日志数量

    /* position of the queue */
    int front, back;//在总日志队列中的前后位置

    /* we compact the log, and thus need to increment the base idx */
    int base_log_idx;

    //日志条目
    raft_entry_t* entries;

    /* callbacks */
    raft_cbs_t *cb;
    void* raft;
} log_private_t;

/**
 * 扩充缓冲区大小
 */
static void __ensurecapacity(log_private_t * me)
{
    int i, j;
    raft_entry_t *temp;

    if (me->count < me->size)
        return;

    temp = (raft_entry_t*)calloc(1, sizeof(raft_entry_t) * me->size * 2);

    for (i = 0, j = me->front; i < me->count; i++, j++)
    {
        if (j == me->size)
            j = 0;
        memcpy(&temp[i], &me->entries[j], sizeof(raft_entry_t));
    }

    /* clean up old entries */
    free(me->entries);

    me->size *= 2;
    me->entries = temp;
    me->front = 0;
    me->back = me->count;
}

/**
 * 创建log对象
 */
log_t* log_new()
{
    log_private_t* me = (log_private_t*)calloc(1, sizeof(log_private_t));
    me->size = INITIAL_CAPACITY;
    me->count = 0;
    me->back = in(me)->front = 0;
    me->entries = (raft_entry_t*)calloc(1, sizeof(raft_entry_t) * me->size);
    return (log_t*)me;
}

/**
 * 设置log对象的回调函数，注意这个回调函数跟raft server的回调函数是共用一个结构体，且是同一个地址
 */
void log_set_callbacks(log_t* me_, raft_cbs_t* funcs, void* raft)
{
    log_private_t* me = (log_private_t*)me_;

    me->raft = raft;
    me->cb = funcs;
}

/**
 * 追加新的日志到当前日志条目中
 */
int log_append_entry(log_t* me_, raft_entry_t* c)
{
    log_private_t* me = (log_private_t*)me_;

    if (0 == c->id)//日志唯一id不能为0
        return -1;
    //设置日志缓冲区大小，如果超过了规定数目，则扩展
    __ensurecapacity(me);

    if (me->cb && me->cb->log_offer)//如果存在最佳日志的回调函数，则执行
        me->cb->log_offer(me->raft, raft_get_udata(me->raft), c, me->back);
    memcpy(&me->entries[me->back], c, sizeof(raft_entry_t));//把日志拷贝到对应的位置
    me->count++;
    me->back++;
    return 0;
}
/**
 * 获取指定索引的日志条目
 */
raft_entry_t* log_get_from_idx(log_t* me_, int idx)
{
    log_private_t* me = (log_private_t*)me_;
    int i;

    assert(0 <= idx - 1);//日志不能从0或者1开始

    //当前日志索引+当前未提交数量 不能小于idx
    if (me->base_log_idx + me->count < idx || idx < me->base_log_idx)
        return NULL;

    /* idx starts at 1 */
    idx -= 1;
    //当前日志位置+需要获取的日志idx位置-当前已经提交的日志索引
    i = (me->front + idx - me->base_log_idx) % me->size;
    return &me->entries[i];
}

/**
 * 日志总数
 */
int log_count(log_t* me_)
{
    return ((log_private_t*)me_)->count;
}

/**
 * 删除指定索引日志
 */
void log_delete(log_t* me_, int idx)
{
    log_private_t* me = (log_private_t*)me_;
    int end;

    /* idx starts at 1 */
    idx -= 1;
    idx -= me->base_log_idx;

    for (end = log_count(me_); idx < end; idx++)
    {
        if (me->cb && me->cb->log_pop)
            me->cb->log_pop(me->raft, raft_get_udata(me->raft),
                            &me->entries[me->back - 1], me->back);
        me->back--;
        me->count--;
    }
}

void *log_poll(log_t * me_)
{
    log_private_t* me = (log_private_t*)me_;

    if (0 == log_count(me_))
        return NULL;

    const void *elem = &me->entries[me->front];
    if (me->cb && me->cb->log_poll)
        me->cb->log_poll(me->raft, raft_get_udata(me->raft),
                         &me->entries[me->front], me->front);
    me->front++;
    me->count--;
    me->base_log_idx++;
    return (void*)elem;
}

raft_entry_t *log_peektail(log_t * me_)
{
    log_private_t* me = (log_private_t*)me_;

    if (0 == log_count(me_))
        return NULL;

    if (0 == me->back)
        return &me->entries[me->size - 1];
    else
        return &me->entries[me->back - 1];
}

/**
 * 清空日志
 */
void log_empty(log_t * me_)
{
    log_private_t* me = (log_private_t*)me_;

    me->front = 0;
    me->back = 0;
    me->count = 0;
}
/**
 * 释放申请的内存
 */
void log_free(log_t * me_)
{
    log_private_t* me = (log_private_t*)me_;//强转对象类型

    free(me->entries);
    free(me);
}
//获取当前日志索引值
int log_get_current_idx(log_t* me_)
{
    log_private_t* me = (log_private_t*)me_;
    return log_count(me_) + me->base_log_idx;
}
