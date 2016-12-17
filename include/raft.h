/**
 * Copyright (c) 2013, Willem-Hendrik Thiart
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @file
 * @author Willem Thiart himself@willemthiart.com
 */

#ifndef RAFT_H_
#define RAFT_H_

#define RAFT_ERR_NOT_LEADER                  -2  //不是领导者
#define RAFT_ERR_ONE_VOTING_CHANGE_ONLY      -3  //
#define RAFT_ERR_SHUTDOWN                    -4  //已停止

#define RAFT_REQUESTVOTE_ERR_GRANTED          1  //同意
#define RAFT_REQUESTVOTE_ERR_NOT_GRANTED      0  //不同意
#define RAFT_REQUESTVOTE_ERR_UNKNOWN_NODE    -1  //不知道的节点


/**
 * 服务状态枚举
 */
typedef enum {
    RAFT_STATE_NONE,//没有
    RAFT_STATE_FOLLOWER, //跟随者
    RAFT_STATE_CANDIDATE, //候选人
    RAFT_STATE_LEADER  //领导
} raft_state_e;


/**
 * 日志类型枚举
 */
typedef enum {
    RAFT_LOGTYPE_NORMAL,                //正常的日志类型
    RAFT_LOGTYPE_ADD_NONVOTING_NODE,    //新增无投票权限节点
    RAFT_LOGTYPE_ADD_NODE,              //新增节点
    RAFT_LOGTYPE_DEMOTE_NODE,           //降级日志
    RAFT_LOGTYPE_REMOVE_NODE,           //移除节点
    RAFT_LOGTYPE_NUM,
} raft_logtype_e;

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

    /** type of entry */
    int type;//日志类型

    raft_entry_data_t data;//日志内容
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

/** Callback for sending request vote messages.  投票请求消息回调
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
    raft_node_t* node,
    msg_requestvote_t* msg
    );

/** Callback for sending append entries messages.  发送日志消息回调
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
    raft_node_t* node,
    msg_appendentries_t* msg
    );

/**
 * 该节点已经有了足够的日志，可以变为有投票权限的节点了，回调这个函数
 * Callback for detecting when non-voting nodes have obtained enough logs.
 * This triggers only when there are no pending configuration changes.
 * @param[in] raft The Raft server making this callback
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] node The node
 * @return 0 does not want to be notified again; otherwise -1 */
typedef int (
*func_node_has_sufficient_logs_f
)   (
    raft_server_t* raft,
    void *user_data,
    raft_node_t* node
    );

#ifndef HAVE_FUNC_LOG
#define HAVE_FUNC_LOG
/** Callback for providing debug logging information. 调试信息 回调
 * This callback is optional
 * @param[in] raft The Raft server making this callback
 * @param[in] node The node that is the subject of this log. Could be NULL.
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] buf The buffer that was logged */
typedef void (
*func_log_f
)    (
    raft_server_t* raft,
    raft_node_t* node,
    void *user_data,
    const char *buf
    );
#endif

/** Callback for saving who we voted for to disk.  保存投票结果到硬盘
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
    int node
    );

/** Callback for saving log entry changes.
 * 保存日志消息的方法定义
 * This callback is used for:
 * <ul>
 *      <li>Adding entries to the log (ie. offer)</li>
 *      <li>Removing the first entry from the log (ie. polling)</li>
 *      <li>Removing the last entry from the log (ie. popping)</li>
 *      <li>Applying entries</li>
 * </ul>
 *
 * For safety reasons this callback MUST flush the change to disk.
 *
 * @param[in] raft The Raft server making this callback
 * @param[in] user_data User data that is passed from Raft server
 * @param[in] entry The entry that the event is happening to.
 *    For offering, polling, and popping, the user is allowed to change the
 *    memory pointed to in the raft_entry_data_t struct. This MUST be done if
 *    the memory is temporary.
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

//raft server 回调函数定义
typedef struct
{
    /** Callback for sending request vote messages */
    //发送投票消息请求
    func_send_requestvote_f send_requestvote;

    /** Callback for sending appendentries messages */
    //发送附加日志条目rpc请求
    func_send_appendentries_f send_appendentries;

    /** Callback for finite state machine application
     * Return 0 on success.
     * Return RAFT_ERR_SHUTDOWN if you want the server to shutdown. */
    //应用日志条目到状态机
    func_logentry_event_f applylog;

    /** Callback for persisting vote data
     * For safety reasons this callback MUST flush the change to disk. */
    //保存投票结果到存储
    func_persist_int_f persist_vote;

    /** Callback for persisting term data
     * For safety reasons this callback MUST flush the change to disk. */
    //保存任期号到存储
    func_persist_int_f persist_term;

    /** Callback for adding an entry to the log
     * For safety reasons this callback MUST flush the change to disk.
     * Return 0 on success.
     * Return RAFT_ERR_SHUTDOWN if you want the server to shutdown. */
    //应用日志条目到状态机，保存到存储
    func_logentry_event_f log_offer;

    /** Callback for removing the oldest entry from the log
     * For safety reasons this callback MUST flush the change to disk.
     * @note If memory was malloc'd in log_offer then this should be the right
     *  time to free the memory. */
    //移除当前日志条目的上一个日志条目，并且返回被移除的日志条目
    func_logentry_event_f log_poll;

    /** Callback for removing the youngest entry from the log
     * For safety reasons this callback MUST flush the change to disk.
     * @note If memory was malloc'd in log_offer then this should be the right
     *  time to free the memory. */
    //删除制定的日志条目
    func_logentry_event_f log_pop;

    /** Callback for determining which node this configuration log entry
     * affects. This call only applies to configuration change log entries.
     * @return the node ID of the node */
    //确定配置修改日志条目中的节点id
    func_logentry_event_f log_get_node_id;

    /** Callback for detecting when a non-voting node has sufficient logs. */
    //无投票权限节点同步了足够多的日志
    func_node_has_sufficient_logs_f node_has_sufficient_logs;

    /** Callback for catching debugging log messages
     * This callback is optional */
    func_log_f log;
} raft_cbs_t;

