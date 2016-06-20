/**
 * Copyright (c) 2013, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @file
 * @author Willem Thiart himself@willemthiart.com
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* for varags */
#include <stdarg.h>

#include "raft.h"
#include "raft_log.h"
#include "raft_private.h"

/**
 * 设置选举超时时间
 */
void raft_set_election_timeout(raft_server_t* me_, int millisec)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    me->election_timeout = millisec;
}

/**
 * 设置附加日志超时时间
 */
void raft_set_request_timeout(raft_server_t* me_, int millisec)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    me->request_timeout = millisec;
}

/**
 * 获取server的节点id
 */
int raft_get_nodeid(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    if (!me->node)
        return -1;
    return raft_node_get_id(me->node);
}

/**
 * 获取选举超时时间
 */
int raft_get_election_timeout(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->election_timeout;
}

/**
 * 获取附加日志超时时间
 */
int raft_get_request_timeout(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->request_timeout;
}

/**
 * 获取server的节点数量
 */
int raft_get_num_nodes(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->num_nodes;
}

/**
 * 获取有投票权限的节点数量
 */
int raft_get_num_voting_nodes(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    int i, num = 0;
    for (i = 0; i < me->num_nodes; i++)
        if (raft_node_is_voting(me->nodes[i]))
            num++;
    return num;
}

/**
 * 获取超时周期时间
 */
int raft_get_timeout_elapsed(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->timeout_elapsed;
}

/**
 * 获取server的日志数量
 */
int raft_get_log_count(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    return log_count(me->log);
}

/**
 * 获取的票数
 */
int raft_get_voted_for(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    return me->voted_for;
}

/**
 * 设置raft server新的任期号
 */
void raft_set_current_term(raft_server_t* me_, const int term)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    if (me->current_term < term)
    {
        me->current_term = term;
        me->voted_for = -1;
        if (me->cb.persist_term)
            me->cb.persist_term(me_, me->udata, term);
    }
}

/**
 * 获取当前任期号
 */
int raft_get_current_term(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->current_term;
}
//获取当前日志索引值
int raft_get_current_idx(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    return log_get_current_idx(me->log);
}

/**
 * 设置已提交的日志索引值
 */
void raft_set_commit_idx(raft_server_t* me_, int idx)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    assert(me->commit_idx <= idx);
    assert(idx <= raft_get_current_idx(me_));
    me->commit_idx = idx;
}

/**
 * 设置已应用的日志条目索引值
 */
void raft_set_last_applied_idx(raft_server_t* me_, int idx)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    me->last_applied_idx = idx;
}

/**
 * 获取已应用的日志条目索引值
 */
int raft_get_last_applied_idx(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->last_applied_idx;
}

/**
 * 获取已提交的日志索引值
 */
int raft_get_commit_idx(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->commit_idx;
}

/**
 * 设置server角色
 */
void raft_set_state(raft_server_t* me_, int state)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    /* if became the leader, then update the current leader entry */
    if (state == RAFT_STATE_LEADER)
        me->current_leader = me->node;
    me->state = state;
}

/**
 * 获取server角色
 */
int raft_get_state(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->state;
}
//获取指定节点
raft_node_t* raft_get_node(raft_server_t *me_, int nodeid)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    int i;

    for (i = 0; i < me->num_nodes; i++)
        if (nodeid == raft_node_get_id(me->nodes[i]))
            return me->nodes[i];

    return NULL;
}

/**
 * 获取server的当前节点
 */
raft_node_t* raft_get_my_node(raft_server_t *me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    int i;

    for (i = 0; i < me->num_nodes; i++)
        if (raft_get_nodeid(me_) == raft_node_get_id(me->nodes[i]))
            return me->nodes[i];

    return NULL;
}

/**
 * 根据nodeid获取节点对象
 */
raft_node_t* raft_get_node_from_idx(raft_server_t* me_, const int idx)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    return me->nodes[idx];
}

/**
 * 获取当前的leader 节点号
 */
int raft_get_current_leader(raft_server_t* me_)
{
    raft_server_private_t* me = (void*)me_;
    if (me->current_leader)
        return raft_node_get_id(me->current_leader);
    return -1;
}

/**
 * 获取当前领导者的节点对象
 */
raft_node_t* raft_get_current_leader_node(raft_server_t* me_)
{
    raft_server_private_t* me = (void*)me_;
    return me->current_leader;
}

/**
 * 获取server的用户数据
 */
void* raft_get_udata(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->udata;
}

/**
 * p判断server是否是追随者
 */
int raft_is_follower(raft_server_t* me_)
{
    return raft_get_state(me_) == RAFT_STATE_FOLLOWER;
}

/**
 * 判断server是否时领导者
 */
int raft_is_leader(raft_server_t* me_)
{
    return raft_get_state(me_) == RAFT_STATE_LEADER;
}

/**
 * 判断server是否是候选者
 */
int raft_is_candidate(raft_server_t* me_)
{
    return raft_get_state(me_) == RAFT_STATE_CANDIDATE;
}

/**
 * 获取最后一个日志条目的任期号
 */
int raft_get_last_log_term(raft_server_t* me_)
{
    int current_idx = raft_get_current_idx(me_);
    if (0 < current_idx)
    {
        raft_entry_t* ety = raft_get_entry_from_idx(me_, current_idx);
        if (ety)
            return ety->term;
    }
    return 0;
}

/**
 * 判断server是否已经链接到领导者
 */
int raft_is_connected(raft_server_t* me_)
{
    return ((raft_server_private_t*)me_)->connected;
}
