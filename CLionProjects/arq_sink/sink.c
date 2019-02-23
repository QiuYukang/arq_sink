#include <stdio.h>
#include "protocol.h"

int process_packet(Packet *pk_ptr, Seq_Link_List *seq_link_list, unsigned short required_seq[], Recv_List_Node recv_buf[]) {
    show_packet(*pk_ptr);

    switch (pk_ptr->type) {
        case reliable0:
        case reliable2:
        case reliable4:
        case reliable6:
            process_data_frame(pk_ptr, seq_link_list, recv_buf);
            break;
        case ack_request:
            process_ack_request_frame(pk_ptr, seq_link_list, required_seq, recv_buf);
        default:
            printf("%s", "\n帧类型错误！\n");
            return 0;
    }

    return 1;
}

int process_data_frame(Packet *pk_ptr, Seq_Link_List *seq_link_list, Recv_List_Node recv_buf[]) {
    // 获取总序号
    unsigned short Seq = pk_ptr->data.packet_data.seq_group.sequence;

    // 把总序号插入到序号链表中
    // 序号插入失败（序号有重复）
    if (!insert_seq_link_list(Seq, seq_link_list)) {
        return 0;
    }

    char QNum = pk_ptr->data.packet_data.seq_group.queue_num;

    // 把Packet插入到队列缓冲区中并在本队列内按序提交
    insert_pk_list(pk_ptr, recv_buf);
    commit_pk_list(QNum, recv_buf);

    return 1;
}

/**
 * 处理 ack request 帧
 * @param pk_ptr 指向要处理的 Packet 指针
 * @param seq_link_list 序号链表
 * @param required_seq 构造 ack response 用的待接收序号数组
 * @return requried_seq 数组的有效序号个数
 */
int process_ack_request_frame(Packet *pk_ptr, Seq_Link_List *seq_link_list, unsigned short required_seq[], Recv_List_Node recv_buf[]) {
    // 遍历ack_request帧中的序号组（放弃接收的序号）
    for (int i = 0; i < pk_ptr->length; i++) {
        Sequence_Group seq_group = pk_ptr->data.packet_ack_request.seq_group[i];
        Packet *new_pk = (Packet *) malloc(sizeof(Packet));

        // 新建一个只有序号组没有实际数据的的Data Packet,并提前提交队列数据
        new_pk->data.packet_data.seq_group = seq_group;
        insert_pk_list(new_pk, recv_buf);
        commit_pk_list(seq_group.queue_num, recv_buf);

        // 处理序号链表
        insert_seq_link_list(seq_group.sequence, seq_link_list);
        int seq_num = commit_seq_link_list(seq_link_list, required_seq);

        // 构造 ack response Packet
        Packet packet = create_ack_response(seq_num, required_seq);

        show_packet(packet);
    }

    return 1;
}

// 初始化序号链表
void init_seq_link_list(Seq_Link_List *seq_link_list) {
    seq_link_list->length = 0;
    seq_link_list->head = NULL;
}

// 把序号插入到序号链表中
int insert_seq_link_list(unsigned short seq, Seq_Link_List *seq_link_list) {
    // 创建新节点
    Seq_Link_List_Node *p_new_node = (Seq_Link_List_Node *) malloc(sizeof(Seq_Link_List_Node));
    p_new_node->sequence = seq;
    p_new_node->next = NULL;

    if (seq_link_list->length == 0) {
        seq_link_list->head = p_new_node;
        seq_link_list->length++;
    } else {
        Seq_Link_List_Node *copy_p = seq_link_list->head;
        Seq_Link_List_Node *copy_p_next = seq_link_list->head->next;

        // 插入到队首
        if (copy_p->sequence > seq) {
            p_new_node->next = copy_p;
            seq_link_list->head = p_new_node;
            seq_link_list->length++;
            return 1;
        }

        // 指针移动
        while (copy_p_next != NULL && seq > copy_p_next->sequence) {
            copy_p = copy_p_next;
            copy_p_next = copy_p_next->next;
        }

        // 插入到队中/尾
        if (seq == copy_p->sequence || (copy_p_next != NULL && seq == copy_p_next->sequence)) {
            printf("%s", "\n收到重复帧\n");
            return 0;
        }

        // 插入
        copy_p->next = p_new_node;
        p_new_node->next = copy_p_next;
        seq_link_list->length++;
    }

    return 1;
}

// 提交序号链表，返回需要重传的总序号数组大小
int commit_seq_link_list(Seq_Link_List *seq_link_list, unsigned short required_seq[]) {
    if (seq_link_list->length == 0)
        return 0;

    Seq_Link_List_Node *copy_p = seq_link_list->head;
    int num = 0;
    unsigned short temp_seq = copy_p->sequence;

    // 序号连续区域
    while (copy_p->next != NULL && copy_p->next->sequence == (temp_seq + 1)) {
        seq_link_list->head = copy_p->next;
        free(copy_p);
        copy_p = seq_link_list->head;
        temp_seq++;
    }

    // 序号不连续区域
    while (copy_p->next != NULL && num <= 16) {
        if (copy_p->next->sequence != (temp_seq + 1)) {
            temp_seq++;
            required_seq[num] = temp_seq;
            num++;
        } else {
            copy_p = copy_p->next;
            temp_seq++;
        }
    }

    return num;
}

// 初始化接收缓冲区
void init_recv_buf(Recv_List_Node recv_buf[]) {
    for (int i = 0; i < QUEUE_NUM; i++) {
        recv_buf[i].require_seq = 0;
        recv_buf[i].pk_list = *(get_pk_list());
    }
}

