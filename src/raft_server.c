/**
 * Copyright (c) 2013, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @file
 * @brief Implementation of a Raft server
 * @author Willem Thiart himself@willemthiart.com
 * @version 0.1
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

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) < (b) ? (b) : (a))

static void __log(raft_server_t *me_, const char *fmt, ...)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    char buf[1024];
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);

    if (me->cb.log)
        me->cb.log(me_, me->udata, buf);
}

/**
 * 初始化 raft server对象
 * 默认情况下 请求超时时间为200毫秒，选举超时时间位1000毫秒
 */
raft_server_t* raft_new()
{
    //申请内存
    raft_server_private_t* me =
        (raft_server_private_t*)calloc(1, sizeof(raft_server_private_t));
    if (!me)
        return NULL;
    me->current_term = 0; //当前任期号
    me->voted_for = -1;   //当前获得选票的候选人id
    me->timeout_elapsed = 0; //超时时间，从上一次获得心跳包到现在的时间
    me->request_timeout = 200; //请求超时时间200毫秒
    me->election_timeout = 1000; //选举超时时间1秒
    me->log = log_new(); //log存取对象
    raft_set_state((raft_server_t*)me, RAFT_STATE_FOLLOWER);//设置当前对象位跟随者
    me->current_leader = -1;//当前领导这id为-1
    return (raft_server_t*)me; //返回对象
}

/**
 * 设置raft server 的回调函数
 */
void raft_set_callbacks(raft_server_t* me_, raft_cbs_t* funcs, void* udata)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;//强转对象类型

    memcpy(&me->cb, funcs, sizeof(raft_cbs_t));//拷贝回调函数的内存
    me->udata = udata;//server对象所需要使用的外部数据或者对象
    log_set_callbacks(me->log, &me->cb, me_);//设置log callbacks
}

/**
 * 释放raft server 对象
 */
void raft_free(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;//强转对象类型

    log_free(me->log);//释放日志对象申请的内存
    free(me_);//释放raft server申请的内存
}

/**
 * 选举开始
 */
void raft_election_start(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;

    //记录开始选举的日志
    __log(me_, "election starting: %d %d, term: %d",
          me->election_timeout, me->timeout_elapsed, me->current_term);
    //改变自己身份，成为候选者，进行候选者应该做的工作
    raft_become_candidate(me_);
}
/**
 * 成为领导
 */
void raft_become_leader(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    int i;

    __log(me_, "becoming leader");

    raft_set_state(me_, RAFT_STATE_LEADER);

    for (i = 0; i < me->num_nodes; i++)
    {
        if (me->nodeid != i)
        {
            raft_node_t* node = raft_get_node(me_, i);
            raft_node_set_next_idx(node, raft_get_current_idx(me_) + 1);
            raft_node_set_match_idx(node, 0);
            raft_send_appendentries(me_, i);
        }
    }
}

/**
 * 成为候选者，开始选举
 */
void raft_become_candidate(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    int i;

    __log(me_, "becoming candidate");

    memset(me->votes_for_me, 0, sizeof(int) * me->num_nodes);//设置node number 位置位0
    me->current_term += 1; //当前任期+1
    raft_vote(me_, me->nodeid);//跳票给自己
    me->current_leader = -1;//当前leader = -1
    raft_set_state(me_, RAFT_STATE_CANDIDATE);//设置自己当前状态位候选者

    /* we need a random factor here to prevent simultaneous candidates */
    /* TODO: this should probably be lower */
    /*
     * 选举超时时间 防止多个候选者瓜分选票,
     * 这样就没办法选出leader了所以需要把每个候选者，选举的超时时间随机设置
     */
    me->timeout_elapsed = rand() % me->election_timeout;

    for (i = 0; i < me->num_nodes; i++)//对除了自己以外的所有节点请求投票
        if (me->nodeid != i)
            raft_send_requestvote(me_, i);
}

/**
 * 成为追随者
 */
void raft_become_follower(raft_server_t* me_)
{
    __log(me_, "becoming follower");
    raft_set_state(me_, RAFT_STATE_FOLLOWER);
}

