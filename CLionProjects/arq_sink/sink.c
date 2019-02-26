#include <stdio.h>
#include "protocol.h"

int
process_packet(Packet *pk_ptr, Seq_Link_List *seq_link_list, unsigned short required_seq[], Recv_List_Node recv_buf[]) {
//    printf("[process_packet]:");
//    show_packet(*pk_ptr);

    switch (pk_ptr->type) {
        case reliable0:
        case reliable2:
        case reliable4:
        case reliable6:
            process_data_frame(pk_ptr, seq_link_list, recv_buf);
            break;
        case ack_request:
            process_ack_request_frame(pk_ptr, seq_link_list, required_seq, recv_buf);
            break;
        default:
            printf("%s", "\npacket type is error��\n");
            return 0;
    }

    return 1;
}

int process_data_frame(Packet *pk_ptr, Seq_Link_List *seq_link_list, Recv_List_Node recv_buf[]) {
    // ��ȡ�����
    unsigned short Seq = pk_ptr->data.packet_data.seq_group.sequence;

    // ������Ų��뵽���������
    // ��Ų���ʧ�ܣ�������ظ���
    printf("[insert_seq_link_list]:insert Seq.%d\n", Seq);
    if (!insert_seq_link_list(Seq, seq_link_list)) {
        show_seq_link_list(*seq_link_list);

        return 0;
    }

    show_seq_link_list(*seq_link_list);

    unsigned char QNum = pk_ptr->data.packet_data.seq_group.queue_num;

    // ��Packet���뵽���л������в��ڱ������ڰ����ύ
    printf("[insert_pk_list]:insert QSeq.%d to queue[%d]\n", pk_ptr->data.packet_data.seq_group.queue_seq, QNum);
    insert_pk_list(pk_ptr, recv_buf);
    show_recv_buf_queue(recv_buf, QNum);

    printf("[commit_pk_list]:commit in queue[%d]\n", QNum);
    commit_pk_list(QNum, recv_buf);
    printf("    -required QSeq.%d\n", recv_buf[QNum].require_seq);

    return 1;
}

/**
 * ���� ack request ֡
 * @param pk_ptr ָ��Ҫ����� Packet ָ��
 * @param seq_link_list �������
 * @param required_seq ���� ack response �õĴ������������
 * @return requried_seq �������Ч��Ÿ���
 */
int process_ack_request_frame(Packet *pk_ptr, Seq_Link_List *seq_link_list, unsigned short required_seq[],
                              Recv_List_Node recv_buf[]) {
    int seq_num; // �����ܵ���Ÿ���

    // ����ack_request֡�е�����飨�������յ���ţ�
    for (int i = 0; i < pk_ptr->length; i++) {
        Sequence_Group seq_group = pk_ptr->data.packet_ack_request.seq_group[i];
        Packet *new_pk = (Packet *) malloc(sizeof(Packet));

        // �½�һ��ֻ�������û��ʵ�����ݵĵ�Data Packet,����ǰ�ύ��������
        new_pk->data.packet_data.seq_group = seq_group;
        new_pk->type = abandoned;

        printf("[insert_pk_list]:insert abandoned QSeq.%d\n", seq_group.queue_seq);
        insert_pk_list(new_pk, recv_buf);
        show_recv_buf_queue(recv_buf, seq_group.queue_num);

        // ��������ǰ�ύ
        printf("[commit_pk_list]:commit in queue[%d]\n", seq_group.queue_num);
        commit_pk_list(seq_group.queue_num, recv_buf);
        printf("    -required QSeq.%d\n", recv_buf[seq_group.queue_num].require_seq);

        // �ѷ������յ���Ų��뵽���������
        printf("[insert_seq_link_list]:insert abandoned Seq.%d\n", seq_group.sequence);
        insert_seq_link_list(seq_group.sequence, seq_link_list);
        show_seq_link_list(*seq_link_list);
    }

    // ���� ack response Packet
    printf("[commit_seq_link_list]:\n");
    seq_num = commit_seq_link_list(seq_link_list, required_seq);
    show_seq_link_list(*seq_link_list);

    Packet packet = create_ack_response(seq_num, required_seq);
    printf("[create_ack_response]:\n");
    show_packet(packet);

    return 1;
}

