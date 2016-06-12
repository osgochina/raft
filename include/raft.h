/**
 * Copyright (c) 2013, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @file
 * @author Willem Thiart himself@willemthiart.com
 * @version 0.1
 */

#ifndef RAFT_H_
#define RAFT_H_

typedef enum {
    RAFT_STATE_NONE,
    RAFT_STATE_FOLLOWER, //跟随者
    RAFT_STATE_CANDIDATE, //候选人
    RAFT_STATE_LEADER  //领导
} raft_state_e;
//日志内容结构体
typedef struct
{
    void *buf;

    unsigned int len;
} raft_entry_data_t;

/** Entry that is stored in the server's entry log. */
//日志条目结构体
typedef struct
{
    /** the entry's term at the point it was created */
    unsigned int term;//任期号

    /** the entry's unique ID */
    unsigned int id;//日志唯一id

    raft_entry_data_t data;
} raft_entry_t;

/** Message sent from client to server.
 * The client sends this message to a server with the intention of having it
 * applied to the FSM. */
/**
 * 客户端向服务端发送消息的结构体
 */
typedef raft_entry_t msg_entry_t;

/** Entry message response.
 * Indicates to client if entry was committed or not. */
/**
 * 日志条目响应
 */
typedef struct
{
    /** the entry's unique ID */
    unsigned int id;//当前日志的唯一id

    /** the entry's term */
    int term;//任期号

    /** the entry's index */
    int idx;//索引
} msg_entry_response_t;

/** Vote request message.
 * Sent to nodes when a server wants to become leader.
 * This message could force a leader/candidate to become a follower. */
//请求投票 RPC
typedef struct
{
    /** currentTerm, to force other leader/candidate to step down */
    int term;//候选人的任期号

    /** candidate requesting vote */
    int candidate_id; //请求选票的候选人的 Id

    /** index of candidate's last log entry */
    int last_log_idx;//候选人的最后日志条目的索引值

    /** term of candidate's last log entry */
    int last_log_term;//候选人最后日志条目的任期号
} msg_requestvote_t;

/** Vote request response message.
 * Indicates if node has accepted the server's vote request. */
/**
 * 请求投票响应
 */
typedef struct
{
    /** currentTerm, for candidate to update itself */
    int term;//当前任期号，以便于候选人去更新自己的任期号

    /** true means candidate received vote */
    int vote_granted;//候选人赢得了此张选票时为真
} msg_requestvote_response_t;

/** Appendentries message.
 * This message is used to tell nodes if it's safe to apply entries to the FSM.
 * Can be sent without any entries as a keep alive message.
 * This message could force a leader/candidate to become a follower. */
/**
 * 附加日志 RPC：
 *
 */
typedef struct
{
    /** currentTerm, to force other leader/candidate to step down */
    int term;//此消息的任期号

    /** the index of the log just before the newest entry for the node who
     * receives this message */
    int prev_log_idx;//新的日志条目紧随之前的索引值

    /** the term of the log just before the newest entry for the node who
     * receives this message */
    int prev_log_term;//这条消息的上个上的的任期号

    /** the index of the entry that has been appended to the majority of the
     * cluster. Entries up to this index will be applied to the FSM */
    int leader_commit;//领导人已经提交的日志的索引值

    /** number of entries within this message */
    int n_entries;//这个消息的日志条目数

    /** array of entries within this message */
    msg_entry_t* entries;//需要存储当然日志条目（表示心跳时为空；一次性发送多个是为了提高效率）
} msg_appendentries_t;

/** Appendentries response message.
 * Can be sent without any entries as a keep alive message.
 * This message could force a leader/candidate to become a follower. */
/**
 * 附加日志响应
 */
typedef struct
{
    /** currentTerm, to force other leader/candidate to step down */
    int term;//当前的任期号，用于领导人去更新自己

    /** true if follower contained entry matching prevLogidx and prevLogTerm */
    int success;//跟随者包含了匹配上 prevLogIndex 和 prevLogTerm 的日志时为真

    /* Non-Raft fields follow: */
    /* Having the following fields allows us to do less book keeping in
     * regards to full fledged RPC */

    /** This is the highest log IDX we've received and appended to our log */
    int current_idx;//当前日志idx

    /** The first idx that we received within the appendentries message */
    int first_idx;//收到消息中第一条日志的索引值
} msg_appendentries_response_t;