//节点配置结构
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

/** De-initialise Raft server. */
//清除一个服务节点为初始化状态
void raft_clear(raft_server_t* me);

/** Set callbacks and user data.
 * 设置定义的会掉函数
 * @param[in] funcs Callbacks
 * @param[in] user_data "User data" - user's context that's included in a callback */
void raft_set_callbacks(raft_server_t* me, raft_cbs_t* funcs, void* user_data);

/** Add node.
 * 添加节点
 * @note This library does not yet support membership changes.
 *  Once raft_periodic has been run this will fail.
 *
 * If a voting node already exists the call will fail.
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
 * @param[in] id The integer ID of this node
 *  This is used for identifying clients across sessions.
 * @param[in] is_self Set to 1 if this "node" is this server
 * @return
 *  node if it was successfully added;
 *  NULL if the node already exists */
raft_node_t* raft_add_node(raft_server_t* me, void* user_data, int id, int is_self);
//别名
#define raft_add_peer raft_add_node

/**
 * 添加无投票权限节点
 * Add a node which does not participate in voting.
 * If a node already exists the call will fail.
 * Parameters are identical to raft_add_node
 * @return
 *  node if it was successfully added;
 *  NULL if the node already exists */
raft_node_t* raft_add_non_voting_node(raft_server_t* me_, void* udata, int id, int is_self);

/**
 * 删除节点
 * Remove node.
 * @param node The node to be removed. */
void raft_remove_node(raft_server_t* me_, raft_node_t* node);

/**
 * 设置心跳频率
 * Set election timeout.
 * The amount of time that needs to elapse before we assume the leader is down
 * @param[in] msec Election timeout in milliseconds */
void raft_set_election_timeout(raft_server_t* me, int msec);

/**
 * 设置日志rpc超时时间
 * Set request timeout in milliseconds.
 * The amount of time before we resend an appendentries message
 * @param[in] msec Request timeout in milliseconds */
void raft_set_request_timeout(raft_server_t* me, int msec);

/**
 * 心跳周期
 * Process events that are dependent on time passing.
 * @param[in] msec_elapsed Time in milliseconds since the last call
 * @return
 *  0 on success;
 *  -1 on failure;
 *  RAFT_ERR_SHUTDOWN when server should be shutdown */
int raft_periodic(raft_server_t* me, int msec_elapsed);

/**
 * 附加日志条目请求
 * Receive an appendentries message.
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
 * @param[in] node The node who sent us this message
 * @param[in] ae The appendentries message
 * @param[out] r The resulting response
 * @return 0 on success */
int raft_recv_appendentries(raft_server_t* me,
                            raft_node_t* node,
                            msg_appendentries_t* ae,
                            msg_appendentries_response_t *r);

/**
 * 附加日志条目响应
 * Receive a response from an appendentries message we sent.
 * @param[in] node The node who sent us this message
 * @param[in] r The appendentries response message
 * @return 0 on success */
int raft_recv_appendentries_response(raft_server_t* me,
                                     raft_node_t* node,
                                     msg_appendentries_response_t* r);