// ��ʼ���������
void init_seq_link_list(Seq_Link_List *seq_link_list) {
    seq_link_list->length = 0;
    seq_link_list->head = NULL;
}

// ����Ų��뵽���������
int insert_seq_link_list(unsigned short seq, Seq_Link_List *seq_link_list) {
    // �����½ڵ�
    Seq_Link_List_Node *p_new_node = (Seq_Link_List_Node *) malloc(sizeof(Seq_Link_List_Node));
    p_new_node->sequence = seq;
    p_new_node->next = NULL;

    // �������Ϊ�գ�ֱ�Ӳ���
    if (seq_link_list->length == 0) {
        seq_link_list->head = p_new_node;
        seq_link_list->length++;

        return 1;
    }

    Seq_Link_List_Node *copy_p = seq_link_list->head;
    Seq_Link_List_Node *copy_p_next = seq_link_list->head->next;

    // ���ֵԽ�磨����������ֵ���Ҳ��ڴ�����
    if (copy_p->sequence > seq && seq >= (unsigned short) (copy_p->sequence + WINDOWS_SIZE)) {
        printf("    Seq.%d: out of window!\n", seq);
        return 0;
    }

    // ���ֵԽ�絫���ڴ�����
    if (copy_p->sequence > seq && seq < (copy_p->sequence + WINDOWS_SIZE)) {
        // ָ���ƶ�-ֱ���ƹ��ֽ��0
        while (copy_p_next != NULL && seq >=
                                      (copy_p_next->sequence > seq_link_list->head->sequence ? 0
                                                                                             : copy_p_next->sequence)) {
            copy_p = copy_p_next;
            copy_p_next = copy_p_next->next;
        }

        // ָ������ƶ�
        while (copy_p_next != NULL && seq > copy_p_next->sequence) {
            copy_p = copy_p_next;
            copy_p_next = copy_p_next->next;
        }
    }

    // ���ֵδԽ�����ڴ�����
    if (copy_p->sequence < seq) {
        // ָ���ƶ�
        while (copy_p_next != NULL && seq >
                                      (copy_p_next->sequence < seq_link_list->head->sequence ? 65536
                                                                                             : copy_p_next->sequence)) {
            copy_p = copy_p_next;
            copy_p_next = copy_p_next->next;
        }
    }

    // ��⵽�ظ����
    if (seq == copy_p->sequence || (copy_p_next != NULL && seq == copy_p_next->sequence)) {
        printf("    Seq.%d: repeat packet!\n", seq);
        return 0;
    }
    // ���뵽����/β
    copy_p->next = p_new_node;
    p_new_node->next = copy_p_next;
    seq_link_list->length++;

    return 1;
}

// �ύ�������������Ҫ�ش�������������С
int commit_seq_link_list(Seq_Link_List *seq_link_list, unsigned short required_seq[]) {
    if (seq_link_list->length == 0)
        return 0;

    Seq_Link_List_Node *copy_p = seq_link_list->head;
    int num = 0; // �����յ�����������Ч��Ÿ���
    unsigned short temp_seq = copy_p->sequence;

    // �����������
    while (copy_p->next != NULL && copy_p->next->sequence == (unsigned short) (temp_seq + 1)) {
        seq_link_list->head = copy_p->next;
        seq_link_list->length--;
        free(copy_p);
        copy_p = seq_link_list->head;
        temp_seq++;
    }

    // ��Ų���������
    while (copy_p->next != NULL && num < 16) {
        if (copy_p->next->sequence != (unsigned short) (temp_seq + 1)) {
            temp_seq++;
            required_seq[num] = temp_seq;
            num++;
        } else {
            copy_p = copy_p->next;
            temp_seq++;
        }
    }

    if (num < 15) {
        temp_seq++;
        required_seq[num] = temp_seq;
        num++;
    }

    return num;
}

// ��ʼ�����ջ�����
void init_recv_buf(Recv_List_Node recv_buf[]) {
    for (int i = 0; i < QUEUE_NUM; i++) {
        recv_buf[i].require_seq = 253;
        recv_buf[i].pk_list = *(get_pk_list());
    }
}

// ��ȡһ���յĻ������
Packet_List *get_pk_list() {
    Packet_List *pk_list = (Packet_List *) malloc(sizeof(Packet_List));
    pk_list->head = NULL;
    pk_list->length = 0;

    return pk_list;
}

