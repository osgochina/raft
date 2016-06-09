#include "raft.h"
#ifndef RAFT_PRIVATE_H_
#define RAFT_PRIVATE_H_

/**
 * Copyright (c) 2013, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. 
 *
 * @file
 * @author Willem Thiart himself@willemthiart.com
 * @version 0.1
 */
//raft server 真正对象
typedef struct {
    /* Persistent state: */

    /* the server's best guess of what the current term is
     * starts at zero */
    //当前任期号，初始化的时候默认是0
    int current_term;

    /* The candidate the server voted for in its current term,
     * or Nil if it hasn't voted for any.  */
    //当前获得选票的候选人id
    int voted_for;

    /* the log which is replicated */
    //log 处理对象
    void* log;

    /* Volatile state: */

    /* idx of highest log entry known to be committed */
    //已知的最大的已经被提交的日志条目的索引值
    int commit_idx;

    /* idx of highest log entry applied to state machine */
    //最后被应用到状态机的日志条目索引值（初始化为 0，持续递增）
    int last_applied_idx;

    /* follower/leader/candidate indicator */
    //当前server的状态 跟随者/领导/候选人
    int state;

    /* amount of time left till timeout */
    //超时时间，从上一次获得心跳包到现在的时间
    int timeout_elapsed;

    /* who has voted for me. This is an array with N = 'num_nodes' elements */
    //得票数 谁跳票给我了，这个一个长度等于node数量的int数组
    int *votes_for_me;

    //节点对象
    raft_node_t* nodes;
    //节点数量
    int num_nodes;

    //选举超时时间
    int election_timeout;
    //响应超时时间
    int request_timeout;

    /* what this node thinks is the node ID of the current leader, or -1 if
     * there isn't a known current leader. */
    //当前的领导id 这个值只会为当前领导的nodeid或者-1,等于-1的时候说明还不知道当前领导是谁
    int current_leader;

    /* callbacks */
    //回调函数列表
    raft_cbs_t cb;
    void* udata;

    /* my node ID */
    //当前nodeid
    int nodeid;
} raft_server_private_t;

//开始选举
void raft_election_start(raft_server_t* me);

//成为领导
void raft_become_leader(raft_server_t* me);
//成为候选人
void raft_become_candidate(raft_server_t* me);
//成为追随者
void raft_become_follower(raft_server_t* me);
//选举投票
void raft_vote(raft_server_t* me, int node);
//设置当前learder的任期号
void raft_set_current_term(raft_server_t* me,int term);

/**
 * @return 0 on error */
//发送投票请求
int raft_send_requestvote(raft_server_t* me, int node);

void raft_send_appendentries(raft_server_t* me, int node);

void raft_send_appendentries_all(raft_server_t* me_);

/**
 * Apply entry at lastApplied + 1. Entry becomes 'committed'.
 * @return 1 if entry committed, 0 otherwise */
int raft_apply_entry(raft_server_t* me_);

/**
 * Appends entry using the current term.
 * Note: we make the assumption that current term is up-to-date
 * @return 0 if unsuccessful */
int raft_append_entry(raft_server_t* me_, raft_entry_t* c);

void raft_set_commit_idx(raft_server_t* me, int commit_idx);
int raft_get_commit_idx(raft_server_t* me_);

void raft_set_last_applied_idx(raft_server_t* me, int idx);

void raft_set_state(raft_server_t* me_, int state);

int raft_get_state(raft_server_t* me_);

raft_node_t* raft_node_new(void* udata);

void raft_node_set_next_idx(raft_node_t* node, int nextIdx);

void raft_node_set_match_idx(raft_node_t* node, int matchIdx);

int raft_node_get_match_idx(raft_node_t* me_);

int raft_votes_is_majority(const int nnodes, const int nvotes);

#endif /* RAFT_PRIVATE_H_ */