typedef void* raft_server_t; //对外提供的raft server对象
typedef void* raft_node_t; //对外提供的raft node 对象

/** Callback for sending request vote messages.
 * @param[in] raft The Raft server making this callback
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] node The node's ID that we are sending this message to
 * @param[in] msg The request vote message to be sent
 * @return 0 on success */
typedef int (
*func_send_requestvote_f
)   (
    raft_server_t* raft,
    void *user_data,
    int node,
    msg_requestvote_t* msg
    );

/** Callback for sending append entries messages.
 * @param[in] raft The Raft server making this callback
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] node The node's ID that we are sending this message to
 * @param[in] msg The appendentries message to be sent
 * @return 0 on success */
typedef int (
*func_send_appendentries_f
)   (
    raft_server_t* raft,
    void *user_data,
    int node,
    msg_appendentries_t* msg
    );

#ifndef HAVE_FUNC_LOG
#define HAVE_FUNC_LOG
/** Callback for providing debug logging information.
 * This callback is optional
 * @param[in] raft The Raft server making this callback
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] buf The buffer that was logged */
typedef void (
*func_log_f
)    (
    raft_server_t* raft,
    void *user_data,
    const char *buf
    );
#endif

/** Callback for applying this log entry to the state machine.
 * @param[in] raft The Raft server making this callback
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] data Data to be applied to the log
 * @param[in] len Length in bytes of data to be applied
 * @return 0 on success */
typedef int (
*func_applylog_f
)   (
    raft_server_t* raft,
    void *user_data,
    const unsigned char *log_data,
    const int log_len
    );

/** Callback for saving who we voted for to disk.
 * For safety reasons this callback MUST flush the change to disk.
 * @param[in] raft The Raft server making this callback
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] voted_for The node we voted for
 * @return 0 on success */
typedef int (
*func_persist_int_f
)   (
    raft_server_t* raft,
    void *user_data,
    const int voted_for
    );

/** Callback for saving log entry changes.
 *
 * This callback is used for:
 * <ul>
 *      <li>Adding entries to the log (ie. offer)
 *      <li>Removing the first entry from the log (ie. polling)
 *      <li>Removing the last entry from the log (ie. popping)
 * </ul>
 *
 * For safety reasons this callback MUST flush the change to disk.
 *
 * @param[in] raft The Raft server making this callback
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] entry The entry that the event is happening to.
 *    The user is allowed to change the memory pointed to in the
 *    raft_entry_data_t struct. This MUST be done if the memory is temporary.
 * @param[in] entry_idx The entries index in the log
 * @return 0 on success */
typedef int (
*func_logentry_event_f
)   (
    raft_server_t* raft,
    void *user_data,
    raft_entry_t *entry,
    int entry_idx
    );

//raft server 回调函数
typedef struct
{
    /** Callback for sending request vote messages */
    func_send_requestvote_f send_requestvote;

    /** Callback for sending appendentries messages */
    func_send_appendentries_f send_appendentries;

    /** Callback for finite state machine application */
    func_applylog_f applylog;

    /** Callback for persisting vote data
     * For safety reasons this callback MUST flush the change to disk. */
    func_persist_int_f persist_vote;

    /** Callback for persisting term data
     * For safety reasons this callback MUST flush the change to disk. */
    func_persist_int_f persist_term;

    /** Callback for adding an entry to the log
     * For safety reasons this callback MUST flush the change to disk. */
    func_logentry_event_f log_offer;

    /** Callback for removing the oldest entry from the log
     * For safety reasons this callback MUST flush the change to disk.
     * @note If memory was malloc'd in log_offer then this should be the right
     *  time to free the memory. */
    func_logentry_event_f log_poll;

    /** Callback for removing the youngest entry from the log
     * For safety reasons this callback MUST flush the change to disk.
     * @note If memory was malloc'd in log_offer then this should be the right
     *  time to free the memory. */
    func_logentry_event_f log_pop;

    /** Callback for catching debugging log messages
     * This callback is optional */
    func_log_f log;
} raft_cbs_t;

typedef struct
{
    /** User data pointer for addressing.
     * Examples of what this could be:
     * - void* pointing to implementor's networking data
     * - a (IP,Port) tuple */
    void* udata_address;
} raft_node_configuration_t;

/** Initialise a new Raft server.
 *
 * Request timeout defaults to 200 milliseconds
 * Election timeout defaults to 1000 milliseconds
 *
 * @return newly initialised Raft server */