/**
 * 投票请求
 * Receive a requestvote message.
 * @param[in] node The node who sent us this message
 * @param[in] vr The requestvote message
 * @param[out] r The resulting response
 * @return 0 on success */
int raft_recv_requestvote(raft_server_t* me,
                          raft_node_t* node,
                          msg_requestvote_t* vr,
                          msg_requestvote_response_t *r);

/**
 * 投票响应
 * Receive a response from a requestvote message we sent.
 * @param[in] node The node this response was sent by
 * @param[in] r The requestvote response message
 * @return
 *  0 on success;
 *  RAFT_ERR_SHUTDOWN server should be shutdown; */
int raft_recv_requestvote_response(raft_server_t* me,
                                   raft_node_t* node,
                                   msg_requestvote_response_t* r);

/**
 * 客户端输入消息
 * Receive an entry message from the client.
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
 * @param[in] node The node who sent us this message
 * @param[in] ety The entry message
 * @param[out] r The resulting response
 * @return
 *  0 on success;
 *  RAFT_ERR_NOT_LEADER server is not the leader;
 *  RAFT_ERR_SHUTDOWN server should be shutdown;
 *  RAFT_ERR_ONE_VOTING_CHANGE_ONLY there is a non-voting change inflight;
 */
int raft_recv_entry(raft_server_t* me,
                    msg_entry_t* ety,
                    msg_entry_response_t *r);

/**
 * 获取当前server的nodeid
 * @return server's node ID; -1 if it doesn't know what it is */
int raft_get_nodeid(raft_server_t* me);

/**
 * 获取当前server对应的node
 * @return the server's node */
raft_node_t* raft_get_my_node(raft_server_t *me_);

/**
 * 获取当前选举超时时间
 * @return currently configured election timeout in milliseconds */
int raft_get_election_timeout(raft_server_t* me);

/**
 * 获取当前server的节点数量
 * @return number of nodes that this server has */
int raft_get_num_nodes(raft_server_t* me);

/**
 * 获取当前server有投票权限的节点数量
 * @return number of voting nodes that this server has */
int raft_get_num_voting_nodes(raft_server_t* me_);

/**
 * 获取当前server的日志总数
 * @return number of items within log */
int raft_get_log_count(raft_server_t* me);

/**
 * 获取当前server的任期号
 * @return current term */
int raft_get_current_term(raft_server_t* me);

/**
 * 获取当前server的日志index
 * @return current log index */
int raft_get_current_idx(raft_server_t* me);

/**
 * 获取当前server已提交的的日志条目当前索引
 * @return commit index */
int raft_get_commit_idx(raft_server_t* me_);

/**
 * 判断server是不是追随者
 * @return 1 if follower; 0 otherwise */
int raft_is_follower(raft_server_t* me);

/**
 * 判断server是不是领导者
 * @return 1 if leader; 0 otherwise */
int raft_is_leader(raft_server_t* me);

/**
 * 判断server是不是候选者
 * @return 1 if candidate; 0 otherwise */
int raft_is_candidate(raft_server_t* me);

/**
 * 获取当前超时时间
 * @return currently elapsed timeout in milliseconds */
int raft_get_timeout_elapsed(raft_server_t* me);

/**
 * 获取日志请求超时时间
 * @return request timeout in milliseconds */
int raft_get_request_timeout(raft_server_t* me);

/**
 * 获取最后被应用到状态机的日志条目索引值
 * @return index of last applied entry */
int raft_get_last_applied_idx(raft_server_t* me);

/**
<<<<<<< HEAD
 * 节点是不是领导者
 * @return 1 if node is leader; 0 otherwise */
int raft_node_is_leader(raft_node_t* node);

/**
 * 获取节点下一个应该发送的日志索引
=======
>>>>>>> willemt/master
 * @return the node's next index */
int raft_node_get_next_idx(raft_node_t* node);

/**
 * 获取该节点已经复制给他的日志的最高索引值
 * @return this node's user data */
int raft_node_get_match_idx(raft_node_t* me);

/**
 * 获取该节点的自定义数据
 * @return this node's user data */
void* raft_node_get_udata(raft_node_t* me);

/**
 * 设置指定节点的自定义数据
 * Set this node's user data */
void raft_node_set_udata(raft_node_t* me, void* user_data);

/**
 * 获取 指定idx的消息
 * @param[in] idx The entry's index
 * @return entry from index */
raft_entry_t* raft_get_entry_from_idx(raft_server_t* me, int idx);

/**
 * 获取指定nodeid的node
 * @param[in] node The node's ID
 * @return node pointed to by node ID */
raft_node_t* raft_get_node(raft_server_t* me_, const int id);

