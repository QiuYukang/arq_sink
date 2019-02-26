//
// Created by QiuYukang on 2019/2/22.
//

#include <stdlib.h>

#ifndef ARQ_SINK_PROTOCOL_H
#define ARQ_SINK_PROTOCOL_H

#endif //ARQ_SINK_PROTOCOL_H

#define QUEUE_NUM 256 // 队列数量(最多支持256个队列)
#define WINDOWS_SIZE  256  // 接受方的接收窗口大小
#define MAX_SEQ 65535   // 最大的总序列号

/* --------------------------------------- */
/* -------------- 分组格式定义 ------------ */
/* --------------------------------------- */

/* 定义帧类型 */
typedef enum Packet_Type {
    reliable0 = 0,
    reliable2,
    reliable4,
    reliable6,
    ack_request = 8,
    ack_response,
    abandoned
} Packet_Type;

/* 序号组结构 */
typedef struct Sequence_Group {
    unsigned short sequence;    // 总序号 16bit
    unsigned char queue_num;   // 队列号 8bit
    unsigned char queue_seq;  // 队内序号 8bit
} Sequence_Group;

/* 数据帧格式（除了 type 和 length 字段 */
typedef struct Packet_Data {
    Sequence_Group seq_group;
    char data[100];
} Packet_Data;

/* 确认请求帧格式（除了 type 和 length 字段 */
typedef struct Packet_Ack_Request {
    Sequence_Group seq_group[16];
} Packet_Ack_Request;

/* 确认响应帧格式（除了 type 和 length 字段 */
typedef struct Packet_Ack_Response {
    unsigned short sequence[16];
} Packet_Ack_Response;

/* 通用帧格式定义 */
typedef struct Packet {
    Packet_Type type;   // 只有4bit
    unsigned char length; // 只有4bit
    union {
        Packet_Data packet_data;
        Packet_Ack_Request packet_ack_request;
        Packet_Ack_Response packet_ack_response;
    } data;
} Packet;

/* --------------------------------------- */
/* --------- 定义序号链表（单向链表）-------- */
/* --------------------------------------- */
// 定义链表的节点元素
typedef struct Seq_Link_List_Node {
    unsigned short sequence;    // 总序号
    struct Seq_Link_List_Node *next;    // 下一个节点
} Seq_Link_List_Node;
// 定义单链表结构
typedef struct Seq_Link_List {
    Seq_Link_List_Node *head;
    int length;
} Seq_Link_List;


/* --------------------------------------- */
/* ------------- 定义接收缓冲区 ----------- */
/* --------------------------------------- */
// 定义单个缓冲队列节点
typedef struct Packet_List_Node {
    Packet *packet;
    struct Packet_List_Node *next;
} Packet_List_Node;
// 定义缓冲队列
typedef struct Packet_List {
    Packet_List_Node *head;
    int length;
} Packet_List;

// 定义接收缓存区单个元素
typedef struct Recv_List_Node {
    unsigned char require_seq; // 期待接收的队内序号
    Packet_List pk_list; // Packet缓存队列
} Recv_List_Node;


/* --------------------------------------- */
/* ---------------- 函数声明--------------- */
/* --------------------------------------- */
int
process_packet(Packet *pk_ptr, Seq_Link_List *seq_link_list, unsigned short required_seq[], Recv_List_Node recv_buf[]);

int process_data_frame(Packet *pk_ptr, Seq_Link_List *seq_link_list, Recv_List_Node recv_buf[]);

int process_ack_request_frame(Packet *pk_ptr, Seq_Link_List *seq_link_list, unsigned short required_seq[],
                              Recv_List_Node recv_buf[]);

void init_seq_link_list(Seq_Link_List *seq_link_list);

int insert_seq_link_list(unsigned short seq, Seq_Link_List *seq_link_list);

int commit_seq_link_list(Seq_Link_List *seq_link_list, unsigned short requred_seq[]);

void init_recv_buf(Recv_List_Node recv_buf[]);

Packet_List *get_pk_list();

int insert_pk_list(Packet *p_pk, Recv_List_Node recv_buf[]);

int commit_pk_list(unsigned char QNum, Recv_List_Node recv_buf[]);

Packet create_ack_response(int seq_num, unsigned short required_seq[]);

void show_packet(Packet packet);

void show_seq_link_list(Seq_Link_List seq_link_list);

void show_recv_buf_queue(Recv_List_Node recv_buf[], unsigned char QNum);