/**
 * 初始化raft server
 * 然后一个已经被初始化好的raft server
 * 默认情况下 请求超时时间为200毫秒，选举超时时间位1000毫秒
 * @return newly 一个初始化好的raft server
 */
raft_server_t* raft_new();

/** De-initialise Raft server.
 * Frees all memory */
/**
 * 释放raft server 对象
 */
void raft_free(raft_server_t* me);

/** Set callbacks and user data.
 *
 * @param[in] funcs Callbacks
 * @param[in] user_data "User data" - user's context that's included in a callback */
void raft_set_callbacks(raft_server_t* me, raft_cbs_t* funcs, void* user_data);

/** Set configuration.
 *
 * @deprecated This function has been replaced by raft_add_node and
 * raft_remove_node
 *
 * @param[in] nodes Array of nodes. End of array is marked by NULL entry
 * @param[in] my_idx Index of the node that refers to this Raft server */
void raft_set_configuration(raft_server_t* me,
                            raft_node_configuration_t* nodes, int my_idx)
__attribute__ ((deprecated));

/** Add node.
 *
 * @note This library does not yet support membership changes.
 *  Once raft_periodic has been run this will fail.
 *
 * @note The order this call is made is important.
 *  This call MUST be made in the same order as the other raft nodes.
 *  This is because the node ID is assigned depending on when this call is made
 *
 * @param[in] user_data The user data for the node.
 *  This is obtained using raft_node_get_udata.
 *  Examples of what this could be:
 *  - void* pointing to implementor's networking data
 *  - a (IP,Port) tuple
 * @param[in] is_self Set to 1 if this "node" is this server
 * @return 0 on success; otherwise -1 */
int raft_add_node(raft_server_t* me, void* user_data, int is_self);

#define raft_add_peer raft_add_node

/** Set election timeout.
 * The amount of time that needs to elapse before we assume the leader is down
 * @param[in] msec Election timeout in milliseconds */
void raft_set_election_timeout(raft_server_t* me, int msec);

/** Set request timeout in milliseconds.
 * The amount of time before we resend an appendentries message
 * @param[in] msec Request timeout in milliseconds */
void raft_set_request_timeout(raft_server_t* me, int msec);

/** Process events that are dependent on time passing.
 * @param[in] msec_elapsed Time in milliseconds since the last call
 * @return 0 on success */
int raft_periodic(raft_server_t* me, int msec_elapsed);

/** Receive an appendentries message.
 *
 * Will block (ie. by syncing to disk) if we need to append a message.
 *
 * Might call malloc once to increase the log entry array size.
 *
 * The log_offer callback will be called.
 *
 * @note The memory pointer (ie. raft_entry_data_t) for each msg_entry_t is
 *   copied directly. If the memory is temporary you MUST either make the
 *   memory permanent (ie. via malloc) OR re-assign the memory within the
 *   log_offer callback.
 *
 * @param[in] node Index of the node who sent us this message
 * @param[in] ae The appendentries message
 * @param[out] r The resulting response
 * @return 0 on success */
int raft_recv_appendentries(raft_server_t* me,
                            int node,
                            msg_appendentries_t* ae,
                            msg_appendentries_response_t *r);

/** Receive a response from an appendentries message we sent.
 * @param[in] node Index of the node who sent us this message
 * @param[in] r The appendentries response message
 * @return 0 on success */
int raft_recv_appendentries_response(raft_server_t* me,
                                     int node,
                                     msg_appendentries_response_t* r);

/** Receive a requestvote message.
 * @param[in] node Index of the node who sent us this message
 * @param[in] vr The requestvote message
 * @param[out] r The resulting response
 * @return 0 on success */
int raft_recv_requestvote(raft_server_t* me,
                          int node,
                          msg_requestvote_t* vr,
                          msg_requestvote_response_t *r);

/** Receive a response from a requestvote message we sent.
 * @param[in] node The node this response was sent by
 * @param[in] r The requestvote response message
 * @return 0 on success */
int raft_recv_requestvote_response(raft_server_t* me,
                                   int node,
                                   msg_requestvote_response_t* r);