int raft_periodic(raft_server_t* me_, int msec_since_last_period)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;

    me->timeout_elapsed += msec_since_last_period;

    if (me->state == RAFT_STATE_LEADER)
    {
        if (me->request_timeout <= me->timeout_elapsed)
            raft_send_appendentries_all(me_);
    }
    else if (me->election_timeout <= me->timeout_elapsed)
    {
        if (1 == me->num_nodes)
            raft_become_leader(me_);
        else
            raft_election_start(me_);
    }

    if (me->last_applied_idx < me->commit_idx)
        if (-1 == raft_apply_entry(me_))
            return -1;

    return 0;
}

/**
 * 获取指定索引的日志条目
 */
raft_entry_t* raft_get_entry_from_idx(raft_server_t* me_, int etyidx)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    return log_get_from_idx(me->log, etyidx);
}

int raft_recv_appendentries_response(raft_server_t* me_,
                                     int node_idx,
                                     msg_appendentries_response_t* r)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;

    __log(me_,
          "received appendentries response node: %d %s cidx: %d 1stidx: %d",
          node_idx, r->success == 1 ? "success" : "fail", r->current_idx,
          r->first_idx);

    if (!raft_is_leader(me_))
        return -1;

    raft_node_t* node = raft_get_node(me_, node_idx);

    /* If response contains term T > currentTerm: set currentTerm = T
       and convert to follower (§5.3) */
    if (me->current_term < r->term)
    {
        raft_set_current_term(me_, r->term);
        raft_become_follower(me_);
        return 0;
    }
    else if (me->current_term != r->term)
        return 0;

    if (0 == r->success)
    {
        /* If AppendEntries fails because of log inconsistency:
           decrement nextIndex and retry (§5.3) */
        assert(0 <= raft_node_get_next_idx(node));

        int next_idx = raft_node_get_next_idx(node);
        assert(0 <= next_idx);
        if (r->current_idx < next_idx - 1)
            raft_node_set_next_idx(node, min(r->current_idx + 1, raft_get_current_idx(me_)));
        else
            raft_node_set_next_idx(node, next_idx - 1);

        /* retry */
        raft_send_appendentries(me_, node_idx);
        return 0;
    }

    assert(r->current_idx <= raft_get_current_idx(me_));

    /* response to a repeat transmission -- ignore */
    if (raft_node_get_match_idx(node) == r->current_idx)
        return 0;

    raft_node_set_next_idx(node, r->current_idx + 1);
    raft_node_set_match_idx(node, r->current_idx);

    /* Update commit idx */
    int votes = 1; /* include me */
    int point = r->current_idx;
    int i;
    for (i = 0; i < me->num_nodes; i++)
    {
        if (me->nodeid == i)
            continue;

        int match_idx = raft_node_get_match_idx(me->nodes[i]);

        if (0 < match_idx)
        {
            raft_entry_t* ety = raft_get_entry_from_idx(me_, match_idx);
            if (ety->term == me->current_term && point <= match_idx)
                votes++;
        }
    }

    if (me->num_nodes / 2 < votes && raft_get_commit_idx(me_) < point)
        raft_set_commit_idx(me_, point);

    /* Aggressively send remaining entries */
    if (raft_get_entry_from_idx(me_, raft_node_get_next_idx(node)))
        raft_send_appendentries(me_, node_idx);

    /* periodic applies committed entries lazily */

    return 0;
}

