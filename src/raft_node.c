/**
 * Copyright (c) 2013, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @file
 * @brief Representation of a peer
 * @author Willem Thiart himself@willemthiart.com
 * @version 0.1
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "raft.h"

typedef struct
{
    void* udata;//
    int next_idx;//对于每一个服务器，需要发送给他的下一个日志条目的索引值（初始化为领导人最后索引值加一）
    int match_idx;//对于每一个服务器，已经复制给他的日志的最高索引值
} raft_node_private_t;
/**
 * 创建raft node 对象
 */
raft_node_t* raft_node_new(void* udata)
{
    raft_node_private_t* me;
    me = (raft_node_private_t*)calloc(1, sizeof(raft_node_private_t));
    me->udata = udata;//用户数据
    me->next_idx  = 1;//下个日志索引
    me->match_idx = 0;//日志的最高索引值
    return (raft_node_t*)me;
}
/**
 * 获取该节点下个需要发送的日志索引值
 */
int raft_node_get_next_idx(raft_node_t* me_)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    return me->next_idx;
}

/**
 * 设置该节点需要发送的下个日志索引值
 */
void raft_node_set_next_idx(raft_node_t* me_, int nextIdx)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    /* log index begins at 1 */
    me->next_idx = nextIdx < 1 ? 1 : nextIdx;
}

/**
 * 获得该节点已发送的最高日志索引值
 */
int raft_node_get_match_idx(raft_node_t* me_)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    return me->match_idx;
}
/**
 * 设置该节点所发送的日志最高索引值
 */
void raft_node_set_match_idx(raft_node_t* me_, int matchIdx)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    me->match_idx = matchIdx;
}

/**
 * 获取节点自定义数据
 */
void* raft_node_get_udata(raft_node_t* me_)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    return me->udata;
}

/**
 * 设置节点自定义数据
 */
void raft_node_set_udata(raft_node_t* me_, void* udata)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    me->udata = udata;
}