/** Receive an entry message from the client.
 *
 * Append the entry to the log and send appendentries to followers.
 *
 * Will block (ie. by syncing to disk) if we need to append a message.
 *
 * Might call malloc once to increase the log entry array size.
 *
 * The log_offer callback will be called.
 *
 * @note The memory pointer (ie. raft_entry_data_t) in msg_entry_t is
 *  copied directly. If the memory is temporary you MUST either make the
 *  memory permanent (ie. via malloc) OR re-assign the memory within the
 *  log_offer callback.
 *
 * Will fail:
 * <ul>
 *      <li>if the server is not the leader
 * </ul>
 *
 * @param[in] node Index of the node who sent us this message
 * @param[in] ety The entry message
 * @param[out] r The resulting response
 * @return 0 on success, -1 on failure */
int raft_recv_entry(raft_server_t* me,
                    int node,
                    msg_entry_t* ety,
                    msg_entry_response_t *r);

/**
 * @return the server's node ID */
int raft_get_nodeid(raft_server_t* me);

/**
 * @return currently configured election timeout in milliseconds */
int raft_get_election_timeout(raft_server_t* me);

/**
 * @return number of nodes that this server has */
int raft_get_num_nodes(raft_server_t* me);

/**
 * @return number of items within log */
int raft_get_log_count(raft_server_t* me);

/**
 * @return current term */
int raft_get_current_term(raft_server_t* me);

/**
 * @return current log index */
int raft_get_current_idx(raft_server_t* me);

/**
 * @return 1 if follower; 0 otherwise */
int raft_is_follower(raft_server_t* me);

/**
 * @return 1 if leader; 0 otherwise */
int raft_is_leader(raft_server_t* me);

/**
 * @return 1 if candidate; 0 otherwise */
int raft_is_candidate(raft_server_t* me);

/**
 * @return currently elapsed timeout in milliseconds */
int raft_get_timeout_elapsed(raft_server_t* me);

/**
 * @return request timeout in milliseconds */
int raft_get_request_timeout(raft_server_t* me);

/**
 * @return index of last applied entry */
int raft_get_last_applied_idx(raft_server_t* me);

/**
 * @return 1 if node is leader; 0 otherwise */
int raft_node_is_leader(raft_node_t* node);

/**
 * @return the node's next index */
int raft_node_get_next_idx(raft_node_t* node);

/**
 * @return this node's user data */
int raft_node_get_match_idx(raft_node_t* me);

/**
 * @return this node's user data */
void* raft_node_get_udata(raft_node_t* me);

/**
 * Set this node's user data */
void raft_node_set_udata(raft_node_t* me, void* user_data);

/**
 * @param[in] idx The entry's index
 * @return entry from index */
raft_entry_t* raft_get_entry_from_idx(raft_server_t* me, int idx);

/**
 * @param[in] node The node's index
 * @return node pointed to by node index */
raft_node_t* raft_get_node(raft_server_t *me, int node);

/**
 * @return number of votes this server has received this election */
int raft_get_nvotes_for_me(raft_server_t* me);

/**
 * @return node ID of who I voted for */
int raft_get_voted_for(raft_server_t* me);

/** Get what this node thinks the node ID of the leader is.
 * @return node of what this node thinks is the valid leader;
 *   -1 if the leader is unknown */
int raft_get_current_leader(raft_server_t* me);

/**
 * @return callback user data */
void* raft_get_udata(raft_server_t* me);

/**
 * @return this server's node ID */
int raft_get_my_id(raft_server_t* me);

/** Vote for a server.
 * This should be used to reload persistent state, ie. the voted-for field.
 * @param[in] node The server to vote for */
void raft_vote(raft_server_t* me, const int node);

/** Set the current term.
 * This should be used to reload persistent state, ie. the current_term field.
 * @param[in] term The new current term */
void raft_set_current_term(raft_server_t* me, const int term);

/** Add an entry to the server's log.
 * This should be used to reload persistent state, ie. the commit log.
 * @param[in] ety The entry to be appended */
int raft_append_entry(raft_server_t* me, raft_entry_t* ety);

/** Confirm if a msg_entry_response has been committed.
 * @param[in] r The response we want to check */
int raft_msg_entry_response_committed(raft_server_t* me_,
                                      const msg_entry_response_t* r);

/** Tell if we are a leader, candidate or follower.
 * @return get state of type raft_state_e. */
int raft_get_state(raft_server_t* me_);

/** The the most recent log's term
 * @return the last log term */
int raft_get_last_log_term(raft_server_t* me_);

#endif /* RAFT_H_ */