int raft_recv_appendentries(
    raft_server_t* me_,
    const int node,
    msg_appendentries_t* ae,
    msg_appendentries_response_t *r
    )
{
    raft_server_private_t* me = (raft_server_private_t*)me_;

    me->timeout_elapsed = 0;

    if (0 < ae->n_entries)
        __log(me_, "recvd appendentries from: %d, %d %d %d %d #%d",
              node,
              ae->term,
              ae->leader_commit,
              ae->prev_log_idx,
              ae->prev_log_term,
              ae->n_entries);

    r->term = me->current_term;

    if (raft_is_candidate(me_) && me->current_term == ae->term)
    {
        me->voted_for = -1;
        raft_become_follower(me_);
    }
    else if (me->current_term < ae->term)
    {
        raft_set_current_term(me_, ae->term);
        raft_become_follower(me_);
    }
    else if (ae->term < me->current_term)
    {
        /* 1. Reply false if term < currentTerm (§5.1) */
        __log(me_, "AE term is less than current term");
        goto fail_with_current_idx;
    }

    /* Not the first appendentries we've received */
    /* NOTE: the log starts at 1 */
    if (0 < ae->prev_log_idx)
    {
        raft_entry_t* e = raft_get_entry_from_idx(me_, ae->prev_log_idx);

        if (!e)
        {
            __log(me_, "AE no log at prev_idx %d", ae->prev_log_idx);
            goto fail_with_current_idx;
        }

        /* 2. Reply false if log doesn't contain an entry at prevLogIndex
           whose term matches prevLogTerm (§5.3) */
        if (raft_get_current_idx(me_) < ae->prev_log_idx)
            goto fail_with_current_idx;

        if (e->term != ae->prev_log_term)
        {
            __log(me_, "AE term doesn't match prev_idx (ie. %d vs %d)",
                  e->term, ae->prev_log_term);
            assert(me->commit_idx < ae->prev_log_idx);
            /* Delete all the following log entries because they don't match */
            log_delete(me->log, ae->prev_log_idx);
            r->current_idx = ae->prev_log_idx - 1;
            goto fail;
        }
    }

    /* 3. If an existing entry conflicts with a new one (same index
       but different terms), delete the existing entry and all that
       follow it (§5.3) */
    if (ae->n_entries == 0 && 0 < ae->prev_log_idx && ae->prev_log_idx + 1 < raft_get_current_idx(me_))
    {
        assert(me->commit_idx < ae->prev_log_idx + 1);
        log_delete(me->log, ae->prev_log_idx + 1);
    }

    r->current_idx = ae->prev_log_idx;

    int i;
    for (i = 0; i < ae->n_entries; i++)
    {
        msg_entry_t* ety = &ae->entries[i];
        int ety_index = ae->prev_log_idx + 1 + i;
        raft_entry_t* existing_ety = raft_get_entry_from_idx(me_, ety_index);
        r->current_idx = ety_index;
        if (existing_ety && existing_ety->term != ety->term)
        {
            assert(me->commit_idx < ety_index);
            log_delete(me->log, ety_index);
            break;
        }
        else if (!existing_ety)
            break;
    }

    /* Pick up remainder in case of mismatch or missing entry */
    for (; i < ae->n_entries; i++)
    {
        int e = raft_append_entry(me_, &ae->entries[i]);
        if (-1 == e)
            goto fail_with_current_idx;

        r->current_idx = ae->prev_log_idx + 1 + i;
    }

    /* 4. If leaderCommit > commitIndex, set commitIndex =
        min(leaderCommit, index of most recent entry) */
    if (raft_get_commit_idx(me_) < ae->leader_commit)
    {
        int last_log_idx = max(raft_get_current_idx(me_), 1);
        raft_set_commit_idx(me_, min(last_log_idx, ae->leader_commit));
    }

    /* update current leader because we accepted appendentries from it */
    me->current_leader = node;

    r->success = 1;
    r->first_idx = ae->prev_log_idx + 1;
    return 0;

fail_with_current_idx:
    r->current_idx = raft_get_current_idx(me_);
fail:
    r->success = 0;
    r->first_idx = 0;
    return -1;
}

static int __should_grant_vote(raft_server_private_t* me, msg_requestvote_t* vr)
{
    if (vr->term < raft_get_current_term((void*)me))
        return 0;

    /* TODO: if voted for is candiate return 1 (if below checks pass) */
    /* we've already voted */
    if (-1 != me->voted_for)
        return 0;

    int current_idx = raft_get_current_idx((void*)me);

    if (0 == current_idx)
        return 1;

    raft_entry_t* e = raft_get_entry_from_idx((void*)me, current_idx);
    if (e->term < vr->last_log_term)
        return 1;

    if (vr->last_log_term == e->term && current_idx <= vr->last_log_idx)
        return 1;

    return 0;
}

int raft_recv_requestvote(raft_server_t* me_,
                          int node,
                          msg_requestvote_t* vr,
                          msg_requestvote_response_t *r)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;

    if (raft_get_current_term(me_) < vr->term)
    {
        raft_set_current_term(me_, vr->term);
        raft_become_follower(me_);
    }

    if (__should_grant_vote(me, vr))
    {
        /* It shouldn't be possible for a leader or candidate to grant a vote
         * Both states would have voted for themselves */
        assert(!(raft_is_leader(me_) || raft_is_candidate(me_)));

        raft_vote(me_, node);
        r->vote_granted = 1;

        /* there must be in an election. */
        me->current_leader = -1;

        me->timeout_elapsed = 0;
    }
    else
        r->vote_granted = 0;

    __log(me_, "node requested vote: %d replying: %s",
          node, r->vote_granted == 1 ? "granted" : "not granted");

    r->term = raft_get_current_term(me_);
    return 0;
}