/**
 * 获取直接索引的node
 * Used for iterating through nodes
 * @param[in] node The node's idx
 * @return node pointed to by node idx */
raft_node_t* raft_get_node_from_idx(raft_server_t* me_, const int idx);

/**
 * 获取已经收到的选举票数
 * @return number of votes this server has received this election */
int raft_get_nvotes_for_me(raft_server_t* me);

/**
 * 获取我已投票的节点id
 * @return node ID of who I voted for */
int raft_get_voted_for(raft_server_t* me);

/**
 * 获取当前的领导者节点id
 * Get what this node thinks the node ID of the leader is.
 * @return node of what this node thinks is the valid leader;
 *   -1 if the leader is unknown */
int raft_get_current_leader(raft_server_t* me);

/**
 * 获取当前领导者的节点对象
 * Get what this node thinks the node of the leader is.
 * @return node of what this node thinks is the valid leader;
 *   NULL if the leader is unknown */
raft_node_t* raft_get_current_leader_node(raft_server_t* me);

/**
 * 获取当前的用户数据
 * @return callback user data */
void* raft_get_udata(raft_server_t* me);

/**
 * 投票
 * Vote for a server.
 * This should be used to reload persistent state, ie. the voted-for field.
 * @param[in] node The server to vote for */
void raft_vote(raft_server_t* me_, raft_node_t* node);

/**
 * 投票到指定节点id
 * Vote for a server.
 * This should be used to reload persistent state, ie. the voted-for field.
 * @param[in] nodeid The server to vote for by nodeid */
void raft_vote_for_nodeid(raft_server_t* me_, const int nodeid);

/**
 * 设置当前任期号
 * Set the current term.
 * This should be used to reload persistent state, ie. the current_term field.
 * @param[in] term The new current term */
void raft_set_current_term(raft_server_t* me, const int term);

/**
 * 设置当期已提交日志条目的idx
 * Set the commit idx.
 * This should be used to reload persistent state, ie. the commit_idx field.
 * @param[in] commit_idx The new commit index. */
void raft_set_commit_idx(raft_server_t* me, int commit_idx);

/**
 * 附加消息到状态机
 * Add an entry to the server's log.
 * This should be used to reload persistent state, ie. the commit log.
 * @param[in] ety The entry to be appended
 * @return
 *  0 on success;
 *  RAFT_ERR_SHUTDOWN server should shutdown */
int raft_append_entry(raft_server_t* me, raft_entry_t* ety);

/**
 * 消息已提交响应函数
 * Confirm if a msg_entry_response has been committed.
 * @param[in] r The response we want to check */
int raft_msg_entry_response_committed(raft_server_t* me_,
                                      const msg_entry_response_t* r);

/**
 * 获取直接节点的节点id
 * Get node's ID.
 * @return ID of node */
int raft_node_get_id(raft_node_t* me_);

/**
 * 获取当前服务的角色
 * Tell if we are a leader, candidate or follower.
 * @return get state of type raft_state_e. */
int raft_get_state(raft_server_t* me_);

/**
 * 获取最后一个日志的任期号
 * The the most recent log's term
 * @return the last log term */
int raft_get_last_log_term(raft_server_t* me_);

/**
 * 设置节点是有投票权限的节点
 * Turn a node into a voting node.
 * Voting nodes can take part in elections and in-regards to commiting entries,
 * are counted in majorities. */
void raft_node_set_voting(raft_node_t* node, int voting);

/**
 * 判断节点是否有投票权限
 * Tell if a node is a voting node or not.
 * @return 1 if this is a voting node. Otherwise 0. */
int raft_node_is_voting(raft_node_t* me_);

/**
 * 提交当前所有的日志到日志条目
 * Apply all entries up to the commit index
 * @return
 *  0 on success;
 *  RAFT_ERR_SHUTDOWN when server should be shutdown */
int raft_apply_all(raft_server_t* me_);

/**
 * 成为领导者
 * Become leader
 * WARNING: this is a dangerous function call. It could lead to your cluster
 * losing it's consensus guarantees. */
void raft_become_leader(raft_server_t* me);

/**
 * 当前节点状态是处于无投票权限节点状态变更中？？？
 * Determine if entry is voting configuration change.
 * @param[in] ety The entry to query.
 * @return 1 if this is a voting configuration change. */
int raft_entry_is_voting_cfg_change(raft_entry_t* ety);

/**
 * 当前节点状态是否处理有投票权限节点状态变更中
 * Determine if entry is configuration change.
 * @param[in] ety The entry to query.
 * @return 1 if this is a configuration change. */
int raft_entry_is_cfg_change(raft_entry_t* ety);

#endif /* RAFT_H_ */