// ��һ��Packet�����Ӧ�Ļ������
int insert_pk_list(Packet *p_pk, Recv_List_Node recv_buf[]) {
    // ��ȡ���ڵĶ��кźͶ������
    unsigned char QNum = p_pk->data.packet_data.seq_group.queue_num;
    unsigned char QSeq = p_pk->data.packet_data.seq_group.queue_seq;
    // �����½ڵ�
    Packet_List_Node *new_pk_list_node = (Packet_List_Node *) malloc(sizeof(Packet_List_Node));
    new_pk_list_node->packet = p_pk;
    new_pk_list_node->next = NULL;

    // ��ǰ�������Ϊ��
    if (recv_buf[QNum].pk_list.length == 0) {
        recv_buf[QNum].pk_list.head = new_pk_list_node;
        recv_buf[QNum].pk_list.length++;

        return 1;
    }

    Packet_List_Node *copy_p = recv_buf[QNum].pk_list.head;
    Packet_List_Node *copy_p_next = copy_p->next;

    // ���յ�Packet��ű�require_seq�󣨻���ڣ�
    if (QSeq >= recv_buf[QNum].require_seq) {
        // ���뵽����
        if (copy_p->packet->data.packet_data.seq_group.queue_seq > QSeq ||
            copy_p->packet->data.packet_data.seq_group.queue_seq < recv_buf[QNum].require_seq
           ) {
            new_pk_list_node->next = copy_p;
            recv_buf[QNum].pk_list.head = new_pk_list_node;
            recv_buf[QNum].pk_list.length++;

            return 1;
        }

        // ���뵽���л��β
        // ָ���ƶ�
        while (copy_p_next != NULL && QSeq > copy_p_next->packet->data.packet_data.seq_group.queue_seq) {
            copy_p = copy_p_next;
            copy_p_next = copy_p_next->next;
        }
    } else {
        // �������ͷָ��ָ���Packet�Ķ������ֵ
        unsigned char head_QSeq = recv_buf[QNum].pk_list.head->packet->data.packet_data.seq_group.queue_seq;

        // ���뵽����
        if(QSeq < head_QSeq && head_QSeq < recv_buf[QNum].require_seq){
            new_pk_list_node->next = copy_p;
            recv_buf[QNum].pk_list.head = new_pk_list_node;
            recv_buf[QNum].pk_list.length++;

            return 1;
        }

        // ָ���ƶ�-ֱ���ƹ��ֽ��0
        while (copy_p_next != NULL && QSeq >=
                                      (copy_p_next->packet->data.packet_data.seq_group.queue_seq > recv_buf[QNum].require_seq ? 0 :
                                       copy_p_next->packet->data.packet_data.seq_group.queue_seq)) {
            copy_p = copy_p_next;
            copy_p_next = copy_p_next->next;
        }

        // ָ������ƶ�
        while (copy_p_next != NULL && QSeq > copy_p_next->packet->data.packet_data.seq_group.queue_seq) {
            copy_p = copy_p_next;
            copy_p_next = copy_p_next->next;
        }
    }

    // ��⵽�ظ���ţ��������Ʋ�Ϊ�����ܣ��������޷�������ȥ��ֱ���˳�
    if (QSeq == copy_p->packet->data.packet_data.seq_group.queue_seq ||
        (copy_p_next != NULL && QSeq == copy_p_next->packet->data.packet_data.seq_group.queue_seq)) {
        printf("!!!!!!!QNum.%d QSeq.%d: repeat packet!\n", QNum, QSeq);
        exit(0);
    }

    // ����Packet�����������
    copy_p->next = new_pk_list_node;
    new_pk_list_node->next = copy_p_next;
    recv_buf[QNum].pk_list.length++;

    return 1;
}

// �ύPacket�������
int commit_pk_list(unsigned char QNum, Recv_List_Node recv_buf[]) {
    while (recv_buf[QNum].pk_list.length != 0 && recv_buf[QNum].require_seq ==
                                                 recv_buf[QNum].pk_list.head->packet->data.packet_data.seq_group.queue_seq) {
        recv_buf[QNum].require_seq++;
        Packet_List_Node *pk_node = recv_buf[QNum].pk_list.head;
        recv_buf[QNum].pk_list.head = pk_node->next;
        recv_buf[QNum].pk_list.length--;

        printf("    cache queue[%d] committed to QSeq.%d \n", QNum, (unsigned char) (recv_buf[QNum].require_seq - 1));
        show_recv_buf_queue(recv_buf, QNum);

        // �ͷ��ڴ�
        free(pk_node->packet);
        free(pk_node);
    }

    return 1;
}