int raft_votes_is_majority(const int num_nodes, const int nvotes)
{
    if (num_nodes < nvotes)
        return 0;
    int half = num_nodes / 2;
    return half + 1 <= nvotes;
}

int raft_recv_requestvote_response(raft_server_t* me_,
                                   int node,
                                   msg_requestvote_response_t* r)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;

    __log(me_, "node responded to requestvote: %d status: %s",
          node, r->vote_granted == 1 ? "granted" : "not granted");

    if (!raft_is_candidate(me_))
        return 0;

    assert(node < me->num_nodes);

    if (raft_get_current_term(me_) < r->term)
    {
        raft_set_current_term(me_, r->term);
        raft_become_follower(me_);
        return 0;
    }
    else if (raft_get_current_term(me_) != r->term)
    {
        /* The node who voted for us would have obtained our term.
         * Therefore this is an old message we should ignore.
         * This happens if the network is pretty choppy. */
        return 0;
    }

    if (1 == r->vote_granted)
    {
        me->votes_for_me[node] = 1;
        int votes = raft_get_nvotes_for_me(me_);
        if (raft_votes_is_majority(me->num_nodes, votes))
            raft_become_leader(me_);
    }

    return 0;
}

int raft_recv_entry(raft_server_t* me_, int node, msg_entry_t* e,
                    msg_entry_response_t *r)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    int i;

    if (!raft_is_leader(me_))
        return -1;

    __log(me_, "received entry from: %d", node);

    raft_entry_t ety;
    ety.term = me->current_term;
    ety.id = e->id;
    memcpy(&ety.data, &e->data, sizeof(raft_entry_data_t));
    raft_append_entry(me_, &ety);
    for (i = 0; i < me->num_nodes; i++)
        /* Only send new entries.
         * Don't send the entry to peers who are behind, to prevent them from
         * becomming congested. */
        if (me->nodeid != i)
        {
            int next_idx = raft_node_get_next_idx(raft_get_node(me_, i));
            if (next_idx == raft_get_current_idx(me_))
                raft_send_appendentries(me_, i);
        }

    /* if we're the only node, we can consider the entry committed */
    if (1 == me->num_nodes)
        me->commit_idx = raft_get_current_idx(me_);

    r->id = e->id;
    r->idx = raft_get_current_idx(me_);
    r->term = me->current_term;
    return 0;
}

/**
 * 请求投票
 */
int raft_send_requestvote(raft_server_t* me_, int node)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    msg_requestvote_t rv;//请求投票消息

    __log(me_, "sending requestvote to: %d", node);//记录发起了投票

    rv.term = me->current_term;//任期为raft server对象的当前任期
    rv.last_log_idx = raft_get_current_idx(me_);//当前日志的idx
    rv.last_log_term = raft_get_last_log_term(me_);//最后一个日志的任期号
    rv.candidate_id = raft_get_nodeid(me_);//候选者nodeid

    if (me->cb.send_requestvote)//如果存在请求投票回调函数，这执行请求投票回调函数
        me->cb.send_requestvote(me_, me->udata, node, &rv);
    return 0;
}

/**
 * 添加日志到当前raft server的日志条目中
 */
int raft_append_entry(raft_server_t* me_, raft_entry_t* ety)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    return log_append_entry(me->log, ety);
}

/**
 * 把日志应用到状态机
 */
int raft_apply_entry(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;

    /* Don't apply after the commit_idx */
    //最后应用到状态机的id 不能等于最后提交id
    if (me->last_applied_idx == me->commit_idx)
        return -1;
    //获取最后applyid+1的日志
    raft_entry_t* e = raft_get_entry_from_idx(me_, me->last_applied_idx + 1);
    if (!e)
        return -1;

    __log(me_, "applying log: %d, size: %d", me->last_applied_idx, e->data.len);

    me->last_applied_idx++;
    if (me->cb.applylog)//如果存在apply回调函数，则回调
        me->cb.applylog(me_, me->udata, e->data.buf, e->data.len);
    return 0;
}

