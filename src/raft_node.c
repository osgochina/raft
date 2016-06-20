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

#define RAFT_NODE_VOTED_FOR_ME 1
#define RAFT_NODE_VOTING 1 << 1 //节点有选举权限
#define RAFT_NODE_HAS_SUFFICIENT_LOG 1 << 2  //节点有足够的日志

typedef struct
{
    void* udata;//用户数据对象

    int next_idx;//对于每一个服务器，需要发送给他的下一个日志条目的索引值（初始化为领导人最后索引值加一）
    int match_idx;//对于每一个服务器，已经复制给他的日志的最高索引值

    int flags;//节点标示

    int id;//节点id
} raft_node_private_t;

/**
 * 创建节点对象
 */
raft_node_t* raft_node_new(void* udata, int id)
{
    raft_node_private_t* me;
    me = (raft_node_private_t*)calloc(1, sizeof(raft_node_private_t));
    if (!me)
        return NULL;
    me->udata = udata;
    me->next_idx = 1;
    me->match_idx = 0;
    me->id = id;
    me->flags = RAFT_NODE_VOTING;
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

/**
 * 投票给指定的节点
 */
void raft_node_vote_for_me(raft_node_t* me_, const int vote)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    if (vote)
        me->flags |= RAFT_NODE_VOTED_FOR_ME;
    else
        me->flags &= ~RAFT_NODE_VOTED_FOR_ME;
}

/**
 * 判断节点是否投票给了我
 */
int raft_node_has_vote_for_me(raft_node_t* me_)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    return (me->flags & RAFT_NODE_VOTED_FOR_ME) != 0;
}

/**
 * 设置节点有投票权限
 */
void raft_node_set_voting(raft_node_t* me_, int voting)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    if (voting)
        me->flags |= RAFT_NODE_VOTING;
    else
        me->flags &= ~RAFT_NODE_VOTING;
}

/**
 * 判断节点是否有投票权限
 */
int raft_node_is_voting(raft_node_t* me_)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    return (me->flags & RAFT_NODE_VOTING) != 0;
}

/**
 * 设置节点日志已经足够
 */
void raft_node_set_has_sufficient_logs(raft_node_t* me_)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    me->flags |= RAFT_NODE_HAS_SUFFICIENT_LOG;
}

/**
 * 判断节点日志是否足够
 */
int raft_node_has_sufficient_logs(raft_node_t* me_)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    return (me->flags & RAFT_NODE_HAS_SUFFICIENT_LOG) != 0;
}

/**
 * 获取节点id
 */
int raft_node_get_id(raft_node_t* me_)
{
    raft_node_private_t* me = (raft_node_private_t*)me_;
    return me->id;
}