// ���� ack response Packet
Packet create_ack_response(int seq_num, unsigned short required_seq[]) {
    Packet *pk_p = (Packet *) malloc(sizeof(Packet));
    pk_p->length = (unsigned char) seq_num;
    pk_p->type = ack_response;

    for (int i = 0; i < seq_num; i++) {
        pk_p->data.packet_ack_response.sequence[i] = required_seq[i];
    }

    return *pk_p;
}

// ��ӡ Packet �е���Ϣ
void show_packet(Packet packet) {
    printf("  Packet Info:\n");
    switch (packet.type) {
        case reliable0:
            printf("   type: reliable0\n");
            printf("   length: %d\n", packet.length);
            printf("   Seq: %d   QNum: %d   QSeq: %d\n",
                   packet.data.packet_data.seq_group.sequence,
                   packet.data.packet_data.seq_group.queue_num,
                   packet.data.packet_data.seq_group.queue_seq);
            break;
        case reliable2:
            printf("   type: reliable2\n");
            printf("   length: %d\n", packet.length);
            printf("   Seq: %d   QNum: %d   QSeq: %d\n",
                   packet.data.packet_data.seq_group.sequence,
                   packet.data.packet_data.seq_group.queue_num,
                   packet.data.packet_data.seq_group.queue_seq);
            break;
        case reliable4:
            printf("   type: reliable4\n");
            printf("   length: %d\n", packet.length);
            printf("   Seq: %d   QNum: %d   QSeq: %d\n",
                   packet.data.packet_data.seq_group.sequence,
                   packet.data.packet_data.seq_group.queue_num,
                   packet.data.packet_data.seq_group.queue_seq);
            break;
        case reliable6:
            printf("   type: reliable6\n");
            printf("   length: %d\n", packet.length);
            printf("   Seq: %d   QNum: %d   QSeq: %d\n",
                   packet.data.packet_data.seq_group.sequence,
                   packet.data.packet_data.seq_group.queue_num,
                   packet.data.packet_data.seq_group.queue_seq);
            break;
        case ack_request:
            printf("    type: ack_request\n");
            printf("    length: %d\n", packet.length);
            printf("    Abandoned Sequence Group:\n");
            for (int i = 0; i < packet.length; i++) {
                printf("        Seq: %d   QNum: %d   QSeq: %d\n",
                       packet.data.packet_ack_request.seq_group[i].sequence,
                       packet.data.packet_ack_request.seq_group[i].queue_num,
                       packet.data.packet_ack_request.seq_group[i].queue_seq);
            }
            break;
        case ack_response:
            printf("   type: ack_response\n");
            printf("   length: %d\n", packet.length);
            printf("   Required Sequence: ");
            for (int i = 0; i < packet.length; i++) {
                printf("%d ", packet.data.packet_ack_response.sequence[i]);
            }
            printf("\n");
            break;
        default:
            printf("type: invalid type!\n");
            break;
    }
    printf("\n");
}

// ��ӡ���������Ϣ
void show_seq_link_list(Seq_Link_List seq_link_list) {
    printf("    seq_link_list:");

    if (seq_link_list.length == 0) {
        printf("NULL");
    }

    Seq_Link_List_Node *node = seq_link_list.head;
    while (node != NULL) {
        printf("%d-->", node->sequence);
        node = node->next;
    }
    printf("\n");
}

// ���������������Ϣ
void show_recv_buf_queue(Recv_List_Node recv_buf[], unsigned char QNum) {
    printf("    cache queue[%d] info:", QNum);

    if (recv_buf[QNum].pk_list.length == 0) {
        printf("NULL");
    }

    Packet_List_Node *node = recv_buf[QNum].pk_list.head;
    while (node != NULL) {
        printf("%d-->", node->packet->data.packet_data.seq_group.queue_seq);
        node = node->next;
    }

    printf("\n");
}