void raft_send_appendentries(raft_server_t* me_, int node_idx)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;

    if (!(me->cb.send_appendentries))
        return;

    raft_node_t* node = raft_get_node(me_, node_idx);

    msg_appendentries_t ae;
    ae.term = me->current_term;
    ae.leader_commit = raft_get_commit_idx(me_);
    ae.prev_log_idx = 0;
    ae.prev_log_term = 0;
    ae.n_entries = 0;
    ae.entries = NULL;

    int next_idx = raft_node_get_next_idx(node);

    msg_entry_t mety;

    raft_entry_t* ety = raft_get_entry_from_idx(me_, next_idx);
    if (ety)
    {
        mety.term = ety->term;
        mety.id = ety->id;
        mety.data.len = ety->data.len;
        mety.data.buf = ety->data.buf;
        ae.entries = &mety;
        // TODO: we want to send more than 1 at a time
        ae.n_entries = 1;
    }

    /* previous log is the log just before the new logs */
    if (1 < next_idx)
    {
        raft_entry_t* prev_ety = raft_get_entry_from_idx(me_, next_idx - 1);
        ae.prev_log_idx = next_idx - 1;
        if (prev_ety)
            ae.prev_log_term = prev_ety->term;
    }

    __log(me_, "sending appendentries node: %d, %d %d %d %d",
          node_idx,
          ae.term,
          ae.leader_commit,
          ae.prev_log_idx,
          ae.prev_log_term);

    me->cb.send_appendentries(me_, me->udata, node_idx, &ae);
}

void raft_send_appendentries_all(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    int i;

    me->timeout_elapsed = 0;
    for (i = 0; i < me->num_nodes; i++)
        if (me->nodeid != i)
            raft_send_appendentries(me_, i);
}

void raft_set_configuration(raft_server_t* me_,
                            raft_node_configuration_t* nodes, int my_idx)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    int num_nodes;

    /* TODO: one memory allocation only please */
    for (num_nodes = 0; nodes->udata_address; nodes++)
    {
        num_nodes++;
        me->nodes =
            (raft_node_t*)realloc(me->nodes, sizeof(raft_node_t*) * num_nodes);
        me->num_nodes = num_nodes;
        me->nodes[num_nodes - 1] = raft_node_new(nodes->udata_address);
    }
    me->votes_for_me = (int*)calloc(num_nodes, sizeof(int));
    me->nodeid = my_idx;
}

int raft_add_node(raft_server_t* me_, void* udata, int is_self)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;

    /* TODO: does not yet support dynamic membership changes */
    if (me->current_term != 0 && me->timeout_elapsed != 0 &&
        me->election_timeout != 0)
        return -1;

    me->num_nodes++;
    me->nodes =
        (raft_node_t*)realloc(me->nodes, sizeof(raft_node_t*) * me->num_nodes);
    me->nodes[me->num_nodes - 1] = raft_node_new(udata);
    me->votes_for_me =
        (int*)realloc(me->votes_for_me, me->num_nodes * sizeof(int));
    me->votes_for_me[me->num_nodes - 1] = 0;
    if (is_self)
        me->nodeid = me->num_nodes - 1;
    return 0;
}

int raft_get_nvotes_for_me(raft_server_t* me_)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    int i, votes;

    for (i = 0, votes = 0; i < me->num_nodes; i++)
        if (me->nodeid != i)
            if (1 == me->votes_for_me[i])
                votes += 1;

    if (me->voted_for == me->nodeid)
        votes += 1;

    return votes;
}
/**
 * 投票
 */
void raft_vote(raft_server_t* me_, const int node)
{
    raft_server_private_t* me = (raft_server_private_t*)me_;
    me->voted_for = node;
    if (me->cb.persist_vote)
        me->cb.persist_vote(me_, me->udata, node);
}

int raft_msg_entry_response_committed(raft_server_t* me_,
                                      const msg_entry_response_t* r)
{
    raft_entry_t* ety = raft_get_entry_from_idx(me_, r->idx);
    if (!ety)
        return 0;

    /* entry from another leader has invalidated this entry message */
    if (r->term != ety->term)
        return -1;
    return r->idx <= raft_get_commit_idx(me_);
}