// 获取一个空的缓存队列
Packet_List *get_pk_list() {
    Packet_List *pk_list = (Packet_List *) malloc(sizeof(Packet_List));
    pk_list->head = NULL;
    pk_list->length = 0;

    return pk_list;
}

// 把一个Packet插入对应的缓存队列
int insert_pk_list(Packet *p_pk, Recv_List_Node recv_buf[]) {
    // 获取包内的队列号和队内序号
    char QNum = p_pk->data.packet_data.seq_group.queue_num;
    char QSeq = p_pk->data.packet_data.seq_group.queue_seq;
    // 创建新节点
    Packet_List_Node *new_pk_list_node = (Packet_List_Node *) malloc(sizeof(Packet_List_Node));
    new_pk_list_node->packet = p_pk;
    new_pk_list_node->next = NULL;

    // 当前缓存队列为空
    if (recv_buf[QNum].pk_list.length == 0) {
        recv_buf[QNum].pk_list.head = new_pk_list_node;
        recv_buf[QNum].pk_list.length++;

        return 1;
    }

    Packet_List_Node *copy_p = recv_buf[QNum].pk_list.head;
    Packet_List_Node *copy_p_next = copy_p->next;

    // 插入到队首
    if (copy_p->packet->data.packet_data.seq_group.queue_seq > QSeq) {
        new_pk_list_node->next = copy_p;
        recv_buf[QNum].pk_list.head = new_pk_list_node;
        recv_buf[QNum].pk_list.length++;
        return 1;
    }

    // 插入到队中或队尾
    // 指针移动
    while (copy_p_next != NULL && QSeq > copy_p_next->packet->data.packet_data.seq_group.queue_seq) {
        copy_p = copy_p_next;
        copy_p_next = copy_p_next->next;
    }
    // 插入
    copy_p->next = new_pk_list_node;
    new_pk_list_node->next = copy_p_next;
    recv_buf[QNum].pk_list.length++;

    return 1;
}

// 提交Packet缓存队列
int commit_pk_list(unsigned char QNum, Recv_List_Node recv_buf[]) {
    while (recv_buf[QNum].pk_list.length != 0 && recv_buf[QNum].require_seq ==
                                                 recv_buf[QNum].pk_list.head->packet->data.packet_data.seq_group.queue_seq) {
        recv_buf[QNum].require_seq++;
        Packet_List_Node *pk_node = recv_buf[QNum].pk_list.head;
        recv_buf[QNum].pk_list.head = pk_node->next;
        recv_buf[QNum].pk_list.length--;

        printf("队列 %d 已经提交到序号 %d \n", QNum, recv_buf[QNum].require_seq - 1);

        // 释放内存
        free(pk_node->packet);
        free(pk_node);
    }

    return 1;
}

// 创建 ack response Packet
Packet create_ack_response(int seq_num, unsigned short required_seq[]) {
    Packet *pk_p = (Packet *) malloc(sizeof(Packet));
    pk_p->length = (unsigned char) seq_num;
    pk_p->type = ack_response;

    for (int i = 0; i < seq_num; i++) {
        pk_p->data.packet_ack_response.sequence[i] = required_seq[i];
    }

    return *pk_p;
}

// 打印 Packet 中的信息
void show_packet(Packet packet) {
    printf("----------打印Packet信息-----------\n");
    switch (packet.type) {
        case reliable0:
            printf("type: reliable0\n");
            printf("length: %d\n", packet.length);
            printf("Seq: %d   QNum: %d   QSeq: %d\n",
                   packet.data.packet_data.seq_group.sequence,
                   packet.data.packet_data.seq_group.queue_num,
                   packet.data.packet_data.seq_group.queue_seq);
            break;
        case reliable2:
            printf("type: reliable2\n");
            printf("length: %d\n", packet.length);
            printf("Seq: %d   QNum: %d   QSeq: %d\n",
                   packet.data.packet_data.seq_group.sequence,
                   packet.data.packet_data.seq_group.queue_num,
                   packet.data.packet_data.seq_group.queue_seq);
            break;
        case reliable4:
            printf("type: reliable4\n");
            printf("length: %d\n", packet.length);
            printf("Seq: %d   QNum: %d   QSeq: %d\n",
                   packet.data.packet_data.seq_group.sequence,
                   packet.data.packet_data.seq_group.queue_num,
                   packet.data.packet_data.seq_group.queue_seq);
            break;
        case reliable6:
            printf("type: reliable6\n");
            printf("length: %d\n", packet.length);
            printf("Seq: %d   QNum: %d   QSeq: %d\n",
                   packet.data.packet_data.seq_group.sequence,
                   packet.data.packet_data.seq_group.queue_num,
                   packet.data.packet_data.seq_group.queue_seq);
            break;
        case ack_request:
            printf("type: ack_request\n");
            printf("length: %d\n", packet.length);
            printf("Abandoned Sequence Group:\n");
            for (int i = 0; i < packet.length; i++) {
                printf("    Seq: %d   QNum: %d   QSeq: %d\n",
                       packet.data.packet_ack_request.seq_group[i].sequence,
                       packet.data.packet_ack_request.seq_group[i].queue_num,
                       packet.data.packet_ack_request.seq_group[i].queue_seq);
            }
            break;
        case ack_response:
            printf("type: ack_response\n");
            printf("length: %d\n", packet.length);
            printf("Required Sequence: ");
            for (int i = 0; i < packet.length; i++) {
                printf("%d ", packet.data.packet_ack_response.sequence[i]);
            }
            printf("\n");
            break;
        default:
            printf("type: 非法类型\n");
            break;
    }
}
