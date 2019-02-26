//
// Created by QiuYukang on 2019/2/22.
//

#include <stdlib.h>

#ifndef ARQ_SINK_PROTOCOL_H
#define ARQ_SINK_PROTOCOL_H

#endif //ARQ_SINK_PROTOCOL_H

#define QUEUE_NUM 256 // ��������(���֧��256������)
#define WINDOWS_SIZE  256  // ���ܷ��Ľ��մ��ڴ�С
#define MAX_SEQ 65535   // ���������к�

/* --------------------------------------- */
/* -------------- �����ʽ���� ------------ */
/* --------------------------------------- */

/* ����֡���� */
typedef enum Packet_Type {
    reliable0 = 0,
    reliable2,
    reliable4,
    reliable6,
    ack_request = 8,
    ack_response,
    abandoned
} Packet_Type;

/* �����ṹ */
typedef struct Sequence_Group {
    unsigned short sequence;    // ����� 16bit
    unsigned char queue_num;   // ���к� 8bit
    unsigned char queue_seq;  // ������� 8bit
} Sequence_Group;

/* ����֡��ʽ������ type �� length �ֶ� */
typedef struct Packet_Data {
    Sequence_Group seq_group;
    char data[100];
} Packet_Data;

/* ȷ������֡��ʽ������ type �� length �ֶ� */
typedef struct Packet_Ack_Request {
    Sequence_Group seq_group[16];
} Packet_Ack_Request;

/* ȷ����Ӧ֡��ʽ������ type �� length �ֶ� */
typedef struct Packet_Ack_Response {
    unsigned short sequence[16];
} Packet_Ack_Response;

/* ͨ��֡��ʽ���� */
typedef struct Packet {
    Packet_Type type;   // ֻ��4bit
    unsigned char length; // ֻ��4bit
    union {
        Packet_Data packet_data;
        Packet_Ack_Request packet_ack_request;
        Packet_Ack_Response packet_ack_response;
    } data;
} Packet;

/* --------------------------------------- */
/* --------- �������������������-------- */
/* --------------------------------------- */
// ��������Ľڵ�Ԫ��
typedef struct Seq_Link_List_Node {
    unsigned short sequence;    // �����
    struct Seq_Link_List_Node *next;    // ��һ���ڵ�
} Seq_Link_List_Node;
// ���嵥����ṹ
typedef struct Seq_Link_List {
    Seq_Link_List_Node *head;
    int length;
} Seq_Link_List;


/* --------------------------------------- */
/* ------------- ������ջ����� ----------- */
/* --------------------------------------- */
// ���嵥��������нڵ�
typedef struct Packet_List_Node {
    Packet *packet;
    struct Packet_List_Node *next;
} Packet_List_Node;
// ���建�����
typedef struct Packet_List {
    Packet_List_Node *head;
    int length;
} Packet_List;

// ������ջ���������Ԫ��
typedef struct Recv_List_Node {
    unsigned char require_seq; // �ڴ����յĶ������
    Packet_List pk_list; // Packet�������
} Recv_List_Node;


/* --------------------------------------- */
/* ---------------- ��������--------------- */
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

